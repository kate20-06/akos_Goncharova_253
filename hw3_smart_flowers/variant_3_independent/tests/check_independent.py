"""Run the independent-process scenarios and basic IPC checks."""

from pathlib import Path
import os
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parent.parent


def ipc_objects(session):
    return list(Path('/dev/shm').glob(f'sf3_{session}_*')) + list(
        Path('/dev/shm').glob(f'sem.sf3_{session}_*')
    )


def run_config(config):
    result = subprocess.run(
        ['bash', 'scripts/run_demo.sh', str(config)],
        cwd=ROOT, capture_output=True, text=True, timeout=15
    )
    assert result.returncode == 0, (config, result.stderr[-500:])
    assert 'ИТОГОВАЯ СТАТИСТИКА' in result.stdout, config
    if config.name == 'overwatering.cfg':
        assert 'DEAD_OVERWATERED' in result.stdout
    print('PASS', config.name)


def check_independent_launch():
    session = 'testing_' + str(os.getpid())
    controller = subprocess.Popen(
        ['./build/garden', 'tests/one_flower.cfg', session],
        cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
    )
    workers = []
    try:
        time.sleep(0.12)
        assert controller.poll() is None, 'controller exited early'
        assert ipc_objects(session), 'IPC objects were not created'
        # Workers are started independently, not forked by the controller.
        for binary in ['flower', 'gardener']:
            workers.append(subprocess.Popen(
                ['./build/' + binary, session, '1'], cwd=ROOT,
                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
            ))
        # A duplicate worker must be rejected.
        duplicate = subprocess.run(
            ['./build/flower', session, '1'], cwd=ROOT,
            capture_output=True, timeout=5
        )
        assert duplicate.returncode != 0, 'duplicate worker accepted'
        assert controller.wait(timeout=8) == 0, 'controller failed'
        for worker in workers:
            assert worker.wait(timeout=4) == 0, 'worker failed'
        assert not ipc_objects(session), 'IPC resources leaked'
        print('PASS separate launch, duplicate registration, cleanup')
    finally:
        if controller.poll() is None:
            controller.send_signal(signal.SIGINT)
            try:
                controller.wait(timeout=3)
            except subprocess.TimeoutExpired:
                controller.kill()
        for worker in workers:
            if worker.poll() is None:
                worker.terminate()
                worker.wait(timeout=3)


def check_interrupt():
    session = 'interrupt_' + str(os.getpid())
    controller = subprocess.Popen(
        ['./build/garden', 'tests/balanced.cfg', session],
        cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    workers = []
    try:
        time.sleep(0.12)
        for kind, count in [('flower', 6), ('gardener', 2)]:
            for i in range(1, count + 1):
                workers.append(subprocess.Popen(
                    ['./build/' + kind, session, str(i)], cwd=ROOT,
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
                ))
        time.sleep(0.2)
        controller.send_signal(signal.SIGINT)
        assert controller.wait(timeout=5) == 0
        for worker in workers:
            assert worker.wait(timeout=5) == 0
        assert not ipc_objects(session), 'IPC leaked after SIGINT'
        print('PASS Ctrl+C and cleanup')
    finally:
        if controller.poll() is None:
            controller.kill()
            controller.wait()
        for worker in workers:
            if worker.poll() is None:
                worker.terminate()
                worker.wait(timeout=3)


def check_bad_input():
    for args in [
        ['./build/garden', 'tests/balanced.cfg', '../bad'],
        ['./build/garden', 'tests/missing.cfg', 'invalid'],
        ['./build/flower', '../bad', '1'],
        ['./build/gardener', 'missing', '0'],
        ['./build/flower', 'missing', '1'],
    ]:
        result = subprocess.run(args, cwd=ROOT, capture_output=True, timeout=4)
        assert result.returncode != 0, args
    print('PASS invalid parameters')


if __name__ == '__main__':
    for config in sorted((ROOT / 'tests').glob('*.cfg')):
        run_config(config)
    check_independent_launch()
    check_interrupt()
    check_bad_input()
    print('All tests passed.')
