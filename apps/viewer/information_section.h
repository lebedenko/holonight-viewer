// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#pragma once

#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

struct InformationSection {
  Q_GADGET
  QML_VALUE_TYPE(informationSection)
  QML_STRUCTURED_VALUE
  Q_PROPERTY(QString key MEMBER key)
  Q_PROPERTY(QString label MEMBER label)
  Q_PROPERTY(QStringList lines MEMBER lines)
 public:
  QString key;
  QString label;
  QStringList lines;
  bool operator==(const InformationSection&) const = default;
};
