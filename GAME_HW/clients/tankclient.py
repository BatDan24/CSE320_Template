"""Client library for the CSE 320 tank server protocol.

Used by the GUI client and the CPU bot client.
Requires Python 3.8+ and only the standard library.

A TankClient owns one TCP connection. A background reader thread parses
everything the server sends:

  * FRAME blocks   -> client.frame (latest Frame), on_frame callback
  * CHAT lines     -> client.chat (queue.Queue of (name, text))
  * GAMEOVER block -> client.gameover (list of ScoreRow), client.game_over event
  * everything else is a reply to one of our commands -> client.replies

Replies arrive in the same order as the commands that caused them, so
request() simply sends a command and waits for the next reply.
"""

import queue
import socket
import threading

DIRS = "NESW"
DELTA = {"N": (0, -1), "E": (1, 0), "S": (0, 1), "W": (-1, 0)}
OPPOSITE = {"N": "S", "S": "N", "E": "W", "W": "E"}
MAX_HP = 3
MAX_BULLETS_PER_TANK = 3
MOVE_COOLDOWN_TICKS = 2
FIRE_COOLDOWN_TICKS = 5
RESPAWN_TICKS = 40


class ProtocolError(Exception):
    """The server sent something that does not follow the protocol."""


class Tank:
    __slots__ = ("id", "name", "x", "y", "dir", "hp", "alive",
                 "kills", "deaths", "shots", "hits")

    @classmethod
    def parse(cls, words):
        # T <id> <name> <x> <y> <dir> <hp> <alive> <kills> <deaths> <shots> <hits>
        if len(words) != 12 or words[0] != "T":
            raise ProtocolError("bad tank line: %r" % " ".join(words))
        t = cls()
        t.id = int(words[1])
        t.name = words[2]
        t.x, t.y = int(words[3]), int(words[4])
        t.dir = words[5]
        t.hp, t.alive = int(words[6]), words[7] == "1"
        t.kills, t.deaths = int(words[8]), int(words[9])
        t.shots, t.hits = int(words[10]), int(words[11])
        return t

    def __repr__(self):
        return "Tank(%d %s @%d,%d %s hp=%d alive=%d k=%d d=%d)" % (
            self.id, self.name, self.x, self.y, self.dir, self.hp,
            self.alive, self.kills, self.deaths)


class Bullet:
    __slots__ = ("id", "owner", "x", "y", "dir")

    @classmethod
    def parse(cls, words):
        # B <id> <owner_id> <x> <y> <dir>
        if len(words) != 6 or words[0] != "B":
            raise ProtocolError("bad bullet line: %r" % " ".join(words))
        b = cls()
        b.id, b.owner = int(words[1]), int(words[2])
        b.x, b.y = int(words[3]), int(words[4])
        b.dir = words[5]
        return b


class Frame:
    """One snapshot of the world, sent by the server once per tick."""

    def __init__(self, tick):
        self.tick = tick
        self.tanks = {}    # id -> Tank
        self.bullets = []  # [Bullet]
        self.events = []   # [[word, ...]], e.g. ["KILL", "3", "5"]

    def tank_by_name(self, name):
        for t in self.tanks.values():
            if t.name == name:
                return t
        return None


class ScoreRow:
    __slots__ = ("id", "name", "kills", "deaths", "shots", "hits")

    @classmethod
    def parse(cls, words):
        # S <id> <name> <kills> <deaths> <shots> <hits>
        if len(words) != 7 or words[0] != "S":
            raise ProtocolError("bad score line: %r" % " ".join(words))
        r = cls()
        r.id, r.name = int(words[1]), words[2]
        r.kills, r.deaths = int(words[3]), int(words[4])
        r.shots, r.hits = int(words[5]), int(words[6])
        return r


class GameMap:
    def __init__(self, width, height, rows):
        self.width = width
        self.height = height
        self.rows = rows

    def is_floor(self, x, y):
        return 0 <= x < self.width and 0 <= y < self.height and self.rows[y][x] == "."

    def floor_cells(self):
        return [(x, y) for y in range(self.height) for x in range(self.width)
                if self.rows[y][x] == "."]

    def clear_line(self, x0, y0, x1, y1):
        """True if (x0,y0) and (x1,y1) share a row or column with no wall between."""
        if x0 != x1 and y0 != y1:
            return False
        dx = (x1 > x0) - (x1 < x0)
        dy = (y1 > y0) - (y1 < y0)
        x, y = x0 + dx, y0 + dy
        while (x, y) != (x1, y1):
            if not self.is_floor(x, y):
                return False
            x, y = x + dx, y + dy
        return True


