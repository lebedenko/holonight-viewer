# Design

Approved through the supplied implementation plan. Set CMake OUTPUT_NAME on the
existing target. Update concrete executable paths and desktop Exec; keep CMake
references to the target. Explicitly include hn-viewer in the runtime audit's
filename filter.

A Bash helper uses a fixed list of six payload paths beneath `${DESTDIR}/usr`.
Use nonrecursive forced removal for files; remove the license directory only
when empty, and propagate real removal errors. Refresh the staged applications
database after removal, skipping refresh only when its directory is absent.
Normal Task invocation clears DESTDIR under sudo to target `/usr`. No build,
provider discovery or manifest parsing is involved.

Spark was assigned the isolated executable rename but hit its usage limit before
editing. The main agent completed that change alongside Taskfile, uninstall helper,
regression checks, documentation and integration. Tests mock sudo and database refresh and use real
file operations in disposable build trees; task command ordering is tested without
host mutation. Installed providers are reused.
