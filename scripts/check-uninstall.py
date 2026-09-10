#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise real staged removals and mock privileged command ordering."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
(ROOT / "build").mkdir(exist_ok=True)
WORK = Path(tempfile.mkdtemp(prefix="uninstall-check.", dir=ROOT / "build"))
MOCK = WORK / "commands"
MOCK.mkdir()
LOG = WORK / "commands.log"
PAYLOAD = (
    "bin/hn-viewer",
    "bin/holonight-viewer",
    "share/applications/org.holonight.Viewer.desktop",
    "share/icons/hicolor/scalable/apps/org.holonight.Viewer.svg",
    "share/licenses/holonight-viewer/LICENSE",
    "share/licenses/holonight-viewer/GPL-3.0-or-later.txt",
)


def command(name, body):
    path = MOCK / name
    path.write_text("#!/usr/bin/env bash\nset -eu\n" + body)
    path.chmod(0o755)


command("update-desktop-database", 'echo "database:$*" >> "$CHECK_LOG"\nexit "${DATABASE_STATUS:-0}"\n')
command("rm", 'echo removal >> "$CHECK_LOG"\nif [[ ${REMOVE_STATUS:-0} != 0 ]]; then exit "$REMOVE_STATUS"; fi\nexec /usr/bin/rm "$@"\n')
command("rmdir", 'echo directory >> "$CHECK_LOG"\nif [[ ${DIRECTORY_STATUS:-0} != 0 ]]; then exit "$DIRECTORY_STATUS"; fi\nexec /usr/bin/rmdir "$@"\n')
env = dict(os.environ, PATH=f"{MOCK}:{os.environ['PATH']}", CHECK_LOG=str(LOG))


def seed(stage, paths):
    for relative in paths:
        path = stage / "usr" / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(relative)


def run(stage, status=0, **overrides):
    LOG.write_text("")
    result = subprocess.run(
        ["bash", str(ROOT / "scripts/uninstall.sh")],
        cwd=WORK, env=dict(env, DESTDIR=str(stage), **overrides),
        capture_output=True, text=True,
    )
    assert result.returncode == status, result.stderr
    return LOG.read_text().splitlines()


stage = WORK / "full stage"
preserved = (
    "lib/libHolonightQt.so", "lib/qt6/qml/Holonight/Controls/qmldir",
    "bin/other", "share/applications/other.desktop",
    "share/icons/hicolor/scalable/apps/other.svg",
    "share/licenses/holonight-viewer/unrelated",
)
seed(stage, PAYLOAD + preserved)
user = stage / "home/user/.config/HoloNight/Viewer.conf"
user.parent.mkdir(parents=True)
user.write_text("settings")
lines = run(stage)
assert lines == ["removal", "directory", f"database:{stage}/usr/share/applications"]
assert all(not (stage / "usr" / p).exists() for p in PAYLOAD)
assert all((stage / "usr" / p).read_text() == p for p in preserved)
assert user.read_text() == "settings"
assert run(stage) == lines

# Both current-only and legacy-only installations; no manifest or build config.
for name in ("hn-viewer", "holonight-viewer"):
    partial = WORK / name
    seed(partial, (f"bin/{name}", *PAYLOAD[2:]))
    run(partial)
    assert not (partial / "usr/share/licenses/holonight-viewer").exists()
    assert not (partial / f"usr/bin/{name}").exists()
    run(partial)
assert run(WORK / "absent") == ["removal"]

failed = WORK / "failure"
seed(failed, PAYLOAD)
assert run(failed, 23, REMOVE_STATUS="23") == ["removal"]
assert all((failed / "usr" / p).exists() for p in PAYLOAD)
assert run(failed, 24, DIRECTORY_STATUS="24") == ["removal", "directory"]
seed(failed, PAYLOAD)
assert run(failed, 25, DATABASE_STATUS="25")[-1].startswith("database:")
assert all(not (failed / "usr" / p).exists() for p in PAYLOAD)
# A directory at a payload-file path must fail, never be recursively removed.
blocked = WORK / "blocked"
(blocked / "usr/bin/hn-viewer").mkdir(parents=True)
assert run(blocked, 1) == ["removal"]
assert (blocked / "usr/bin/hn-viewer").is_dir()

# Exercise the real task entry; fake sudo validates its full command and redirects
# only this test invocation to a disposable tree after checking DESTDIR clearing.
command("sudo", '''echo "sudo:$*" >> "$CHECK_LOG"
if [[ ${SUDO_STATUS:-0} != 0 ]]; then exit "$SUDO_STATUS"; fi
[[ $# == 5 && $1 == env && $2 == -u && $3 == DESTDIR && $4 == bash && $5 == scripts/uninstall.sh ]]
exec env -u DESTDIR DESTDIR="$TASK_STAGE" bash scripts/uninstall.sh
''')
task_stage = WORK / "task"
seed(task_stage, PAYLOAD)
for sudo_status in (26, 0):
    LOG.write_text("")
    result = subprocess.run(
        ["task", "uninstall"], cwd=ROOT,
        env=dict(env, DESTDIR="/must-not-be-used", TASK_STAGE=str(task_stage),
                 SUDO_STATUS=str(sudo_status)), capture_output=True, text=True,
    )
    lines = LOG.read_text().splitlines()
    assert lines[0] == "sudo:env -u DESTDIR bash scripts/uninstall.sh"
    if sudo_status:
        assert result.returncode != 0 and len(lines) == 1, result.stderr
        assert all((task_stage / "usr" / p).exists() for p in PAYLOAD)
    else:
        assert result.returncode == 0, result.stderr
        assert lines[1:] == ["removal", "directory", f"database:{task_stage}/usr/share/applications"]
        assert all(not (task_stage / "usr" / p).exists() for p in PAYLOAD)
for failure in ("REMOVE_STATUS", "DIRECTORY_STATUS", "DATABASE_STATUS"):
    seed(task_stage, PAYLOAD)
    LOG.write_text("")
    result = subprocess.run(
        ["task", "uninstall"], cwd=ROOT,
        env=dict(env, TASK_STAGE=str(task_stage), **{failure: "27"}),
        capture_output=True, text=True,
    )
    assert result.returncode != 0, result.stderr
    lines = LOG.read_text().splitlines()
    assert lines[0].startswith("sudo:") and lines[1] == "removal"
    if failure == "REMOVE_STATUS":
        assert len(lines) == 2
    elif failure == "DIRECTORY_STATUS":
        assert lines[2:] == ["directory"]
    else:
        assert lines[2:] == ["directory", f"database:{task_stage}/usr/share/applications"]
print(f"Uninstall checks passed: {WORK}")
