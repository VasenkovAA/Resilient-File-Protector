#pragma once

#include <QDialog>
#include <QMap>
#include <QString>
#include <QTextBrowser>
#include <QTreeWidget>

class HelpDialog : public QDialog {
  Q_OBJECT
public:
  explicit HelpDialog(QWidget *parent = nullptr);

  void setLanguage(const QString &lang);
  QString currentLanguage() const { return currentLang_; }

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onItemClicked(QTreeWidgetItem *item, int column);

private:
  void buildToc();
  void loadPage(const QString &pageName);

  QTreeWidget *tocWidget_;
  QTextBrowser *textBrowser_;
  QString currentLang_ = "en";
};