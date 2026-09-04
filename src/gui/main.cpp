#include "MainWindow.h"
#include <QApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QTranslator>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);


  QSettings settings("RFP", "RFP-GUI");
  QString lang = settings.value("Preview/language", "en").toString();


  QTranslator qtTranslator;
  QTranslator appTranslator;

  if (lang == "ru") {

    if (qtTranslator.load("qt_ru",
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
      app.installTranslator(&qtTranslator);
    } else if (qtTranslator.load("qt_ru", "/usr/share/qt6/translations")) {
      app.installTranslator(&qtTranslator);
    }


    QStringList searchPaths;
    searchPaths
        << ":/translations"
        << "./translations"
        << QDir(QCoreApplication::applicationDirPath())
               .absoluteFilePath("translations")
        << QDir(QCoreApplication::applicationDirPath())
               .absoluteFilePath("../translations")
        << QDir::home().absoluteFilePath(".local/share/rfp-gui/translations");

    bool loaded = false;
    for (const QString &path : searchPaths) {
      if (appTranslator.load("rfp-gui_ru", path)) {
        app.installTranslator(&appTranslator);
        loaded = true;
        break;
      }
    }

    if (!loaded) {

      if (appTranslator.load("rfp-gui_ru", "/usr/share/rfp-gui/translations")) {
        app.installTranslator(&appTranslator);
        loaded = true;
      }
    }

    if (!loaded) {
      QMessageBox::warning(nullptr, "R.F.P.",
                           "Russian translation not found. Using English.");
    }
  }

  MainWindow window;
  window.show();
  return app.exec();
}