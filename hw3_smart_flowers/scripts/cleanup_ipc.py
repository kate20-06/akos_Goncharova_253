"""Remove abandoned IPC names after a forced SIGKILL on Linux.

Usage: python3 scripts/cleanup_ipc.py <variant: 1-4> <parent_pid|session>
Only use this after all processes of the session have stopped.
"""

import os
from pathlib import Path
import re
import sys


def active_session(variant, identifier):
    """Refuse cleanup while related processes are still running."""
    for directory in Path('/proc').iterdir():
        if not directory.name.isdigit():
            continue
        try:
            args = (directory / 'cmdline').read_bytes().split(b'\0')
        except (OSError, PermissionError):
            continue
        args = [arg.decode('utf-8', errors='replace') for arg in args if arg]
        if not args:
            continue
        if variant in (1, 2):
            if directory.name == identifier:
                return True
        elif (Path(args[0]).name == 'garden'
              and len(args) >= 2 and args[-1] == identifier):
            # Workers end with their index, so their session is penultimate.
            return True
        elif (Path(args[0]).name in ('flower', 'gardener')
              and len(args) >= 3 and args[-2] == identifier):
            return True
    return False


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ('1', '2', '3', '4'):
        print('Usage: cleanup_ipc.py <variant: 1-4> <parent PID or session>', file=sys.stderr)
        return 2
    variant = int(sys.argv[1])
    identifier = sys.argv[2]
    if variant < 3:
        if not identifier.isascii() or not identifier.isdecimal() or int(identifier) <= 1:
            print('Expected a positive parent PID.', file=sys.stderr)
            return 2
    elif not re.fullmatch(r'[A-Za-z0-9_-]{1,24}', identifier):
        print('Invalid session identifier.', file=sys.stderr)
        return 2
    if active_session(variant, identifier):
        print('A related process is still running; cleanup refused.', file=sys.stderr)
        return 1

    targets = []
    shm = Path('/dev/shm')
    queues = Path('/dev/mqueue')
    if variant in (1, 2):
        targets.append(shm / ('smart_flowers_' + identifier))
        if variant == 2:
            targets.extend(shm.glob('sem.sf3_gardener_' + identifier + '_*'))
            targets.append(shm / ('sem.sf3_model_' + identifier))
    elif variant == 3:
        targets.append(shm / ('sf3_' + identifier + '_shared'))
        targets.append(shm / ('sem.sf3_' + identifier + '_lock'))
        targets.extend(shm.glob('sem.sf3_' + identifier + '_gardener_*'))
    else:
        targets.extend(queues.glob('sf4_' + identifier + '_*'))
        targets.extend(shm.glob('sem.sf4_' + identifier + '_*'))

    removed = 0
    for target in targets:
        try:
            target.unlink()
            removed += 1
        except FileNotFoundError:
            pass
        except OSError as error:
            print(f'Cannot remove {target}: {error}', file=sys.stderr)
            return 1
    print(f'Removed {removed} abandoned IPC name(s).')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
