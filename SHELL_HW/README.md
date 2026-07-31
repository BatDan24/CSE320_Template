# CSE 320 — Shell Lab (Job Control)

## Overview

In this assignment you will implement a Unix-like shell with basic job control.
Your shell will:

- Parse command lines and run external programs
- Support built-in commands (`quit`, `jobs`, `bg`, `fg`)
- Support background jobs (`&`) and foreground jobs
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
│   ├── sdriver.pl           # trace-driven driver
│   └── traces/              # basic public traces (trace01–trace05)
├── myspin.c                 # helper program used by traces
├── mysplit.c                # helper program used by traces
├── mystop.c                 # helper program used by traces
└── myint.c                  # helper program used by traces
```

## What you implement

`src/tsh.c` contains the full framework and helper functions. You must implement:

- `eval()`: parse/evaluate each command line
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

## Building

```bash
make
```

This builds:

- `tsh` (your shell)
- helper programs: `myspin`, `mysplit`, `mystop`, `myint`

## Running tests

Run a single basic trace:

```bash
make test01
make test03
```

Run the included basic traces:

```bash
make test
```

Or invoke the driver directly:

```bash
perl test/sdriver.pl -t test/traces/trace01.txt -s ./tsh -a "-p"
```

Final grading uses additional traces beyond the five included here.

## Notes

- Do not rename the prompt (`tsh> `) or change the driver-facing behavior; the
  traces depend on it.
