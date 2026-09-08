#!/usr/bin/env python3
"""Five fresh release processes; preserve XML latency and per-process peak RSS."""
import json
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

binary = Path(sys.argv[1]).resolve(strict=True)
output = Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
results = []
for run in range(5):
    xml = output / f'run-{run}.xml'
    with (output / f'run-{run}.log').open('w') as log:
        process = subprocess.Popen([str(binary),
                        '--gtest_filter=ReleasePerformance.LargeWorkflow',
                        f'--gtest_output=xml:{xml}'], stdout=log, stderr=subprocess.STDOUT,
                                   env=dict(os.environ, VIEWER_PERFORMANCE='1'))
        _, status, usage = os.wait4(process.pid, 0)
        process.returncode = os.waitstatus_to_exitcode(status)
        if process.returncode != 0:
            raise RuntimeError(f'Benchmark failed: {output}/run-{run}.log')
    tree = ET.parse(xml)
    if any(True for _ in tree.iter('skipped')) or not list(tree.iter('property')):
        raise RuntimeError('Benchmark did not produce measurements')
    properties = {p.attrib['name']: int(p.attrib['value'])
                  for p in tree.iter('property')}
    properties['peak_rss_kib'] = usage.ru_maxrss
    results.append(properties)
(output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
