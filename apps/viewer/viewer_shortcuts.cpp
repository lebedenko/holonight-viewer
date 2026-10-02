// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include "viewer_shortcuts.h"

#include <QKeySequence>
QString ViewerShortcuts::sequence(Command command) {
  switch (command) {
    case Open:
      return QStringLiteral("Ctrl+O");
    case Refresh:
      return QStringLiteral("Ctrl+R");
    case Grid:
      return QStringLiteral("Ctrl+G");
    case Previous:
      return QStringLiteral("[");
    case Next:
      return QStringLiteral("]");
    case Fit:
      return QStringLiteral("Ctrl+0");
    case ActualSize:
      return QStringLiteral("1");
    case ZoomIn:
      return QStringLiteral("Ctrl++");
    case ZoomOut:
      return QStringLiteral("Ctrl+-");
    case Fullscreen:
      return QStringLiteral("F");
    case Quit:
      return QStringLiteral("Q");
    case RotateClockwise:
      return QStringLiteral("R");
    case RotateCounterclockwise:
      return QStringLiteral("Shift+R");
    case FlipHorizontal:
      return QStringLiteral("X");
    case FlipVertical:
      return QStringLiteral("Shift+X");
    case CopyImage:
      return QStringLiteral("Ctrl+C");
    case CopyPath:
      return QStringLiteral("Ctrl+Shift+C");
    case Information:
      return QStringLiteral("I");
    case Help:
      return QStringLiteral("?");
    case Escape:
      return QStringLiteral("Escape");
    case Playback:
      return QStringLiteral("Space");
    case GridLeft:
      return QStringLiteral("H");
    case MenuDown:
    case GridDown:
      return QStringLiteral("J");
    case MenuUp:
    case GridUp:
      return QStringLiteral("K");
    case GridRight:
      return QStringLiteral("L");
    case PageUp:
      return QStringLiteral("Ctrl+U");
    case PageDown:
      return QStringLiteral("Ctrl+D");
    case Activate:
      return QStringLiteral("Return");
    case PanLeft:
      return QStringLiteral("Left");
    case PanRight:
      return QStringLiteral("Right");
    case PanUp:
      return QStringLiteral("Up");
    case PanDown:
      return QStringLiteral("Down");
    case GridFirst:
      return QStringLiteral("G, G");
    case GridLast:
      return QStringLiteral("Shift+G");
  }
  return {};
}
QVariantList ViewerShortcuts::sequences(Command command) {
  if (command == Open) {
    return {static_cast<int>(QKeySequence::Open)};
  }
  QVariantList result{sequence(command)};
  result.append(aliasSequences(command));
  return result;
}
QVariantList ViewerShortcuts::keyGroups(Command command) {
  switch (command) {
    case Open:
      return {QVariantList{static_cast<int>(Qt::Key_Control), static_cast<int>(Qt::Key_O)}};
    case Refresh:
      return {QVariantList{static_cast<int>(Qt::Key_Control), static_cast<int>(Qt::Key_R)}};
    case Grid:
      return {QVariantList{static_cast<int>(Qt::Key_Control), static_cast<int>(Qt::Key_G)}};
    case Previous:
      return {QVariantList{static_cast<int>(Qt::Key_BracketLeft)}};
    case Next:
      return {QVariantList{static_cast<int>(Qt::Key_BracketRight)}};
    case Fit:
      return {QVariantList{static_cast<int>(Qt::Key_Control), static_cast<int>(Qt::Key_0)}};
    case ActualSize:
      return {QVariantList{static_cast<int>(Qt::Key_1)}};
    case ZoomIn:
      return {QVariantList{static_cast<int>(Qt::Key_Control), static_cast<int>(Qt::Key_Plus)}};
    case ZoomOut:
      return {QVariantList{static_cast<int>(Qt::Key_Control), static_cast<int>(Qt::Key_Minus)}};
    case Fullscreen:
      return {QVariantList{static_cast<int>(Qt::Key_F)}};
    case Quit:
      return {QVariantList{static_cast<int>(Qt::Key_Q)}};
    case RotateClockwise:
      return {QVariantList{static_cast<int>(Qt::Key_R)}};
    case RotateCounterclockwise:
      return {QVariantList{static_cast<int>(Qt::Key_Shift), static_cast<int>(Qt::Key_R)}};
    case FlipHorizontal:
      return {QVariantList{static_cast<int>(Qt::Key_X)}};
    case FlipVertical:
      return {QVariantList{static_cast<int>(Qt::Key_Shift), static_cast<int>(Qt::Key_X)}};
    case CopyImage:
      return {QVariantList{static_cast<int>(Qt::Key_Control), static_cast<int>(Qt::Key_C)}};
    case CopyPath:
      return {
          QVariantList{
              static_cast<int>(Qt::Key_Control),
              static_cast<int>(Qt::Key_Shift),
              static_cast<int>(Qt::Key_C),
          },
      };
    case Information:
      return {QVariantList{static_cast<int>(Qt::Key_I)}};
    case Help:
      return {QVariantList{static_cast<int>(Qt::Key_Question)}};
    case Escape:
      return {QVariantList{static_cast<int>(Qt::Key_Escape)}};
    case Playback:
      return {QVariantList{static_cast<int>(Qt::Key_Space)}};
    case GridLeft:
      return {QVariantList{static_cast<int>(Qt::Key_H)}};
    case MenuDown:
    case GridDown:
      return {QVariantList{static_cast<int>(Qt::Key_J)}};
    case MenuUp:
    case GridUp:
      return {QVariantList{static_cast<int>(Qt::Key_K)}};
    case GridRight:
      return {QVariantList{static_cast<int>(Qt::Key_L)}};
    case PageUp:
      return {QVariantList{static_cast<int>(Qt::Key_Control), static_cast<int>(Qt::Key_U)}};
    case PageDown:
      return {QVariantList{static_cast<int>(Qt::Key_Control), static_cast<int>(Qt::Key_D)}};
    case Activate:
      return {QVariantList{static_cast<int>(Qt::Key_Enter)}};
    case PanLeft:
      return {QVariantList{static_cast<int>(Qt::Key_Left)}};
    case PanRight:
      return {QVariantList{static_cast<int>(Qt::Key_Right)}};
    case PanUp:
      return {QVariantList{static_cast<int>(Qt::Key_Up)}};
    case PanDown:
      return {QVariantList{static_cast<int>(Qt::Key_Down)}};
    case GridFirst:
      return {};  // Sequential binding is displayed as literal text, never a chord.
    case GridLast:
      return {QVariantList{static_cast<int>(Qt::Key_Shift), static_cast<int>(Qt::Key_G)}};
  }
  return {};
}

