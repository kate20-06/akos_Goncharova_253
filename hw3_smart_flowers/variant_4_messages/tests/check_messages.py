"""Check the message-queue variant with independent worker processes."""

from pathlib import Path
import os
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parent.parent


def ipc_objects(session):
    queues = list(Path('/dev/mqueue').glob(f'sf4_{session}_*'))
    semaphores = list(Path('/dev/shm').glob(f'sem.sf4_{session}_*'))
    return queues + semaphores


def run_config(config):
    result = subprocess.run(
        ['bash', 'scripts/run_demo.sh', str(config)],
        cwd=ROOT, capture_output=True, text=True, timeout=30
    )
    assert result.returncode == 0, (config, result.stderr[-500:])
    assert 'ИТОГОВАЯ СТАТИСТИКА' in result.stdout, config
    if config.name == 'overwatering.cfg':
        assert 'DEAD_OVERWATERED' in result.stdout
    print('PASS', config.name)


def run_manual():
    session = 'manual_' + str(os.getpid())
    controller = subprocess.Popen(
        ['./build/garden', 'tests/one_flower.cfg', session],
        cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
    )
    workers = []
    try:
        time.sleep(0.2)
        assert ipc_objects(session), 'IPC objects not created'
        for binary in ['flower', 'gardener']:
            workers.append(subprocess.Popen(
                ['./build/' + binary, session, '1'], cwd=ROOT,
                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
            ))
        duplicate = subprocess.run(
            ['./build/flower', session, '1'], cwd=ROOT,
            capture_output=True, timeout=4
        )
        assert duplicate.returncode != 0, 'duplicate accepted'
        assert controller.wait(timeout=12) == 0
        for process in workers:
            assert process.wait(timeout=5) == 0
        assert not ipc_objects(session), 'IPC resources remain'
        print('PASS independent launch and duplicate registration')
    finally:
        if controller.poll() is None:
            controller.send_signal(signal.SIGINT)
            controller.wait(timeout=5)
        for process in workers:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)


def run_interrupt():
    session = 'interrupt_' + str(os.getpid())
    controller = subprocess.Popen(
        ['./build/garden', 'tests/balanced.cfg', session],
        cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
    )
    workers = []
    try:
        time.sleep(0.15)
        for binary, count in [('flower', 6), ('gardener', 2)]:
            for index in range(1, count + 1):
                workers.append(subprocess.Popen(
                    ['./build/' + binary, session, str(index)], cwd=ROOT,
                    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
                ))
        time.sleep(0.2)
        controller.send_signal(signal.SIGINT)
        assert controller.wait(timeout=10) == 0
        for process in workers:
            assert process.wait(timeout=5) == 0
        assert not ipc_objects(session), 'IPC resources remain after interrupt'
        print('PASS SIGINT and cleanup')
    finally:
        if controller.poll() is None:
            controller.kill()
            controller.wait()
        for process in workers:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)


def run_invalid():
    for argument in ['0', '-1', 'xyz', '999999']:
        result = subprocess.run(
            ['./build/flower', 'missing_session', argument],
            cwd=ROOT, capture_output=True, timeout=3
        )
        assert result.returncode != 0
    result = subprocess.run(
        ['./build/garden', 'tests/one_flower.cfg', 'bad/session'],
        cwd=ROOT, capture_output=True, timeout=3
    )
    assert result.returncode != 0
    print('PASS invalid worker IDs and session names')


if __name__ == '__main__':
    for config in sorted((ROOT / 'tests').glob('*.cfg')):
        run_config(config)
    run_manual()
    run_interrupt()
    run_invalid()
    print('ALL CHECKS PASSED')
