# Smart Flowers — Homework 3, Variant 3

The third implementation of variant 14 uses **independently started Linux
processes**, POSIX shared memory, and **named POSIX semaphores**. The model,
request selection, configuration format, and English Doxygen comments are
based on the previous homework implementations.

Unlike variants 1 and 2, the controller does not call `fork()`. The flower and
gardener executables are separate programs and attach to an existing shared
session by name.


## Interactive input

To enter parameters in the controller terminal instead of loading a configuration
file, pass only a session name:

```bash
./build/garden demo
```

The program prompts for the flower count, individual drying rates, thresholds,
watering and recovery times, gardener count, speed, and request policy. Input
errors can be corrected before starting the session. For example, the request
threshold must be strictly greater than the dry threshold.

After entering all values, open other terminals and start the required workers:

```bash
./build/flower demo 1
./build/gardener demo 1
```

For more than one flower or gardener, start **one worker for each index**, from
1 through the count entered. The controller waits up to 15 seconds for every
worker to join. To launch workers automatically, use the existing
`bash scripts/run_demo.sh tests/balanced.cfg` command instead; that script
requires a configuration file to determine how many workers to start.

The file-based syntax remains available:

```bash
./build/garden tests/one_flower.cfg demo
```

## Build and run

Requirements: Linux, GCC, Make, Bash, and Python 3 for `make check`.
Run all commands from `variant_3_independent`:

```bash
make
bash scripts/run_demo.sh tests/balanced.cfg
bash scripts/run_demo.sh tests/overwatering.cfg
bash scripts/run_demo.sh tests/max_processes.cfg
make check
```

The Bash helper launches the executables independently, waits for them, and
handles interruption. Each run uses a unique session ID, so several runs can
coexist. The log is saved as `simulation_<session>.log`.

## Launching processes manually

For a small example, use `tests/one_flower.cfg` (one flower and one gardener).
Open **three terminals** in the project directory:

```bash
# Terminal 1: creates shared resources and waits for workers
./build/garden tests/one_flower.cfg demo

# Terminal 2: starts flower 1
./build/flower demo 1

# Terminal 3: starts gardener 1
./build/gardener demo 1
```

The controller starts the simulation when every required process has registered.
The workers use **one-based indices**. For a larger configuration, start
exactly as many flower and gardener processes as specified in the file.

The session name must consist of 1–24 Latin letters, digits, underscores, or
hyphens. A session cannot be reused while it is active. Workers must join
within 15 seconds of controller startup. Duplicate worker indices are rejected.

Press **Ctrl+C in the controller terminal** to stop the simulation. The
controller signals the workers through shared memory, then closes and removes
POSIX shared-memory and semaphore names. Named semaphores and memory objects
can be inspected in `/dev/shm` on Linux.

## Model and input

All flowers begin with 100% moisture. They lose moisture at their individual
rates, request watering at or below the request threshold, and die if their
moisture falls to the dry threshold. Gardeners select the first available
request or the driest flower, travel to it, and water it. Recovery and
protection intervals can cause additional effects, including overwatering.

| Configuration key | Allowed value |
|---|---|
| `flower_count` | 1–100 |
| `drying_rates` | One positive integer per flower |
| `request_threshold` | 1–100; greater than `dry_threshold` |
| `dry_threshold` | 0–99; smaller than `request_threshold` |
| `watering_duration` | Positive integer |
| `recovery_duration` | Nonnegative integer |
| `protection_interval` | Nonnegative integer |
| `gardener_count` | 1–10 |
| `gardener_speed` | Positive integer |
| `request_policy` | `first` or `driest` |

For example, `request_threshold=30` and `dry_threshold=10` mean a flower
requests watering at 30% moisture and dies when it reaches 10%.

## Project structure

```text
variant_3_independent/
├── include/                  # C headers and shared structures
├── src/
│   ├── controller.c          # Controller entry point
│   ├── worker_main.c         # Flower/gardener entry point
│   ├── simulation.c          # Simulation supervision
│   ├── simulation_ipc.c      # Shared memory and named semaphores
│   ├── simulation_processes.c # Worker loops
│   ├── flower_logic.c        # Flower behavior
│   ├── gardener_logic.c      # Gardener behavior
│   └── ...                   # Existing input, model, and statistics modules
├── scripts/run_demo.sh       # Starts independent processes automatically
├── tests/                    # Configurations and check_independent.py
├── Makefile
├── Doxyfile
└── README.md
```

## Interprocess interaction

The controller creates a POSIX shared-memory object with `shm_open()` and
`mmap()`. It also creates named semaphores using `sem_open()`: one to guard the
shared model and one notification semaphore for each gardener. Independent
workers call `shm_open()` and `sem_open()` using the same session name.

The controller waits for every worker to register, runs at most 50 ticks
(25 ms per tick), and stops if all flowers are inactive. The mutex semaphore
protects moisture, request assignments, counters, and shutdown flags. Workers
receive notification when flowers create requests. Duplicate registration is
rejected. All participants close their local handles and the controller calls
`sem_unlink()` and `shm_unlink()` on normal completion or Ctrl+C.

If the controller is forcibly killed with `SIGKILL`, workers eventually detect
its disappearance and exit, but the named IPC objects may remain because
`SIGKILL` cannot be handled. Do not reuse that session name until stale objects
have been removed.

## Tests

`make check` runs 13 configuration scenarios, validates duplicate registration,
checks independently started executables and their termination, tests Ctrl+C,
invalid arguments, and checks IPC cleanup after normal termination and Ctrl+C.
