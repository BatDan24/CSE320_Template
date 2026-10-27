#!/usr/bin/env python3
"""CPU-controlled tanks to play against the CSE 320 tank server.

Examples:
    python3 clients/bot_client.py -p 9000                      # 4 mixed bots
    python3 clients/bot_client.py -p 9000 -n 8 -s hunter       # 8 hunters
    python3 clients/bot_client.py -p 9000 -n 6 -s spammer -d 10

Strategies:
    hunter   chase the nearest enemy, line up with it, and shoot; dodge bullets
    wander   drive around at random and fire now and then
    sniper   never move; turn toward any enemy in a clear line and shoot
    spammer  heavy load: pipeline random commands as fast as allowed
             without waiting for replies (use with -r to set the rate)
    mix      cycle through hunter, wander, and sniper

Every bot runs on its own Python thread with its own connection, so N bots
means N concurrent clients for the server.
"""

import argparse
import collections
import os
import random
import signal
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tankclient import DELTA, DIRS, TankClient  # noqa: E402

STRATEGIES = ("hunter", "wander", "sniper", "spammer")


def direction_to(x0, y0, x1, y1):
    """Direction from (x0,y0) to an aligned cell (x1,y1)."""
    if x0 == x1:
        return "S" if y1 > y0 else "N"
    return "E" if x1 > x0 else "W"


def line_of_fire_cells(game_map, x, y):
    """Floor cells that have a clear straight line to (x, y)."""
    cells = set()
    for d in DIRS:
        dx, dy = DELTA[d]
        cx, cy = x + dx, y + dy
        while game_map.is_floor(cx, cy):
            cells.add((cx, cy))
            cx, cy = cx + dx, cy + dy
    return cells


def first_step(game_map, start, goals, blocked, limit=4000):
    """BFS from start to any cell in goals. Returns the first direction or None."""
    if start in goals:
        return None
    seen = {start: None}
    q = collections.deque([start])
    while q and len(seen) < limit:
        cur = q.popleft()
        for d in DIRS:
            dx, dy = DELTA[d]
            nxt = (cur[0] + dx, cur[1] + dy)
            if nxt in seen or nxt in blocked or not game_map.is_floor(*nxt):
                continue
            seen[nxt] = (cur, d)
            if nxt in goals:
                # Walk back to the step taken from start.
                node = nxt
                while seen[node][0] != start:
                    node = seen[node][0]
                return seen[node][1]
            q.append(nxt)
    return None


def incoming_bullet(frame, me, horizon=5):
    """Direction of an enemy bullet about to hit me, or None."""
    for b in frame.bullets:
        if b.owner == me.id:
            continue
        dx, dy = DELTA[b.dir]
        for k in range(1, horizon + 1):
            if (b.x + dx * k, b.y + dy * k) == (me.x, me.y):
                return b.dir
    return None


