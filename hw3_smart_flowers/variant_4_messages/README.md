# Smart Flowers — Homework 3, Variant 4

This implementation models smart flowers using **independently launched Linux processes**, **POSIX message queues**, and **named POSIX semaphores**. It is the fourth implementation of variant 14.


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

## Requirements

Linux, GCC (C11), GNU Make, Bash, Python 3 (for automated tests), and POSIX message queues (`/dev/mqueue` must be available).

## Build and run

```bash
make
bash scripts/run_demo.sh tests/balanced.cfg
bash scripts/run_demo.sh tests/overwatering.cfg
bash scripts/run_demo.sh tests/max_processes.cfg
make check
```

The script starts the controller and each flower/gardener as an independent executable. It does not use `fork()` in the controller.

You can also start a small session manually in three terminals:

```bash
# Terminal 1
./build/garden tests/one_flower.cfg demo

# Terminal 2
./build/flower demo 1

# Terminal 3
./build/gardener demo 1
```

Start the controller first. Workers register within 15 seconds. Each worker index starts at 1; every index may be registered only once. Choose a unique session name (up to 24 letters, digits, `_` or `-`) for each simultaneous simulation. Press `Ctrl+C` in the controller terminal to stop the simulation.

## Communication

The controller maintains the authoritative model in its **own private memory**. It creates a POSIX event queue and a private reply queue for each worker ID. Workers register by sending a message to the event queue. During the simulation, the controller sends model snapshots through the reply queues. Each flower or gardener computes its next state with the same model functions used in previous homework and sends the result back as a message.

The controller applies received updates one at a time. This prevents two gardeners from claiming the same request. A named POSIX semaphore notifies the controller when the event queue receives a message. Separate named ownership semaphores prevent duplicate worker instances.

**This version does not use shared memory (`shm_open` or `mmap`) for exchanging model state.** The controller serializes gardener updates, so the implementation favors correctness over maximum concurrency.

The controller removes all message queues and named semaphores after normal completion, `Ctrl+C` or `SIGTERM`, including ownership names of workers that crashed. As with other POSIX IPC implementations, `SIGKILL` cannot trigger cleanup and may leave named resources; use a different session identifier if this happens.

## Main files

```text
variant_4_messages/
├── include/             # Model and IPC declarations
├── src/
│   ├── controller.c      # Garden controller executable
│   ├── worker_main.c     # Flower and gardener executables
│   ├── simulation.c      # Controller and event processing
│   ├── simulation_ipc.c  # POSIX queues and named semaphores
│   ├── simulation_processes.c # Worker-side model updates
│   ├── flower_logic.c
│   ├── gardener_logic.c
│   └── ...
├── scripts/run_demo.sh  # Start all independent processes
├── tests/               # Scenario files and automated checks
├── Makefile
└── README.md
```

## Input and results

The configuration format and restrictions match the previous Smart Flowers variants. The controller reads a `.cfg` file, prints settings and records events in `simulation_<session>.log`. At the end, it prints the number of surviving, dried-out and overwatered flowers as well as the number of completed waterings.

The automated test suite runs 13 scenarios, exercises independent launches, duplicate registration, `Ctrl+C`, and basic invalid input. Run it with `make check`.
