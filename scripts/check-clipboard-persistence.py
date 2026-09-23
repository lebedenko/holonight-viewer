#!/usr/bin/env python3
"""Validate passive evidence without a desktop session."""
import importlib.util
from pathlib import Path
import unittest
import contextlib
import io
import json
import tempfile
from types import SimpleNamespace
import sys

sys.dont_write_bytecode = True

spec = importlib.util.spec_from_file_location('persistence', Path(__file__).with_name('clipboard-persistence.py'))
persistence = importlib.util.module_from_spec(spec)
spec.loader.exec_module(persistence)


class EvidenceTest(unittest.TestCase):
    protocol = '[100.000] -> wl_data_device#12.set_selection(wl_data_source#42, 9)\n[112.500] wl_data_source#42.cancelled()\n'
    receiver = 'receive_begin\nread_ms=7 image 2x3 rgba-sha256=' + 'a' * 64 + ' success=1\n'

    def record(self):
        return {'service_pid': 123, 'service_final_pid': 123, 'exit_codes': [0, 0], 'viewer_exited_before_receive': True,
                'expected': [2, 3, 'a' * 64],
                'samples': [{role: {'VmRSS': 10, 'VmHWM': 20} for role in ('viewer', 'service', 'receiver')},
                            {role: {'VmRSS': 30, 'VmHWM': 40} for role in ('viewer', 'service', 'receiver')}]}

    def test_separate_measurements(self):
        result = persistence.summarize(self.record(), self.protocol, self.receiver)
        self.assertEqual(result['service_handoff_ms'], 12.5)
        self.assertEqual(result['receiver_read_ms'], 7)
        self.assertEqual(result['service']['peak_sampled_rss_kib'], 30)
        self.assertEqual(result['receiver']['process_hwm_kib'], 40)

    def test_wayland_object_notations(self):
        self.assertEqual(persistence.handoff_ms(self.protocol.replace('#', '@')), 12.5)

    def test_wall_clock_wayland_timestamps(self):
        protocol = self.protocol.replace('100.000', '19:17:09.950512').replace('112.500', '19:17:09.952572')
        self.assertAlmostEqual(persistence.handoff_ms(protocol), 2.06, places=4)

    def test_ambiguous_handoff(self):
        for protocol in ('', self.protocol * 2, self.protocol.splitlines()[0],
                         self.protocol.replace('source#42.cancelled', 'source#43.cancelled')):
            with self.subTest(protocol=protocol), self.assertRaises(ValueError):
                persistence.handoff_ms(protocol)

    def test_wrong_pixels_or_dimensions(self):
        for expected in ([3, 2, 'a' * 64], [2, 3, 'b' * 64]):
            record = self.record()
            record['expected'] = expected
            with self.assertRaises(ValueError):
                persistence.summarize(record, self.protocol, self.receiver)

    def test_failure_or_receive_before_exit(self):
        for key, value in [('exit_codes', [0, 1]), ('viewer_exited_before_receive', False), ('service_final_pid', 456)]:
            record = self.record()
            record[key] = value
            with self.assertRaises(ValueError):
                persistence.summarize(record, self.protocol, self.receiver)

    def test_five_trial_report_and_mismatched_provenance(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / str(i) for i in range(5)]
            row = self.record()
            for key in ('fixture_sha256', 'viewer_sha256', 'receiver_sha256', 'service_sha256',
                        'tool_sha256', 'versions', 'environment', 'monitors'):
                row[key] = 'same'
            for path in paths:
                path.mkdir()
                (path / 'raw.json').write_text(json.dumps(row))
                (path / 'viewer.log').write_text(self.protocol)
                (path / 'receiver.log').write_text(self.receiver)
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                persistence.report(SimpleNamespace(trials=paths))
            report = json.loads(output.getvalue())
            self.assertEqual(report['explicit_user_cost_acceptance'], 'pending')
            self.assertEqual(report['service_handoff_ms']['median'], 12.5)
            with self.assertRaises(ValueError):
                persistence.report(SimpleNamespace(trials=paths[:4]))
            with self.assertRaises(ValueError):
                persistence.report(SimpleNamespace(trials=[paths[0]] * 5))
            row['fixture_sha256'] = 'different'
            (paths[-1] / 'raw.json').write_text(json.dumps(row))
            with self.assertRaises(ValueError):
                persistence.report(SimpleNamespace(trials=paths))

    def test_missing_memory_or_multiple_receives(self):
        record = self.record()
        record['samples'] = []
        with self.assertRaises(ValueError):
            persistence.summarize(record, self.protocol, self.receiver)
        with self.assertRaises(ValueError):
            persistence.summarize(self.record(), self.protocol, self.receiver * 2)


if __name__ == '__main__':
    unittest.main()
