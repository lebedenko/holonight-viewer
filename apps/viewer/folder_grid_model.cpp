#include "folder_grid_model.h"

#include "directory_model.h"

FolderGridModel::FolderGridModel(DirectoryModel* source, QObject* parent)
    : QSortFilterProxyModel(parent), directory_(source) {
  connect(this, &QAbstractItemModel::modelReset, this, &FolderGridModel::countChanged);
  connect(this, &QAbstractItemModel::rowsInserted, this, &FolderGridModel::countChanged);
  connect(this, &QAbstractItemModel::rowsRemoved, this, &FolderGridModel::countChanged);
  setSourceModel(source);
}

QUrl FolderGridModel::urlAt(int row) const {
  const auto index = this->index(row, 0);
  return index.isValid() ? directory_->urlAt(mapToSource(index).row()) : QUrl{};
}

int FolderGridModel::indexOfUrl(const QUrl& url) const {
  const auto row = directory_->indexOf(url);
  if (row < 0) {
    return -1;
  }
  const auto index = mapFromSource(directory_->index(row));
  return index.isValid() ? index.row() : -1;
}

bool FolderGridModel::filterAcceptsRow(int sourceRow, const QModelIndex& /*sourceParent*/) const {
  const auto missing = directory_->missingUrl();
  return missing.isEmpty() || directory_->urlAt(sourceRow) != missing;
}
