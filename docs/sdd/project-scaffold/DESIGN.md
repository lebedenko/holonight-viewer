# Scaffold design

The supplied plan authorizes this design. apps/viewer contains a small executable
and the HolonightViewer QML module. main.cpp sets identity org.holonight.Viewer,
handles CLI options, rejects positional arguments, and reports QML creation failure
through a queued nonzero exit. QML owns only shell presentation and shortcuts.

Main.qml uses HnApplicationWindow's native-decoration default, a 1000×700 initial
canvas with a 420×280 minimum, centered HnEmptyState, and a bottom HnLabel with
shared caption typography, muted palette color, and HnMetrics spacing. No image
state or mock controls are created. Fullscreen remembers maximized state.

qtquickcontrols2.conf is embedded at the resource root. Installed provider QML
modules are dynamically discovered; interface CMake targets do not bundle them.
Import scanning for static plugin linkage is disabled because providers are
installed shared modules. Development tasks supply installed-prefix runtime paths and CMake supplies lint
imports. System packages use normal Qt module lookup. Custom prefixes require
explicit runtime search paths; the executable embeds no development paths.

Provider preparation builds config then qt with Wayland extras disabled in local
build/deps directories. The prefix uses lib for predictable development imports.
CI builds its Arch image in the workflow and checks out config at
fe69a59e6b73167fd5349223a4d265d75386c139 and qt at
22ded7815727ce483fd91e82a9cc04bfe252ec3b. Rolling Arch packages may require future
maintenance; the provider source revisions are fixed.

Qt/GTest smoke tests instantiate the real window QML and exercise actual shortcuts.
A separate Controls Button verifies the embedded style selection with environment
overrides cleared. CLI tests check help/version/rejection. Install checks start
the staged binary using only installed provider imports. Visual acceptance remains
a separate check for actual compositor decorations and appearance/scale.

QtQuick.Controls is imported before shared controls to initialize the configured
style before their explicit Basic imports. The smoke test verifies both the style
name and a HoloNight-specific Button property after window creation.

Development launch registers the desktop entry and icon in the user's XDG data
home before opening the window. The generated entry refers to the selected build
and provider paths. This lets the running host portal resolve the application ID;
changing only the child's XDG search paths would not register it with that service.

Stage 0 adds `task desktop-check` to CI. It registers debug then release in one
isolated XDG data home under build/, validates generated entries and launch
arguments, executes their version commands, and checks packaged-entry separation.
It complements the staged runtime check and does not alter host registration.

## Fullscreen restoration correction

R3 also covers compositor-managed tiling: leaving fullscreen must restore the
preceding tile, without requesting maximization. QML retains action routing; a
small public-QWindow helper changes only the WindowFullScreen flag, preserving
other current window-state flags. This avoids treating a compositor-reported
Maximized state as an instruction to maximize. All fullscreen entry points use
the same helper. No compositor-name checks or private Qt APIs are needed.

Verification must allow native configure events between key presses and compare
restored geometry as well as Qt visibility. Native Wayland protocol evidence
must show fullscreen requests without maximize/unmaximize requests from toggling.
