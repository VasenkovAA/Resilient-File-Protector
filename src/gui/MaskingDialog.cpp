#include "MaskingDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace {

// Strip leading dots, asterisks, whitespace and a leading "*." so that
// user input like ".png", "*.png", " png " all normalise to "png".
QString normaliseExtension(QString ext) {
  ext = ext.trimmed();
  while (!ext.isEmpty() && (ext.startsWith(QLatin1Char('.')) ||
                            ext.startsWith(QLatin1Char('*')))) {
    ext.remove(0, 1);
  }
  return ext;
}

} // namespace

MaskingDialog::MaskingDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Access Masking"));
  resize(600, 400);

  auto *mainLayout = new QVBoxLayout(this);

  auto *form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  dirEdit_ = new QLineEdit(this);
  dirEdit_->setPlaceholderText(tr("Select a directory..."));
  auto *browseBtn = new QPushButton(tr("Browse..."), this);
  auto *dirRow = new QWidget(this);
  auto *dirLayout = new QHBoxLayout(dirRow);
  dirLayout->setContentsMargins(0, 0, 0, 0);
  dirLayout->addWidget(dirEdit_, 1);
  dirLayout->addWidget(browseBtn);
  form->addRow(tr("Directory:"), dirRow);

  extEdit_ = new QLineEdit(this);
  extEdit_->setPlaceholderText(tr("e.g. png"));
  extEdit_->setToolTip(tr("File extension without the leading dot. "
                          "Leave empty to match all files."));
  form->addRow(tr("Extension:"), extEdit_);

  countSpin_ = new QSpinBox(this);
  countSpin_->setRange(1, 100'000);
  countSpin_->setValue(10);
  form->addRow(tr("Files to touch:"), countSpin_);

  recursiveCheck_ = new QCheckBox(tr("Include subdirectories"), this);
  form->addRow(recursiveCheck_);

  mainLayout->addLayout(form);

  logEdit_ = new QPlainTextEdit(this);
  logEdit_->setReadOnly(true);
  logEdit_->setPlaceholderText(tr("Log will appear here"));
  mainLayout->addWidget(logEdit_, 1);

  auto *btnBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Close, this);
  runButton_ = btnBox->button(QDialogButtonBox::Ok);
  runButton_->setText(tr("Run"));
  connect(runButton_, &QPushButton::clicked, this, &MaskingDialog::onRun);
  connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::close);
  mainLayout->addWidget(btnBox);

  connect(browseBtn, &QPushButton::clicked, this, &MaskingDialog::onBrowse);
}

void MaskingDialog::setDefaultDirectory(const QString &dir) {
  if (dir.isEmpty() || !dirEdit_->text().isEmpty())
    return;
  dirEdit_->setText(dir);
}

void MaskingDialog::setExcludeFile(const QString &file) { excludeFile_ = file; }

void MaskingDialog::appendLog(const QString &text) {
  logEdit_->appendPlainText(text);
}

void MaskingDialog::setBusy(bool busy) {
  if (runButton_)
    runButton_->setEnabled(!busy);
}

void MaskingDialog::onBrowse() {
  const QString dir = QFileDialog::getExistingDirectory(
      this, tr("Select directory"), dirEdit_->text());
  if (!dir.isEmpty())
    dirEdit_->setText(dir);
}

void MaskingDialog::onRun() {
  const QString dir = dirEdit_->text().trimmed();
  if (dir.isEmpty()) {
    logEdit_->appendPlainText(tr("Directory is empty."));
    return;
  }
  if (!QFileInfo(dir).isDir()) {
    logEdit_->appendPlainText(tr("Directory does not exist: %1").arg(dir));
    return;
  }

  const QString ext = normaliseExtension(extEdit_->text());
  const int count = countSpin_->value();
  const bool recursive = recursiveCheck_->isChecked();

  logEdit_->clear();
  logEdit_->appendPlainText(tr("Starting masking..."));
  setBusy(true);

  emit maskingRequested(dir, ext, count, recursive, excludeFile_);
}