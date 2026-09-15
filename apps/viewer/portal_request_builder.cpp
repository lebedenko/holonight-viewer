#include "portal_request_builder.h"

#include <QDBusMetaType>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

QDBusArgument& operator<<(QDBusArgument& argument, const PortalFilterPattern& pattern) {
  argument.beginStructure();
  argument << pattern.type << pattern.pattern;
  argument.endStructure();
  return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, PortalFilterPattern& pattern) {
  argument.beginStructure();
  argument >> pattern.type >> pattern.pattern;
  argument.endStructure();
  return argument;
}

QDBusArgument& operator<<(QDBusArgument& argument, const PortalFileFilter& filter) {
  argument.beginStructure();
  argument << filter.label << filter.patterns;
  argument.endStructure();
  return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, PortalFileFilter& filter) {
  argument.beginStructure();
  argument >> filter.label >> filter.patterns;
  argument.endStructure();
  return argument;
}

void PortalRequest::registerMetaTypes() {
  qDBusRegisterMetaType<PortalFilterPattern>();
  qDBusRegisterMetaType<QList<PortalFilterPattern>>();
  qDBusRegisterMetaType<PortalFileFilter>();
  qDBusRegisterMetaType<QList<PortalFileFilter>>();
}

QList<PortalFileFilter> PortalRequest::filters(const QStringList& nameFilters) {
  QList<PortalFileFilter> result;
  for (const auto& entry : nameFilters) {
    const auto open = entry.lastIndexOf('(');
    const auto close = entry.lastIndexOf(')');
    const auto bracketed = open >= 0 && close > open;
    // Qt treats an entry without parentheses as a bare pattern list.
    const auto label = (bracketed ? entry.left(open) : entry).trimmed();
    const auto globs = bracketed ? entry.mid(open + 1, close - open - 1) : entry;
    PortalFileFilter filter;
    for (const auto& glob : globs.split(' ', Qt::SkipEmptyParts)) {
      filter.patterns.append({.type = 0, .pattern = glob});
    }
    if (filter.patterns.isEmpty()) {
      continue;
    }
    filter.label = label.isEmpty() ? globs.trimmed() : label;
    result.append(std::move(filter));
  }
  return result;
}

QByteArray PortalRequest::currentFolder(const QString& localFilePath) {
  if (localFilePath.isEmpty()) {
    return {};
  }
  auto bytes = QFile::encodeName(QDir::cleanPath(QFileInfo(localFilePath).absolutePath()));
  bytes.append('\0');
  return bytes;
}

QString PortalRequest::parentWindow(const QString& exportedHandle) {
  return exportedHandle.isEmpty() ? QString() : QStringLiteral("wayland:") + exportedHandle;
}

QString PortalRequest::senderPathElement(const QString& uniqueBusName) {
  auto element = uniqueBusName.startsWith(':') ? uniqueBusName.mid(1) : uniqueBusName;
  return element.replace('.', '_');
}

QString PortalRequest::requestPath(const QString& uniqueBusName, const QString& handleToken) {
  return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2")
      .arg(senderPathElement(uniqueBusName), handleToken);
}

QString PortalRequest::newHandleToken() {
  // Object path elements admit only [A-Za-z0-9_].
  return QStringLiteral("hn_") + QUuid::createUuid().toString(QUuid::WithoutBraces).replace('-', '_');
}
