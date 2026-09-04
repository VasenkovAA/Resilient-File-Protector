#include "HelpDialog.h"
#include <QFile>
#include <QSplitter>
#include <QVBoxLayout>

HelpDialog::HelpDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Help"));
  resize(900, 600);

  auto *splitter = new QSplitter(Qt::Horizontal, this);
  tocWidget_ = new QTreeWidget(this);
  tocWidget_->setHeaderHidden(true);
  tocWidget_->setMinimumWidth(200);
  splitter->addWidget(tocWidget_);

  textBrowser_ = new QTextBrowser(this);
  textBrowser_->setOpenExternalLinks(true);
  splitter->addWidget(textBrowser_);

  auto *layout = new QVBoxLayout(this);
  layout->addWidget(splitter);
  setLayout(layout);

  connect(tocWidget_, &QTreeWidget::itemClicked, this,
          &HelpDialog::onItemClicked);

  buildToc();
  loadPage("index");
}

void HelpDialog::setLanguage(const QString &lang) {
  if (lang == currentLang_)
    return;
  QString currentPage;
  if (tocWidget_->currentItem())
    currentPage = tocWidget_->currentItem()->data(0, Qt::UserRole).toString();
  currentLang_ = lang;
  buildToc();
  if (!currentPage.isEmpty()) {
    QTreeWidgetItemIterator it(tocWidget_);
    while (*it) {
      if ((*it)->data(0, Qt::UserRole).toString() == currentPage) {
        tocWidget_->setCurrentItem(*it);
        loadPage(currentPage);
        return;
      }
      ++it;
    }
  }
  loadPage("index");
}

void HelpDialog::buildToc() {
  tocWidget_->clear();
  struct PageInfo {
    QString display;
    QString file;
  };
  QList<PageInfo> pages = {{tr("Introduction"), "index"},
                           {tr("Architecture"), "architecture"},
                           {tr("Steganography parameters"), "steganography"},
                           {tr("Security recommendations"), "security"},
                           {tr("GUI guide"), "gui"},
                           {tr("Command line"), "cli"},
                           {tr("FAQ"), "faq"}};
  for (const auto &p : pages) {
    auto *item = new QTreeWidgetItem(tocWidget_);
    item->setText(0, p.display);
    item->setData(0, Qt::UserRole, p.file);
    tocWidget_->addTopLevelItem(item);
  }
  tocWidget_->expandAll();
}

void HelpDialog::loadPage(const QString &pageName) {
  QString path = QString(":/docs/%1/%2.md").arg(currentLang_, pageName);
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    textBrowser_->setHtml("<h1>Error</h1><p>Page not found.</p>");
    return;
  }
  QString content = QString::fromUtf8(file.readAll());
  file.close();
  textBrowser_->setMarkdown(content);
}

void HelpDialog::onItemClicked(QTreeWidgetItem *item, int /*column*/) {
  QString page = item->data(0, Qt::UserRole).toString();
  if (!page.isEmpty())
    loadPage(page);
}

void HelpDialog::closeEvent(QCloseEvent *event) { QDialog::closeEvent(event); }