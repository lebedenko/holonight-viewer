#pragma once

#include <QString>

#include <memory>

class QWindow;

// One xdg-foreign export of a toplevel surface. It is created per portal request and never
// cached, because the compositor invalidates handles when Qt recreates the surface.
class WaylandForeignExport {
 public:
  WaylandForeignExport() noexcept;
  WaylandForeignExport(WaylandForeignExport&& other) noexcept;
  WaylandForeignExport& operator=(WaylandForeignExport&& other) noexcept;
  WaylandForeignExport(const WaylandForeignExport&) = delete;
  WaylandForeignExport& operator=(const WaylandForeignExport&) = delete;
  ~WaylandForeignExport();

  [[nodiscard]] bool isValid() const;
  [[nodiscard]] QString handle() const;

 private:
  friend class WaylandForeignExporter;
  struct Impl;
  explicit WaylandForeignExport(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;
};

// Binds zxdg_exporter_v2 lazily on a private event queue, leaving Qt's queues untouched.
class WaylandForeignExporter {
 public:
  WaylandForeignExporter();
  WaylandForeignExporter(const WaylandForeignExporter&) = delete;
  WaylandForeignExporter& operator=(const WaylandForeignExporter&) = delete;
  WaylandForeignExporter(WaylandForeignExporter&&) = delete;
  WaylandForeignExporter& operator=(WaylandForeignExporter&&) = delete;
  // Exports must be destroyed before their exporter.
  ~WaylandForeignExporter();

  // Invalid, without a warning, off Wayland, without the compositor global, before the
  // surface exists, or when the roundtrip fails; the portal then gets no parent window.
  [[nodiscard]] WaylandForeignExport exportWindow(QWindow* window);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
