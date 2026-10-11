#pragma once

#include <QByteArray>
#include <QDBusArgument>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

// One (type, pattern) entry; type 0 is a glob and the only type the Viewer sends.
struct PortalFilterPattern {
  quint32 type = 0;
  QString pattern;
  friend bool operator==(const PortalFilterPattern&, const PortalFilterPattern&) = default;
};

// Marshals as "(sa(us))"; a list of these is the FileChooser "filters" option.
struct PortalFileFilter {
  QString label;
  QList<PortalFilterPattern> patterns;
  friend bool operator==(const PortalFileFilter&, const PortalFileFilter&) = default;
};

QDBusArgument& operator<<(QDBusArgument& argument, const PortalFilterPattern& pattern);
const QDBusArgument& operator>>(const QDBusArgument& argument, PortalFilterPattern& pattern);
QDBusArgument& operator<<(QDBusArgument& argument, const PortalFileFilter& filter);
const QDBusArgument& operator>>(const QDBusArgument& argument, PortalFileFilter& filter);

Q_DECLARE_METATYPE(PortalFilterPattern)
Q_DECLARE_METATYPE(PortalFileFilter)

// Value-only helpers for the FileChooser OpenFile request; no bus or display access.
namespace PortalRequest {
void registerMetaTypes();
// "Images (*.png *.jpg)" becomes label "Images" with glob patterns; blank entries are skipped.
QList<PortalFileFilter> filters(const QStringList& nameFilters);
// NUL-terminated folder of a local file, or empty when there is no local file.
QByteArray currentFolder(const QString& localFilePath);
QString parentWindow(const QString& exportedHandle);
QString senderPathElement(const QString& uniqueBusName);
QString requestPath(const QString& uniqueBusName, const QString& handleToken);
QString newHandleToken();
}  // namespace PortalRequest