class Bot(threading.Thread):
    """One CPU player. Call start(), later stop.set() and join()."""

    def __init__(self, host, port, name, strategy, seed=None, rate=200,
                 stop=None, quit_when_done=True, verbose=False):
        super().__init__(daemon=True, name="bot-" + name)
        self.host, self.port = host, port
        self.name = name
        self.strategy = strategy
        self.rng = random.Random(seed)
        self.rate = rate
        self.stop = stop or threading.Event()
        self.quit_when_done = quit_when_done
        self.verbose = verbose

        self.client = None
        self.joined = threading.Event()   # set once JOIN succeeded (or failed)
        self.idle = threading.Event()     # set once the bot stopped acting
        self.join_error = None
        self.errors = collections.Counter()
        self.exception = None
        self._wander_dir = self.rng.choice(DIRS)

    # ---------------------------------------------------------------- main

    def run(self):
        try:
            self.client = TankClient(self.host, self.port,
                                     keep_replies=(self.strategy != "spammer"))
            if self.strategy == "spammer":
                self.client.send("JOIN %s" % self.name)
                self.client.sync()
                if self.client.my_id is None:
                    self.join_error = "JOIN_FAILED"
            else:
                r = self.client.join(self.name)
                if r.kind != "WELCOME":
                    self.join_error = r.error or r.kind
            self.joined.set()
            if self.join_error is None:
                if self.strategy == "spammer":
                    self._spam()
                else:
                    self._play()
        except Exception as e:  # report instead of dying silently
            server_closed = self.client is not None and self.client.closed.is_set()
            if not (server_closed and isinstance(e, (OSError, TimeoutError))):
                self.exception = e
                if self.verbose:
                    print("[%s] %r" % (self.name, e), file=sys.stderr)
        finally:
            self.joined.set()
            self.idle.set()
            if self.quit_when_done and self.client is not None:
                self.shutdown()

    def shutdown(self):
        try:
            if not self.client.closed.is_set():
                self.client.quit()
        except Exception:
            self.client.close()

    def _log(self, msg):
        if self.verbose:
            print("[%s] %s" % (self.name, msg))

    # --------------------------------------------------- frame-driven play

    def _play(self):
        c = self.client
        last_tick = None
        decide = getattr(self, "_decide_" + self.strategy)
        while not self.stop.is_set() and not c.closed.is_set():
            try:
                frame = c.wait_frame(after_tick=last_tick, timeout=1.0)
            except TimeoutError:
                continue
            last_tick = frame.tick
            me = frame.tanks.get(c.my_id)
            if me is None or not me.alive:
                continue
            cmd = decide(frame, me)
            if cmd is None:
                continue
            r = c.request(cmd)
            if r.kind == "ERR":
                self.errors[r.error] += 1
        self.idle.set()
        if not self.quit_when_done:
            self.stop.wait()

    def _enemies(self, frame, me):
        return [t for t in frame.tanks.values() if t.alive and t.id != me.id]

    def _shoot_if_lined_up(self, frame, me, enemies):
        m = self.client.map
        for e in sorted(enemies, key=lambda t: abs(t.x - me.x) + abs(t.y - me.y)):
            if m.clear_line(me.x, me.y, e.x, e.y):
                d = direction_to(me.x, me.y, e.x, e.y)
                return "FIRE" if me.dir == d else "TURN %s" % d
        return None

    def _dodge(self, frame, me):
        bdir = incoming_bullet(frame, me)
        if bdir is None:
            return None
        sides = ["E", "W"] if bdir in "NS" else ["N", "S"]
        self.rng.shuffle(sides)
        occupied = {(t.x, t.y) for t in frame.tanks.values() if t.alive}
        for d in sides:
            nx, ny = me.x + DELTA[d][0], me.y + DELTA[d][1]
            if self.client.map.is_floor(nx, ny) and (nx, ny) not in occupied:
                return "MOVE %s" % d
        return None

    def _decide_hunter(self, frame, me):
        cmd = self._dodge(frame, me)
        if cmd:
            return cmd
        enemies = self._enemies(frame, me)
        if not enemies:
            return self._decide_wander(frame, me)
        cmd = self._shoot_if_lined_up(frame, me, enemies)
        if cmd:
            return cmd
        target = min(enemies, key=lambda t: abs(t.x - me.x) + abs(t.y - me.y))
        goals = line_of_fire_cells(self.client.map, target.x, target.y)
        blocked = {(t.x, t.y) for t in frame.tanks.values()
                   if t.alive and t.id != me.id}
        d = first_step(self.client.map, (me.x, me.y), goals, blocked)
        if d is None:
            return self._decide_wander(frame, me)
        return "MOVE %s" % d

    def _decide_wander(self, frame, me):
        roll = self.rng.random()
        if roll < 0.15:
            return "FIRE"
        if roll < 0.25:
            self._wander_dir = self.rng.choice(DIRS)
            return "TURN %s" % self._wander_dir
        nx, ny = me.x + DELTA[self._wander_dir][0], me.y + DELTA[self._wander_dir][1]
        if not self.client.map.is_floor(nx, ny) or self.rng.random() < 0.1:
            self._wander_dir = self.rng.choice(DIRS)
        return "MOVE %s" % self._wander_dir

    def _decide_sniper(self, frame, me):
        cmd = self._shoot_if_lined_up(frame, me, self._enemies(frame, me))
        if cmd:
            return cmd
        if frame.tick % 10 == 0:
            return "TURN %s" % DIRS[(DIRS.index(me.dir) + 1) % 4]
        return None

    # ----------------------------------------------------- heavy-load client

    def _spam(self):
        c = self.client
        choices = (["MOVE %s" % d for d in DIRS] * 3 +
                   ["TURN %s" % d for d in DIRS] +
                   ["FIRE"] * 4 + ["PING", "SCORES", "STATS"])
        delay = 1.0 / self.rate if self.rate > 0 else 0.0
        while not self.stop.is_set() and not c.closed.is_set():
            c.send(self.rng.choice(choices))
            if delay:
                time.sleep(delay)
        c.sync()
        self.idle.set()
        if not self.quit_when_done:
            self.stop.wait()


def main():
    ap = argparse.ArgumentParser(description="CPU tanks for the CSE 320 tank server.")
    ap.add_argument("-H", "--host", default="127.0.0.1")
    ap.add_argument("-p", "--port", type=int, default=9000)
    ap.add_argument("-n", "--count", type=int, default=4, help="number of bots")
    ap.add_argument("-s", "--strategy", default="mix",
                    choices=STRATEGIES + ("mix",))
    ap.add_argument("--name", default="bot", help="name prefix (default: bot)")
    ap.add_argument("-d", "--duration", type=float, default=0,
                    help="seconds to play (default: until Ctrl-C or GAMEOVER)")
    ap.add_argument("-r", "--rate", type=float, default=200,
                    help="spammer commands per second per bot (0 = unlimited)")
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    stop = threading.Event()
    signal.signal(signal.SIGINT, lambda *_: stop.set())
    mix = ("hunter", "wander", "sniper")
    bots = []
    for i in range(args.count):
        strat = mix[i % len(mix)] if args.strategy == "mix" else args.strategy
        seed = None if args.seed is None else args.seed + i
        bots.append(Bot(args.host, args.port, "%s%d" % (args.name, i), strat,
                        seed=seed, rate=args.rate, stop=stop,
                        verbose=args.verbose))
    for b in bots:
        b.start()

    deadline = time.time() + args.duration if args.duration > 0 else None
    while not stop.is_set():
        if deadline and time.time() >= deadline:
            break
        if all(not b.is_alive() for b in bots):
            break
        if any(b.client and b.client.game_over.is_set() for b in bots):
            break
        time.sleep(0.2)
    stop.set()
    for b in bots:
        b.join(timeout=10)

    print("%-12s %-8s %6s %6s %6s  %s" % ("bot", "strategy", "sent", "kills",
                                          "deaths", "notes"))
    for b in bots:
        c = b.client
        frame = c.frame if c else None
        t = frame.tanks.get(c.my_id) if frame and c.my_id else None
        rows = c.gameover if c and c.gameover else None
        if rows:
            row = next((r for r in rows if r.name == b.name), None)
            kills, deaths = (row.kills, row.deaths) if row else ("-", "-")
        else:
            kills, deaths = (t.kills, t.deaths) if t else ("-", "-")
        notes = b.join_error or (repr(b.exception) if b.exception else "")
        print("%-12s %-8s %6s %6s %6s  %s" % (b.name, b.strategy,
                                              c.lines_sent if c else 0,
                                              kills, deaths, notes))


if __name__ == "__main__":
    main()
