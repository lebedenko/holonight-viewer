// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#pragma once

#include "exif_metadata.h"

#include <QDateTime>
#include <QSize>

struct ImageInformation {
  QString format;
  qint64 encoded_size = -1;
  QDateTime modified;
  QSize decoded_size;
  ExifDetails exif;
};
