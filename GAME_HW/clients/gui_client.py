#!/usr/bin/env python3
"""Graphical client for the CSE 320 tank server (tkinter, standard library only).

    python3 clients/gui_client.py -p 9000 --name alice     # play
    python3 clients/gui_client.py -p 9000 --watch          # spectate

Controls (click the arena first so it has keyboard focus):
    Arrow keys / WASD       move (turn and drive one cell)
    Shift + arrow / WASD    turn in place
    Space                   fire
    Enter                   type a chat message (Enter again to send, Esc to cancel)
    Q                       quit

Threads: tkinter may only be used from the thread that created the window,
but the network must be read continuously. The TankClient reader thread
therefore only stores the latest frame and queues chat and replies
(producer), and the GUI thread polls them every few milliseconds with
root.after() and redraws (consumer).
"""

import argparse
import os
import queue
import sys
import time
import tkinter as tk

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tankclient import DELTA, MAX_HP, MOVE_COOLDOWN_TICKS, TankClient  # noqa: E402

COLORS = ["#e6194b", "#3cb44b", "#4363d8", "#f58231", "#911eb4", "#46f0f0",
          "#f032e6", "#bcf60c", "#fabebe", "#008080", "#e6beff", "#9a6324",
          "#fffac8", "#800000", "#aaffc3", "#808000"]
BG = "#1e1e24"
FLOOR = "#2b2b33"
WALL = "#6b6b78"
TEXT = "#e8e8f0"
MUTED = "#9a9aac"

KEY_DIRS = {"Up": "N", "Down": "S", "Left": "W", "Right": "E",
            "w": "N", "s": "S", "a": "W", "d": "E",
            "W": "N", "S": "S", "A": "W", "D": "E"}


