// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#pragma once
#include "grid_selection_controller.h"
#include "image_document.h"

#include <QPointer>
class ViewerController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(ImageDocument* document READ document WRITE setDocument NOTIFY documentChanged REQUIRED)
  Q_PROPERTY(GridSelectionController* selection READ selection NOTIFY documentChanged)
  Q_PROPERTY(bool gridMode READ gridMode NOTIFY changed)
  Q_PROPERTY(bool modalActive READ modalActive WRITE setModalActive NOTIFY changed)
  Q_PROPERTY(bool windowSuspended READ windowSuspended WRITE setWindowSuspended NOTIFY changed)
  Q_PROPERTY(bool canEnterGrid READ canEnterGrid NOTIFY changed)
  Q_PROPERTY(bool canToggleGrid READ canToggleGrid NOTIFY changed)
  Q_PROPERTY(bool canInspect READ canInspect NOTIFY changed)
  Q_PROPERTY(bool hasPath READ hasPath NOTIFY changed)
  Q_PROPERTY(bool canShowInformation READ canShowInformation NOTIFY changed)
  Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY changed)
  Q_PROPERTY(bool canNext READ canNext NOTIFY changed)
 public:
  explicit ViewerController(QObject* parent = nullptr) : QObject(parent) {}
  [[nodiscard]] ImageDocument* document() const { return document_; }
  void setDocument(ImageDocument* document);
  [[nodiscard]] GridSelectionController* selection() const { return selection_; }
  [[nodiscard]] bool gridMode() const { return grid_mode_; }
  [[nodiscard]] bool modalActive() const { return modal_active_; }
  [[nodiscard]] bool windowSuspended() const { return window_suspended_; }
  void setModalActive(bool active);
  void setWindowSuspended(bool suspended);
  [[nodiscard]] bool hasPath() const;
  [[nodiscard]] bool canEnterGrid() const;
  [[nodiscard]] bool canToggleGrid() const;
  [[nodiscard]] bool canInspect() const;
  [[nodiscard]] bool canShowInformation() const;
  [[nodiscard]] bool canPrevious() const;
  [[nodiscard]] bool canNext() const;
  Q_INVOKABLE void open(const QList<QUrl>& urls);
  Q_INVOKABLE void browse(int direction);
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void enterGrid();
  Q_INVOKABLE void leaveGrid();
  Q_INVOKABLE void toggleGrid();
  Q_INVOKABLE void moveSelection(int move);
  Q_INVOKABLE void activateSelection();
  Q_INVOKABLE void togglePlayback();
 signals:
  void changed();
  void documentChanged();

 private:
  void updateSuspension();
  QPointer<ImageDocument> document_;
  GridSelectionController* selection_ = nullptr;
  bool grid_mode_ = false;
  bool modal_active_ = false;
  bool window_suspended_ = false;
};
