# Smart Flowers — Homework 4 (TCP), Variant 14

A Linux client–server simulation written in C. Flowers gradually dry out and
request watering; gardeners accept available requests and water flowers. This
version uses **TCP sockets**, so workers and observers can run on different
computers. The model includes flower moisture, watering requests, gardeners, and recovery periods.

## Requirements

- Linux (Ubuntu / Astra Linux), GCC, Make, pthreads and POSIX sockets.
- Python 3 for `make check`; Bash for the optional demonstration script.
- A reachable IPv4 address and a free TCP port. No external packages are needed.

## Build

```bash
make
```

Executables are placed in `build/`: `server`, `flower`, `gardener`, `observer`.

## Run on one computer

Use `tests/one_flower.cfg` to try one flower and one gardener. Open four terminals
in the project directory and run:

```bash
# Terminal 1: the server (IP to bind, port, configuration)
./build/server 127.0.0.1 19014 tests/one_flower.cfg

# Terminal 2: flower 1
./build/flower 127.0.0.1 19014 1

# Terminal 3: gardener 1
./build/gardener 127.0.0.1 19014 1

# Terminal 4: a full-garden observer
./build/observer 127.0.0.1 19014
```

The server waits for **all configured flower and gardener clients**
before starting simulation ticks.
Each worker computes its own model step and returns the result over TCP.
The server accepts and records the result; observers show the full garden
state after each completed tick.

To start an automatic local demonstration using a configuration file:

```bash
bash scripts/run_demo.sh tests/balanced.cfg
bash scripts/run_demo.sh tests/overwatering.cfg 19015
```

The script starts the number of flower and gardener clients specified in the
configuration. Do not use the same port for two simultaneous servers.

## Interactive settings

Run the server without a configuration file:

```bash
./build/server 127.0.0.1 19014
```

The terminal prompts for flower count, each drying rate, the thresholds,
watering/recovery/protection periods, gardener count/speed and request policy.
The prompt shows the allowed range and lets you correct an invalid value.
Start the flower and gardener programs in other terminals after input.

## Run across several computers

On the server computer, replace `127.0.0.1` with its reachable IPv4 address
(or `0.0.0.0` to listen on all interfaces):

```bash
./build/server 0.0.0.0 19014 tests/balanced.cfg
```

On other computers, use **the server's actual IP address**, not `0.0.0.0`:

```bash
./build/flower 192.168.1.10 19014 1
./build/gardener 192.168.1.10 19014 1
./build/observer 192.168.1.10 19014
```

The address `192.168.1.10` is only an example. Allow incoming TCP connections
to the chosen port in the server's firewall. Each computer needs the compiled
client executable, not the whole server configuration. This is a plain-text
educational protocol, so use it on a trusted network.

## Connecting and disconnecting clients

- Client identifiers are **1-based**. Start one `flower` per configured flower
  and one `gardener` per gardener. Duplicate worker identifiers are rejected.
- An observer can connect at any time while the simulation is running. Several
  observers can watch simultaneously from different computers.
- An observer can be stopped with `Ctrl+C` and launched again without
  interrupting other observers.
- A flower or gardener can disconnect and reconnect with the same identifier.
  **The simulation pauses while a required worker is absent.** The server
  retains the latest state and resumes once that worker returns. It never
  calculates a missing worker's steps on the server.
- A flower receives an `FJOB` message, computes its next state using
  `update_flower()`, and returns `FRESULT`. A gardener receives a `GJOB`
  snapshot, computes its next step using `update_gardener()`, and sends
  `GRESULT`. The server validates and commits the results in tick order.
- If a connected worker becomes unresponsive, the server closes the stale
  connection and waits for a new one. To stop a session that is waiting
  for workers, press `Ctrl+C` in the server terminal.
- After 50 ticks, when all flowers are inactive, or when the server is stopped
  with `Ctrl+C`/`SIGTERM`, it sends `FINISH` to connected clients and prints
  statistics. A client that loses its TCP connection unexpectedly exits with
  an error.

The default tick duration is 120 ms. For faster runs, set:

```bash
SMART_FLOWERS_TICK_MS=40 ./build/server 127.0.0.1 19014 tests/balanced.cfg
```

Allowed values are 10–2000 ms. Changing this affects only the real-time delay,
not the number of model ticks.

## Configuration values

All flowers start at 100 units of moisture. The settings files use `key=value`
lines and the following limits:

| Key | Meaning / allowed value |
|---|---|
| `flower_count` | Number of flowers, 1–100 |
| `drying_rates` | One positive integer per flower, separated by commas |
| `request_threshold` | Watering request threshold, 1–100 |
| `dry_threshold` | Death threshold, 0–99 and strictly less than request threshold |
| `watering_duration` | Duration in ticks, positive integer |
| `recovery_duration` | Recovery in ticks, nonnegative integer |
| `protection_interval` | Time in ticks during which another watering is dangerous, nonnegative integer |
| `gardener_count` | Number of gardeners, 1–10 |
| `gardener_speed` | Work units per tick, positive integer |
| `request_policy` | `first` or `driest` |

For example, `request_threshold=30` and `dry_threshold=10` allow a flower to
request watering at moisture 30 before it dies at moisture 10. Flower states
include `NORMAL`, `WILTING`, `WATERING`, `RECOVERING`, `DRY` and
`DEAD_OVERWATERED`.

## Checks and documentation

```bash
make check       # TCP integration checks
make report      # Rebuild report/report.pdf (requires pdflatex)
make clean       # Remove executables and the simulation log
```

The `tests/` directory contains 13 configuration examples covering different model parameters. The
`make check` script covers startup with missing workers, two simultaneous
observers, reconnecting workers, and duplicate or invalid worker IDs.

The server saves its event log to `simulation.log`. The clients print their
own model events in their terminals. The server prints accepted state changes
and final statistics. The report is in
`report/report.tex` and `report/report.pdf`.

## Project structure

```text
smart_flowers_hw4/
├── include/              # Model, socket, and worker protocol interfaces
├── src/
│   ├── server.c          # TCP server, simulation and client threads
│   ├── client.c          # Flower, gardener and observer entry point
│   ├── network.c         # TCP connections and line protocol
│   ├── flower_logic.c    # Flower state updates
│   ├── gardener_logic.c  # Gardener state updates
│   ├── request_logic.c   # Watering request selection
│   └── ...               # Input, model, logging and statistics
├── tests/                # Configuration files and TCP checks
├── scripts/run_demo.sh   # Starts a local demonstration
├── report/               # Russian LaTeX report and PDF
├── Makefile
├── Doxyfile
└── README.md
```
