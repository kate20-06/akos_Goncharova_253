# Smart Flowers — Homework 3, Variant 1

This is the **first of four planned implementations** for Homework 3 (variant 14).
It simulates smart flowers and gardeners using **separate Linux processes**.
The parent creates child processes with `fork()`. Processes share model data
through POSIX shared memory (`shm_open`, `mmap`) and synchronize using
**unnamed POSIX semaphores** initialized with `pshared = 1`.

## Build and run

Requires Linux, GCC, Make, and Python 3 for the optional test script.
Run these commands from the `variant_1_unnamed` directory:

```bash
make
./build/smart_flowers tests/balanced.cfg
./build/smart_flowers tests/overwatering.cfg
./build/smart_flowers tests/max_processes.cfg
```

For interactive input:

```bash
./build/smart_flowers
```

The program explains the accepted values before asking for them. During a run,
press **Ctrl+C** to stop. Events and final statistics appear in the terminal and
are saved in `simulation.log`. The observation period is at most 50 ticks
(25 ms per tick). The run may finish earlier when all flowers become inactive.

To run the tests:

```bash
make check
```

To remove compiled files and the log:

```bash
make clean
```

## Configuration parameters

| Parameter | Accepted values | Meaning |
|---|---|---|
| `flower_count` | 1–100 | Number of flower processes |
| `drying_rates` | `flower_count` positive integers | Moisture lost by each flower per update |
| `request_threshold` | 1–100, greater than `dry_threshold` | A flower asks for water at or below this moisture |
| `dry_threshold` | 0–99, less than `request_threshold` | A flower dies from drying at or below this moisture |
| `watering_duration` | Positive integer | Gardener steps required for watering |
| `recovery_duration` | Nonnegative integer | Recovery steps after watering |
| `protection_interval` | Nonnegative integer | Period when another watering is dangerous |
| `gardener_count` | 1–10 | Number of gardener processes |
| `gardener_speed` | Positive integer | Progress towards a flower per step |
| `request_policy` | `first` or `driest` | Request selection rule |

For example, set `request_threshold=30` and `dry_threshold=10`:
the flower requests water at 30% moisture and dies if it reaches 10%.
Each flower starts at 100% moisture.

## Project structure

```text
variant_1_unnamed/
├── include/                 # Header files and shared data structures
├── src/
│   ├── flower_logic.c       # Flower state transitions
│   ├── gardener_logic.c     # Gardener movement and watering
│   ├── request_logic.c      # Request selection
│   ├── simulation.c         # Shared memory, fork, parent supervision
│   ├── simulation_processes.c # Child process worker routines
│   └── ...                  # Input, logging, model and statistics
├── tests/                   # Configuration cases and check_processes.py
├── Makefile
├── Doxyfile
└── README.md
```

## Process interaction

The parent creates one flower process per flower and one gardener process per
gardener. It also initializes a shared model semaphore and a notification
semaphore for each gardener. Flower processes change moisture, create requests
and notify gardeners. Gardener processes claim requests and perform watering.

A claim and its state updates happen while holding `model_mutex`; therefore,
only one gardener can claim a particular request. The per-gardener notification
semaphores allow idle gardeners to wait without polling.

On termination, the parent sets the shared stop flag, wakes waiting gardeners,
waits for children with `waitpid()`, destroys all semaphores, unmaps and unlinks
the POSIX shared memory object. A unique name based on the parent PID prevents
collisions between shared-memory objects of separate runs. The log filename is
fixed, so run one instance at a time in the same directory if you need its log.

This implementation intentionally uses **unnamed semaphores only**. The named
semaphore and independently launched variants are separate later tasks.
