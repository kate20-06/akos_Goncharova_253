# Smart Flowers — Homework 3, Variant 2

This is the **second implementation** of Homework 3 (variant 14). It uses
**separate Linux processes**, **POSIX shared memory**, and **named POSIX
semaphores**. It is based on the first implementation, which uses unnamed
semaphores. The flower and gardener behavior is unchanged.

## Build and run

Requires Linux, GCC, Make, and Python 3 for the optional tests. Run commands
from the `variant_2_named` directory:

```bash
make
./build/smart_flowers tests/balanced.cfg
./build/smart_flowers tests/overwatering.cfg
./build/smart_flowers tests/max_processes.cfg
```

Start without arguments to enter settings interactively:

```bash
./build/smart_flowers
```

The program prints an explanation of each parameter and its allowed range.
Press **Ctrl+C** to stop the simulation safely. The terminal and
`simulation.log` show events and final statistics. The observation period is
at most 50 ticks (25 ms per tick), or fewer if all flowers become inactive.

Run automatic tests with `make check` and delete build files with `make clean`.

## Configuration parameters

| Parameter | Valid values | Description |
|---|---|---|
| `flower_count` | 1–100 | Number of flower processes |
| `drying_rates` | One positive integer per flower | Moisture lost each update |
| `request_threshold` | 1–100, strictly above `dry_threshold` | Flower requests watering at or below this moisture |
| `dry_threshold` | 0–99, strictly below `request_threshold` | Flower dies from dryness at or below this moisture |
| `watering_duration` | Positive integer | Steps spent watering |
| `recovery_duration` | Nonnegative integer | Recovery steps after watering |
| `protection_interval` | Nonnegative integer | Period when another watering causes damage |
| `gardener_count` | 1–10 | Number of gardener processes |
| `gardener_speed` | Positive integer | Progress towards flower per step |
| `request_policy` | `first` or `driest` | How gardeners select available requests |

For example, `request_threshold=30` and `dry_threshold=10` means a flower
requests water at 30% moisture and dies if it reaches 10%. All flowers begin
at 100% moisture.

## Project structure

```text
variant_2_named/
├── include/                 # Headers and shared model
├── src/
│   ├── flower_logic.c       # Flower behavior
│   ├── gardener_logic.c     # Movement and watering
│   ├── request_logic.c      # Selecting requests
│   ├── simulation.c         # Shared memory, named semaphores, fork, cleanup
│   ├── simulation_processes.c # Child process routines
│   └── ...                  # Input, logging and statistics
├── tests/                   # Test configurations and check_processes.py
├── Makefile
├── Doxyfile
└── README.md
```

## Process interaction

The parent process creates one child for each flower and gardener using
`fork()`. The parent creates POSIX shared memory using `shm_open()` and `mmap()`.
It also opens **named semaphores** using `sem_open()`:

- One binary semaphore protects the shared model when processes modify flower
  states, assignments, and statistics.
- One semaphore per gardener notifies that process when a request is available.

The semaphore names contain the parent PID (for example,
`/sf3_model_12345` and `/sf3_gardener_12345_0`), allowing separate runs to
use different names. The parent signals shutdown, waits for children via
`waitpid()`, then calls `sem_close()` and `sem_unlink()` for all named semaphores.
It also unmaps and removes the shared-memory object.

On Linux, named semaphores can be inspected while the program is running in
`/dev/shm/sem.sf3_*`. All names are removed after normal termination or
**Ctrl+C**. An uncatchable `SIGKILL` can prevent the parent from performing
cleanup, so an object may remain after a forced kill.

## Tests

```bash
make check
```

The test script runs 13 configurations twice, checks invalid settings and
expected overwatering behavior, checks Ctrl+C, and verifies that named
semaphores and shared memory are cleaned up after normal termination and
interruption. On Linux it uses `/dev/shm` to inspect IPC objects.

The event log is written to a fixed filename. If running multiple instances
in the same directory, use separate working directories to keep their logs.
