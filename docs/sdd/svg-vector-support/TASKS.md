# SDD Tasks — svg-vector-support

- [x] T-001: Add kSvgFileLimitBytes constant to image_limits.h
  - REQs: REQ-C-001
  - Check: image_limits.h defines kSvgFileLimitBytes as 10 * 1024 * 1024 and the constant compiles without errors.

- [x] T-002: Add Qt6::Svg to root CMakeLists.txt find_package
  - REQs: REQ-C-002
  - Check: Root CMakeLists.txt line 17 includes Svg in find_package(Qt6 ... COMPONENTS ... Svg) and build succeeds.

- [x] T-003: Add Qt6::Svg to apps/viewer/CMakeLists.txt target_link_libraries
  - REQs: REQ-C-002, REQ-C-003
  - Check: apps/viewer/CMakeLists.txt links Qt6::Svg to holonight-viewer executable target and build succeeds.

- [x] T-004: Add Qt6::Svg to tests/CMakeLists.txt
  - REQs: REQ-C-002
  - Check: tests/CMakeLists.txt links Qt6::Svg to test binaries and CTest with BUILD_TESTING=ON succeeds.

- [x] T-005: Add svgData field to DecodeResult struct
  - REQs: REQ-F-009
  - Check: DecodeResult in image_document.h includes QByteArray svgData member and struct compiles.

- [x] T-006: Implement svgIntrinsicSize() free function
  - REQs: REQ-F-005, REQ-F-006
  - Check: svgIntrinsicSize(const QSvgRenderer&) in image_document.h returns viewBox().size() when valid or defaultSize() as fallback.

- [x] T-007: Implement decodeSvg() helper function in image_document.cpp
  - REQs: REQ-F-008, REQ-F-009, REQ-F-012, REQ-F-013
  - Check: decodeSvg() checks file.size() <= kSvgFileLimitBytes, loads bytes into transient QSvgRenderer, and returns DecodeResult with populated svgData on success or human-readable error string on parse failure.

- [x] T-008: Add SVG branch to decodeImage() in image_document.cpp
  - REQs: REQ-F-008, REQ-F-009
  - Check: decodeImage() detects .svg file suffix and routes to decodeSvg() before any raster path or ExifMetadata extraction.

- [x] T-009: Add svg_data_ and svg_renderer_ members to ImageDocument class
  - REQs: REQ-F-009
  - Check: ImageDocument header declares QByteArray svg_data_ and QSvgRenderer svg_renderer_ as value members.

- [x] T-010: Update ImageDocument::complete() for SVG dispatch
  - REQs: REQ-F-009, REQ-F-010, REQ-F-011
  - Check: complete() moves result.svgData into svg_data_, loads the renderer from the local SVG path before notifying observers, and treats a second-load failure as an ordinary decode failure.

- [x] T-011: Update ImageDocument reset paths (select and open early-return)
  - REQs: REQ-F-009
  - Check: select() and open() methods clear svg_data_ alongside existing image_ and information_ resets.

- [x] T-012: Add svgRenderer() property and accessor to ImageDocument
  - REQs: REQ-F-004, REQ-NF-003
  - Check: ImageDocument declares Q_PROPERTY svgRenderer with READ returning non-const QSvgRenderer* that is non-null only when state is Ready and information_.format is "SVG".

- [x] T-013: Implement previewImage() property on ImageDocument
  - REQs: REQ-NF-002, REQ-F-014
  - Check: previewImage() returns decoded QImage for raster formats, on-demand renders SVG to bounded QImage (≤256px), and EXIF Camera/Location sections do not appear in popup for SVG documents.

- [x] T-014: Update ImageDocument::nameFilters() to include SVG
  - REQs: REQ-F-002, REQ-F-001
  - Check: nameFilters() hardcodes "*.svg" entry independent of QImageReader::supportedImageFormats().

- [x] T-015: Add svg_renderer_ and content_size_ members to ImageCanvas
  - REQs: REQ-F-004, REQ-NF-001, REQ-NF-003
  - Check: ImageCanvas header declares QSvgRenderer* svg_renderer_ and QSize content_size_ as private members.

- [x] T-016: Add svgRenderer property and setSvgRenderer() method to ImageCanvas
  - REQs: REQ-F-004
  - Check: ImageCanvas declares Q_PROPERTY svgRenderer with READ svgRenderer and WRITE setSvgRenderer, and setSvgRenderer() calls svgIntrinsicSize() and view_.setImage().

- [x] T-017: Update ImageCanvas::setImage() for dual-mode setup
  - REQs: REQ-F-004, REQ-F-007
  - Check: setImage() clears svg_renderer_, sets content_size_ from image.size(), and calls view_.setImage(ImageOrientation::dimensions(...)).

- [x] T-018: Update ImageCanvas::paint() with dual-mode branching
  - REQs: REQ-F-004, REQ-NF-001, REQ-NF-003
  - Check: paint() branches on (svg_renderer_ != nullptr && svg_renderer_->isValid()) to call render() for SVG or drawImage() for raster, with explanatory comments.

- [x] T-019: Update directory_model.cpp suffix set with SVG
  - REQs: REQ-F-003, REQ-F-001
  - Check: scanDirectory() adds "svg" to suffixes set independent of QImageReader::supportedImageFormats().

- [x] T-020: Add svgRenderer binding to Main.qml ImageCanvas
  - REQs: REQ-F-004, REQ-F-007
  - Check: Main.qml main ImageCanvas has svgRenderer: window.document.svgRenderer binding.

- [x] T-021: Update ImageInformationPopup.qml popup canvas to use previewImage
  - REQs: REQ-NF-002
  - Check: ImageInformationPopup.qml popup ImageCanvas binds image to root.document.previewImage and omits svgRenderer binding entirely.

- [x] T-022: Create SVG test fixtures
  - REQs: REQ-F-005, REQ-F-006, REQ-F-008, REQ-F-010
  - Check: Test fixtures exist: valid-with-viewBox.svg (with viewBox attribute), valid-without-viewBox.svg (width/height only), malformed.svg (invalid XML), and oversized.svg (>10 MiB).

- [x] T-023: Wire SVG tests into test suite
  - REQs: REQ-F-005, REQ-F-006, REQ-F-008, REQ-F-010, REQ-F-011, REQ-NF-001, REQ-NF-002
  - Check: image_document_test.cpp and view_geometry_test.cpp exercise SVG decoding, viewBox/defaultSize size computation, malformed/oversized rejection, error reporting, fit-to-window magnification, dual-mode canvas rendering, and crispness during zoom; CTest passes.
