// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>

#pragma once

#include "grid_navigation.h"

#include <QKeyEvent>
#include <QObject>
#include <QPointer>
#include <QQmlListProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

// QObject owns identity and disables copying/moving.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class WindowKeyRouter : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QQuickWindow* target READ window WRITE setWindow NOTIFY windowChanged)
  Q_PROPERTY(QQmlListProperty<QQuickItem> headerFocusTargets READ focusTargets NOTIFY focusTargetsChanged)
  Q_PROPERTY(QQuickItem* playbackButton READ playbackButton WRITE setPlaybackButton NOTIFY focusTargetsChanged)
  Q_PROPERTY(bool imageReady MEMBER image_ready_)
  Q_PROPERTY(bool modalActive MEMBER modal_active_)
  Q_PROPERTY(bool menuOpen MEMBER menu_open_)
  Q_PROPERTY(bool playbackAvailable MEMBER playback_available_)
  Q_PROPERTY(bool gridActive MEMBER grid_active_)

 public:
  explicit WindowKeyRouter(QObject* parent = nullptr);
  ~WindowKeyRouter() override;
  [[nodiscard]] QQuickWindow* window() const;
  void setWindow(QQuickWindow* window);
  QQmlListProperty<QQuickItem> focusTargets();
  [[nodiscard]] QList<QQuickItem*> headerFocusTargets() const;
  void setHeaderFocusTargets(const QList<QQuickItem*>& targets);
  [[nodiscard]] QQuickItem* playbackButton() const;
  void setPlaybackButton(QQuickItem* target);

 signals:
  void windowChanged();
  void focusTargetsChanged();
  void panRequested(int horizontal, int vertical);
  void playbackToggleRequested();
  // A GridNavigation::Move value.
  void gridMoveRequested(int move);
  void gridActivateRequested();

 protected:
  bool eventFilter(QObject* object, QEvent* event) override;

 private:
  [[nodiscard]] bool headerOrPlaybackButtonFocused() const;
  bool routeMenu(int key);
  bool routeTab(bool backwards);
  bool routeGrid(const QKeyEvent& event, bool gridPage);
  bool routeImage(const QKeyEvent& event);
  QList<QPointer<QQuickItem>> header_targets_;
  QPointer<QQuickItem> playback_button_;
  QPointer<QQuickWindow> window_;
  bool image_ready_ = false;
  bool modal_active_ = false;
  bool menu_open_ = false;
  bool playback_available_ = false;
  bool grid_active_ = false;
  bool forwarding_ = false;
};
