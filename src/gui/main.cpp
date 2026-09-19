#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLibraryInfo>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTranslator>

namespace {

const QString kOrganizationName = QStringLiteral("RFP");
const QString kApplicationName = QStringLiteral("RFP-GUI");
const QString kApplicationVer = QStringLiteral("1.0.0");
const QString kDisplayName =
    QStringLiteral("R.F.P. - Resilient File Protector");
const QString kDefaultLanguage = QStringLiteral("en");
const QString kRussianLanguage = QStringLiteral("ru");

// Directories searched (in order) for application translation (.qm) files.
QStringList translationSearchPaths() {
  const QString appDir = QCoreApplication::applicationDirPath();

  QStringList paths;
  paths << QStringLiteral(":/translations") << QStringLiteral("translations")
        << QDir(appDir).absoluteFilePath(QStringLiteral("translations"))
        << QDir(appDir).absoluteFilePath(QStringLiteral("../translations"))
        << QDir(appDir).absoluteFilePath(
               QStringLiteral("../share/rfp-gui/translations"))
        << QStringLiteral("/usr/share/rfp-gui/translations")
        << QDir::home().absoluteFilePath(
               QStringLiteral(".local/share/rfp-gui/translations"));

  paths.removeDuplicates();
  return paths;
}

// Attempts to load and install the Qt base translation for `language`.
// Returns true when a translator was found and installed.
bool installQtTranslation(QApplication &app, QTranslator &translator,
                          const QString &language) {
  const QString name = QStringLiteral("qt_") + language;
  const QString qtPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);

  if (translator.load(name, qtPath) ||
      translator.load(name, QStringLiteral("/usr/share/qt6/translations"))) {
    app.installTranslator(&translator);
    return true;
  }
  return false;
}

// Attempts to load and install the application's own translation for
// `language` by searching every candidate directory.
bool installAppTranslation(QApplication &app, QTranslator &translator,
                           const QString &language) {
  const QString name = QStringLiteral("rfp-gui_") + language;
  for (const QString &path : translationSearchPaths()) {
    if (translator.load(name, path)) {
      app.installTranslator(&translator);
      return true;
    }
  }
  return false;
}

} // namespace

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  QCoreApplication::setOrganizationName(kOrganizationName);
  QCoreApplication::setApplicationName(kApplicationName);
  QCoreApplication::setApplicationVersion(kApplicationVer);
  QApplication::setApplicationDisplayName(kDisplayName);

  const QString language =
      QSettings()
          .value(QStringLiteral("Preview/language"), kDefaultLanguage)
          .toString();

  QTranslator qtTranslator;
  QTranslator appTranslator;

  if (language == kRussianLanguage) {
    if (!installQtTranslation(app, qtTranslator, language)) {
      qWarning().noquote() << QStringLiteral(
                                  "Qt translation for '%1' was not found; "
                                  "falling back to system locale.")
                                  .arg(language);
    }
    if (!installAppTranslation(app, appTranslator, language)) {
      qWarning().noquote() << QStringLiteral(
                                  "Application translation for '%1' was not "
                                  "found; using English.")
                                  .arg(language);
    }
  }

  MainWindow window;
  window.show();
  return app.exec();
}