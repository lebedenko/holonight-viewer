#!/usr/bin/env python3
"""Sequential offscreen Release measurements; never automate desktop interaction."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
SCENARIOS = {
    'large': 'ReleasePerformance.LargeWorkflow',
    'navigation': 'ReleasePerformance.RepeatedNavigation',
    'gif-playback': 'ReleasePerformance.AnimatedGifFrameSwap',
    'gif-scan': 'ReleasePerformance.AnimatedGifScanNeverBlocksTheGui',
    'folder': 'Browsing.LargeFolderAndImages',
}

REQUIRED = {
    SCENARIOS['large']: {'open_ms', 'navigation_ms', 'transforms_ms', 'copy_ms',
                         'max_gui_timer_gap_ms', 'timer_ticks', 'standalone_png_encoding_ms'},
    SCENARIOS['navigation']: {'first_open_ms', 'warm_20_selections_ms', 'rapid_latest_selection_ms',
                              'shutdown_ms', 'max_gui_timer_gap_ms', 'timer_ticks', 'image_bytes',
                              'image_count', 'rss_first_open_kib', 'rss_warm_kib', 'rss_shutdown_kib',
                              *(f'pressure_cycle_{i}_ms' for i in range(3)),
                              *(f'rss_pressure_cycle_{i}_kib' for i in range(3))},
    SCENARIOS['gif-playback']: {'frame_pixels', 'replace_frame_ns', 'max_gui_stall_ms', 'swaps'},
    SCENARIOS['gif-scan']: {'open_ms', 'animated_ms', 'frame_count_known_ms', 'file_bytes', 'max_gui_stall_ms'},
    SCENARIOS['folder']: {'scan_ms', 'open_ms', 'two_navigations_ms', 'timer_ticks'},
}


def measurements(xml, expected):
    try:
        tree = ET.parse(xml)
        cases = list(tree.iter('testcase'))
        if (len(cases) != 1 or cases[0].get('status') != 'run'
                or cases[0].get('result') != 'completed'
                or f"{cases[0].get('classname')}.{cases[0].get('name')}" != expected
                or list(tree.iter('skipped')) or list(tree.iter('failure'))):
            raise ValueError('expected one completed, passing scenario')
        result = {}
        for prop in cases[0].iter('property'):
            name = prop.attrib['name']
            value = int(prop.attrib['value'])
            if name in result or value < 0:
                raise ValueError('duplicate or negative measurement')
            result[name] = value
        if not result or not REQUIRED.get(expected, set()).issubset(result):
            raise ValueError('missing measurements')
        return result
    except (OSError, ET.ParseError, ValueError, KeyError) as error:
        raise RuntimeError(f'Invalid measurements in {xml}: {error}') from error


def run_trial(binary, directory, scenario, run, env, timeout=300):
    xml = directory / f'run-{run}.xml'
    log = directory / f'run-{run}.log'
    samples = []
    started = time.monotonic()
    with log.open('w') as output:
        process = subprocess.Popen([str(binary), f'--gtest_filter={scenario}',
                                    f'--gtest_output=xml:{xml}'],
                                   stdout=output, stderr=subprocess.STDOUT, env=env)
        try:
            while True:
                pid, status, usage = os.wait4(process.pid, os.WNOHANG)
                if pid:
                    process.returncode = os.waitstatus_to_exitcode(status)
                    break
                elapsed = time.monotonic() - started
                if elapsed > timeout:
                    raise RuntimeError(f'Benchmark timed out: {log}')
                try:
                    for line in Path(f'/proc/{process.pid}/status').read_text().splitlines():
                        if line.startswith('VmRSS:'):
                            samples.append({'elapsed_ms': round(elapsed * 1000),
                                            'rss_kib': int(line.split()[1])})
                            break
                except FileNotFoundError:
                    pass  # Child exited between wait4 and reading /proc.
                time.sleep(0.02)
        finally:
            if process.returncode is None:
                process.kill()
                process.wait()
            (directory / f'run-{run}-rss.json').write_text(json.dumps(samples, indent=2) + '\n')
    if process.returncode != 0:
        raise RuntimeError(f'Benchmark failed ({process.returncode}): {log}')
    result = measurements(xml, scenario)
    result['peak_rss_kib'] = usage.ru_maxrss
    return result


def summary(results):
    names = set(results[0])
    if any(set(row) != names for row in results):
        raise RuntimeError('Measurement fields differ across trials')
    return {name: {'median': statistics.median(row[name] for row in results),
                   'min': min(row[name] for row in results),
                   'max': max(row[name] for row in results)} for name in sorted(names)}


def command(*args):
    return subprocess.check_output(args, text=True, cwd=ROOT).strip()


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def provenance(binary, prefix):
    cache = binary.parent.parent / 'CMakeCache.txt'
    values = {}
    for line in cache.read_text().splitlines():
        if '=' in line and not line.startswith(('#', '//')):
            key, value = line.split('=', 1)
            values[key.split(':')[0]] = value
    if values.get('CMAKE_BUILD_TYPE') != 'Release':
        raise RuntimeError('Measurements require a Release build')
    if Path(values['CMAKE_HOME_DIRECTORY']).resolve() != ROOT:
        raise RuntimeError('Binary build must belong to this instrumented source checkout')
    if str(prefix) not in values.get('CMAKE_PREFIX_PATH', '').split(';'):
        raise RuntimeError('Provider prefix differs from the configured build')
    files = ['scripts/measure-release.py', 'tests/release_performance_test.cpp',
             'tests/folder_browsing_test.cpp', 'tests/fixtures/dbus-session.conf']
    providers = {}
    for name in ('holonight-config', 'holonight-qt', 'holonight-images'):
        repo = ROOT.parent / name
        providers[name] = {'revision': command('git', '-C', str(repo), 'rev-parse', 'HEAD'),
                           'status': command('git', '-C', str(repo), 'status', '--porcelain')}
    libraries = {str(path.relative_to(prefix)): digest(path)
                 for path in sorted((prefix / 'lib').glob('libholonight*')) if path.is_file()}
    return {'revision': command('git', 'rev-parse', 'HEAD'),
            'source_diff_sha256': hashlib.sha256(command('git', 'diff', 'HEAD').encode()).hexdigest(),
            'instrumentation_sha256': {name: digest(ROOT / name) for name in files},
            'binary_sha256': digest(binary), 'cmake_cache_sha256': digest(cache),
            'compiler': command(values['CMAKE_CXX_COMPILER'], '--version'),
            'qt': command('pkg-config', '--modversion', 'Qt6Core'),
            'system': platform.platform(), 'cpu': command('lscpu'),
            'providers': providers, 'installed_library_sha256': libraries,
            'configuration': {key: value for key, value in values.items()
                              if key in ('CMAKE_BUILD_TYPE', 'CMAKE_CXX_COMPILER', 'CMAKE_CXX_FLAGS',
                                         'CMAKE_CXX_FLAGS_RELEASE', 'CMAKE_PREFIX_PATH', 'QML_IMPORT_PATH',
                                         'BUILD_TESTING')},
            'rss_note': 'Whole-process RSS includes fixture generation and allocator retention; '
                        'filesystem caches are not flushed. Offscreen is not native rendering acceptance.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--scenario', choices=[*SCENARIOS, 'all'], default='large')
    parser.add_argument('--prefix', type=Path, default=ROOT / 'build/deps/prefix')
    parser.add_argument('--private-bus', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.private_bus:
        return subprocess.call(['dbus-run-session',
                                f'--config-file={ROOT}/tests/fixtures/dbus-session.conf', '--',
                                sys.executable, str(Path(__file__).resolve()), *sys.argv[1:], '--private-bus'])
    binary = args.binary.resolve(strict=True)
    prefix = args.prefix.resolve(strict=True)
    metadata = provenance(binary, prefix)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError('Use an empty output directory to preserve earlier evidence')
    (output / 'environment.json').write_text(json.dumps(metadata, indent=2) + '\n')
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen', QSG_RHI_BACKEND='software',
               QML_IMPORT_PATH=str(prefix / 'lib/qt6/qml'), LD_LIBRARY_PATH=str(prefix / 'lib'),
               VIEWER_PERFORMANCE='1', VIEWER_BROWSE_BENCHMARK='1')
    for name in ('VIEWER_COPY_TRANSFER', 'VIEWER_NATIVE_PERFORMANCE', 'VIEWER_CAPTURE_PREFIX'):
        env.pop(name, None)
    scenarios = SCENARIOS if args.scenario == 'all' else {args.scenario: SCENARIOS[args.scenario]}
    summaries = {}
    for name, scenario in scenarios.items():
        directory = output / name if args.scenario == 'all' else output
        directory.mkdir(exist_ok=True)
        results = []
        for run in range(5):
            print(f'{name}: trial {run + 1}/5', flush=True)
            results.append(run_trial(binary, directory, scenario, run, env))
            (directory / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        summaries[name] = summary(results)
    (output / 'summary.json').write_text(json.dumps(summaries, indent=2) + '\n')
    print(json.dumps(summaries, indent=2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
