"""Simple tests for the Smart Flowers program."""

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROGRAM = ROOT / "build" / "smart_flowers"
TESTS = ROOT / "tests"


def run_test(config):
    result = subprocess.run(
        [str(PROGRAM), str(config)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=15,
    )

    if result.returncode != 0:
        print(f"FAIL: {config.name} (program error)")
        return False

    if "ИТОГОВАЯ СТАТИСТИКА" not in result.stdout:
        print(f"FAIL: {config.name} (no statistics)")
        return False

    if config.name == "overwatering.cfg":
        if "Погибших от перелива: " not in result.stdout or "Погибших от перелива: 0" in result.stdout:
            print(f"FAIL: {config.name} (no overwatering)")
            return False

    print(f"PASS: {config.name}")
    return True


def main():
    configs = sorted(TESTS.glob("*.cfg"))
    passed = 0

    for config in configs:
        if run_test(config):
            passed += 1

    print(f"\nPassed: {passed}/{len(configs)}")
    if passed != len(configs):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
