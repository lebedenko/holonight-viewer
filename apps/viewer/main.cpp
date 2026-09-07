#include "image_document.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QTextStream>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>

#include <cstdlib>

Q_IMPORT_QML_PLUGIN(HolonightViewerPlugin)

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("holonight-viewer"));
  QGuiApplication::setApplicationDisplayName(QStringLiteral("HoloNight Viewer"));
  QGuiApplication::setApplicationVersion(QStringLiteral(VIEWER_VERSION));
  QGuiApplication::setOrganizationDomain(QStringLiteral("holonight.org"));
  QGuiApplication::setDesktopFileName(QStringLiteral("org.holonight.Viewer"));
  QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("org.holonight.Viewer")));
  QCommandLineParser parser;
  parser.setApplicationDescription(
      QCoreApplication::translate("main", "HoloNight Viewer — open one local static image."));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument(QStringLiteral("file"), QCoreApplication::translate("main", "Local image to open"),
                               QStringLiteral("[file]"));
  parser.process(app);
  const auto arguments = parser.positionalArguments();
  const auto url = arguments.isEmpty() ? QUrl{} : commandLineUrl(arguments.first());
  if (arguments.size() > 1 || (!arguments.isEmpty() && !ImageDocument::isLocalUrl(url))) {
    QTextStream(stderr) << QCoreApplication::translate(
                               "main", "Open exactly one local image file. Remote URLs are not supported.")
                        << '\n';
    return EXIT_FAILURE;
  }
  QGuiApplication::setQuitOnLastWindowClosed(false);
  ImageDocument document;
  QObject::connect(&app, &QGuiApplication::lastWindowClosed, &document, &ImageDocument::shutdown);
  QObject::connect(&document, &ImageDocument::shutdownFinished, &app, &QCoreApplication::quit);
  QObject::connect(&document, &ImageDocument::changed, &app, [&document] {
    if (document.state() == ImageDocument::Error) {
      QTextStream(stderr)
          << QCoreApplication::translate("main", "Error opening %1: %2").arg(document.fileName(), document.error())
          << '\n';
    }
  });
  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(EXIT_FAILURE); },
      Qt::QueuedConnection);
  engine.setInitialProperties({{QStringLiteral("document"), QVariant::fromValue(&document)}});
  engine.loadFromModule("HolonightViewer", "Main");
  if (engine.rootObjects().isEmpty()) {
    return EXIT_FAILURE;
  }
  if (!arguments.isEmpty()) {
    QTimer::singleShot(0, &document, [&document, url] { document.open({url}); });
  }
  return QGuiApplication::exec();
}
