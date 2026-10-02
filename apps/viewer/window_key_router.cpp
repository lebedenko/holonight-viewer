// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include "window_key_router.h"

#include "viewer_shortcuts.h"

#include <QCoreApplication>
#include <QMouseEvent>

#include <algorithm>
WindowKeyRouter::WindowKeyRouter(QObject* parent) : QObject(parent) {}
WindowKeyRouter::~WindowKeyRouter() { setWindow(nullptr); }
QQuickWindow* WindowKeyRouter::window() const { return window_; }
void WindowKeyRouter::setWindow(QQuickWindow* window) {
  if (window_ == window) {
    return;
  }
  if (window_) {
    window_->removeEventFilter(this);
    disconnect(window_, &QQuickWindow::activeFocusItemChanged, this, nullptr);
  }
  first_press_.invalidate();
  window_ = window;
  if (window_) {
    window_->installEventFilter(this);
    connect(window_, &QQuickWindow::activeFocusItemChanged, this, [this] { first_press_.invalidate(); });
  }
  emit windowChanged();
}
QQmlListProperty<QQuickItem> WindowKeyRouter::focusTargets() {
  return {this,
          this,
          [](QQmlListProperty<QQuickItem>* list, QQuickItem* item) {
            auto* router = static_cast<WindowKeyRouter*>(list->data);
            router->header_targets_.append(item);
            emit router->focusTargetsChanged();
          },
          [](QQmlListProperty<QQuickItem>* list) {
            return static_cast<WindowKeyRouter*>(list->data)->header_targets_.size();
          },
          [](QQmlListProperty<QQuickItem>* list, qsizetype index) {
            return static_cast<WindowKeyRouter*>(list->data)->header_targets_.at(index).data();
          },
          [](QQmlListProperty<QQuickItem>* list) {
            auto* router = static_cast<WindowKeyRouter*>(list->data);
            router->header_targets_.clear();
            emit router->focusTargetsChanged();
          }};
}
QList<QQuickItem*> WindowKeyRouter::headerFocusTargets() const {
  QList<QQuickItem*> result;
  for (const auto& target : header_targets_) {
    result.append(target.data());
  }
  return result;
}
void WindowKeyRouter::setHeaderFocusTargets(const QList<QQuickItem*>& targets) {
  if (headerFocusTargets() == targets) {
    return;
  }
  header_targets_.clear();
  for (auto* target : targets) {
    header_targets_.append(target);
  }
  emit focusTargetsChanged();
}
QQuickItem* WindowKeyRouter::playbackButton() const { return playback_button_; }
void WindowKeyRouter::setPlaybackButton(QQuickItem* target) {
  if (playback_button_ == target) {
    return;
  }
  playback_button_ = target;
  emit focusTargetsChanged();
}
bool WindowKeyRouter::headerOrPlaybackButtonFocused() const {
  const auto* focused = window_ ? window_->activeFocusItem() : nullptr;
  if (focused == nullptr) {
    return false;
  }
  if (focused == playback_button_) {
    return true;
  }
  return std::ranges::any_of(header_targets_, [focused](const auto& target) { return target == focused; });
}
bool WindowKeyRouter::eventFilter(QObject* object, QEvent* event) {
  if (object != window_) {
    return false;
  }
  resetSequenceForEvent(*event);
  if (routeMouse(*event)) {
    return true;
  }
  if (event->type() != QEvent::KeyPress || modal_active_) {
    return false;
  }
  const auto* keyEvent = dynamic_cast<QKeyEvent*>(event);
  if (keyEvent == nullptr) {
    return false;
  }
  const int key = keyEvent->key();
  const auto modifiers = keyEvent->modifiers() & ~Qt::KeypadModifier;
  const bool gridPage =
      grid_active_ && modifiers == Qt::ControlModifier &&
      (key == ViewerShortcuts::key(ViewerShortcuts::PageDown) || key == ViewerShortcuts::key(ViewerShortcuts::PageUp));
  if (!gridPage && (modifiers & ~Qt::ShiftModifier) != Qt::NoModifier) {
    return false;
  }
  if (menu_open_) {
    return routeMenu(key);
  }
  if (key == Qt::Key_Tab || key == Qt::Key_Backtab) {
    return routeTab(keyEvent->modifiers().testFlag(Qt::ShiftModifier) || key == Qt::Key_Backtab);
  }
  if (grid_active_) {
    return routeGrid(*keyEvent, gridPage);
  }
  return image_ready_ && routeImage(*keyEvent);
}
void WindowKeyRouter::resetSequenceForEvent(const QEvent& event) {
  switch (event.type()) {
    case QEvent::FocusOut:
    case QEvent::WindowDeactivate:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    case QEvent::Wheel:
      first_press_.invalidate();
      break;
    // Cancel before another shortcut consumes the key.
    case QEvent::ShortcutOverride:
    case QEvent::KeyPress: {
      const auto* key = dynamic_cast<const QKeyEvent*>(&event);
      if (key == nullptr || key->key() != ViewerShortcuts::key(ViewerShortcuts::GridFirst) ||
          (key->modifiers() & ~Qt::KeypadModifier) != Qt::NoModifier) {
        first_press_.invalidate();
      }
      break;
    }
    default:
      break;
  }
}
bool WindowKeyRouter::routeMouse(const QEvent& event) {
  if (event.type() != QEvent::MouseButtonPress && event.type() != QEvent::MouseButtonRelease &&
      event.type() != QEvent::MouseButtonDblClick) {
    return false;
  }
  const auto* mouse = dynamic_cast<const QMouseEvent*>(&event);
  if (mouse == nullptr || (mouse->button() != Qt::BackButton && mouse->button() != Qt::ForwardButton)) {
    return false;
  }
  if (!modal_active_ && event.type() != QEvent::MouseButtonRelease) {
    emit browseRequested(mouse->button() == Qt::BackButton ? -1 : 1);
  }
  return true;
}
bool WindowKeyRouter::routeMenu(int key) {
  if (key != ViewerShortcuts::key(ViewerShortcuts::MenuDown) && key != ViewerShortcuts::key(ViewerShortcuts::MenuUp)) {
    return false;
  }
  if (forwarding_) {
    return false;
  }
  forwarding_ = true;
  QKeyEvent mapped(QEvent::KeyPress,
                   key == ViewerShortcuts::key(ViewerShortcuts::MenuDown)
                       ? ViewerShortcuts::key(ViewerShortcuts::PanDown)
                       : ViewerShortcuts::key(ViewerShortcuts::PanUp),
                   Qt::NoModifier);
  QCoreApplication::sendEvent(window_, &mapped);
  forwarding_ = false;

  return true;
}
bool WindowKeyRouter::routeTab(bool backwards) {
  const auto& buttons = header_targets_;
  const int count = static_cast<int>(buttons.size());
  if (count == 0) {
    return false;
  }
  const int direction = backwards ? -1 : 1;
  int current = direction > 0 ? -1 : 0;
  for (int i = 0; i < count; ++i) {
    if (buttons[i] && buttons[i] == window_->activeFocusItem()) {
      current = i;
    }
  }
  for (int offset = 1; offset <= count; ++offset) {
    const int index = (current + (direction * offset) + (2 * count)) % count;
    if (buttons[index] && buttons[index]->isEnabled()) {
      buttons[index]->forceActiveFocus(direction > 0 ? Qt::TabFocusReason : Qt::BacktabFocusReason);
      break;
    }
  }

  return true;
}
bool WindowKeyRouter::routeGrid(const QKeyEvent& event, bool gridPage) {
  const int key = event.key();

  using Move = GridNavigation::Move;
  if (routeGridBoundary(event)) {
    return true;
  }
  // Space is never handled here, so a focused header button keeps it.
  if (event.modifiers().testFlag(Qt::ShiftModifier)) {
    return false;
  }
  switch (key) {
    case ViewerShortcuts::key(ViewerShortcuts::PanLeft):
    case ViewerShortcuts::key(ViewerShortcuts::GridLeft):
      emit gridMoveRequested(static_cast<int>(Move::Previous));
      break;
    case ViewerShortcuts::key(ViewerShortcuts::PanRight):
    case ViewerShortcuts::key(ViewerShortcuts::GridRight):
      emit gridMoveRequested(static_cast<int>(Move::Next));
      break;
    case ViewerShortcuts::key(ViewerShortcuts::PanUp):
    case ViewerShortcuts::key(ViewerShortcuts::GridUp):
      emit gridMoveRequested(static_cast<int>(Move::RowUp));
      break;
    case ViewerShortcuts::key(ViewerShortcuts::PanDown):
    case ViewerShortcuts::key(ViewerShortcuts::GridDown):
      emit gridMoveRequested(static_cast<int>(Move::RowDown));
      break;
    case ViewerShortcuts::key(ViewerShortcuts::PageDown):
    case ViewerShortcuts::key(ViewerShortcuts::PageUp):
      if (!gridPage) {
        return false;
      }
      emit gridMoveRequested(
          static_cast<int>(key == ViewerShortcuts::key(ViewerShortcuts::PageDown) ? Move::PageDown : Move::PageUp));
      break;
    case ViewerShortcuts::key(ViewerShortcuts::Activate):
    case Qt::Key_Enter:
      // A focused header button activates instead of opening the file.
      if (event.isAutoRepeat() || headerOrPlaybackButtonFocused()) {
        return false;
      }
      emit gridActivateRequested();
      break;
    default:
      return false;
  }

  return true;
}
bool WindowKeyRouter::routeGridBoundary(const QKeyEvent& event) {
  if (event.key() != ViewerShortcuts::key(ViewerShortcuts::GridFirst)) {
    return false;
  }
  const auto modifiers = event.modifiers() & ~Qt::KeypadModifier;
  if (modifiers == Qt::ShiftModifier) {
    if (!event.isAutoRepeat()) {
      emit gridMoveRequested(static_cast<int>(GridNavigation::Move::Last));
    }
    return true;
  }
  if (modifiers != Qt::NoModifier) {
    return false;
  }
  if (event.isAutoRepeat()) {
    return true;
  }
  if (first_press_.isValid() && first_press_.elapsed() < 1000) {
    first_press_.invalidate();
    emit gridMoveRequested(static_cast<int>(GridNavigation::Move::First));
  } else {
    first_press_.start();
  }
  return true;
}
bool WindowKeyRouter::routeImage(const QKeyEvent& event) {
  const int key = event.key();

  switch (key) {
    case ViewerShortcuts::key(ViewerShortcuts::PanLeft):
      emit panRequested(40, 0);
      break;
    case ViewerShortcuts::key(ViewerShortcuts::PanRight):
      emit panRequested(-40, 0);
      break;
    case ViewerShortcuts::key(ViewerShortcuts::PanUp):
      emit panRequested(0, 40);
      break;
    case ViewerShortcuts::key(ViewerShortcuts::PanDown):
      emit panRequested(0, -40);
      break;
    case ViewerShortcuts::key(ViewerShortcuts::Playback):
      // A focused button keeps Space so that it activates as usual; auto-repeat would flicker the state.
      if (!playback_available_ || event.modifiers().testFlag(Qt::ShiftModifier) || event.isAutoRepeat() ||
          headerOrPlaybackButtonFocused()) {
        return false;
      }
      emit playbackToggleRequested();
      break;
    default:
      return false;
  }

  return true;
}
