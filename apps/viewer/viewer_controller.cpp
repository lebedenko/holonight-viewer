// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include "viewer_controller.h"

#include "pasted_path_p.h"

#include <QClipboard>
#include <QGuiApplication>
void ViewerController::setDocument(ImageDocument* document) {
  if (document_ == document) {
    return;
  }
  if (document_) {
    disconnect(document_, nullptr, this, nullptr);
    document_->animation()->setSuspendedByModal(false);
    document_->animation()->setSuspendedByWindow(false);
  }
  delete selection_;
  document_ = document;
  selection_ = (document != nullptr) ? new GridSelectionController(document, this) : nullptr;
  grid_mode_ = false;
  if (document != nullptr) {
    connect(document, &ImageDocument::pastedPathOpened, this, [this] {
      grid_mode_ = false;
      updateSuspension();
      emit changed();
    });
    connect(document, &ImageDocument::changed, this, &ViewerController::changed);
    connect(selection_, &GridSelectionController::changed, this, &ViewerController::changed);
  }
  updateSuspension();
  emit documentChanged();
  emit changed();
}
void ViewerController::updateSuspension() {
  if (!document_) {
    return;
  }
  document_->animation()->setSuspendedByModal(modal_active_ || grid_mode_);
  document_->animation()->setSuspendedByWindow(window_suspended_);
}
void ViewerController::setModalActive(bool active) {
  if (modal_active_ == active) {
    return;
  }
  modal_active_ = active;
  updateSuspension();
  emit changed();
}
void ViewerController::setWindowSuspended(bool suspended) {
  if (window_suspended_ == suspended) {
    return;
  }
  window_suspended_ = suspended;
  updateSuspension();
  emit changed();
}
bool ViewerController::hasPath() const { return document_ && !modal_active_ && !document_->localPath().isEmpty(); }
bool ViewerController::canEnterGrid() const {
  return hasPath() && (document_->scanning() || document_->folder()->count() > 0);
}
bool ViewerController::canToggleGrid() const { return !modal_active_ && (grid_mode_ || canEnterGrid()); }
bool ViewerController::canInspect() const {
  return document_ && !modal_active_ && !grid_mode_ && document_->state() == ImageDocument::Ready;
}
bool ViewerController::canShowInformation() const { return hasPath() && !grid_mode_; }
bool ViewerController::canPrevious() const {
  return document_ && !modal_active_ && (grid_mode_ ? selection_->canPrevious() : document_->canPrevious());
}
bool ViewerController::canNext() const {
  return document_ && !modal_active_ && (grid_mode_ ? selection_->canNext() : document_->canNext());
}
void ViewerController::open(const QList<QUrl>& urls) {
  if (!document_) {
    return;
  }
  // Picker acceptance is delivered while its modal is still active; cancellation never calls open.
  grid_mode_ = false;
  updateSuspension();
  document_->open(urls);
  emit changed();
}
void ViewerController::browse(int direction) {
  if (direction < 0 ? !canPrevious() : !canNext()) {
    return;
  }
  if (grid_mode_) {
    document_->cancelPastedPath();
    selection_->move(static_cast<int>(direction < 0 ? GridNavigation::Move::Previous : GridNavigation::Move::Next));
  } else if (direction < 0) {
    document_->previous();
  } else {
    document_->next();
  }
}
void ViewerController::paste() {
  if (!document_ || modal_active_ || QGuiApplication::clipboard() == nullptr) {
    return;
  }
  const auto url = parsePastedPath(QGuiApplication::clipboard()->text(QClipboard::Clipboard));
  document_->tryOpenPastedPath(url.value_or(QUrl{}));
}
void ViewerController::refresh() {
  if (hasPath()) {
    document_->refresh();
  }
}
void ViewerController::enterGrid() {
  if (!canEnterGrid() || grid_mode_) {
    return;
  }
  grid_mode_ = true;
  selection_->enter(document_->url());
  updateSuspension();
  emit changed();
}
void ViewerController::leaveGrid() {
  if (modal_active_ || !grid_mode_) {
    return;
  }
  grid_mode_ = false;
  updateSuspension();
  emit changed();
}
void ViewerController::toggleGrid() {
  if (grid_mode_) {
    leaveGrid();
  } else {
    enterGrid();
  }
}
void ViewerController::moveSelection(int move) {
  if (grid_mode_ && !modal_active_) {
    document_->cancelPastedPath();
    selection_->move(move);
  }
}
void ViewerController::activateSelection() {
  if (!grid_mode_ || modal_active_ || !selection_->canActivate()) {
    return;
  }
  document_->openFromFolder(selection_->selectedUrl());
  leaveGrid();
}
void ViewerController::togglePlayback() {
  if (canInspect() && document_->animation()->canToggle()) {
    document_->animation()->toggle();
  }
}
