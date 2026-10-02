// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#pragma once
#include "folder_grid_model.h"
#include "grid_navigation.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>
class ImageDocument;
class GridSelectionController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Owned by ViewerController")
  Q_PROPERTY(QUrl selectedUrl READ selectedUrl NOTIFY changed)
  Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY changed)
  Q_PROPERTY(int columns MEMBER columns_ NOTIFY changed)
  Q_PROPERTY(int visibleRows MEMBER visible_rows_ NOTIFY changed)
  Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY changed)
  Q_PROPERTY(bool canNext READ canNext NOTIFY changed)
  Q_PROPERTY(bool canActivate READ canActivate NOTIFY changed)
 public:
  explicit GridSelectionController(ImageDocument* document, QObject* parent = nullptr);
  [[nodiscard]] QUrl selectedUrl() const { return selected_url_; }
  [[nodiscard]] int selectedIndex() const { return selected_index_; }
  [[nodiscard]] bool canPrevious() const { return canMove(static_cast<int>(GridNavigation::Move::Previous)); }
  [[nodiscard]] bool canNext() const { return canMove(static_cast<int>(GridNavigation::Move::Next)); }
  void enter(const QUrl& url);
  void reconcile();
  Q_INVOKABLE void select(int index);
  Q_INVOKABLE [[nodiscard]] bool canMove(int move) const;
  Q_INVOKABLE void move(int move);
  [[nodiscard]] bool canActivate() const;
 signals:
  void changed();

 private:
  [[nodiscard]] int target(int move) const;
  ImageDocument* document_;
  QUrl selected_url_;
  int selected_index_ = -1;
  int last_index_ = 0;
  int columns_ = 1;
  int visible_rows_ = 1;
};
