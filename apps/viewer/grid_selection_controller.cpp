// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include "grid_selection_controller.h"

#include "image_document.h"

#include <algorithm>
GridSelectionController::GridSelectionController(ImageDocument* document, QObject* parent)
    : QObject(parent), document_(document) {
  if (document != nullptr) {
    connect(document, &ImageDocument::changed, this, &GridSelectionController::reconcile);
    connect(document, &QObject::destroyed, this, &GridSelectionController::reconcile);
  }
}
void GridSelectionController::enter(const QUrl& url) {
  selection_.single(url.toEncoded());
  last_index_ = 0;
  selected_index_ = -1;
  reconcile();
}
void GridSelectionController::reconcile() {
  if (!document_) {
    selection_.clear();
    selected_index_ = -1;
    last_index_ = 0;
  } else if (!document_->scanning()) {
    auto* model = document_->folder();
    int index = model->indexOfUrl(selectedUrl());
    if (index < 0 && model->count() > 0) {
      index = std::clamp(last_index_, 0, model->count() - 1);
    }
    selected_index_ = index;
    if (index >= 0) {
      last_index_ = index;
      selection_.single(model->urlAt(index).toEncoded());
    }
  }
  emit changed();
}
void GridSelectionController::select(int index) {
  if (!document_ || document_->scanning() || index < 0 || index >= document_->folder()->count()) {
    return;
  }
  selected_index_ = index;
  last_index_ = index;
  selection_.single(document_->folder()->urlAt(index).toEncoded());
  emit changed();
}
int GridSelectionController::target(int move) const {
  if (!document_) {
    return -1;
  }
  if (move < 0 || move > static_cast<int>(GridNavigation::Move::Last)) {
    return selected_index_;
  }
  return GridNavigation::target(selected_index_, static_cast<GridNavigation::Move>(move), document_->folder()->count(),
                                columns_, visible_rows_);
}
bool GridSelectionController::canMove(int move) const {
  const int index = target(move);
  return document_ && !document_->scanning() && index >= 0 && index != selected_index_;
}
void GridSelectionController::move(int move) {
  // Boundary commands also reveal an already-selected item after manual scrolling.
  if (move == static_cast<int>(GridNavigation::Move::First) || move == static_cast<int>(GridNavigation::Move::Last) ||
      canMove(move)) {
    select(target(move));
  }
}
bool GridSelectionController::canActivate() const {
  return document_ && !document_->scanning() && selected_index_ >= 0 && selected_index_ < document_->folder()->count();
}
