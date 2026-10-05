#!/usr/bin/env python3
"""Synthetic protocol test with two real dedicated-server process lifetimes.

No translated module, game, CARD, controller, renderer or audio device is used.
Each child is hidden on Windows, bounded and owned by this test.
"""
import argparse
import hashlib
import json
from pathlib import Path
import queue
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time

NAMESPACE = 'BlueWake.WindWaker.GZLE01.Progression'
SECRET = 'synthetic-room-credential'

def text(value):
    encoded = value.encode('ascii')
    return struct.pack('>H', len(encoded)) + encoded

def identity(digest='1'):
    return text(NAMESPACE) + struct.pack('>I', 1) + text('GZLE01') + text(digest * 64) * 3

def packet(kind, sequence, body):
    return struct.pack('>4sHHIQ', b'BWPN', 1, kind, len(body), sequence) + body

def exact(connection, size):
    result = b''
    while len(result) < size:
        chunk = connection.recv(size-len(result))
        assert chunk, 'Server closed before a complete frame.'
        result += chunk
    return result

def receive(connection, wanted):
    # Roster packets are independent presence. No ignored progression commit.
    for _ in range(32):
        magic, version, kind, size, sequence = struct.unpack('>4sHHIQ', exact(connection, 20))
        assert magic == b'BWPN' and version == 1 and size <= 4096
        body = exact(connection, size)
        if kind == wanted:
            return body
        assert kind == 7, ('Unexpected protocol response', kind, wanted)
    raise AssertionError('Excessive roster responses.')

class Reader:
    def __init__(self, data):
        self.data, self.at = data, 0
    def take(self, size):
        assert size <= len(self.data)-self.at
        out = self.data[self.at:self.at+size]
        self.at += size
        return out
    def number(self, kind):
        return struct.unpack(kind, self.take(struct.calcsize(kind)))[0]
    def text(self):
        return self.take(self.number('>H')).decode('ascii')
    def done(self):
        assert self.at == len(self.data)

def welcome(body):
    r = Reader(body)
    assert r.text() == NAMESPACE and r.number('>I') == 1 and r.text() == 'GZLE01'
    for _ in range(3):
        assert r.text() == '1'*64
    room, revision, acknowledged = r.text(), r.number('>Q'), r.number('>Q')
    count = r.number('>H')
    assert count <= 128
    facts = {}
    for _ in range(count):
        key, value = r.number('>H'), r.number('>I')
        assert key not in facts
        facts[key] = value
    r.done()
    return dict(room_id=room, revision=revision, acknowledged=acknowledged, facts=facts)

def commit(body):
    r = Reader(body)
    result = dict(origin=r.text(), sequence=r.number('>Q'), revision=r.number('>Q'), key=r.number('>H'), value=r.number('>I'))
    r.done()
    return result

def join(port, player, room='checkpoint', create=False, digest='1', password=SECRET):
    connection = socket.create_connection(('127.0.0.1', port), timeout=5)
    connection.settimeout(5)
    body = identity(digest) + text(room) + text(player*32) + text('SyntheticLink') + text(password) + bytes([create])
    connection.sendall(packet(1, 0, body))
    return connection

def delta(connection, sequence, key, value):
    connection.sendall(packet(4, sequence, struct.pack('>HI', key, value)))

