#!/usr/bin/env python3
"""Sequential native labwc trials, interactive receiver and independent pixel checks.

Requires wtype, PyGObject/GdkPixbuf, a native Wayland desktop and installed providers.
Use identical binaries' instrumentation/environment and fixtures for both revisions.
All output must be beneath the invoking checkout's build directory.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import time
import xml.etree.ElementTree as ET

import gi

gi.require_version('GdkPixbuf', '2.0')
from gi.repository import GdkPixbuf


def wait_for(predicate, processes, timeout=65):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        if any(process.poll() is not None for process in processes):
            raise RuntimeError('Process exited before expected marker; inspect trial logs')
        time.sleep(0.005)
    raise TimeoutError('Native trial did not complete; inspect trial logs')


def reap(process, timeout=65):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        pid, status, resource = os.wait4(process.pid, os.WNOHANG)
        if pid:
            process.returncode = os.waitstatus_to_exitcode(status)
            return resource
        time.sleep(0.005)
    raise TimeoutError('Child did not exit after reception')


def check_pixels(path, width, height, rgba):
    image = GdkPixbuf.Pixbuf.new_from_file(str(path))
    size = [image.get_width(), image.get_height()]
    channels = image.get_n_channels()
    data = image.get_pixels()
    stride = image.get_rowstride()
    row = bytes(rgba[:channels]) * width
    correct = size == [width, height] and channels == 4
    if correct:
        correct = all(data[y * stride:y * stride + width * channels] == row
                      for y in range(height))
    return {'correct': correct, 'dimensions': size, 'channels': channels,
            'first_pixel': list(data[:channels]), 'expected_rgba': rgba,
            'expected_dimensions': [width, height]}


def trial(binary, receiver, output, env):
    output.mkdir(parents=True, exist_ok=False)
    env = dict(env, VIEWER_NATIVE_OUTPUT_DIR=str(output))
    processes = []
    logs = []
    usage = {}
    try:
        def launch(args, name):
            log = (output / f'{name}.log').open('w')
            logs.append(log)
            process = subprocess.Popen(args, env=env, stdout=log, stderr=subprocess.STDOUT)
            processes.append(process)
            return process

        viewer = launch([str(binary), '--gtest_filter=ReleasePerformance.NativeWorkflow',
                         f'--gtest_output=xml:{output}/metrics.xml'], 'viewer')
        wait_for(lambda: (output / 'viewer-ready').exists(), [viewer])
        # Give the owner a native input serial before it publishes a selection.
        subprocess.run(['wtype', '-k', 'Shift_L'], env=env, check=True, timeout=10)
        (output / 'viewer-activated').write_text('native input delivered\n')
        wait_for(lambda: (output / 'copy-published').exists(), [viewer])
        probe = launch([str(receiver), '--interactive', 'image', str(output / 'received.png')],
                       'receiver')
        wait_for(lambda: (output / 'receiver-ready').exists(), [viewer, probe])
        # Real virtual-keyboard Wayland protocol, delivered by the compositor to
        # the newly active receiver. No in-process receiver invocation.
        subprocess.run(['wtype', '-k', 'Return'], env=env, check=True, timeout=10)
        wait_for(lambda: (output / 'receiver-complete').exists(), [viewer, probe])
        # wait4 measures each child independently, including later save/hash costs.
        resource = reap(probe)
        usage['receiver_peak_rss_kib'] = resource.ru_maxrss
        if probe.returncode:
            raise RuntimeError('Receiver failed')
        pixels = check_pixels(output / 'received.png', 4000, 8000, [0, 255, 0, 128])
        (output / 'pixels.json').write_text(json.dumps(pixels, indent=2) + '\n')
        (output / 'receiver-validated').write_text('validation recorded\n')
        resource = reap(viewer)
        usage['viewer_peak_rss_kib'] = resource.ru_maxrss
        if viewer.returncode:
            raise RuntimeError('Viewer workflow failed')
        tree = ET.parse(output / 'metrics.xml')
        if list(tree.iter('failure')) or list(tree.iter('skipped')):
            raise RuntimeError('Workflow failed or skipped')
        properties = {p.attrib['name']: p.attrib['value'] for p in tree.iter('property')}
        if not properties:
            raise RuntimeError('Missing metrics')
        metrics = {key: float(value) if key != 'render_loop' else value
                   for key, value in properties.items()}
        received_log = (output / 'receiver.log').read_text()
        metrics['receiver_read_ms'] = int(re.search(r'read_ms=(\d+)', received_log)[1])
        result = dict(metrics, **usage, pixels=pixels)
        (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        return result
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        for log in logs:
            log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('receiver', type=Path)
    parser.add_argument('fixtures', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--trials', type=int, default=5)
    parser.add_argument('--allow-incorrect-baseline', action='store_true')
    args = parser.parse_args()
    if args.trials < 1:
        parser.error('--trials must be positive')
    if args.allow_incorrect_baseline and args.revision != 'de7ef473eaca2e5d4e2de0b0084dbc691412c473':
        parser.error('Incorrect-output allowance is limited to the historical baseline')
    output = args.output.resolve()
    output.relative_to(Path('build').resolve())
    output.mkdir(parents=True, exist_ok=False)
    fixtures = args.fixtures.resolve(strict=True)
    for filename, color in [('1.png', [255, 0, 0, 128]), ('2.png', [0, 255, 0, 128])]:
        if not check_pixels(fixtures / filename, 8000, 4000, color)['correct']:
            raise RuntimeError(f'Fixture mismatch: {filename}')
    env = dict(os.environ, QT_QPA_PLATFORM='wayland', QSG_RHI_BACKEND='opengl',
               QSG_RENDER_LOOP='basic', QSG_INFO='1', QT_SCALE_FACTOR='1',
               VIEWER_NATIVE_PERFORMANCE='1', VIEWER_RECEIVER_ONESHOT='1',
               VIEWER_NATIVE_FIXTURE_DIR=str(fixtures))
    env.pop('DISPLAY', None)
    environment = {name: env.get(name) for name in [
        'WAYLAND_DISPLAY', 'XDG_CURRENT_DESKTOP', 'QT_QPA_PLATFORM', 'QT_QPA_PLATFORMTHEME',
        'QSG_RHI_BACKEND', 'QSG_RENDER_LOOP', 'QT_SCALE_FACTOR', 'QML_IMPORT_PATH', 'LD_LIBRARY_PATH']}
    environment.update(revision=args.revision, fixtures={
        name: hashlib.sha256((fixtures / name).read_bytes()).hexdigest() for name in ['1.png', '2.png']},
        memory_boundary='Linux wait4 ru_maxrss, per child lifetime including capture/save/hash; not GPU memory',
        transfer_boundary='copy initiation to receiver read/convert completion; includes receiver startup and compositor activation',
        frame_boundary='Qt frameSwapped after updated canvas synchronization, not physical display latency',
        persistence_service='deferred; no service launched')
    (output / 'environment.json').write_text(json.dumps(environment, indent=2) + '\n')
    results = []
    for number in range(args.trials):
        result = trial(args.binary.resolve(strict=True), args.receiver.resolve(strict=True),
                       output / f'run-{number + 1}', env)
        results.append(result)
        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        print(json.dumps(result), flush=True)
    summary = {key: {'median': statistics.median(run[key] for run in results),
                     'min': min(run[key] for run in results), 'max': max(run[key] for run in results)}
               for key, value in results[0].items() if isinstance(value, (int, float))}
    summary['correct_runs'] = sum(run['pixels']['correct'] for run in results)
    summary['trials'] = args.trials
    summary['output_pass'] = summary['correct_runs'] == args.trials
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    if not summary['output_pass'] and not args.allow_incorrect_baseline:
        raise RuntimeError('Candidate output is incorrect; qualification failed')


if __name__ == '__main__':
    main()
