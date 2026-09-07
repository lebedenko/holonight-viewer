Exactly. What I drew is effectively **client-side decoration (CSD)**. If labwc then applies server-side decoration (SSD), you get the classic nested-titlebar problem.

For HoloNight applications, I would explicitly support **both decorated and undecorated modes**, selected according to the environment.

### Under Hyprland

Hyprland doesn't provide conventional SSD titlebars, so HoloNight can own the complete window surface:

```text
┌──────────────────────────────────────────────────┐
│ ←  →       ~/Pictures                ⌕   —  □  × │ ← HoloNight CSD
├──────────────────────────────────────────────────┤
│                                                  │
│                   content                        │
│                                                  │
└──────────────────────────────────────────────────┘
```

Here the top area is genuinely the application's titlebar/headerbar. It provides dragging, close/maximize/minimize, perhaps window menu, etc.

For the image viewer I'd make it even more minimal and probably auto-hide this UI along with the other controls.

### Under labwc

I'd let the compositor own decorations:

```text
╭──────────────────────────────────────────────────╮
│ HoloNight Files                              □ × │ ← labwc SSD
├──────────────────────────────────────────────────┤
│ ←  →       ~/Pictures                     ⌕  ⋮  │ ← app toolbar
├──────────────────────────────────────────────────┤
│                                                  │
│                   content                        │
│                                                  │
╰──────────────────────────────────────────────────╯
```

The important detail is that **the application's first row changes semantics**. It is no longer pretending to be a titlebar. No application title, no minimize/maximize/close buttons.

That's also why I wouldn't design the app around a permanent custom titlebar component. I'd design a reusable `HnWindowHeader` whose contents adapt.

Conceptually:

```cpp
enum class DecorationMode {
    Auto,
    ClientSide,
    ServerSide
};
```

with:

```text
Auto
 ├─ compositor supports/uses SSD → ServerSide
 └─ otherwise                    → ClientSide
```

And expose a user override because Wayland compositor behavior and personal preference can make automatic policy imperfect.

## Wayland actually has a protocol for this

The relevant protocol is `xdg-decoration-unstable-v1`. The client can request:

* `server_side`
* `client_side`

and the compositor ultimately configures the decoration mode.

So don't detect `"Hyprland"` versus `"labwc"` and hard-code behavior if you can avoid it. Make the decision from the Wayland decoration negotiation.

Your Qt platform integration is also relevant here. Since HoloNight already has `holonight-qt`, I'd want this policy centralized rather than having:

```text
holonight-files
holonight-viewer
holonight-settings
holonight-ai
...
```

each independently figuring out window decoration behavior.

Something like:

```text
                holonight-qt
                     │
              HnApplicationWindow
                     │
          ┌──────────┴──────────┐
          │                     │
     CSD requested         SSD requested
          │                     │
    HnTitleBar.qml        native window
          │                     │
 close/max/drag etc.      compositor decoration
```

Then every HoloNight application gets consistent behavior.

## There is another option: always request CSD

You could simply say that HoloNight applications always render their own decoration, including under labwc.

That gives you absolute visual consistency:

```text
Hyprland                         labwc

┌ HoloNight titlebar ┐           ┌ HoloNight titlebar ┐
│                    │           │                    │
│      content       │           │      content       │
│                    │           │                    │
└────────────────────┘           └────────────────────┘
```

I **wouldn't choose this as the default**, though.

HoloNight Shell supporting stacking compositors means compositor decorations are part of that environment. A user might deliberately configure labwc's decoration theme, button placement, titlebar actions, double-click behavior, etc. HoloNight applications shouldn't unnecessarily bypass it.

There's also an architectural elegance to saying:

> HoloNight owns window chrome when the compositor doesn't; otherwise the compositor owns it.

That makes the applications integrate correctly outside the complete HoloNight environment too.

### One change I'd make to the mockups

In fact, looking back at the design I generated, I'd modify the distinction slightly.

**HoloNight Viewer** shouldn't really have a conventional application toolbar at all. In SSD mode:

```text
╭─ DSC_4832.jpg ─────────────────────────────── □ × ─╮
│                                                     │
│                                                     │
│                      IMAGE                          │
│                                                     │
│                                                     │
│           DSC_4832.jpg · 34% · 17/86                │
╰─────────────────────────────────────────────────────╯
```

And in CSD mode, its custom top chrome can disappear when viewing:

```text
┌─────────────────────────────────────────────────────┐
│                                                     │
│                                                     │
│                      IMAGE                          │
│                                                     │
│                                                     │
│           DSC_4832.jpg · 34% · 17/86                │
└─────────────────────────────────────────────────────┘
```

Move the mouse to the top and the CSD titlebar fades in.

**HoloNight Files** is different. It genuinely needs a navigation toolbar. So SSD mode naturally has two horizontal regions, but they have clearly different purposes:

```text
labwc titlebar:       HoloNight Files                 ×
────────────────────────────────────────────────────────
application toolbar: ← → ↑ │ /home/andrii/Projects │ ⌕ ⋮
────────────────────────────────────────────────────────
                     FILES
```

That's not the ugly “two titlebars” problem—the second row is clearly application navigation.

So I'd actually embrace **adaptive SSD/CSD as part of `holonight-qt`**. It will matter beyond these two applications: settings, AI, package manager and future HoloNight apps all have exactly the same issue.
