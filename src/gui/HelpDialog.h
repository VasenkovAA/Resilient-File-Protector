#pragma once

#include <QDialog>
#include <QString>
#include <QTextBrowser>
#include <QTreeWidget>

class HelpDialog : public QDialog {
  Q_OBJECT
public:
  explicit HelpDialog(QWidget *parent = nullptr);

  void setLanguage(const QString &lang);
  [[nodiscard]] QString currentLanguage() const { return currentLang_; }

private slots:
  void onItemClicked(QTreeWidgetItem *item, int column);

private:
  void buildToc();
  void loadPage(const QString &pageName);

  QTreeWidget *tocWidget_ = nullptr;
  QTextBrowser *textBrowser_ = nullptr;
  QString currentLang_ = QStringLiteral("en");
};