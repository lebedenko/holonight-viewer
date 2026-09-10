#include <QClipboard>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>

#include <functional>

namespace {
QImage readImage(QClipboard* clipboard, const QString& format) {
  if (format != "png") {
    return clipboard->image();
  }
  const auto* data = clipboard->mimeData();
  return data != nullptr ? QImage::fromData(data->data("image/png"), "PNG") : QImage{};
}

bool markCompletion(const QString& name) {
  const auto outputDir = qEnvironmentVariable("VIEWER_NATIVE_OUTPUT_DIR");
  if (outputDir.isEmpty()) {
    return true;
  }
  QElapsedTimer timestamp;
  timestamp.start();
  QSaveFile marker(outputDir + "/" + name);
  const auto bytes = QByteArray::number(timestamp.msecsSinceReference());
  return marker.open(QIODevice::WriteOnly) && marker.write(bytes) == bytes.size() && marker.commit();
}

bool receiveClipboard(const QString& format, const QString& outputFile) {
  auto* clipboard = QGuiApplication::clipboard();
  QElapsedTimer readTimer;
  readTimer.start();
  bool success = false;
  qint64 readMs = 0;
  QTextStream report(stdout);

  if (format == "text") {
    const auto bytes = clipboard->text().toUtf8();
    readMs = readTimer.elapsed();
    success = !bytes.isEmpty();
    if (success) {
      QFile file(outputFile);
      success = file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    }
    report << "read_ms=" << readMs << ' ' << "text=" << QString::fromUtf8(bytes)
           << " success=" << static_cast<int>(success);
  } else {
    QImage image = readImage(clipboard, format);
    if (!image.isNull()) {
      image = image.convertToFormat(QImage::Format_RGBA8888);
    }
    readMs = readTimer.elapsed();
    // Timestamp decode completion before PNG saving and hashing the payload.
    success = !image.isNull() && markCompletion("receiver-complete");
    success = success && image.save(outputFile);
    report << "read_ms=" << readMs << ' ' << "image " << image.width() << 'x' << image.height();
    if (!image.isNull()) {
      const QByteArrayView pixels(image.constBits(), image.sizeInBytes());
      report << " first-pixel=" << image.pixelColor(0, 0).name(QColor::HexArgb)
             << " rgba-sha256=" << QCryptographicHash::hash(pixels, QCryptographicHash::Sha256).toHex();
    }
    report << " success=" << static_cast<int>(success);
  }
  report << '\n';
  report.flush();

  return success;
}

class ReceiverWindow : public QQuickWindow {
 public:
  std::function<void()> receive;

 protected:
  void keyPressEvent(QKeyEvent* event) override {
    if (receive &&
        (event->key() == Qt::Key_Return || event->key() == Qt::Key_Space || event->matches(QKeySequence::Paste))) {
      receive();
      event->accept();
    } else {
      QQuickWindow::keyPressEvent(event);
    }
  }
  void mousePressEvent(QMouseEvent* event) override {
    if (receive && event->button() == Qt::LeftButton) {
      receive();
      event->accept();
    } else {
      QQuickWindow::mousePressEvent(event);
    }
  }
};
}  // namespace

// Separate receiver: exercise platform MIME transport, not in-process QMimeData.
int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  auto args = QCoreApplication::arguments();
  const bool interactive = args.removeOne(QStringLiteral("--interactive"));
  const bool oneShot = interactive && qEnvironmentVariableIsSet("VIEWER_RECEIVER_ONESHOT");
  if (args.size() != 3 || (args.at(1) != "image" && args.at(1) != "png" && args.at(1) != "text")) {
    QTextStream(stderr) << "Usage: clipboard-probe [--interactive] image|png|text output-file\n";
    return 2;
  }

  ReceiverWindow window;
  window.resize(640, 160);
  window.setTitle(QStringLiteral("Clipboard receiver — click or press Enter / Ctrl+V to read"));
  QObject::connect(&window, &QWindow::activeChanged, &app, [&] {
    if (window.isActive() && !markCompletion("receiver-ready")) {
      QCoreApplication::exit(3);
    }
  });
  window.show();
  window.requestActivate();

  const auto receive = [&] {
    const bool success = receiveClipboard(args.at(1), args.at(2));
    if (interactive) {
      if (oneShot) {
        QCoreApplication::exit(success ? 0 : 1);
      }
      window.setTitle(success ? QStringLiteral("Received — see terminal/output; click to read again")
                              : QStringLiteral("Receive failed — activate owner, copy, then click here again"));
    } else {
      QCoreApplication::exit(success ? 0 : 1);
    }
  };

  if (interactive) {
    window.receive = receive;
  } else {
    QTimer::singleShot(500, &app, receive);
  }
  return QGuiApplication::exec();
}
