// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>

#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <algorithm>
#include <cstdint>

class GridNavigation : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

 public:
  enum class Move : std::uint8_t { Previous, Next, RowUp, RowDown, PageUp, PageDown, First, Last };
  Q_ENUM(Move)

  explicit GridNavigation(QObject* parent = nullptr) : QObject(parent) {}

  // The new index in [0, count - 1], or -1 when the grid is empty. A current index outside the
  // list counts as 0, columns below 1 as 1 and visibleRows below 0 as 0, so a grid that has not
  // been laid out yet still moves by one row.
  Q_INVOKABLE static constexpr int target(int current, Move move, int count, int columns, int visibleRows) {
    if (count <= 0) {
      return -1;
    }
    const int last = count - 1;
    const int width = std::max(columns, 1);
    const int from = (current < 0 || current > last) ? 0 : current;
    const int page = std::max(1, std::max(visibleRows, 0) / 2) * width;
    switch (move) {
      case Move::First:
        return 0;
      case Move::Last:
        return last;
      case Move::Previous:
        return std::max(from - 1, 0);
      case Move::Next:
        return std::min(from + 1, last);
      case Move::RowUp:
        return from / width == 0 ? from : from - width;
      case Move::RowDown:
        return from / width == last / width ? from : std::min(from + width, last);
      case Move::PageUp:
        return std::max(from - page, 0);
      case Move::PageDown:
        return std::min(from + page, last);
    }
    return from;
  }
};
