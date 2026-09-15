#include "wayland_foreign_export.h"

#include "xdg-foreign-unstable-v2-client-protocol.h"

#include <QGuiApplication>
#include <QWindow>
#include <QtGui/qpa/qplatformwindow_p.h>

#include <cstring>
#include <wayland-client.h>

struct WaylandForeignExport::Impl {
  Impl() = default;
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;
  ~Impl() {
    if (exported != nullptr) {
      zxdg_exported_v2_destroy(exported);
      wl_display_flush(display);
    }
  }

  wl_display* display = nullptr;
  zxdg_exported_v2* exported = nullptr;
  QString handle;
  bool received = false;
};

WaylandForeignExport::WaylandForeignExport() noexcept = default;
WaylandForeignExport::WaylandForeignExport(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
WaylandForeignExport::WaylandForeignExport(WaylandForeignExport&& other) noexcept = default;
WaylandForeignExport& WaylandForeignExport::operator=(WaylandForeignExport&& other) noexcept = default;
WaylandForeignExport::~WaylandForeignExport() = default;

bool WaylandForeignExport::isValid() const { return impl_ != nullptr && !impl_->handle.isEmpty(); }

QString WaylandForeignExport::handle() const { return impl_ != nullptr ? impl_->handle : QString(); }

struct WaylandForeignExporter::Impl {
  Impl() = default;
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;
  ~Impl() {
    if (exporter != nullptr) {
      zxdg_exporter_v2_destroy(exporter);
    }
    if (registry != nullptr) {
      wl_registry_destroy(registry);
    }
    if (wrapper != nullptr) {
      wl_proxy_wrapper_destroy(wrapper);
    }
    if (queue != nullptr) {
      wl_event_queue_destroy(queue);
    }
    if (display != nullptr) {
      wl_display_flush(display);
    }
  }

  bool bind(wl_display* target) {
    display = target;
    queue = wl_display_create_queue(display);
    // A wrapper makes the registry's first events land on our queue instead of racing
    // Qt's reader thread on the default queue.
    wrapper = static_cast<wl_display*>(wl_proxy_create_wrapper(display));
    if (queue == nullptr || wrapper == nullptr) {
      return false;
    }
    // wl_display is a wl_proxy in libwayland's C object model.
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(wrapper), queue);  // NOLINT(*-reinterpret-cast)
    registry = wl_display_get_registry(wrapper);
    static constexpr wl_registry_listener listener{
        .global =
            [](void* data, wl_registry* source, uint32_t name, const char* interface, uint32_t /*version*/) {
              auto* self = static_cast<Impl*>(data);
              if (self->exporter == nullptr && std::strcmp(interface, zxdg_exporter_v2_interface.name) == 0) {
                self->exporter =
                    static_cast<zxdg_exporter_v2*>(wl_registry_bind(source, name, &zxdg_exporter_v2_interface, 1));
              }
            },
        .global_remove = [](void*, wl_registry*, uint32_t) {},
    };
    return registry != nullptr && wl_registry_add_listener(registry, &listener, this) == 0 &&
           wl_display_roundtrip_queue(display, queue) >= 0 && exporter != nullptr;
  }

  wl_display* display = nullptr;
  wl_event_queue* queue = nullptr;
  wl_display* wrapper = nullptr;
  wl_registry* registry = nullptr;
  zxdg_exporter_v2* exporter = nullptr;
  bool attempted = false;
  bool available = false;
};

WaylandForeignExporter::WaylandForeignExporter() : impl_(std::make_unique<Impl>()) {}

WaylandForeignExporter::~WaylandForeignExporter() = default;

WaylandForeignExport WaylandForeignExporter::exportWindow(QWindow* window) {
  auto* application = qGuiApp != nullptr ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
  if (window == nullptr || application == nullptr || application->display() == nullptr) {
    return {};
  }
  if (!impl_->attempted) {
    impl_->attempted = true;
    impl_->available = impl_->bind(application->display());
  }
  // The surface is fetched fresh for every request because Qt may have recreated it.
  auto* platformWindow = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
  auto* surface = platformWindow != nullptr ? platformWindow->surface() : nullptr;
  if (!impl_->available || surface == nullptr) {
    return {};
  }
  auto result = std::make_unique<WaylandForeignExport::Impl>();
  result->display = impl_->display;
  // The new object inherits the exporter's private queue.
  result->exported = zxdg_exporter_v2_export_toplevel(impl_->exporter, surface);
  if (result->exported == nullptr) {
    return {};
  }
  static constexpr zxdg_exported_v2_listener listener{
      .handle =
          [](void* data, zxdg_exported_v2* /*exported*/, const char* handle) {
            auto* target = static_cast<WaylandForeignExport::Impl*>(data);
            target->handle = QString::fromUtf8(handle);
            target->received = true;
          },
  };
  zxdg_exported_v2_add_listener(result->exported, &listener, result.get());
  // The compositor sends the handle before answering the roundtrip's sync. Only our queue is
  // dispatched here; libwayland's read protocol allows Qt's reader thread alongside.
  if (wl_display_roundtrip_queue(impl_->display, impl_->queue) < 0 || !result->received) {
    return {};
  }
  return WaylandForeignExport(std::move(result));
}
