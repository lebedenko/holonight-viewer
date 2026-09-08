// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#pragma once

#include <QIODevice>
#include <QImage>

// Internal encoding boundary; accepts a device so write failures can be verified.
inline bool encodeClipboardPng(const QImage& image, QIODevice& device) {
  return !image.isNull() && image.save(&device, "PNG");
}
