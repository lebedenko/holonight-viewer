Yes to both, but with different implementation strategies.

For the **image viewer**, I would build a HoloNight-native application rather than fork imv/qView. For the **file manager**, I would also build one, but deliberately constrain its scope and borrow the *interaction model* from Vifm/Yazi rather than their code.

### 1. HoloNight image viewer

This is almost an ideal HoloNight application: small scope, highly visible UI, and existing viewers seem to cluster at the two extremes you described.

I would **not base it on imv**. Its implementation and UI architecture don't buy you much because HoloNight is already Qt6/QML-oriented.

qView is closer technologically: it's C++ and now supports Qt6, but it's fundamentally a Qt Widgets application and GPL-3.0. ([GitHub][1]) By the time you replace its UI, input model, settings, theme integration and probably parts of image handling, you're maintaining a fork while using surprisingly little of qView.

A clean Qt implementation isn't large. The core can initially be roughly:

```text
holonight-viewer

C++
 ├── ImageDocument
 │    ├── QImageReader
 │    ├── metadata
 │    └── animation state
 │
 ├── DirectoryModel
 │    └── sibling images
 │
 └── ImageLoader
      └── async loading / caching

QML
 ├── MainWindow
 ├── ImageCanvas
 ├── OverlayToolbar
 ├── InfoOverlay
 └── CommandPalette
```

Qt already gives you `QImageReader`, image plugins, EXIF-oriented metadata access through the reader where available, scaling, transformations, clipboard support, etc. You don't need an image-viewer framework.

I think the right HoloNight feature boundary would be:

* open image
* next/previous image in directory
* zoom / fit / 1:1
* pan
* rotate/flip
* fullscreen
* copy image/path
* delete/trash
* basic image information
* animated GIF/WebP support
* maybe crop later
* **no editing suite**
* **no albums**
* **no photo management**
* **no tagging/database**
* **no giant permanent toolbar**

And importantly, make it keyboard-first:

```text
h/j/k/l      pan
+/-          zoom
0            fit
1            100%
[/]          previous / next
r/R          rotate
f            fullscreen
i            info
yy           copy image
yp           copy path
dd           trash
:            command mode
q            quit
```

That would actually give it a distinct reason to exist rather than merely being “qView with HoloNight colors.”

I'd have almost no persistent chrome. Move the mouse and you get a small translucent HoloNight overlay; keyboard use leaves just the image. Perhaps a very thin bottom HUD:

```text
DSC_4832.CR3     6000 × 4000     34%       17 / 86
```

That feels much more consistent with HoloNight.

---

## 2. HoloNight file manager

This one is more interesting.

**Yes, I think you should make it.**

But don't think:

> I need to implement a replacement for Dolphin.

Think:

> I want a graphical Vifm designed around HoloNight.

That's a substantially better product definition.

Vifm explicitly models itself as a Vim-like environment, including modes, commands, registers and mappings rather than just attaching `hjkl` to a conventional file manager. ([GitHub][2]) That's the aspect I'd copy.

Yazi is also worth studying. Its current architecture separates the manager UI, asynchronous task engine, and file metadata/preview subsystem. ([Yazi][3]) Its default interaction model already includes visual selection, yank/cut/paste, trash and permanent deletion, and it has background task management for file operations. ([Yazi][4])

I would build **that concept as a graphical Qt application**.

Something like:

```text
┌─────────────────────────────────────────────────────────────────────────┐
│ ~/Projects/holonight-shell                                      ⌕      │
├──────────────────┬──────────────────────────────────┬───────────────────┤
│ Places           │ Name                    Size     │ Preview           │
│                  │                                   │                   │
│ ~                │ apps/                             │ README.md         │
│ Downloads        │ assets/                           │                   │
│ Projects         │ internal/                         │ # HoloNight       │
│                  │ LICENSE                  1.1K      │                   │
│                  │ README.md               8.4K      │ ...               │
│                  │ SPEC.md                 14.2K      │                   │
├──────────────────┴──────────────────────────────────┴───────────────────┤
│ NORMAL │ ~/Projects/holonight-shell                     12 items        │
└─────────────────────────────────────────────────────────────────────────┘
```

But the left and right panes should be optional. The **file list is the application**, not a sidebar/sidebar/sidebar/toolbar/status bar shell like Dolphin.

### Vim semantics should be real

Don't merely implement:

```text
j = down
k = up
```

Implement a proper state machine:

```text
NORMAL
VISUAL
COMMAND
SEARCH
INSERT     ← rename/create dialogs
```

Then operations become natural:

```text
j/k         move
gg / G      first / last
5j          down five
h           parent
l / Enter   enter/open

v           visual selection
V           range selection

yy          yank/copy
dd          cut
p           paste
D           trash/delete

a           create
r           rename

/           search current directory
:           command mode

:mkdir foo
:touch foo.txt
:cd ~/Downloads
:sort size
:hidden
```

Then eventually:

```text
ma          mark directory as a
'a          jump to mark a

"ayy        yank into register a
"ap         paste register a
```

That last part might sound excessive for a GUI file manager, but it's exactly the sort of functionality that would make a HoloNight file manager *different*.

