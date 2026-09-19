#include "HelpDialog.h"

#include <QFile>
#include <QSplitter>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace {

const QStringList kSupportedLanguages = {QStringLiteral("en"),
                                         QStringLiteral("ru")};

QString normaliseLanguage(const QString &lang) {
  return kSupportedLanguages.contains(lang) ? lang : QStringLiteral("en");
}

} // namespace

HelpDialog::HelpDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Help"));
  setMinimumSize(700, 480);
  resize(900, 600);

  auto *splitter = new QSplitter(Qt::Horizontal, this);

  tocWidget_ = new QTreeWidget(this);
  tocWidget_->setHeaderHidden(true);
  tocWidget_->setRootIsDecorated(false);
  tocWidget_->setMinimumWidth(200);
  splitter->addWidget(tocWidget_);

  textBrowser_ = new QTextBrowser(this);
  textBrowser_->setOpenExternalLinks(true);
  splitter->addWidget(textBrowser_);

  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(splitter);

  connect(tocWidget_, &QTreeWidget::itemClicked, this,
          &HelpDialog::onItemClicked);

  buildToc();
  loadPage(QStringLiteral("index"));
}

void HelpDialog::setLanguage(const QString &lang) {
  const QString normalised = normaliseLanguage(lang);
  if (normalised == currentLang_)
    return;

  QString currentPage;
  if (auto *item = tocWidget_->currentItem())
    currentPage = item->data(0, Qt::UserRole).toString();

  currentLang_ = normalised;
  buildToc();

  if (currentPage.isEmpty()) {
    loadPage(QStringLiteral("index"));
    return;
  }

  QTreeWidgetItemIterator it(tocWidget_);
  while (*it) {
    if ((*it)->data(0, Qt::UserRole).toString() == currentPage) {
      tocWidget_->setCurrentItem(*it);
      loadPage(currentPage);
      return;
    }
    ++it;
  }
  loadPage(QStringLiteral("index"));
}

void HelpDialog::buildToc() {
  tocWidget_->clear();

  struct PageInfo {
    QString display;
    QString file;
  };

  const QList<PageInfo> pages = {
      {tr("Introduction"), QStringLiteral("index")},
      {tr("Architecture"), QStringLiteral("architecture")},
      {tr("Steganography parameters"), QStringLiteral("steganography")},
      {tr("Security recommendations"), QStringLiteral("security")},
      {tr("GUI guide"), QStringLiteral("gui")},
      {tr("Command line"), QStringLiteral("cli")},
      {tr("FAQ"), QStringLiteral("faq")},
  };

  for (const auto &p : pages) {
    auto *item = new QTreeWidgetItem(tocWidget_);
    item->setText(0, p.display);
    item->setData(0, Qt::UserRole, p.file);
  }
  tocWidget_->expandAll();
}

void HelpDialog::loadPage(const QString &pageName) {
  const QString path =
      QStringLiteral(":/docs/%1/%2.md").arg(currentLang_, pageName);

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    textBrowser_->setHtml(
        QStringLiteral("<h1>%1</h1><p>%2</p>")
            .arg(tr("Page not found"),
                 tr("Documentation page '%1' is not bundled with this build.")
                     .arg(pageName.toHtmlEscaped())));
    return;
  }

  const QString content = QString::fromUtf8(file.readAll());
  textBrowser_->setMarkdown(content);
}

void HelpDialog::onItemClicked(QTreeWidgetItem *item, int /*column*/) {
  if (!item)
    return;
  const QString page = item->data(0, Qt::UserRole).toString();
  if (!page.isEmpty())
    loadPage(page);
}