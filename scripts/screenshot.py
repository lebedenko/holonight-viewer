#!/usr/bin/env python3
"""Capture a reproducible Hyprland screenshot of the Viewer window.

Workflow:
1. Start `task run` with any forwarded CLI arguments.
2. Detect the newly created Viewer client via `hyprctl clients -j`.
3. Optionally sleep for `--delay` seconds.
4. Read client geometry, expand it by `--margin`, and capture via `grim -g`.
5. Close only the started viewer window and clean up the runner.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import signal
import subprocess
import sys
import time
import math
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Set, Tuple


def parse_args(argv: Sequence[str]) -> Tuple[argparse.Namespace, List[str]]:
    """Parse screenshot options and separate arguments to `task run`."""

    if "--" in argv:
        sep = argv.index("--")
        script_args = list(argv[:sep])
        run_args = list(argv[sep + 1 :])
    else:
        script_args = list(argv)
        run_args = []

    parser = argparse.ArgumentParser(description="Capture Viewer screenshot")
    parser.add_argument(
        "--delay",
        type=float,
        default=0.0,
        help="Seconds to wait after window appears before capture (default: 0)",
    )
    parser.add_argument(
        "--margin",
        type=int,
        default=0,
        help="Expand the screenshot area by this many pixels on all sides (default: 0)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=60.0,
        help="Timeout waiting for Viewer window in seconds (default: 60)",
    )
    parser.add_argument(
        "--output",
        default="build/screenshot.png",
        help="Output screenshot path (default: build/screenshot.png)",
    )

    known, unknown = parser.parse_known_args(script_args)
    run_args.extend(unknown)
    return known, run_args


def ensure_dependency(name: str) -> None:
    if shutil.which(name) is None:
        raise RuntimeError(f"required command not found: {name}")


def parse_hyprctl_stat(stat_line: str) -> int:
    # Follows Linux /proc/<pid>/stat structure: pid (comm) state ppid ...
    close_paren = stat_line.rfind(")")
    if close_paren < 0:
        raise ValueError("invalid /proc stat format")
    suffix = stat_line[close_paren + 1 :].strip().split()
    if len(suffix) < 2:
        raise ValueError("invalid /proc stat format")
    return int(suffix[1])


def descendant_pids(root_pid: int) -> Set[int]:
    descendants: Set[int] = {root_pid}

    children_by_parent: Dict[int, List[int]] = {}
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        proc_pid = int(entry)
        try:
            with open(f"/proc/{proc_pid}/stat", "r", encoding="utf-8") as proc_stat:
                ppid = parse_hyprctl_stat(proc_stat.read())
            children_by_parent.setdefault(ppid, []).append(proc_pid)
        except (FileNotFoundError, PermissionError, OSError, ValueError):
            continue

    stack = [root_pid]
    while stack:
        parent = stack.pop()
        for child in children_by_parent.get(parent, ()):
            if child not in descendants:
                descendants.add(child)
                stack.append(child)

    return descendants


def hypr_clients() -> List[Dict[str, Any]]:
    result = subprocess.run(
        ["hyprctl", "clients", "-j"],
        check=True,
        text=True,
        capture_output=True,
    )
    data = json.loads(result.stdout)
    if not isinstance(data, list):
        raise RuntimeError("unexpected output from `hyprctl clients -j`")
    return [item for item in data if isinstance(item, dict)]


def client_identifier(client: Dict[str, Any]) -> Optional[str]:
    address = client.get("address")
    if isinstance(address, str) and address:
        return address
    return None


def client_position_size(client: Dict[str, Any]) -> Tuple[int, int, int, int]:
    at = client.get("at")
    size = client.get("size")
    if not (isinstance(at, list) and len(at) >= 2 and isinstance(size, list) and len(size) >= 2):
        raise RuntimeError(f"client geometry missing for {client_identifier(client)}")
    x, y = int(at[0]), int(at[1])
    w, h = int(size[0]), int(size[1])
    return x, y, w, h


def classify_client(
    client: Dict[str, Any],
    baseline: Set[str],
    target_pids: Set[int],
) -> bool:
    address = client_identifier(client)
    if not address or address in baseline:
        return False
    return client.get("pid") in target_pids


def find_viewer_window(
    baseline: Set[str],
    proc_pid: int,
    timeout: float,
) -> Optional[Dict[str, Any]]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        clients = hypr_clients()
        target_pids = descendant_pids(proc_pid)
        candidates = [
            c
            for c in clients
            if classify_client(c, baseline, target_pids)
        ]
        if candidates:
            def sort_key(item: Dict[str, Any]) -> int:
                try:
                    _, _, w, h = client_position_size(item)
                    area = w * h
                except Exception:
                    area = 0
                return area

            return sorted(candidates, key=sort_key, reverse=True)[0]

        time.sleep(0.2)

    return None


def capture_region(output: Path, window: Dict[str, Any], margin: int) -> None:
    x, y, w, h = client_position_size(window)
    x -= margin
    y -= margin
    w += margin * 2
    h += margin * 2
    geometry = f"{x},{y} {w}x{h}"
    output.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        ["grim", "-g", geometry, str(output)],
        check=False,
        text=True,
        capture_output=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            "grim failed with status "
            f"{result.returncode}: {result.stderr.strip() or result.stdout.strip()}"
        )


def close_window_by_address(address: str) -> None:
    subprocess.run(
        ["hyprctl", "dispatch", "closewindow", f"address:{address}"],
        check=False,
        text=True,
        capture_output=True,
    )


def main() -> int:
    try:
        args, run_args = parse_args(sys.argv[1:])
    except SystemExit as exc:
        return int(exc.code)

    try:
        if args.delay < 0 or not math.isfinite(args.delay):
            raise ValueError("--delay must be a non-negative number")
        if args.margin < 0:
            raise ValueError("--margin must be a non-negative integer")
        if args.timeout <= 0 or not math.isfinite(args.timeout):
            raise ValueError("--timeout must be positive")
    except ValueError as err:
        print(f"error: {err}", file=sys.stderr)
        return 2

    try:
        ensure_dependency("task")
        ensure_dependency("hyprctl")
        ensure_dependency("grim")
        base_clients = {client_identifier(c) for c in hypr_clients() if client_identifier(c)}
    except (RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"error: cannot access Hyprland screenshot tools: {exc}", file=sys.stderr)
        return 1

    runner = subprocess.Popen(
        ["task", "run", *(["--", *run_args] if run_args else [])],
        text=True,
        stdout=None,
        stderr=None,
        start_new_session=True,
    )

    def _cleanup() -> None:
        if target_address:
            close_window_by_address(target_address)
            try:
                runner.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                pass
        if runner.poll() is None:
            os.killpg(runner.pid, signal.SIGTERM)
            try:
                runner.wait(timeout=1.5)
            except subprocess.TimeoutExpired:
                os.killpg(runner.pid, signal.SIGKILL)
                runner.wait()

    target_address: Optional[str] = None

    try:
        window = find_viewer_window(
            baseline=base_clients,
            proc_pid=runner.pid,
            timeout=args.timeout,
        )
        if window is None:
            raise RuntimeError("Viewer window did not appear in time")

        target_address = client_identifier(window)
        if not target_address:
            raise RuntimeError("Selected client has no address")

        if args.delay > 0:
            time.sleep(args.delay)

        # Refresh geometry right before capture.
        refreshed: Optional[Dict[str, Any]] = None
        for _ in range(10):
            clients = hypr_clients()
            for client in clients:
                if client_identifier(client) == target_address:
                    refreshed = client
                    break
            if refreshed is not None:
                break
            time.sleep(0.1)

        if refreshed is None:
            raise RuntimeError("Captured window disappeared before screenshot")

        output = Path(args.output)
        capture_region(output, refreshed, args.margin)
        print(str(output))
        return 0
    except (KeyboardInterrupt, SystemExit):
        raise
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    finally:
        _cleanup()
        if runner.poll() is None:
            try:
                runner.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                pass


if __name__ == "__main__":
    raise SystemExit(main())
