"""Game-free checks for the GE runtime log comparator."""

import contextlib
import io
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from compare_ge_traces import compare, parse_trace  # noqa: E402


class TraceComparisonTests(unittest.TestCase):
    def test_parser_uses_guest_return_address(self):
        sample = [
            "[ge-submit] enqueue vblank=4801 thread=main 1 dispatch=08801000 "
            "pc=08804110 ra=08804008 a0=00100000 a1=00000000 a2=00000000",
            "[perf] fps 30.0 game 30.0 | lists 30/s draws 20/20",
            "[ge-submit] update-stall vblank=4802 thread=main 1 dispatch=08803000 "
            "pc=08804120 ra=0880400C a0=00000001 a1=00100010 a2=00000000",
        ]
        trace = parse_trace(sample)
        self.assertEqual(len(trace.events), 2)
        self.assertEqual(trace.events[0].thread, "main 1")
        self.assertEqual(trace.events[0].site, ("enqueue", 0x08804008))
        self.assertEqual(trace.events[1].site, ("update-stall", 0x0880400C))
        self.assertEqual(trace.lists_per_second, [30])

    def test_lost_candidate_calls_are_reported_cautiously(self):
        reference = parse_trace([
            "[ge-submit] enqueue vblank=5000 thread=main dispatch=08801000 "
            "pc=08804110 ra=08804008 a0=00000000 a1=00000000 a2=00000000",
        ])
        candidate = parse_trace(["[perf] fps 30.0 | lists 0/s draws 0/0"])
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            compare(reference, candidate)
        self.assertIn("No candidate GE calls were captured", output.getvalue())
        self.assertIn("window was never", output.getvalue())

    def test_comparison_reports_guest_sites_not_host_dispatch(self):
        reference = parse_trace([
            "[ge-submit] enqueue vblank=5000 thread=main dispatch=08801000 "
            "pc=08804110 ra=08804008 a0=00000000 a1=00000000 a2=00000000",
        ])
        candidate = parse_trace([
            "[ge-submit] enqueue vblank=5000 thread=main dispatch=08803000 "
            "pc=08804110 ra=08804008 a0=00000000 a1=00000000 a2=00000000",
        ])
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            compare(reference, candidate)
        self.assertIn("1 shared, 0 reference-only, 0 candidate-only", output.getvalue())


if __name__ == "__main__":
    unittest.main()