class Reply:
    """A reply line (or block) sent in response to one of our commands."""

    def __init__(self, words, game_map=None, scores=None):
        self.words = words
        self.kind = words[0] if words else ""
        self.map = game_map
        self.scores = scores

    @property
    def ok(self):
        return self.kind in ("OK", "WELCOME", "WATCHING", "PONG", "BYE",
                             "SCORES", "STATS")

    @property
    def error(self):
        return self.words[1] if self.kind == "ERR" and len(self.words) > 1 else None

    def stats(self):
        """Parse 'STATS k=v k=v ...' into a dict of ints."""
        if self.kind != "STATS":
            raise ProtocolError("not a STATS reply: %r" % " ".join(self.words))
        out = {}
        for kv in self.words[1:]:
            k, v = kv.split("=", 1)
            out[k] = int(v)
        return out

    def __repr__(self):
        return "Reply(%s)" % " ".join(self.words)


class TankClient:
    def __init__(self, host="127.0.0.1", port=9000, on_frame=None,
                 keep_replies=True, connect_timeout=5.0):
        """Connect to the server.

        on_frame:     optional callback(frame), called on the reader thread.
        keep_replies: if False, replies are counted but not stored (useful
                      when pipelining many commands without waiting).
        """
        self.sock = socket.create_connection((host, port), timeout=connect_timeout)
        self.sock.settimeout(None)
        self._rfile = self.sock.makefile("r", encoding="ascii", newline="\n")
        self._send_lock = threading.Lock()
        self._frame_cond = threading.Condition()
        self._reply_cond = threading.Condition()

        self.on_frame = on_frame
        self.keep_replies = keep_replies
        self.replies = queue.Queue()
        self.reply_count = 0
        self.chat = queue.Queue()
        self.frame = None
        self.frame_count = 0
        self.gameover = None
        self.game_over = threading.Event()
        self.closed = threading.Event()
        self.error = None

        self.map = None
        self.my_id = None
        self.tick_rate = None
        self.lines_sent = 0

        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    # ------------------------------------------------------------------ I/O

    def send(self, line):
        """Send one command line without waiting for its reply."""
        data = (line + "\n").encode("ascii")
        with self._send_lock:
            self.sock.sendall(data)
            if line.strip():
                self.lines_sent += 1

    def sync(self, timeout=10.0):
        """Wait until every command sent so far has been answered.

        The server sends exactly one reply per non-empty command line, in
        order, so once reply_count == lines_sent the server has finished
        processing everything we sent.
        """
        with self._reply_cond:
            ok = self._reply_cond.wait_for(
                lambda: self.closed.is_set() or self.reply_count >= self.lines_sent,
                timeout)
        if not ok:
            raise TimeoutError("%d of %d replies after %.1fs" %
                               (self.reply_count, self.lines_sent, timeout))
        return self.reply_count >= self.lines_sent

    def next_reply(self, timeout=5.0):
        try:
            return self.replies.get(timeout=timeout)
        except queue.Empty:
            if self.closed.is_set():
                raise ConnectionError("connection closed by server")
            raise TimeoutError("no reply from server within %.1fs" % timeout)

    def request(self, line, timeout=5.0):
        """Send one command and return its Reply."""
        self.send(line)
        return self.next_reply(timeout)

    def close(self):
        try:
            self.sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self.sock.close()

    # ------------------------------------------------------------ Commands

    def join(self, name, timeout=5.0):
        """JOIN as a player. On WELCOME, my_id, tick_rate, and map are set."""
        return self.request("JOIN %s" % name, timeout)

    def watch(self, timeout=5.0):
        """WATCH as a spectator. On WATCHING, tick_rate and map are set."""
        return self.request("WATCH", timeout)

    def quit(self, timeout=5.0):
        """Send QUIT, wait for BYE (all earlier commands are then done), close."""
        try:
            if self.keep_replies:
                return self.request("QUIT", timeout)
            self.send("QUIT")
            self.sync(timeout)
            return None
        finally:
            self.close()

    def wait_frame(self, after_tick=None, timeout=5.0):
        """Wait for a frame newer than after_tick (or any frame) and return it."""
        with self._frame_cond:
            ok = self._frame_cond.wait_for(
                lambda: self.closed.is_set() or (
                    self.frame is not None and
                    (after_tick is None or self.frame.tick > after_tick)),
                timeout)
            if not ok or self.frame is None:
                raise TimeoutError("no frame within %.1fs" % timeout)
            return self.frame

    def my_tank(self, frame=None):
        frame = frame or self.frame
        if frame is None or self.my_id is None:
            return None
        return frame.tanks.get(self.my_id)

    # --------------------------------------------------------------- Parser

    def _readline(self):
        line = self._rfile.readline()
        if not line:
            raise EOFError()
        return line.rstrip("\r\n")

    def _read_block(self, count):
        return [self._readline() for _ in range(count)]

    def _expect_end(self):
        line = self._readline()
        if line != "END":
            raise ProtocolError("expected END, got %r" % line)

    def _read_map(self):
        words = self._readline().split()
        if len(words) != 3 or words[0] != "MAP":
            raise ProtocolError("expected MAP header, got %r" % " ".join(words))
        w, h = int(words[1]), int(words[2])
        rows = self._read_block(h)
        self._expect_end()
        return GameMap(w, h, rows)

    def _read_scores(self, words):
        if len(words) != 2 or words[0] != "SCORES":
            raise ProtocolError("expected SCORES header, got %r" % " ".join(words))
        rows = [ScoreRow.parse(l.split()) for l in self._read_block(int(words[1]))]
        self._expect_end()
        return rows

    def _read_frame(self, words):
        # FRAME <tick> <ntanks> <nbullets> <nevents>
        if len(words) != 5:
            raise ProtocolError("bad FRAME header: %r" % " ".join(words))
        frame = Frame(int(words[1]))
        for line in self._read_block(int(words[2])):
            t = Tank.parse(line.split())
            frame.tanks[t.id] = t
        for line in self._read_block(int(words[3])):
            frame.bullets.append(Bullet.parse(line.split()))
        for line in self._read_block(int(words[4])):
            ev = line.split()
            if not ev or ev[0] != "E":
                raise ProtocolError("bad event line: %r" % line)
            frame.events.append(ev[1:])
        self._expect_end()
        return frame

    def _deliver(self, reply):
        if self.keep_replies:
            self.replies.put(reply)
        with self._reply_cond:
            self.reply_count += 1
            self._reply_cond.notify_all()

    def _read_loop(self):
        try:
            while True:
                line = self._readline()
                words = line.split()
                if not words:
                    continue
                kind = words[0]
                if kind == "FRAME":
                    frame = self._read_frame(words)
                    with self._frame_cond:
                        self.frame = frame
                        self.frame_count += 1
                        self._frame_cond.notify_all()
                    if self.on_frame is not None:
                        self.on_frame(frame)
                elif kind == "CHAT":
                    parts = line.split(" ", 2)
                    self.chat.put((parts[1], parts[2] if len(parts) > 2 else ""))
                elif kind == "GAMEOVER":
                    self.gameover = self._read_scores(self._readline().split())
                    self.game_over.set()
                elif kind in ("WELCOME", "WATCHING"):
                    game_map = self._read_map()
                    self.map = game_map
                    self.tick_rate = int(words[-1])
                    if kind == "WELCOME":
                        self.my_id = int(words[1])
                    self._deliver(Reply(words, game_map=game_map))
                elif kind == "SCORES":
                    self._deliver(Reply(words, scores=self._read_scores(words)))
                else:
                    self._deliver(Reply(words))
        except EOFError:
            pass
        except (OSError, ValueError) as e:
            self.error = e
        except ProtocolError as e:
            self.error = e
        finally:
            try:
                self._rfile.close()
            except OSError:
                pass
            self.closed.set()
            with self._frame_cond:
                self._frame_cond.notify_all()
            with self._reply_cond:
                self._reply_cond.notify_all()