### Keep the filesystem backend yours

Unlike the image viewer, this is where I would avoid thinking “QFileSystemModel and we're done.”

Have explicit layers:

```text
holonight-files

UI
 └── QML

Navigation
 ├── DirectoryController
 ├── SelectionController
 ├── VimCommandController
 └── HistoryController

Models
 ├── DirectoryModel
 ├── PlacesModel
 └── SearchModel

Filesystem
 ├── FileOperationService
 │    ├── copy
 │    ├── move
 │    ├── trash
 │    ├── delete
 │    └── rename
 │
 ├── MimeService
 ├── ThumbnailService
 ├── FileWatcher
 └── MountService

Tasks
 └── TaskManager
      ├── progress
      ├── cancellation
      ├── conflicts
      └── queue

Preview
 ├── ImagePreview
 ├── TextPreview
 ├── PdfPreview
 └── GenericMetadataPreview
```

The `TaskManager` matters from day one. A naïve file manager becomes unpleasant as soon as someone copies 30 GB and the GUI thread gets involved.

The hard parts aren't drawing filenames. They're things such as:

* recursive operations
* symlinks
* name collisions
* permission errors
* cancellation
* operation progress
* trash semantics
* filesystem notifications
* mounting/removable devices
* MIME detection
* thumbnails
* enormous directories
* preserving metadata
* clipboard interoperability

That's why I'd constrain v1 aggressively.

### V1 should be local filesystem only

I'd explicitly *not* support SFTP, SMB, Google Drive, MTP, archive-as-directory, plugins, embedded terminals, Git UI, etc.

Just:

```text
navigate
open
multi-select
copy
move
rename
create
trash/delete
search/filter
sort
hidden files
places
removable media
preview
```

That's enough for 95% of what you described.

And it avoids the trap Dolphin/Nautilus/etc. eventually fell into: every useful feature becomes another subsystem.

---

## The viewer and file manager should integrate

This is where making both becomes more compelling.

For example, the preview pane doesn't need an entirely separate image rendering implementation:

```text
             ┌── holonight-viewer
             │
ImageEngine ─┤
             │
             └── holonight-files ImagePreview
```

Likewise:

```text
holonight-files
      │
      └── Enter image
             ↓
       holonight-viewer image.jpg
```

The viewer can understand the directory context and provide seamless `[ / ]` traversal.

I wouldn't make the viewer a component *of* the file manager, though. It should remain a normal standalone application.

---

## One particularly HoloNight feature

I'd make **Quick Look** a first-class shell facility.

From the file manager:

```text
Space
```

opens a temporary preview overlay:

```text
                    ╭────────────────────────────╮
                    │                            │
                    │          IMAGE             │
                    │                            │
                    │ DSC_1042.jpg               │
                    │ 6240 × 4160 · 8.3 MB       │
                    ╰────────────────────────────╯
```

Release/press Space/Escape and it's gone.

It can handle:

```text
image    → image
text     → syntax-highlighted preview
PDF      → page preview
video    → poster + metadata
audio    → artwork + metadata
folder   → contents summary
archive  → file listing
```

That would fit HoloNight extremely well and means you rarely need to actually launch an application merely to inspect something.

---

### Repository-wise

Given how HoloNight is developing, I would probably use:

```text
holonight-files
holonight-viewer
```

as separate applications/repos, both consuming your existing HoloNight Qt/theme infrastructure.

I **wouldn't put either into `holonight-shell`**. They're applications, not components necessary for running the desktop shell.

And I wouldn't fork qView/imv/Vifm. Study them, copy good UX concepts, possibly reuse small appropriately licensed algorithms if worthwhile, but starting with your Qt6/C++ architecture will probably result in *less* code and technical debt than bending those projects into HoloNight.

The strongest idea here, in my view, is **not “HoloNight's alternative to Dolphin.” It is “Vifm/Yazi interaction philosophy expressed as a polished graphical HoloNight application.”** That fills a niche I think is genuinely underserved: GUI file managers tend to be mouse-oriented even when they provide keyboard shortcuts, while Vim-oriented managers tend to be terminal applications. ([GitHub][5])

For the image viewer, the implementation cost is sufficiently low that I'd put it fairly high on the HoloNight roadmap. For the file manager, I'd do it too, but only after writing down the strict v1 boundary so it doesn't accidentally evolve into Dolphin 2.0.

[1]: https://github.com/jurplel/qView?utm_source=chatgpt.com "GitHub - jurplel/qView: Practical and minimal image viewer · GitHub"
[2]: https://github.com/vifm/vifm/blob/master/README?utm_source=chatgpt.com "vifm/README at master · vifm/vifm · GitHub"
[3]: https://yazi-rs.github.io/docs/term/?utm_source=chatgpt.com "Terminology | Yazi"
[4]: https://yazi-rs.github.io/docs/quick-start/?utm_source=chatgpt.com "Quick Start | Yazi"
[5]: https://github.com/vifm/vifm?utm_source=chatgpt.com "GitHub - vifm/vifm: Vifm is a file manager with curses interface, which provides Vim-like environment for managing objects within file systems, extended with some useful ideas from mutt. · GitHub"