def hidden_kwargs():
    out = dict(stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
    if sys.platform == 'win32':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        out.update(startupinfo=startup, creationflags=subprocess.CREATE_NO_WINDOW)
    return out

class Server:
    def __init__(self, executable, directory):
        self.lines, self.stderr = [], []
        self.events = queue.Queue()
        self.process = subprocess.Popen([str(executable), '--port', '0', '--seconds', '45', '--state-dir', str(directory)], **hidden_kwargs())
        def collect(stream, target, ready=False):
            for line in stream:
                target.append(line)
                if ready:
                    self.events.put(line)
        self.readers = [threading.Thread(target=collect, args=(self.process.stdout, self.lines, True), daemon=True),
                        threading.Thread(target=collect, args=(self.process.stderr, self.stderr), daemon=True)]
        for reader in self.readers:
            reader.start()
        try:
            first = json.loads(self.events.get(timeout=10))
            assert first['ready'] and first['durable'] and first['namespace'] == NAMESPACE
            self.port = first['port']
        except BaseException:
            self.close()
            raise
    def close(self):
        if self.process.poll() is None:
            self.process.terminate()  # Explicit owned-server crash after ACK; OS releases lease.
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=5)
        for reader in self.readers:
            reader.join(timeout=2)
        return dict(return_code=self.process.returncode, stdout=''.join(self.lines), stderr=''.join(self.stderr),
                    shutdown='owned process terminated after protocol checkpoints; not graceful-save acceptance')

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--receipt', type=Path)
    parser.add_argument('--work-dir', type=Path, help='Fresh isolated synthetic files, retained with the receipt.')
    args = parser.parse_args()
    executable = args.build / ('bluewake_network_server.exe' if sys.platform == 'win32' else 'bluewake_network_server')
    receipt = dict(scope='Separate hidden process restart with synthetic protocol facts; no game/cards/devices',
                   server_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(), checks={}, processes=[])
    started = time.monotonic()
    servers, connections = [], []
    try:
        if args.work_dir:
            root = args.work_dir.absolute()
            assert not root.exists(), 'Preserve prior synthetic attempts.'
            root.mkdir(parents=True)
        else:
            root = Path(tempfile.mkdtemp(prefix='BlueWake-durable-protocol-'))
        receipt['synthetic_state_directory'] = str(root)
        directory = root / 'state-\u2603'
        # Negative control is a synthetic unrelated file, never a native save.
        personal = root / 'unrelated.bin'
        personal.write_bytes(b'unrelated synthetic baseline')
        original_personal = hashlib.sha256(personal.read_bytes()).hexdigest()
        first = Server(executable, directory)
        servers.append(first)
        sender = join(first.port, 'a', create=True)
        connections.append(sender)
        baseline = welcome(receive(sender, 2))
        assert baseline['revision'] == baseline['acknowledged'] == 0 and not baseline['facts']
        observer = join(first.port, 'b')
        connections.append(observer)
        assert welcome(receive(observer, 2))['facts'] == {}
        delta(sender, 1, 2, 0x22)
        one = commit(receive(sender, 5))
        assert one == commit(receive(observer, 5)) and one['sequence'] == one['revision'] == 1
        delta(sender, 2, 34, 60)
        two = commit(receive(sender, 5))
        assert two == commit(receive(observer, 5)) and two['sequence'] == two['revision'] == 2
        files = list(directory.glob('*.bwroom'))
        assert len(files) == 1
        persisted_hash = hashlib.sha256(files[0].read_bytes()).hexdigest()
        assert SECRET.encode() not in files[0].read_bytes()
        receipt['checks']['two_live_commits_before_restart'] = True
        for connection in connections:
            connection.close()
        connections.clear()
        receipt['processes'].append(first.close())
        servers.clear()

        second = Server(executable, directory)
        servers.append(second)
        fresh = join(second.port, 'a')
        connections.append(fresh)
        restored = welcome(receive(fresh, 2))
        assert restored == dict(room_id=baseline['room_id'], revision=2, acknowledged=2, facts={2:0x22, 34:60})
        receipt['checks']['new_process_restores_without_any_client_reseed'] = True
        observer = join(second.port, 'b')
        connections.append(observer)
        assert welcome(receive(observer, 2))['facts'] == {2:0x22, 34:60}
        delta(fresh, 1, 2, 0x22)
        replay = commit(receive(fresh, 5))
        assert replay['sequence'] == 1 and replay['revision'] == 2 and replay['value'] == 0x22
        assert hashlib.sha256(files[0].read_bytes()).hexdigest() == persisted_hash
        # Only roster may be waiting; a replay must not create another live Commit.
        observer.settimeout(0.15)
        try:
            receive(observer, 5)
            raise AssertionError('Replay generated a duplicate live commit.')
        except socket.timeout:
            pass
        observer.settimeout(5)
        receipt['checks']['replay_receipt_survives_restart_exactly_once'] = True
        delta(fresh, 2, 34, 99)
        receive(fresh, 3)
        receipt['checks']['changed_replay_payload_rejected_after_restart'] = True
        incompatible = join(second.port, 'c', digest='2')
        connections.append(incompatible)
        receive(incompatible, 3)
        wrong_password = join(second.port, 'c', password='wrong')
        connections.append(wrong_password)
        receive(wrong_password, 3)
        separate = join(second.port, 'd', room='isolated', create=True)
        connections.append(separate)
        isolated = welcome(receive(separate, 2))
        assert isolated['facts'] == {} and isolated['room_id'] != baseline['room_id']
        receipt['checks']['compatibility_password_and_room_isolation'] = True
        # Force a real filesystem publication failure, without test-only hooks.
        # Preserve the accepted synthetic bytes and block its exact hashed target.
        saved = files[0].read_bytes()
        files[0].unlink()
        files[0].mkdir()
        delta(observer, 1, 3, 0x25)
        receive(observer, 3)  # No Commit or ACK may precede this refusal.
        receipt['checks']['filesystem_failure_rejected_before_ack'] = True
        for connection in connections:
            connection.close()
        connections.clear()
        receipt['processes'].append(second.close())
        servers.clear()
        files[0].rmdir()
        files[0].write_bytes(saved)
        third = Server(executable, directory)
        servers.append(third)
        verify = join(third.port, 'b')
        connections.append(verify)
        after_failure = welcome(receive(verify, 2))
        assert after_failure['revision'] == 2 and after_failure['acknowledged'] == 0 and after_failure['facts'] == {2:0x22, 34:60}
        receipt['checks']['failed_transaction_never_enters_persisted_progress_or_ack'] = True
        assert hashlib.sha256(personal.read_bytes()).hexdigest() == original_personal
        receipt['checks']['unrelated_file_preserved'] = True
        verify.close()
        connections.clear()
        receipt['processes'].append(third.close())
        servers.clear()
        receipt['passed'] = all(receipt['checks'].values())
        assert receipt['passed']
    except BaseException as error:
        receipt.update(passed=False, failure=f'{type(error).__name__}: {error}')
        raise
    finally:
        for connection in connections:
            connection.close()
        for server in servers:
            receipt['processes'].append(server.close())
        receipt['elapsed_seconds'] = time.monotonic()-started
        if args.receipt:
            args.receipt.parent.mkdir(parents=True, exist_ok=True)
            assert not args.receipt.exists(), 'Preserve prior attempt receipts.'
            args.receipt.write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
        print(json.dumps(receipt, indent=2))

if __name__ == '__main__':
    main()