QVariantList ViewerShortcuts::aliasSequences(Command command) {
  switch (command) {
    case Previous:
      return {QStringLiteral("H")};
    case Next:
      return {QStringLiteral("L")};
    case ZoomIn:
      return {QStringLiteral("Ctrl+="), QStringLiteral("+")};
    case ZoomOut:
      return {QStringLiteral("-")};
    case Fit:
      return {QStringLiteral("0")};
    case GridLeft:
      return {QStringLiteral("Left")};
    case GridRight:
      return {QStringLiteral("Right")};
    case MenuDown:
    case GridDown:
      return {QStringLiteral("Down")};
    case MenuUp:
    case GridUp:
      return {QStringLiteral("Up")};
    default:
      return {};
  }
}
QVariantList ViewerShortcuts::aliasKeyGroups(Command command) {
  switch (command) {
    case Previous:
      return keyGroups(GridLeft);
    case Next:
      return keyGroups(GridRight);
    case ZoomIn:
      return {QVariantList{static_cast<int>(Qt::Key_Control), static_cast<int>(Qt::Key_Equal)},
              QVariantList{static_cast<int>(Qt::Key_Plus)}};
    case ZoomOut:
      return {QVariantList{static_cast<int>(Qt::Key_Minus)}};
    case Fit:
      return {QVariantList{static_cast<int>(Qt::Key_0)}};
    case GridLeft:
      return keyGroups(PanLeft);
    case GridRight:
      return keyGroups(PanRight);
    case MenuDown:
    case GridDown:
      return keyGroups(PanDown);
    case MenuUp:
    case GridUp:
      return keyGroups(PanUp);
    default:
      return {};
  }
}
