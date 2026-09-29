// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>

#pragma once

#include "grid_navigation.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

class WindowKeyRouter : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QQuickWindow* target READ window WRITE setWindow NOTIFY windowChanged)
  Q_PROPERTY(bool imageReady MEMBER m_imageReady)
  Q_PROPERTY(bool modalActive MEMBER m_modalActive)
  Q_PROPERTY(bool menuOpen MEMBER m_menuOpen)
  Q_PROPERTY(bool playbackAvailable MEMBER m_playbackAvailable)
  Q_PROPERTY(bool gridActive MEMBER m_gridActive)

 public:
  explicit WindowKeyRouter(QObject* parent = nullptr) : QObject(parent) {}
  ~WindowKeyRouter() override { setWindow(nullptr); }

  QQuickWindow* window() const { return m_window; }
  void setWindow(QQuickWindow* window) {
    if (m_window == window) return;
    if (m_window) m_window->removeEventFilter(this);
    m_window = window;
    if (m_window) m_window->installEventFilter(this);
    emit windowChanged();
  }

 signals:
  void windowChanged();
  void panRequested(int horizontal, int vertical);
  void playbackToggleRequested();
  // A GridNavigation::Move value.
  void gridMoveRequested(int move);
  void gridActivateRequested();

 protected:
  bool eventFilter(QObject* object, QEvent* event) override {
    if (object != m_window || event->type() != QEvent::KeyPress || m_modalActive) return false;
    const auto* keyEvent = static_cast<QKeyEvent*>(event);
    const int key = keyEvent->key();
    const bool shift = keyEvent->modifiers().testFlag(Qt::ShiftModifier);
    const auto modifiers = keyEvent->modifiers() & ~Qt::KeypadModifier;
    // Ctrl+D and Ctrl+U page through the grid; every other Ctrl chord stays with the shortcuts.
    const bool gridPage = m_gridActive && modifiers == Qt::ControlModifier && (key == Qt::Key_D || key == Qt::Key_U);
    if (!gridPage && (modifiers & ~Qt::ShiftModifier)) return false;
    if (m_menuOpen) {
      if (key != Qt::Key_J && key != Qt::Key_K) return false;
      if (m_forwarding) return false;
      m_forwarding = true;
      QKeyEvent mapped(QEvent::KeyPress, key == Qt::Key_J ? Qt::Key_Down : Qt::Key_Up, Qt::NoModifier);
      QCoreApplication::sendEvent(m_window, &mapped);
      m_forwarding = false;
    } else if (key == Qt::Key_Tab || key == Qt::Key_Backtab) {
      QQuickItem* buttons[] = {m_window->findChild<QQuickItem*>(QStringLiteral("informationButton")),
                               m_window->findChild<QQuickItem*>(QStringLiteral("fullscreenButton")),
                               m_window->findChild<QQuickItem*>(QStringLiteral("actionsButton"))};
      const int direction = shift || key == Qt::Key_Backtab ? -1 : 1;
      int current = direction > 0 ? -1 : 0;
      for (int i = 0; i < 3; ++i)
        if (buttons[i] == m_window->activeFocusItem()) current = i;
      for (int offset = 1; offset <= 3; ++offset) {
        const int index = (current + direction * offset + 6) % 3;
        if (buttons[index] && buttons[index]->isEnabled()) {
          buttons[index]->forceActiveFocus(direction > 0 ? Qt::TabFocusReason : Qt::BacktabFocusReason);
          break;
        }
      }
    } else if (m_gridActive) {
      using Move = GridNavigation::Move;
      // Space is never handled here, so a focused header button keeps it.
      if (shift) return false;
      switch (key) {
        case Qt::Key_Left:
        case Qt::Key_H:
          emit gridMoveRequested(static_cast<int>(Move::Previous));
          break;
        case Qt::Key_Right:
        case Qt::Key_L:
          emit gridMoveRequested(static_cast<int>(Move::Next));
          break;
        case Qt::Key_Up:
        case Qt::Key_K:
          emit gridMoveRequested(static_cast<int>(Move::RowUp));
          break;
        case Qt::Key_Down:
        case Qt::Key_J:
          emit gridMoveRequested(static_cast<int>(Move::RowDown));
          break;
        case Qt::Key_D:
        case Qt::Key_U:
          if (!gridPage) return false;
          emit gridMoveRequested(static_cast<int>(key == Qt::Key_D ? Move::PageDown : Move::PageUp));
          break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
          // A focused header button activates instead of opening the file.
          if (keyEvent->isAutoRepeat() || headerOrPlaybackButtonFocused()) return false;
          emit gridActivateRequested();
          break;
        default:
          return false;
      }
    } else if (m_imageReady) {
      switch (key) {
        case Qt::Key_Left:
          emit panRequested(40, 0);
          break;
        case Qt::Key_Right:
          emit panRequested(-40, 0);
          break;
        case Qt::Key_Up:
          emit panRequested(0, 40);
          break;
        case Qt::Key_Down:
          emit panRequested(0, -40);
          break;
        case Qt::Key_Space:
          // A focused button keeps Space so that it activates as usual; auto-repeat would flicker the state.
          if (!m_playbackAvailable || shift || keyEvent->isAutoRepeat() || headerOrPlaybackButtonFocused())
            return false;
          emit playbackToggleRequested();
          break;
        default:
          return false;
      }
    } else {
      return false;
    }
    return true;
  }

 private:
  bool headerOrPlaybackButtonFocused() const {
    const auto* focused = m_window->activeFocusItem();
    if (!focused) return false;
    for (const auto* name : {"informationButton", "fullscreenButton", "actionsButton", "playPauseButton"})
      if (m_window->findChild<QQuickItem*>(QString::fromLatin1(name)) == focused) return true;
    return false;
  }
  QQuickWindow* m_window = nullptr;
  bool m_imageReady = false;
  bool m_modalActive = false;
  bool m_menuOpen = false;
  bool m_playbackAvailable = false;
  bool m_gridActive = false;
  bool m_forwarding = false;
};
