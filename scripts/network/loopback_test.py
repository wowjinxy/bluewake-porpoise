#!/usr/bin/env python3
"""Run a dedicated server and two independent clients without desktop input."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--receipt', type=Path)
    args = parser.parse_args()
    suffix = '.exe' if sys.platform == 'win32' else ''
    server = args.build / ('bluewake_network_server' + suffix)
    client = args.build / ('bluewake_network_core_test' + suffix)
    kwargs = {'text': True, 'stdout': subprocess.PIPE, 'stderr': subprocess.PIPE}
    if sys.platform == 'win32':
        kwargs['creationflags'] = subprocess.CREATE_NO_WINDOW
    processes = []
    started = time.monotonic()
    try:
        host = subprocess.Popen([str(server), '--port', '0', '--seconds', '30'], **kwargs)
        processes.append(host)
        # Every child has its own bounded timer; its stdout is a tiny JSON receipt.
        ready = json.loads(host.stdout.readline())
        assert ready['ready'] and ready['namespace'] == 'BlueWake.WindWaker.GZLE01.Progression'
        pub = subprocess.Popen([str(client), '--port', str(ready['port']), '--role', 'publisher'], **kwargs)
        processes.append(pub)
        pub_ready = json.loads(pub.stdout.readline())
        assert pub_ready == {'ready': True, 'role': 'publisher'}
        sub = subprocess.Popen([str(client), '--port', str(ready['port']), '--role', 'receiver'], **kwargs)
        processes.append(sub)
        receiver_out, receiver_err = sub.communicate(timeout=20)
        publisher_out, publisher_err = pub.communicate(timeout=20)
        assert sub.returncode == 0, receiver_err
        assert pub.returncode == 0, publisher_err
        receiver = [json.loads(line) for line in receiver_out.splitlines()]
        publisher = [json.loads(line) for line in publisher_out.splitlines()]
        assert receiver[-1]['passed'] and receiver[-1]['live'] == 2 and receiver[-1]['reconnect_snapshot'] == 2
        assert publisher[-1]['passed'] and publisher[-1]['revision'] == 2
        receipt = {'scope': 'standalone synthetic progression fixtures; no game/cards/assets',
                   'test_facts': [{'key': 2, 'value': 0x22, 'meaning': 'Wind Waker ownership'},
                                  {'key': 34, 'value': 60, 'meaning': 'Arrow capacity'}],
                   'server_sha256': hashlib.sha256(server.read_bytes()).hexdigest(),
                   'client_sha256': hashlib.sha256(client.read_bytes()).hexdigest(),
                   'publisher': publisher, 'receiver': receiver,
                   'checks': {'separate_server_and_two_clients': True, 'exactly_two_live_updates': True,
                              'reconnect_full_whitelist_snapshot': True, 'no_desktop_input': True},
                   'elapsed_seconds': time.monotonic() - started}
        if args.receipt:
            args.receipt.parent.mkdir(parents=True, exist_ok=True)
            args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')
        print(json.dumps(receipt, indent=2))
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)

if __name__ == '__main__':
    main()
