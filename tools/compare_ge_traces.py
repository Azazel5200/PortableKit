#!/usr/bin/env python3
"""Compare two bounded PSP GE HLE traces without reading any game data.

Input files are ordinary runtime logs. This tool does not require an ISO,
generated corpus, network access, or platform-specific Python packages.
"""

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
import re
from statistics import median


GE_LINE = re.compile(
    r"\[ge-submit\]\s+(enqueue|update-stall)\s+vblank=(\d+)\s+"
    r"thread=(.*?)\s+dispatch=([0-9a-fA-F]{8})\s+"
    r"pc=([0-9a-fA-F]{8})\s+ra=([0-9a-fA-F]{8})\b"
)
PERF_LISTS = re.compile(r"\[perf\].*?\blists\s+(\d+)/s\b")


@dataclass(frozen=True)
class GeEvent:
    operation: str
    vblank: int
    thread: str
    dispatch_pc: int
    pc: int
    ra: int

    @property
    def site(self) -> tuple[str, int]:
        # Guest return address is comparable across the interpreter and AOT.
        # Their dispatch_pc may represent different dispatcher boundaries.
        return self.operation, self.ra


@dataclass
class Trace:
    events: list[GeEvent]
    lists_per_second: list[int]


def parse_trace(lines: list[str]) -> Trace:
    events: list[GeEvent] = []
    rates: list[int] = []
    for line in lines:
        match = GE_LINE.search(line)
        if match:
            operation, vblank, thread, dispatch_pc, pc, ra = match.groups()
            events.append(
                GeEvent(
                    operation,
                    int(vblank),
                    thread,
                    int(dispatch_pc, 16),
                    int(pc, 16),
                    int(ra, 16),
                )
            )
        perf = PERF_LISTS.search(line)
        if perf:
            rates.append(int(perf.group(1)))
    return Trace(events, rates)


def print_summary(name: str, trace: Trace) -> None:
    operations = Counter(event.operation for event in trace.events)
    print(f"{name}: {len(trace.events)} recorded GE calls (bounded sample)")
    print(f"  enqueue={operations['enqueue']} update-stall={operations['update-stall']}")
    if trace.events:
        print(f"  PSP vblanks: {trace.events[0].vblank} .. {trace.events[-1].vblank}")
    if trace.lists_per_second:
        print(f"  [perf] median submitted lists/s: {median(trace.lists_per_second):g}")
    sites = Counter(event.site for event in trace.events)
    for (operation, ra), count in sites.most_common(5):
        print(f"  caller {operation} ra=0x{ra:08X}: {count} calls")


def compare(reference: Trace, candidate: Trace) -> None:
    print_summary("Reference", reference)
    print_summary("Candidate", candidate)
    if not reference.events:
        print("\nNo reference GE calls in the selected window; comparison is inconclusive.")
        return
    if not candidate.events:
        print("\nNo candidate GE calls were captured. Either its trace window was never")
        print("reached or the guest did not submit GE calls in that window.")
        print("Check the per-second [perf] list counts before diagnosing the renderer.")
        return

    ref_sites = {event.site for event in reference.events}
    candidate_sites = {event.site for event in candidate.events}
    missing = sorted(ref_sites - candidate_sites)
    introduced = sorted(candidate_sites - ref_sites)
    print(f"\nGE caller sites: {len(ref_sites & candidate_sites)} shared, "
          f"{len(missing)} reference-only, {len(introduced)} candidate-only")
    for operation, ra in missing[:8]:
        print(f"  reference-only: {operation} ra=0x{ra:08X}")
    for operation, ra in introduced[:8]:
        print(f"  candidate-only: {operation} ra=0x{ra:08X}")
    if reference.events[0].vblank != candidate.events[0].vblank:
        print("Note: first captured vblanks differ; verify that both runs used")
        print("the same scripted inputs and TRACE_GE_SUBMIT_FROM setting.")
    print("These bounded samples do not prove why a call is absent or that")
    print("the game scenes match. Inspect the AOT HLE histogram and [perf] too.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path, help="Known-good interpreter runtime log")
    parser.add_argument("candidate", type=Path, help="AOT runtime log to compare")
    args = parser.parse_args()
    try:
        reference = parse_trace(args.reference.read_text(encoding="utf-8", errors="replace").splitlines())
        candidate = parse_trace(args.candidate.read_text(encoding="utf-8", errors="replace").splitlines())
    except OSError as exc:
        parser.error(str(exc))
    compare(reference, candidate)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
