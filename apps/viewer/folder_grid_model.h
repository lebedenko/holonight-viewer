// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>

#pragma once

#include "directory_model.h"

#include <QSortFilterProxyModel>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// The folder listing as the grid sees it: source order, minus an entry known to be missing on disk. The
// document keeps browsing the unfiltered DirectoryModel, so a deleted current file stays reachable there.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class FolderGridModel : public QSortFilterProxyModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
  QML_ELEMENT
  QML_UNCREATABLE("Owned by the document")

 public:
  explicit FolderGridModel(DirectoryModel* source, QObject* parent = nullptr);
  [[nodiscard]] int count() const { return rowCount(); }
  Q_INVOKABLE [[nodiscard]] QUrl urlAt(int row) const;
  // -1 when the URL is absent or hidden.
  Q_INVOKABLE [[nodiscard]] int indexOfUrl(const QUrl& url) const;

 signals:
  void countChanged();

 protected:
  [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

 private:
  DirectoryModel* directory_;
};
