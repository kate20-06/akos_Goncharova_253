# Smart Flowers — Variant 14

A multithreaded console simulation of smart flowers and gardeners, written in C for Linux. Each flower and each gardener runs in its own POSIX thread. Mutexes protect shared data, and semaphores notify gardeners when a flower needs water.

## Requirements

- Linux
- GCC and `make`
- POSIX threads and semaphores (provided by Linux)
- Python 3, only for running the automated tests

To rebuild the PDF report, `pdflatex` with Russian language support is also needed. Doxygen is optional.

## Build

Open a terminal in the project directory and run:

```bash
make
```

The executable will be created at `build/smart_flowers`.

## Run

Run the program with interactive input:

```bash
./build/smart_flowers
```

Or provide a configuration file:

```bash
./build/smart_flowers tests/balanced.cfg
```

Other examples:

```bash
./build/smart_flowers tests/one_flower.cfg
./build/smart_flowers tests/driest_policy.cfg
./build/smart_flowers tests/overwatering.cfg
./build/smart_flowers tests/large_garden.cfg
```

The program prints simulation events and a final summary. It also writes the events to `simulation.log` in the project directory. **The log is overwritten on each run**, so copy it before starting another test if you want to keep it.

Press `Ctrl+C` to stop the simulation early. The program asks its threads to finish, joins them, and prints the available statistics.

## Configuration files

Configuration files contain one `key=value` entry per line. 

| Key | Allowed values | Meaning |
| --- | --- | --- |
| `flower_count` | Integer from 1 to 100 | Number of flowers |
| `drying_rates` | Exactly `flower_count` positive integers (>= 1), separated by commas | Moisture lost by each flower per simulation step |
| `request_threshold` | Integer from 1 to 100; **greater than `dry_threshold`** | Flower requests watering when its moisture falls to this level |
| `dry_threshold` | Integer from 0 to `request_threshold - 1` | Flower dies from drying when its moisture falls to this level |
| `watering_duration` | Integer >= 1 | Number of simulation steps spent watering |
| `recovery_duration` | Integer >= 0 | Steps spent recovering after watering; 0 disables recovery delay |
| `protection_interval` | Integer >= 0 | Steps after watering during which another watering would be harmful; 0 disables protection |
| `gardener_count` | Integer from 1 to 10 | Number of gardener threads |
| `gardener_speed` | Integer >= 1 | Approach work units completed per simulation step |
| `request_policy` | `first` or `driest` | Choose the first available request or the driest available flower |

**Why must the request threshold exceed the drying threshold?** Moisture starts at 100 and goes down. A flower must request help *before* it reaches the level at which it dies. For example, `request_threshold=30` and `dry_threshold=10` is valid. The values `request_threshold=2` and `dry_threshold=3` are not: the flower would reach the drying threshold before the request threshold.

An example configuration (the number of drying rates must match the number of flowers):

```ini
flower_count=3
drying_rates=2,3,4
request_threshold=30
dry_threshold=10
watering_duration=4
recovery_duration=3
protection_interval=1
gardener_count=2
gardener_speed=5
request_policy=first
```

In interactive mode, prompts show valid ranges and let you retry an invalid number. Invalid configuration files instead produce an error and stop the program.

All flowers start at moisture level 100. The simulation has a maximum observation period of 50 steps, with a 25 ms delay between steps. Different threads run independently, so the order of messages can differ between runs.

## Tests

Build the program and run all supplied test configurations:

```bash
make check
```

This runs `tests/check_parallel.py`. The script checks that the program exits successfully and prints its final statistics for every `.cfg` file. For `overwatering.cfg`, it also checks that at least one flower dies from overwatering. These are **basic end-to-end checks**, not a complete test for thread races or synchronization errors.

You can also run an individual configuration using the commands above and inspect its output and `simulation.log`.

Included scenarios: `balanced`, `single_gardener`, `one_flower`, `driest_policy`, `fast_drying`, `many_gardeners`, `large_garden`, `long_recovery`, `no_protection`, and `overwatering`.

## Project structure

```text
hw2_smart_flowers/
├── include/              # Header files
├── src/                  # C source files
│   ├── main.c            # Entry point
│   ├── app.c             # Application startup
│   ├── flower_logic.c    # Flower behavior
│   ├── gardener_logic.c  # Gardener behavior
│   ├── request_logic.c   # Watering requests
│   ├── simulation.c      # Simulation management
│   ├── simulation_threads.c # Thread routines
│   └── ...               # Other modules
├── tests/                # Test configurations and Python test runner
├── report/               # LaTeX source and PDF report
├── Makefile              # Build and test commands
├── README.md             # Project documentation
├── Doxyfile              # Doxygen configuration
└── .gitignore
```


## Using the console

To enter parameters manually, start the executable without arguments:

```bash
./build/smart_flowers
```

The program asks for the simulation settings before starting. Once it starts, it prints flower and gardener events in the terminal. Press `Ctrl+C` to stop it early. There is no command menu during the simulation; settings cannot be changed while it is running.

To skip manual input, pass a configuration file instead:

```bash
./build/smart_flowers tests/overwatering.cfg
```

This is sufficient for the console-based interface: interactive input is available before the simulation, while progress is displayed continuously during execution.

## Other commands

```bash
make clean     # Remove build files, generated documentation and simulation.log
make docs      # Generate documentation (requires Doxygen)
make report    # Rebuild the PDF report (requires pdflatex)
```

The program itself is written in C; Python is only used for the test runner.
