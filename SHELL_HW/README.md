# CSE 320 — Shell Lab (Job Control)

## Overview

In this assignment you will implement a Unix-like shell with basic job control.
Your shell will:

- Parse command lines and run external programs
- Support built-in commands (`quit`, `jobs`, `bg`, `fg`)
- Support background jobs (`&`) and foreground jobs
- Support file redirection (`<`, `>`, and `>>`) for external commands
- Support pipelines (`|`) as a single job
- Correctly handle `SIGCHLD`, `SIGINT` (Ctrl-C), and `SIGTSTP` (Ctrl-Z)

Only **`src/tsh.c`** is submitted for grading. All other files are provided for
build and testing.

## Project layout

```
SHELL_HW/
├── README.md
├── Makefile
├── src/
│   └── tsh.c                # your tiny shell (submit)
├── test/
│   ├── sdriver.pl           # optional transcript driver
│   └── traces/              # traces for the optional driver
└── tshref.out               # sample transcript from the reference shell
```

## What you implement

`src/tsh.c` contains the full framework and helper functions. You must implement:

- `eval()`: parse/evaluate each command line, including `<`, `>`, `>>`, and `|`
- `builtin_cmd()`: handle built-in commands
- `do_bgfg()`: implement `bg`/`fg` behavior
- `waitfg()`: wait for a foreground job to finish or stop
- `sigchld_handler()`: reap child processes and update job list
- `sigint_handler()`: forward Ctrl-C to the foreground job’s process group
- `sigtstp_handler()`: forward Ctrl-Z to the foreground job’s process group

The provided job list helpers (`addjob`, `deletejob`, `fgpid`, `listjobs`, …) must
be used consistently to keep the job table correct.

## Behavioral requirements

- **Process groups**: each job must run in its own process group so that signals
  (Ctrl-C / Ctrl-Z) can be delivered to the entire job.
- **No races**: block `SIGCHLD` around critical sections where you fork and add a
  job to the job list.
- **Reaping**: `sigchld_handler` must reap all available children (looped `waitpid`)
  and correctly handle:
  - normal exit
  - termination by signal
  - stop by signal
- **`bg`/`fg`**: accept either `%jid` or `pid` syntax, and resume stopped jobs with
  `SIGCONT`.
- **File redirection** (external commands only):
  - `command < infile` reads stdin from `infile`
  - `command > outfile` writes stdout to `outfile` (create or truncate)
  - `command >> outfile` appends stdout to `outfile` (create if it does not exist)
  - `<`, `>`, and `>>` may be combined on one line, in any order: `cmd < in >> out`
  - Redirection works for foreground and background jobs (`cmd >> out &`)
  - If both `>` and `>>` appear on the same command, the last one wins
  - Operators and filenames are **not** arguments; strip them from `argv` before `execve`
  - Operators are separate whitespace-delimited tokens (`cmd >> file`, not `cmd>>file`)
  - Apply `open` / `dup2` in the **child**, after `fork` and `setpgid`, before `execve`
  - `>` uses `O_TRUNC`. `>>` uses `O_APPEND`. Both use `O_WRONLY | O_CREAT`
  - If a redirected file cannot be opened, print `filename: <strerror>` (for a
    missing input file that is `filename: No such file or directory`) and exit
    the child; the shell must keep running
- **Pipes** (external commands only):
  - `cmd1 | cmd2 | cmd3` connects stdout of each stage to stdin of the next
  - Every stage of a pipeline is one process in the **same process group**, and the whole pipeline is **one job** (one `jobs` entry, one `bg` / `fg` target)
  - The job’s pid is the process-group leader (the first stage). `bg` / `fg` and Ctrl-C / Ctrl-Z apply to the whole group
  - A foreground pipeline blocks the shell until **every** stage has exited or the job stops, even if the first stage exits first
  - `cmd1 | cmd2 &` runs the pipeline in the background
  - Redirection binds to the stage it is written on and overrides that stage’s pipe end: `cmd1 < in | cmd2 > out`, `cmd1 | cmd2 >> out`, and `cmd1 > file | cmd2` (cmd1 writes the file; cmd2 sees EOF)
  - An empty stage (`cmd |`, `| cmd`, `cmd | | cmd`) prints `Invalid pipeline` and runs nothing
  - The shell keeps running when a stage cannot be exec’d (`Command not found`)
  - Built-ins are recognized only when the line has a single stage. Pipes of built-ins, stderr (`2>`), and redirection of built-ins (`jobs > file`) are **not** required

`parseline()` does not handle redirection or pipes. It does treat a trailing `&` as background. Parse `|`, `<`, `>`, and `>>` yourself in `eval()` after `parseline` returns. You will need `pipe` and `open` (`<unistd.h>` and `<fcntl.h>`).

## Building

```bash
make
```

This builds:

- `tsh` (your shell)

## Running tests

`make test01` runs the provided test traces with expected output available in `tshref.out`. It does not decide pass or fail.

## Notes

- Do not rename the prompt (`tsh> `). The harness synchronizes on it.

