// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include "viewer_preferences.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <toml++/toml.hpp>

QString ViewerPreferences::configurationDirectory(const QString& configHome, const QString& home) {
  return QDir(configHome.isEmpty() ? QDir(home).filePath(QStringLiteral(".config")) : configHome)
      .absoluteFilePath(QStringLiteral("holonight-viewer"));
}
ViewerPreferences::ViewerPreferences(QObject* parent)
    : ViewerPreferences(qEnvironmentVariable("XDG_CONFIG_HOME"), qEnvironmentVariable("HOME"),
                        qEnvironmentVariable("XDG_CURRENT_DESKTOP"), parent) {}
ViewerPreferences::ViewerPreferences(const QString& configHome, const QString& home, const QString& desktop,
                                     QObject* parent)
    : QObject(parent), directory_(configurationDirectory(configHome, home)) {
  for (auto name : desktop.split(':', Qt::SkipEmptyParts)) {
    bool valid = true;
    for (auto& character : name) {
      const auto code = character.unicode();
      const bool allowed = (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9') || code == '-' || code == '_';
      if (code >= 'A' && code <= 'Z') {
        character = QChar(code + ('a' - 'A'));
      } else if (!allowed) {
        valid = false;
      }
    }
    if (!valid) {
      qWarning() << "Viewer preferences: invalid desktop name" << name;
      continue;
    }
    candidates_.append(QDir(directory_).filePath(QStringLiteral("config-%1.toml").arg(name)));
  }
  debounce_.setSingleShot(true);
  debounce_.setInterval(100);
  connect(&watcher_, &QFileSystemWatcher::fileChanged, &debounce_, qOverload<>(&QTimer::start));
  connect(&watcher_, &QFileSystemWatcher::directoryChanged, &debounce_, qOverload<>(&QTimer::start));
  connect(&debounce_, &QTimer::timeout, this, &ViewerPreferences::reload);
  reload();
}
void ViewerPreferences::refreshWatches() {
  if (!watcher_.files().isEmpty()) {
    watcher_.removePaths(watcher_.files());
  }
  if (!watcher_.directories().isEmpty()) {
    watcher_.removePaths(watcher_.directories());
  }
  QStringList paths = candidates_;
  paths.append(QDir(directory_).filePath(QStringLiteral("config.toml")));
  paths.append(directory_);
  QStringList watches;
  for (auto path : paths) {
    // Retain parent watches as well: directory replacement must be observable.
    while (!path.isEmpty()) {
      if (QFileInfo::exists(path) && !watches.contains(path)) {
        watches.append(path);
      }
      const auto parent = QFileInfo(path).absolutePath();
      if (parent == path) {
        break;
      }
      path = parent;
    }
  }
  if (!watches.isEmpty()) {
    watcher_.addPaths(watches);
  }
}
void ViewerPreferences::applyFile(const QString& path, Source source, Snapshot& snapshot) {
  if (!QFileInfo::exists(path)) {
    return;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Viewer preferences: unreadable file" << path << file.errorString();
    return;
  }
  const auto bytes = file.readAll();
  if (file.error() != QFileDevice::NoError) {
    qWarning() << "Viewer preferences: cannot read" << path << file.errorString();
    return;
  }
  try {
    const auto table = toml::parse(std::string_view(bytes.constData(), bytes.size()));
    for (const auto& [key, node] : table) {
      if (key != "header") {
        qWarning() << "Viewer preferences: unknown key" << path << QString::fromStdString(std::string(key.str()));
      }
    }
    const auto* header = table["header"].as_table();
    if (header == nullptr) {
      if (table.contains("header")) {
        qWarning() << "Viewer preferences: header must be a table" << path;
      }
      return;
    }
    for (const auto& [key, node] : *header) {
      if (key != "show_title" && key != "show_close_button") {
        qWarning() << "Viewer preferences: unknown header key" << path
                   << QString::fromStdString(std::string(key.str()));
        continue;
      }
      const auto* value = node.as_boolean();
      if (value == nullptr) {
        qWarning() << "Viewer preferences: expected boolean" << path << QString::fromStdString(std::string(key.str()));
        continue;
      }
      if (key == "show_title") {
        snapshot.title = value->get();
        snapshot.title_source = source;
      } else {
        snapshot.close = value->get();
        snapshot.close_source = source;
      }
    }
  } catch (const toml::parse_error& error) {
    qWarning() << "Viewer preferences: malformed file" << path << error.what();
  }
}
void ViewerPreferences::reload() {
  Snapshot snapshot;
  selected_override_.clear();
  for (const auto& candidate : candidates_) {
    if (QFileInfo::exists(candidate)) {
      selected_override_ = candidate;
      break;
    }
  }
  applyFile(QDir(directory_).filePath(QStringLiteral("config.toml")), Source::Base, snapshot);
  if (!selected_override_.isEmpty()) {
    applyFile(selected_override_, Source::Override, snapshot);
  }
  refreshWatches();
  const bool titleChanged = show_title_ != snapshot.title;
  const bool closeChanged = show_close_ != snapshot.close;
  show_title_ = snapshot.title;
  show_close_ = snapshot.close;
  title_source_ = snapshot.title_source;
  close_source_ = snapshot.close_source;
  if (titleChanged) {
    emit showTitleChanged();
  }
  if (closeChanged) {
    emit showCloseButtonChanged();
  }
}
