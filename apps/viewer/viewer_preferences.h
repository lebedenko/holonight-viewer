// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <cstdint>

class ViewerPreferences : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Preferences are supplied by the application")
  Q_PROPERTY(bool showTitle READ showTitle NOTIFY showTitleChanged)
  Q_PROPERTY(bool showCloseButton READ showCloseButton NOTIFY showCloseButtonChanged)
 public:
  enum class Source : std::uint8_t { Default, Base, Override };
  explicit ViewerPreferences(QObject* parent = nullptr);
  ViewerPreferences(const QString& configHome, const QString& home, const QString& desktop, QObject* parent = nullptr);
  [[nodiscard]] static QString configurationDirectory(const QString& configHome, const QString& home);
  [[nodiscard]] bool showTitle() const { return show_title_; }
  [[nodiscard]] bool showCloseButton() const { return show_close_; }
  [[nodiscard]] Source titleSource() const { return title_source_; }
  [[nodiscard]] Source closeSource() const { return close_source_; }
  [[nodiscard]] QString selectedOverride() const { return selected_override_; }
 signals:
  void showTitleChanged();
  void showCloseButtonChanged();

 private:
  struct Snapshot {
    bool title = true;
    bool close = true;
    Source title_source = Source::Default;
    Source close_source = Source::Default;
  };
  static void applyFile(const QString& path, Source source, Snapshot& snapshot);
  void reload();
  void refreshWatches();
  QString directory_;
  QStringList candidates_;
  QString selected_override_;
  QFileSystemWatcher watcher_;
  QTimer debounce_;
  bool show_title_ = true;
  bool show_close_ = true;
  Source title_source_ = Source::Default;
  Source close_source_ = Source::Default;
};
