# Design

QML features are footer/, information/ and shortcuts/ below qml/. Resource aliases retain the Main entry and icon paths relative to the module. Feature files use local imports and ImageInformationPopup explicitly imports the application module for its C++ types.

add_viewer_application creates each executable, registers its QML-facing C++ classes and embeds style configuration. viewer-private holds non-QML implementation; viewer-protocol isolates generated Wayland C bindings from owned-code warnings. Neither target is exported or installed.

Preserve ViewerMenuSeparator on Controls.MenuSeparator with HnSeparator content: the shared style currently uses a Rectangle and does not provide the existing physical-pixel hairline at fractional scene positions. This is a retained Viewer rendering requirement, not a Basic workaround. No provider changes are in scope.

One recursive Python formatting inventory serves CMake and Task. Import policy follows the accepted Settings checker and adds AbstractButton. Metadata checks inspect production-generated qmltypes. Opening and desktop fixture scripts accept a build directory, falling back to the system temporary directory when standalone. PNG-only overlay fixtures remove optional-decoder ordering assumptions.

Standalone removal is retired with a rejecting compatibility script. README documents explicit CMake/DESTDIR installation and ownership review for legacy cleanup. The umbrella will reject existing unowned paths.
