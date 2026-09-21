#pragma once

#include <QByteArray>
#include <QImageReader>
#include <QList>

// Builds minimal GIFs for tests. Every frame is a single pixel patch; `second` picks the alternate LZW payload so
// consecutive frames differ. loopField -1 omits the NETSCAPE2.0 extension, otherwise it is the raw repeat field.
namespace gif {
struct Frame {
  int delayCs;
  bool second;  // selects the alternate one-pixel LZW payload so frames differ
};

// Loop values: -1 omits the NETSCAPE2.0 extension; otherwise the raw 16-bit repeat field.
inline QByteArray bytes(const QByteArray& version, const QList<Frame>& frames, int loopField = -1, int width = 1,
                        int height = 1) {
  QByteArray bytes(version);
  bytes.append(char(width & 255)).append(char(width >> 8)).append(char(height & 255)).append(char(height >> 8));
  bytes += QByteArray::fromHex("800000") + QByteArray::fromHex("000000ffffff");
  if (loopField >= 0) {
    bytes += QByteArray::fromHex("21ff0b") + "NETSCAPE2.0" + QByteArray::fromHex("0301");
    bytes.append(char(loopField & 255)).append(char(loopField >> 8)).append(char(0));
  }
  for (const auto& frame : frames) {
    bytes += QByteArray::fromHex("21f90401");
    bytes.append(char(frame.delayCs & 255)).append(char(frame.delayCs >> 8));
    bytes += QByteArray::fromHex("0000") + QByteArray::fromHex("2c0000000001000100000202");
    bytes += QByteArray::fromHex(frame.second ? "4c0100" : "440100");
  }
  return bytes + QByteArray::fromHex("3b");
}

inline bool available() { return QImageReader::supportedImageFormats().contains("gif"); }
}  // namespace gif