class TankGui:
    def __init__(self, root, client, cell, spectator):
        self.root = root
        self.client = client
        self.cell = cell
        self.spectator = spectator
        self.names = {}          # tank id -> name (remembered after they leave)
        self.feed = []           # recent kill feed lines
        self.status = ""
        self.status_until = 0.0
        self.held = []           # movement keys currently held, newest last
        self.shift = False
        self.last_move = 0.0
        self.last_tick = -1
        self.closing = False

        m = client.map
        width, height = m.width * cell, m.height * cell
        root.title("CSE 320 Tanks - %s" % ("spectator" if spectator else
                                           "tank %d" % client.my_id))
        root.configure(bg=BG)

        left = tk.Frame(root, bg=BG)
        left.pack(side=tk.LEFT, padx=8, pady=8)
        self.canvas = tk.Canvas(left, width=width, height=height, bg=FLOOR,
                                highlightthickness=0)
        self.canvas.pack()
        self.status_var = tk.StringVar()
        tk.Label(left, textvariable=self.status_var, bg=BG, fg=MUTED,
                 anchor="w", font=("Helvetica", 11)).pack(fill=tk.X, pady=(6, 0))

        right = tk.Frame(root, bg=BG)
        right.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 8), pady=8)
        tk.Label(right, text="Scoreboard", bg=BG, fg=TEXT,
                 font=("Helvetica", 13, "bold")).pack(anchor="w")
        self.scores = tk.Text(right, width=30, height=18, bg=BG, fg=TEXT,
                              relief=tk.FLAT, font=("Courier", 11))
        self.scores.pack(anchor="w")
        tk.Label(right, text="Events & chat", bg=BG, fg=TEXT,
                 font=("Helvetica", 13, "bold")).pack(anchor="w", pady=(8, 0))
        self.log = tk.Text(right, width=30, height=10, bg=BG, fg=MUTED,
                           relief=tk.FLAT, font=("Courier", 10), wrap=tk.WORD)
        self.log.pack(anchor="w", fill=tk.Y, expand=True)
        self.chat_entry = tk.Entry(right, bg=FLOOR, fg=TEXT, insertbackground=TEXT,
                                   relief=tk.FLAT)
        self.chat_entry.pack(fill=tk.X, pady=(6, 0))
        self.chat_entry.bind("<Return>", self.send_chat)
        self.chat_entry.bind("<Escape>", lambda e: self.canvas.focus_set())
        if spectator:
            self.chat_entry.configure(state=tk.DISABLED)

        # Walls never change: draw them once.
        for y in range(m.height):
            for x in range(m.width):
                if not m.is_floor(x, y):
                    self.canvas.create_rectangle(x * cell, y * cell, (x + 1) * cell,
                                                 (y + 1) * cell, fill=WALL,
                                                 outline=WALL)

        self.canvas.bind("<Button-1>", lambda e: self.canvas.focus_set())
        self.canvas.bind("<KeyPress>", self.key_down)
        self.canvas.bind("<KeyRelease>", self.key_up)
        root.protocol("WM_DELETE_WINDOW", self.quit)
        self.canvas.focus_set()
        self.poll()

    # --------------------------------------------------------------- input

    def key_down(self, event):
        k = event.keysym
        if k in ("Shift_L", "Shift_R"):
            self.shift = True
        elif k == "space":
            self.command("FIRE")
        elif k == "Return":
            self.chat_entry.focus_set()
        elif k in ("q", "Q"):
            self.quit()
        elif k in KEY_DIRS:
            d = KEY_DIRS[k]
            if self.shift or event.state & 0x1:
                self.command("TURN %s" % d)
            else:
                if d in self.held:
                    self.held.remove(d)
                self.held.append(d)
                self.try_move(force=True)

    def key_up(self, event):
        k = event.keysym
        if k in ("Shift_L", "Shift_R"):
            self.shift = False
        elif k in KEY_DIRS and KEY_DIRS[k] in self.held:
            self.held.remove(KEY_DIRS[k])

    def try_move(self, force=False):
        """Send at most one MOVE per move cooldown while a key is held."""
        if not self.held or self.spectator:
            return
        interval = MOVE_COOLDOWN_TICKS / float(self.client.tick_rate or 20)
        now = time.time()
        if force or now - self.last_move >= interval:
            self.last_move = now
            self.command("MOVE %s" % self.held[-1])

    def command(self, line):
        if self.spectator or self.client.closed.is_set():
            return
        try:
            self.client.send(line)
        except OSError:
            pass

    def send_chat(self, event=None):
        text = self.chat_entry.get().strip()
        self.chat_entry.delete(0, tk.END)
        if text:
            self.command("SAY %s" % text[:200])
        self.canvas.focus_set()

    def quit(self):
        if self.closing:
            return
        self.closing = True
        try:
            if not self.client.closed.is_set():
                self.client.send("QUIT")
        except OSError:
            pass
        self.client.close()
        self.root.destroy()

    # -------------------------------------------------------------- update

    def set_status(self, text, seconds=1.5):
        self.status = text
        self.status_until = time.time() + seconds

    def add_log(self, line):
        self.log.insert(tk.END, line + "\n")
        self.log.see(tk.END)

    def poll(self):
        if self.closing:
            return
        c = self.client
        # Replies to our commands: only errors are interesting to show.
        while True:
            try:
                r = c.replies.get_nowait()
            except queue.Empty:
                break
            if r.kind == "ERR" and r.error not in ("COOLDOWN",):
                self.set_status("Server: %s" % r.error)
        while True:
            try:
                name, text = c.chat.get_nowait()
            except queue.Empty:
                break
            self.add_log("<%s> %s" % (name, text))

        self.try_move()
        frame = c.frame
        if frame is not None and frame.tick != self.last_tick:
            self.last_tick = frame.tick
            self.draw(frame)

        if c.game_over.is_set():
            self.show_game_over(c.gameover)
            return
        if c.closed.is_set():
            self.status_var.set("Disconnected from server.")
            return
        self.root.after(15, self.poll)

    def draw(self, frame):
        cv, cs = self.canvas, self.cell
        cv.delete("dyn")
        for t in frame.tanks.values():
            self.names[t.id] = t.name
        for ev in frame.events:
            if ev[0] == "KILL":
                killer = self.names.get(int(ev[1]), "?")
                victim = self.names.get(int(ev[2]), "?")
                self.add_log("%s destroyed %s" % (killer, victim))
            elif ev[0] == "JOIN":
                self.add_log("%s joined" % ev[2])
            elif ev[0] == "LEAVE":
                self.add_log("%s left" % self.names.get(int(ev[1]), "?"))

        for b in frame.bullets:
            cx, cy = (b.x + 0.5) * cs, (b.y + 0.5) * cs
            r = max(2, cs // 7)
            cv.create_oval(cx - r, cy - r, cx + r, cy + r, fill="#ffe066",
                           outline="", tags="dyn")

        for t in frame.tanks.values():
            color = COLORS[(t.id - 1) % len(COLORS)]
            x0, y0 = t.x * cs, t.y * cs
            pad = max(2, cs // 8)
            if not t.alive:
                cv.create_line(x0 + pad, y0 + pad, x0 + cs - pad, y0 + cs - pad,
                               fill=color, width=2, tags="dyn")
                cv.create_line(x0 + cs - pad, y0 + pad, x0 + pad, y0 + cs - pad,
                               fill=color, width=2, tags="dyn")
                continue
            mine = (t.id == self.client.my_id)
            cv.create_rectangle(x0 + pad, y0 + pad, x0 + cs - pad, y0 + cs - pad,
                                fill=color, outline="white" if mine else "",
                                width=2, tags="dyn")
            cx, cy = x0 + cs / 2.0, y0 + cs / 2.0
            dx, dy = DELTA.get(t.dir, (0, -1))
            cv.create_line(cx, cy, cx + dx * cs * 0.6, cy + dy * cs * 0.6,
                           fill="black", width=max(2, cs // 6), tags="dyn")
            for i in range(MAX_HP):
                fill = "#7CFC00" if i < t.hp else "#444"
                hx = x0 + pad + i * (cs - 2 * pad) / float(MAX_HP)
                cv.create_rectangle(hx, y0, hx + (cs - 2 * pad) / float(MAX_HP) - 1,
                                    y0 + max(2, cs // 10), fill=fill, outline="",
                                    tags="dyn")
            cv.create_text(cx, y0 + cs + 1, text=t.name, fill=TEXT, anchor="n",
                           font=("Helvetica", max(7, cs // 3)), tags="dyn")

        rows = sorted(frame.tanks.values(), key=lambda t: (-t.kills, t.deaths, t.id))
        self.scores.delete("1.0", tk.END)
        self.scores.insert(tk.END, "%-14s %3s %3s %3s\n" % ("name", "K", "D", "HP"))
        for t in rows:
            marker = "*" if t.id == self.client.my_id else " "
            self.scores.insert(tk.END, "%s%-13s %3d %3d %3s\n" % (
                marker, t.name, t.kills, t.deaths, t.hp if t.alive else "--"))

        me = self.client.my_tank(frame)
        if time.time() < self.status_until:
            text = self.status
        elif self.spectator:
            text = "Spectating - tick %d" % frame.tick
        elif me is None:
            text = "Waiting..."
        elif not me.alive:
            text = "Destroyed! Respawning soon..."
        else:
            text = "Tick %d   HP %d/%d   shots %d   hits %d" % (
                frame.tick, me.hp, MAX_HP, me.shots, me.hits)
        self.status_var.set(text)

    def show_game_over(self, rows):
        cv = self.canvas
        w, h = int(cv["width"]), int(cv["height"])
        cv.create_rectangle(w * 0.15, h * 0.15, w * 0.85, h * 0.85, fill=BG,
                            outline=TEXT, width=2, tags="dyn")
        lines = ["GAME OVER", ""]
        for i, r in enumerate(rows or []):
            lines.append("%2d. %-15s %3d kills %3d deaths" % (i + 1, r.name,
                                                              r.kills, r.deaths))
        cv.create_text(w / 2, h * 0.2, text="\n".join(lines), fill=TEXT,
                       anchor="n", font=("Courier", 13), tags="dyn")
        self.status_var.set("Match over. Close the window to exit.")


def main():
    ap = argparse.ArgumentParser(description="GUI client for the CSE 320 tank server.")
    ap.add_argument("-H", "--host", default="127.0.0.1")
    ap.add_argument("-p", "--port", type=int, default=9000)
    ap.add_argument("-n", "--name", default=None, help="tank name (default: $USER)")
    ap.add_argument("--watch", action="store_true", help="spectate instead of playing")
    ap.add_argument("--cell", type=int, default=24, help="cell size in pixels")
    args = ap.parse_args()

    try:
        client = TankClient(args.host, args.port)
    except OSError as e:
        sys.exit("Cannot connect to %s:%d: %s" % (args.host, args.port, e))

    if args.watch:
        r = client.watch()
    else:
        name = args.name or "".join(ch for ch in os.environ.get("USER", "player")
                                    if ch.isalnum() or ch in "_-")[:15] or "player"
        r = client.join(name)
    if r.kind not in ("WELCOME", "WATCHING"):
        client.close()
        sys.exit("Server refused: %s" % " ".join(r.words))

    root = tk.Tk()
    TankGui(root, client, args.cell, args.watch)
    root.mainloop()


if __name__ == "__main__":
    main()
