#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
"""Exercise benchmark acceptance using small independent executable producers."""
import importlib.util
import os
from pathlib import Path
import tempfile
import sys
import unittest

sys.dont_write_bytecode = True

spec = importlib.util.spec_from_file_location('measure', Path(__file__).with_name('measure-release.py'))
measure = importlib.util.module_from_spec(spec)
spec.loader.exec_module(measure)


class Measurements(unittest.TestCase):
    def trial(self, body, timeout=5):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            binary = directory / 'producer'
            binary.write_text('#!/usr/bin/env python3\nimport pathlib, sys, time\n'
                              'xml = pathlib.Path(sys.argv[2].split("xml:", 1)[1])\n' + body)
            binary.chmod(0o755)
            return measure.run_trial(binary, directory, 'Suite.Case', 0, os.environ, timeout)

    def xml(self, content):
        return self.trial(f'xml.write_text({content!r})\n')

    def test_completed_measurements_and_summary(self):
        result = self.xml('<testsuites><testsuite><testcase classname="Suite" name="Case" '
                          'status="run" result="completed"><properties>'
                          '<property name="latency_ms" value="12"/>'
                          '</properties></testcase></testsuite></testsuites>')
        self.assertEqual(result['latency_ms'], 12)
        self.assertGreater(result['peak_rss_kib'], 0)
        self.assertEqual(measure.summary([{'x': 1}, {'x': 9}, {'x': 2}]),
                         {'x': {'median': 2, 'min': 1, 'max': 9}})

    def test_invalid_or_incomplete_measurements_fail(self):
        case = '<testcase classname="Suite" name="Case" status="run" result="completed">{}</testcase>'
        prop = '<properties><property name="latency_ms" value="12"/></properties>'
        for content in ('broken XML', '<testsuites/>', case.format(''),
                        case.format('<skipped/>' + prop), case.format('<failure/>' + prop),
                        case.format(prop.replace('12', 'nan')), case.format(prop.replace('12', '-1')),
                        case.format(prop + prop), case.format(prop).replace('Case', 'Other'),
                        case.format(prop) + case.format(prop)):
            with self.subTest(content=content), self.assertRaises(RuntimeError):
                self.xml(content)

    def test_missing_required_metric_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            xml = Path(temporary) / 'result.xml'
            xml.write_text('<testcase classname="ReleasePerformance" name="LargeWorkflow" '
                           'status="run" result="completed"><properties>'
                           '<property name="open_ms" value="1"/></properties></testcase>')
            with self.assertRaises(RuntimeError):
                measure.measurements(xml, 'ReleasePerformance.LargeWorkflow')

    def test_process_failure_missing_output_and_timeout_fail(self):
        for body in ('sys.exit(3)', 'pass', 'time.sleep(1)'):
            with self.subTest(body=body), self.assertRaises(RuntimeError):
                self.trial(body, timeout=0.1)

    def test_inconsistent_metrics_fail(self):
        with self.assertRaises(RuntimeError):
            measure.summary([{'a': 1}, {'b': 2}])


if __name__ == '__main__':
    unittest.main()
