# Smart Flowers — Variant 14

A console application written in C that sequentially simulates the interaction between smart flowers and gardeners.

## Build

```bash
make
```

## Run

Interactive mode:

```bash
./build/smart_flowers
```

Run with a prepared configuration file:

```bash
./build/smart_flowers tests/test1.cfg
```

After startup, the program creates `simulation.log`, which contains a copy of the simulation events printed to the console.

Press `Ctrl+C` to terminate the simulation safely before it reaches the normal stopping condition.

## Project structure

- `src/main.c` — minimal program entry point;
- `src/app.c` — application startup, input mode selection, validation, logging setup, and `Ctrl+C` handling;
- `src/input.c` — interactive console input and parameter printing;
- `src/config_parser.c` — parsing of `.cfg` configuration files;
- `src/input_validation.c` — validation of simulation parameters;
- `src/soft_assert.c` — recoverable runtime checks used for invalid internal arguments;
- `include/constants.h` — model limits and named constants used instead of magic numbers;
- `src/model.c` — basic operations on flowers and gardeners;
- `src/flower_logic.c` — flower state changes and watering requests;
- `src/gardener_logic.c` — request selection and gardener actions;
- `src/statistics.c` — final simulation statistics;
- `src/simulation.c` — main sequential simulation loop;
- `src/logger.c` — event output to the console and log file;
- `include/` — header files;
- `build/` — generated object files and executable (created by `make`);
- `tests/` — prepared `.cfg` test data sets;
- `report/report.tex` — LaTeX report for the project.
```
.
├── Doxyfile
├── include
│   ├── app.h
│   ├── config_parser.h
│   ├── constants.h
│   ├── flower_logic.h
│   ├── gardener_logic.h
│   ├── input.h
│   ├── input_validation.h
│   ├── logger.h
│   ├── model.h
│   ├── simulation.h
│   ├── soft_assert.h
│   └── statistics.h
├── Makefile
├── README.md
├── report
│   ├── report.aux
│   ├── report.fdb_latexmk
│   ├── report.fls
│   ├── report.log
│   ├── report.out
│   ├── report.pdf
│   ├── report.tex
│   └── report.toc
├── src
│   ├── app.c
│   ├── config_parser.c
│   ├── constants.c
│   ├── flower_logic.c
│   ├── gardener_logic.c
│   ├── input.c
│   ├── input_validation.c
│   ├── logger.c
│   ├── main.c
│   ├── model.c
│   ├── simulation.c
│   ├── soft_assert.c
│   └── statistics.c
├── tests
│   ├── test1.cfg
│   ├── test2.cfg
│   └── test3.cfg
└── tree.txt

5 directories, 40 files
```

## Simulation model

One iteration of the main loop represents one simulation tick.

During each tick:

1. Every active flower loses moisture according to its individual drying rate.
2. When the watering-request threshold is reached, the flower creates one watering request.
3. Idle gardeners choose unassigned requests according to the configured request-selection rule (`first` or `driest`).
4. Gardener speed determines how quickly the gardener reaches the selected flower.
5. Watering lasts for the configured number of ticks.
6. When watering is complete, the flower returns to maximum moisture, its request is closed, and the flower enters recovery.
7. A repeat-watering protection interval is active after watering. Watering again before it expires is treated as overwatering.
8. A dried-out or overwatered dead flower no longer participates in the simulation.


## Input parameters

The program asks only for the parameters listed in variant 14:

- number of flowers;
- individual drying rates;
- watering-request and drying thresholds;
- watering and recovery durations;
- repeat-watering protection interval;
- number of gardeners and their work speed;
- request-selection rule.

The observation period and initial moisture are internal model constants, not user input. A completed watering restores the flower to maximum moisture, so there is no extra `water_amount` parameter.

## Test data

Prepared tests can be run with:

```bash
make test1
make test2
make test3
```

The test files use simple `key=value` configuration entries. Individual drying rates are written as a comma-separated list.

## Doxygen documentation

Data structures and functions are documented with Doxygen comments.

```bash
make docs
```

The generated HTML documentation is placed in `docs/html/index.html`.

## LaTeX report

The report source is located at `report/report.tex`.

If `pdflatex` is installed, build it with:

```bash
make report
```

The generated file is `report/report.pdf`.

## Cleanup

```bash
make clean
```

## Error handling

Input is parsed strictly: malformed integers, out-of-range values, invalid configuration lines, missing parameters, and invalid drying rates cause a controlled error message and a non-zero program exit. Recoverable internal precondition checks use `SOFT_ASSERT`; they report the failed condition and source location without calling `abort()`.
