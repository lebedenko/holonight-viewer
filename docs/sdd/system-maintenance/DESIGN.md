# Design

The supplied implementation plan approves this design. First restore packaged
CMake desktop installation and staged/isolated desktop checks removed by the
previous agent. Remove only development registration scripts and tasks.

Use sequential Task commands for maintenance and installation so failures stop
later steps. Configure system installation explicitly without development presets
in its own build directory, with `/usr` provider/cache and runtime settings.
Preserve normal preset and CLI forwarding for development launches. Clean uses
four explicit Viewer build paths, never the whole build tree.

Spark owns only Taskfile.yml. The main agent owns script removal, documentation,
CI, integration and verification. Existing provider installations are reused.
