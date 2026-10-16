// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>

#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <HolonightFileBrowser/grid_navigation.h>
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
    return HolonightFileBrowser::gridTarget(current, static_cast<HolonightFileBrowser::GridMove>(move), count, columns,
                                            visibleRows);
  }
};
