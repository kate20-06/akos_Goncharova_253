# Smart Flowers — Homework 3 (variant 14)

Four independent implementations of the same flower/gardener simulation in C for Linux.

| Directory | Process model | Communication | Semaphores |
|---|---|---|---|
| `variant_1_unnamed/` | Parent uses `fork()` | POSIX shared memory | Unnamed, process-shared |
| `variant_2_named/` | Parent uses `fork()` | POSIX shared memory | Named POSIX |
| `variant_3_independent/` | Separate executables | POSIX shared memory | Named POSIX |
| `variant_4_messages/` | Separate executables | POSIX message queues | Named POSIX |

Each variant has its own `Makefile`, `README.md`, source code and test cases. The four implementations are built and run independently.

## Build and run

Requirements: Linux, GCC, GNU Make, POSIX IPC support, Bash and Python 3 for tests. For example:

```bash
cd variant_1_unnamed
make
./build/smart_flowers tests/balanced.cfg
./build/smart_flowers            # Enter parameters in the terminal
make check
```

Variant 2 uses the same build and launch commands in its own directory.

For variants 3 and 4, use the launcher with a config file:

```bash
cd variant_3_independent         # or variant_4_messages
make
bash scripts/run_demo.sh tests/balanced.cfg
make check
```

To enter parameters **interactively** in variants 3 and 4, start the garden controller with a session name only:

```bash
./build/garden demo
```

After entering the parameters, start the configured workers in separate terminals:

```bash
./build/flower demo 1
./build/gardener demo 1
```

For more flowers or gardeners, launch each number from 1 up to the number entered. The controller waits up to 15 seconds for all workers. You can instead start the controller with a config file:

```bash
./build/garden tests/one_flower.cfg demo
```

The helper `run_demo.sh` reads the worker counts from a `.cfg` file, so it is for file-based runs only. Each run writes a log; press Ctrl+C in the controller terminal to interrupt. The session identifier must be unique while a session is active.

## Input restrictions

Every flower starts with 100 moisture units. The request threshold must be **strictly greater** than the drying/death threshold, so that there is time to request watering before death. For example, `request_threshold=30` and `dry_threshold=10` are valid.

- Flowers: 1–100; gardeners: 1–10.
- Individual drying rates and gardener speed: positive integers.
- Request threshold: 1–100; dry threshold: 0–99 and smaller than the request threshold.
- Watering duration: positive integer; recovery and protection durations: nonnegative integers.
- Request policy: `first` or `driest`.

See the READMEs inside each variant for details, file structure and failure cases.

## Report

The common report for all four variants is in `report/report.tex` and `report/report.pdf` (Russian). It describes the model, IPC mechanisms, startup, testing, limitations and results. Recompile it from the report directory with `pdflatex report.tex` twice for the table of contents.

## Cleanup after forced termination

A process cannot handle `SIGKILL`, so POSIX shared-memory objects, named
semaphores or message queues can remain after a controller is killed. Once all
processes of that run have stopped, remove its abandoned objects with:

```bash
python3 scripts/cleanup_ipc.py 3 demo       # Variant 3, session "demo"
python3 scripts/cleanup_ipc.py 4 demo       # Variant 4, session "demo"
python3 scripts/cleanup_ipc.py 1 12345      # Variant 1, original parent PID
python3 scripts/cleanup_ipc.py 2 12345      # Variant 2, original parent PID
```

The cleanup command refuses to remove a session belonging to a still-running
controller or worker. Never use it while a simulation is active. IPC resource
limits vary between Linux systems; creating 100 flower processes and 10 gardener
processes may fail on systems with very restrictive limits. In that case the
program reports an error instead of silently running with missing workers.

The boundary suite also tests `INT_MAX` integer parameters and refuses a second
controller that tries to use an occupied session name.
