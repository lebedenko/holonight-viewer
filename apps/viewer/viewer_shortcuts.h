// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#pragma once
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
class ViewerShortcuts : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
 public:
  // NOLINTNEXTLINE(cppcoreguidelines-use-enum-class,performance-enum-size)
  enum Command {
    Open,
    Refresh,
    Grid,
    Previous,
    Next,
    Fit,
    ActualSize,
    ZoomIn,
    ZoomOut,
    Fullscreen,
    Quit,
    RotateClockwise,
    RotateCounterclockwise,
    FlipHorizontal,
    FlipVertical,
    CopyImage,
    CopyPath,
    Information,
    Help,
    Escape,
    Playback,
    GridLeft,
    GridDown,
    GridUp,
    GridRight,
    PageUp,
    PageDown,
    Activate,
    PanLeft,
    PanRight,
    PanUp,
    PanDown,
    GridFirst,
    GridLast,
    MenuDown,
    MenuUp,
  };
  Q_ENUM(Command)
  explicit ViewerShortcuts(QObject* parent = nullptr) : QObject(parent) {}
  Q_INVOKABLE static QVariantList sequences(Command command);
  Q_INVOKABLE static QString sequence(Command command);
  Q_INVOKABLE static QVariantList keyGroups(Command command);
  Q_INVOKABLE static QVariantList aliasSequences(Command command);
  Q_INVOKABLE static QVariantList aliasKeyGroups(Command command);
  static constexpr int key(Command command) {
    switch (command) {
      case GridLeft:
        return Qt::Key_H;
      case MenuDown:
      case GridDown:
        return Qt::Key_J;
      case MenuUp:
      case GridUp:
        return Qt::Key_K;
      case GridRight:
        return Qt::Key_L;
      case GridFirst:
      case GridLast:
        return Qt::Key_G;
      case PageUp:
        return Qt::Key_U;
      case PageDown:
        return Qt::Key_D;
      case Playback:
        return Qt::Key_Space;
      case Activate:
        return Qt::Key_Return;
      case PanLeft:
        return Qt::Key_Left;
      case PanRight:
        return Qt::Key_Right;
      case PanUp:
        return Qt::Key_Up;
      case PanDown:
        return Qt::Key_Down;
      default:
        return 0;
    }
  }
};
