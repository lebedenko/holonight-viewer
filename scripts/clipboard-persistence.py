#!/usr/bin/env python3
"""Passive, human-driven clipboard trials. Never sends input or activates windows."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import time


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def memory(pid):
    try:
        status = Path(f'/proc/{pid}/status').read_text()
        values = dict(re.findall(r'^(VmRSS|VmHWM):\s+(\d+) kB$', status, re.M))
        return {key: int(value) for key, value in values.items()}
    except FileNotFoundError:
        return {}


def service_pid():
    result = subprocess.run(['pgrep', '-u', str(os.getuid()), '-x', 'wl-clip-persist'],
                            capture_output=True, text=True, check=False)
    pids = result.stdout.split()
    if len(pids) != 1:
        raise ValueError(f'Expected exactly one wl-clip-persist process, found {len(pids)}')
    pid = int(pids[0])
    command = Path(f'/proc/{pid}/cmdline').read_bytes().split(b'\0')
    if command[1:3] != [b'--clipboard', b'regular'] or any(command[3:]):
        raise ValueError('Service must use --clipboard regular with upstream defaults')
    return pid


def handoff_ms(protocol):
    offers = re.findall(r'\[\s*([\d:.]+)\].*-> wl_data_device[#@]\d+\.set_selection\(wl_data_source[#@](\d+),', protocol)
    if len(offers) != 1:
        raise ValueError('Trial requires exactly one Viewer clipboard publication')
    start, source = offers[0]
    ends = re.findall(r'\[\s*([\d:.]+)\].*wl_data_source[#@]' + source + r'\.cancelled\(', protocol)
    def milliseconds(value):
        if ':' not in value:
            return float(value)
        hours, minutes, seconds = value.split(':')
        return (int(hours) * 3600 + int(minutes) * 60 + float(seconds)) * 1000

    if len(ends) != 1 or milliseconds(ends[0]) < milliseconds(start):
        raise ValueError('Missing or ambiguous ownership handoff; trial cannot be timed')
    return milliseconds(ends[0]) - milliseconds(start)


def summarize(record, protocol, receiver):
    if record['exit_codes'] != [0, 0] or not record['viewer_exited_before_receive']:
        raise ValueError('Viewer must exit successfully before successful reception')
    if record['service_pid'] != record['service_final_pid']:
        raise ValueError('Service restarted during trial')
    if receiver.count('receive_begin') != 1:
        raise ValueError('Missing or repeated receive-start marker')
    received = re.findall(r'read_ms=(\d+) image (\d+)x(\d+).*rgba-sha256=([0-9a-f]{64}) success=1', receiver)
    if len(received) != 1:
        raise ValueError('Expected exactly one successful image receive')
    latency, width, height, pixels = received[0]
    if [int(width), int(height), pixels] != record['expected']:
        raise ValueError('Received dimensions or RGBA pixels differ from expected fixture')
    result = {'service_handoff_ms': handoff_ms(protocol), 'receiver_read_ms': int(latency)}
    for role in ('service', 'receiver', 'viewer'):
        samples = [sample[role] for sample in record['samples'] if sample[role].get('VmRSS') is not None]
        if not samples:
            raise ValueError(f'No memory samples for {role}')
        result[role] = {'baseline_rss_kib': samples[0]['VmRSS'],
                        'peak_sampled_rss_kib': max(s['VmRSS'] for s in samples),
                        'last_sampled_rss_kib': samples[-1]['VmRSS'],
                        'process_hwm_kib': max(s.get('VmHWM', 0) for s in samples)}
    return result


def capture(args):
    if os.environ.get('XDG_SESSION_TYPE') != 'wayland':
        raise ValueError('Run in the native Wayland session')
    pid = service_pid()
    # Preserve lexical source path, including a symlink, for path acceptance.
    fixture = Path(os.path.abspath(args.fixture))
    viewer = args.viewer.resolve(strict=True)
    receiver = args.receiver.resolve(strict=True)
    args.output.mkdir(parents=True, exist_ok=False)
    record = {'fixture': str(fixture), 'fixture_sha256': digest(fixture),
              'viewer_sha256': digest(viewer), 'receiver_sha256': digest(receiver),
              'service_sha256': digest(f'/proc/{pid}/exe'), 'service_pid': pid,
              'tool_sha256': digest(__file__), 'date_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
              'expected': [args.width, args.height, args.rgba_sha256],
              'service_command': '/usr/bin/wl-clip-persist --clipboard regular',
              'environment': {key: os.environ.get(key) for key in ('QT_SCALE_FACTOR', 'QT_QPA_PLATFORMTHEME', 'QML_IMPORT_PATH', 'LD_LIBRARY_PATH')},
              'versions': {}, 'samples': [], 'viewer_exited_before_receive': False}
    for name, command in {'package': ['pacman', '-Q', 'wl-clip-persist'],
                          'qt': ['pkg-config', '--modversion', 'Qt6Core'],
                          'compositor': ['hyprctl', 'version'],
                          'gimp': ['gimp', '--version'],
                          'viewer_revision': ['git', 'rev-parse', 'HEAD']}.items():
        record['versions'][name] = subprocess.check_output(command, text=True, cwd=Path(__file__).resolve().parents[1]).strip()
    record['monitors'] = [{key: row[key] for key in ('name', 'width', 'height', 'scale', 'refreshRate')}
                          for row in json.loads(subprocess.check_output(['hyprctl', 'monitors', '-j'], text=True))]
    environment = dict(os.environ, QT_QPA_PLATFORM='wayland')
    environment.pop('DISPLAY', None)
    started = time.monotonic_ns()
    with (args.output / 'viewer.log').open('w') as viewer_log, (args.output / 'receiver.log').open('w') as receiver_log:
        owner = subprocess.Popen([str(viewer), str(fixture)], env=dict(environment, WAYLAND_DEBUG='1'),
                                 stdout=viewer_log, stderr=viewer_log)
        consumer = subprocess.Popen([str(receiver), '--interactive', 'image', str(args.output / 'received.png')],
                                    env=dict(environment, VIEWER_RECEIVER_ONESHOT='1'),
                                    stdout=receiver_log, stderr=receiver_log)
        print('Manually activate Viewer, copy once, wait for Copied, quit Viewer; then activate the receiver and paste.', flush=True)
        # The receiver flushes receive_begin before requesting data; require exit before that marker.
        try:
            while owner.poll() is None or consumer.poll() is None:
                sample = {'elapsed_ns': time.monotonic_ns() - started,
                          'service': memory(pid), 'viewer': memory(owner.pid), 'receiver': memory(consumer.pid)}
                record['samples'].append(sample)
                if owner.poll() is not None and 'viewer_exit_ns' not in record:
                    record['viewer_exit_ns'] = sample['elapsed_ns']
                    record['viewer_exited_before_receive'] = 'receive_begin' not in (args.output / 'receiver.log').read_text()
                time.sleep(0.02)
            record['exit_codes'] = [owner.returncode, consumer.returncode]
            record['service_final_pid'] = service_pid()
        finally:
            (args.output / 'raw.json').write_text(json.dumps(record, indent=2) + '\n')
    result = summarize(record, (args.output / 'viewer.log').read_text(), (args.output / 'receiver.log').read_text())
    (args.output / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


def report(args):
    if len(args.trials) != 5 or len({p.resolve() for p in args.trials}) != 5:
        raise ValueError('Acceptance requires exactly five identical trials')
    records = [json.loads((p / 'raw.json').read_text()) for p in args.trials]
    for key in ('fixture_sha256', 'viewer_sha256', 'receiver_sha256', 'service_sha256', 'tool_sha256', 'expected', 'versions', 'environment', 'monitors'):
        if any(row[key] != records[0][key] for row in records):
            raise ValueError(f'Trials differ: {key}')
    results = [summarize(row, (path / 'viewer.log').read_text(), (path / 'receiver.log').read_text())
               for row, path in zip(records, args.trials)]
    output = {'trials': results, 'explicit_user_cost_acceptance': 'pending'}
    for key in ('service_handoff_ms', 'receiver_read_ms'):
        values = [row[key] for row in results]
        output[key] = {'median': statistics.median(values), 'min': min(values), 'max': max(values)}
    print(json.dumps(output, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    trial = commands.add_parser('capture')
    for name in ('viewer', 'receiver', 'fixture', 'output'):
        trial.add_argument('--' + name, type=Path, required=True)
    trial.add_argument('--width', type=int, required=True)
    trial.add_argument('--height', type=int, required=True)
    trial.add_argument('--rgba-sha256', required=True)
    trial.set_defaults(run=capture)
    summary = commands.add_parser('report')
    summary.add_argument('trials', type=Path, nargs='+')
    summary.set_defaults(run=report)
    args = parser.parse_args()
    args.run(args)


if __name__ == '__main__':
    main()
