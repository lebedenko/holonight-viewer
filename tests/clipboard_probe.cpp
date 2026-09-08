#include <QClipboard>
#include <QCryptographicHash>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QQuickWindow>
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
  if (args.size() != 3 || (args.at(1) != "image" && args.at(1) != "png" && args.at(1) != "text")) {
    QTextStream(stderr) << "Usage: clipboard-probe [--interactive] image|png|text output-file\n";
    return 2;
  }
  ReceiverWindow window;
  window.resize(640, 160);
  window.setTitle(QStringLiteral("Clipboard receiver — click or press Enter / Ctrl+V to read"));
  window.show();
  window.requestActivate();
  const auto receive = [&] {
    auto* clipboard = QGuiApplication::clipboard();
    bool success = false;
    QTextStream report(stdout);
    if (args.at(1) != "text") {
      const auto received = readImage(clipboard, args.at(1));
      const auto image = received.convertToFormat(QImage::Format_RGBA8888);
      success = !image.isNull() && image.save(args.at(2));
      report << "image " << image.width() << 'x' << image.height();
      if (!image.isNull()) {
        const QByteArrayView pixels(image.constBits(), image.sizeInBytes());
        report << " first-pixel=" << image.pixelColor(0, 0).name(QColor::HexArgb)
               << " rgba-sha256=" << QCryptographicHash::hash(pixels, QCryptographicHash::Sha256).toHex();
      }
    } else {
      QFile file(args.at(2));
      const auto bytes = clipboard->text().toUtf8();
      success = !bytes.isEmpty() && file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
      report << "text=" << QString::fromUtf8(bytes);
    }
    report << " success=" << success << '\n';
    report.flush();
    if (interactive) {
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
