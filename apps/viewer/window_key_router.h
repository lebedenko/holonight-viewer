// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>

#pragma once

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

 protected:
  bool eventFilter(QObject* object, QEvent* event) override {
    if (object != m_window || event->type() != QEvent::KeyPress || m_modalActive) return false;
    const auto* keyEvent = static_cast<QKeyEvent*>(event);
    const int key = keyEvent->key();
    const bool shift = keyEvent->modifiers().testFlag(Qt::ShiftModifier);
    if (keyEvent->modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier)) return false;
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
        default:
          return false;
      }
    } else {
      return false;
    }
    return true;
  }

 private:
  QQuickWindow* m_window = nullptr;
  bool m_imageReady = false;
  bool m_modalActive = false;
  bool m_menuOpen = false;
  bool m_forwarding = false;
};
