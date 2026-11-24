"""Basic checks for the TCP flower simulation."""

import os
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENV = dict(os.environ, SMART_FLOWERS_TICK_MS="20")


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def start(name, port, *args):
    return subprocess.Popen(
        [str(ROOT / "build" / name), "127.0.0.1", str(port), *map(str, args)],
        cwd=ROOT, env=ENV, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True,
    )


def finish(process, expected=0):
    try:
        out, err = process.communicate(timeout=12)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate()
        raise AssertionError(f"{process.args[0]} did not finish")
    assert process.returncode == expected, (process.args, process.returncode, err)
    return out


def check_wait_for_workers():
    port = free_port()
    server = start("server", port, ROOT / "tests/one_flower.cfg")
    try:
        time.sleep(0.35)
        assert server.poll() is None, "Server finished without workers"
        flower = start("flower", port, 1)
        time.sleep(0.2)
        assert server.poll() is None, "Server finished without a gardener"
        gardener = start("gardener", port, 1)
        assert "FINISH" in finish(flower)
        assert "FINISH" in finish(gardener)
        output = finish(server)
        assert "Такт 1" in output and "ИТОГОВАЯ СТАТИСТИКА" in output
        print("PASS: workers are required")
    finally:
        if server.poll() is None:
            server.kill()
            server.communicate()


def check_observers_and_reconnect():
    port = free_port()
    server = start("server", port, ROOT / "tests/one_flower.cfg")
    time.sleep(0.08)
    flower = start("flower", port, 1)
    gardener = start("gardener", port, 1)
    first = start("observer", port)
    second = start("observer", port)
    time.sleep(0.18)
    flower.terminate()
    flower.communicate(timeout=3)
    first.terminate()
    first.communicate(timeout=3)
    time.sleep(0.2)
    assert server.poll() is None, "Server ended while a flower was disconnected"
    flower = start("flower", port, 1)
    first = start("observer", port)
    assert "FINISH" in finish(flower)
    assert "FINISH" in finish(gardener)
    assert "SNAPSHOT" in finish(first)
    assert "SNAPSHOT" in finish(second)
    assert "ИТОГОВАЯ СТАТИСТИКА" in finish(server)
    print("PASS: observers and worker reconnection")


def check_invalid_connections():
    port = free_port()
    server = start("server", port, ROOT / "tests/one_flower.cfg")
    time.sleep(0.08)
    flower = start("flower", port, 1)
    duplicate = start("flower", port, 1)
    output, _ = duplicate.communicate(timeout=4)
    assert duplicate.returncode != 0 and "ERROR" in output
    invalid = start("gardener", port, 99)
    output, _ = invalid.communicate(timeout=4)
    assert invalid.returncode != 0 and "ERROR" in output
    gardener = start("gardener", port, 1)
    finish(flower)
    finish(gardener)
    finish(server)
    print("PASS: duplicate and invalid workers")


if __name__ == "__main__":
    check_wait_for_workers()
    check_observers_and_reconnect()
    check_invalid_connections()
