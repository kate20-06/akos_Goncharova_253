"""Run basic and boundary tests of the process-based simulation."""

import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent
PROGRAM = ROOT / "build" / "smart_flowers"
CONFIGS = sorted((ROOT / "tests").glob("*.cfg"))


def run(config):
    return subprocess.run([str(PROGRAM), str(config)], cwd=ROOT,
                          text=True, capture_output=True, timeout=10)


def check_cases():
    for config in CONFIGS:
        for _ in range(2):
            result = run(config)
            assert result.returncode == 0, f"{config.name}: {result.stderr}"
            assert "ИТОГОВАЯ СТАТИСТИКА" in result.stdout, config.name
            assert "Запущены процессы:" in result.stdout, config.name
        print("PASS", config.name)

    output = run(ROOT / "tests" / "overwatering.cfg").stdout
    assert "Погибших от перелива: 3" in output
    print("PASS overwatering outcome")


def check_invalid_inputs():
    original = (ROOT / "tests" / "balanced.cfg").read_text()
    variants = [
        ("flower_count=6", "flower_count=0"),
        ("flower_count=6", "flower_count=101"),
        ("gardener_count=2", "gardener_count=0"),
        ("gardener_count=2", "gardener_count=11"),
        ("request_threshold=47", "request_threshold=14"),
        ("dry_threshold=14", "dry_threshold=-1"),
        ("watering_duration=2", "watering_duration=0"),
        ("gardener_speed=2", "gardener_speed=0"),
        ("drying_rates=4,7,5,9,6,8", "drying_rates=4,,5,9,6,8"),
        ("request_policy=first", "request_policy=wrong"),
    ]
    with tempfile.TemporaryDirectory() as temporary:
        for i, (old, new) in enumerate(variants):
            path = Path(temporary) / f"invalid_{i}.cfg"
            path.write_text(original.replace(old, new))
            result = run(path)
            assert result.returncode != 0, new
    print("PASS invalid inputs")


def check_interrupt():
    process = subprocess.Popen([str(PROGRAM), str(ROOT / "tests" / "balanced.cfg")],
                               cwd=ROOT, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE, text=True)
    try:
        time.sleep(0.2)
        assert process.poll() is None, "Simulation ended before Ctrl+C"
        children = subprocess.check_output(
            ["ps", "-o", "pid=", "--ppid", str(process.pid)], text=True)
        child_ids = [int(pid) for pid in children.split()]
        assert len(child_ids) == 8, f"Expected 8 processes, got {len(child_ids)}"
        os.kill(process.pid, signal.SIGINT)
        assert process.wait(timeout=5) == 0
        for pid in child_ids:
            assert not Path(f"/proc/{pid}").exists(), f"Child {pid} survived"
        assert not Path(f"/dev/shm/smart_flowers_{process.pid}").exists()
        assert not list(Path("/dev/shm").glob(f"sem.sf3_*_{process.pid}*")), \
            "Named semaphores were not removed after Ctrl+C"
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
    print("PASS Ctrl+C, child processes and shared memory cleanup")


def check_named_semaphores():
    """Check semaphore creation, names, and cleanup after normal exit."""
    process = subprocess.Popen([str(PROGRAM), str(ROOT / "tests" / "balanced.cfg")],
                               cwd=ROOT, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE, text=True)
    try:
        time.sleep(0.15)
        assert process.poll() is None, "Simulation stopped too early"
        names = list(Path("/dev/shm").glob(f"sem.sf3_*_{process.pid}*"))
        assert len(names) == 3, f"Expected 3 named semaphores, found {len(names)}"
        assert process.wait(timeout=5) == 0
        assert not list(Path("/dev/shm").glob(f"sem.sf3_*_{process.pid}*"))
        assert not Path(f"/dev/shm/smart_flowers_{process.pid}").exists()
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
    print("PASS named semaphores and normal cleanup")


if __name__ == "__main__":
    check_cases()
    check_invalid_inputs()
    check_interrupt()
    check_named_semaphores()
    print("All tests passed")
