#include "MaskingDialog.h"
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

MaskingDialog::MaskingDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Access Masking"));
  resize(600, 400);

  auto *mainLayout = new QVBoxLayout(this);

  auto *form = new QFormLayout;
  dirEdit_ = new QLineEdit(this);
  auto *browseBtn = new QPushButton(tr("Browse..."), this);
  auto *dirLayout = new QHBoxLayout;
  dirLayout->addWidget(dirEdit_);
  dirLayout->addWidget(browseBtn);
  form->addRow(tr("Directory:"), dirLayout);

  extEdit_ = new QLineEdit(this);
  extEdit_->setPlaceholderText(tr("e.g. png"));
  form->addRow(tr("Extension:"), extEdit_);

  countSpin_ = new QSpinBox(this);
  countSpin_->setRange(1, 1000000);
  countSpin_->setValue(10);
  form->addRow(tr("Files to touch:"), countSpin_);

  recursiveCheck_ = new QCheckBox(tr("Include subdirectories"), this);
  form->addRow(recursiveCheck_);

  mainLayout->addLayout(form);

  logEdit_ = new QPlainTextEdit(this);
  logEdit_->setReadOnly(true);
  logEdit_->setPlaceholderText(tr("Log will appear here"));
  mainLayout->addWidget(logEdit_);

  auto *btnBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  auto *runBtn = btnBox->button(QDialogButtonBox::Ok);
  runBtn->setText(tr("Run"));
  connect(runBtn, &QPushButton::clicked, this, &MaskingDialog::onRun);
  connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  mainLayout->addWidget(btnBox);

  connect(browseBtn, &QPushButton::clicked, this, &MaskingDialog::onBrowse);
}

void MaskingDialog::setDefaultDirectory(const QString &dir) {
  if (!dir.isEmpty() && dirEdit_->text().isEmpty())
    dirEdit_->setText(dir);
}

void MaskingDialog::setExcludeFile(const QString &file) { excludeFile_ = file; }

void MaskingDialog::onBrowse() {
  QString dir = QFileDialog::getExistingDirectory(this, tr("Select directory"),
                                                  dirEdit_->text());
  if (!dir.isEmpty())
    dirEdit_->setText(dir);
}

void MaskingDialog::onRun() {
  if (dirEdit_->text().isEmpty()) {
    logEdit_->appendPlainText(tr("Directory is empty."));
    return;
  }
  QString ext = extEdit_->text().trimmed();
  int count = countSpin_->value();
  bool recursive = recursiveCheck_->isChecked();
  logEdit_->clear();
  logEdit_->appendPlainText(tr("Starting masking..."));
  emit maskingRequested(dirEdit_->text(), ext, count, recursive, excludeFile_);

  accept();
}

void MaskingDialog::appendLog(const QString &text) {
  logEdit_->appendPlainText(text);
}