#pragma once

#include <QAbstractListModel>
#include <QThread>
#include <QUrl>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

struct DirectoryResult {
  QList<QUrl> urls;
  QString error;
};
QUrl normalizedLocalUrl(const QUrl& url);
bool naturalFileNameLess(const QString& left, const QString& right);
DirectoryResult scanDirectory(const QUrl& selected, const std::atomic_bool& cancelled);

class DirectoryModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum Role { FileNameRole = Qt::UserRole + 1, UrlRole };
  using Scanner = std::function<DirectoryResult(const QUrl&, const std::atomic_bool&)>;
  explicit DirectoryModel(QObject* parent = nullptr);
  explicit DirectoryModel(Scanner scanner, QObject* parent = nullptr);
  ~DirectoryModel() override;
  int rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  QUrl urlAt(int index) const;
  int indexOf(const QUrl& url) const;
  bool scanning() const { return scanning_; }
  QString error() const { return error_; }
  void scan(const QUrl& selected);
  void clear();
  void shutdown();
 signals:
  void changed();
  void shutdownFinished();

 private:
  void startPending();
  void replace(QList<QUrl> urls);
  QThread thread_;
  QObject* worker_;
  Scanner scanner_;
  QList<QUrl> urls_;
  QString error_;
  std::optional<QUrl> pending_;
  std::shared_ptr<std::atomic_bool> cancellation_;
  quint64 generation_ = 0;
  bool scanning_ = false;
  bool busy_ = false;
  bool stopping_ = false;
};
