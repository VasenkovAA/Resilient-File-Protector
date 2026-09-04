#include "SettingsDialog.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Settings"));
  auto *layout = new QVBoxLayout(this);

  auto *form = new QFormLayout;
  themeCombo_ = new QComboBox(this);
  themeCombo_->addItem(tr("Light"), "light");
  themeCombo_->addItem(tr("Dark"), "dark");
  form->addRow(tr("Theme:"), themeCombo_);

  langCombo_ = new QComboBox(this);
  langCombo_->addItem("English", "en");
  langCombo_->addItem("Русский", "ru");
  form->addRow(tr("Language:"), langCombo_);

  opacitySlider_ = new QSlider(Qt::Horizontal, this);
  opacitySlider_->setRange(0, 100);
  opacitySlider_->setValue(50);
  form->addRow(tr("Overlay opacity:"), opacitySlider_);

  showPreviewCheck_ = new QCheckBox(tr("Show preview"), this);
  showPreviewCheck_->setChecked(true);
  form->addRow(showPreviewCheck_);

  previewModeCombo_ = new QComboBox(this);
  previewModeCombo_->addItem(tr("Original"));
  previewModeCombo_->addItem(tr("Dispersion overlay"));
  previewModeCombo_->addItem(tr("Comparison"));
  form->addRow(tr("Preview mode:"), previewModeCombo_);

  highlightChangesCheck_ = new QCheckBox(tr("Highlight changes"), this);
  highlightChangesCheck_->setChecked(false);
  form->addRow(highlightChangesCheck_);

  writeHeaderCheck_ =
      new QCheckBox(tr("Write payload size header (4 bytes)"), this);
  writeHeaderCheck_->setChecked(true);
  form->addRow(writeHeaderCheck_);

  layout->addLayout(form);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this,
          &SettingsDialog::onAccepted);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
}

void SettingsDialog::onAccepted() {
  accept();
  emit settingsChanged();
}

bool SettingsDialog::isDarkTheme() const {
  return themeCombo_->currentData() == "dark";
}

QString SettingsDialog::language() const {
  return langCombo_->currentData().toString();
}

int SettingsDialog::overlayOpacity() const { return opacitySlider_->value(); }

bool SettingsDialog::showPreview() const {
  return showPreviewCheck_->isChecked();
}

int SettingsDialog::previewMode() const {
  return previewModeCombo_->currentIndex();
}

bool SettingsDialog::highlightChanges() const {
  return highlightChangesCheck_->isChecked();
}

bool SettingsDialog::writeHeader() const {
  return writeHeaderCheck_->isChecked();
}