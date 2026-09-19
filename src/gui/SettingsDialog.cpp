#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("Settings"));
  setMinimumWidth(400);

  auto *layout = new QVBoxLayout(this);

  auto *form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  themeCombo_ = new QComboBox(this);
  themeCombo_->addItem(tr("Light"), "light");
  themeCombo_->addItem(tr("Dark"), "dark");
  form->addRow(tr("Theme:"), themeCombo_);

  langCombo_ = new QComboBox(this);
  langCombo_->addItem("English", "en");
  langCombo_->addItem(QString::fromUtf8("Русский"), "ru");
  langCombo_->setToolTip(
      tr("Language change requires an application restart."));
  form->addRow(tr("Language:"), langCombo_);

  opacitySlider_ = new QSlider(Qt::Horizontal, this);
  opacitySlider_->setRange(0, 100);
  opacitySlider_->setValue(50);
  opacitySlider_->setToolTip(
      tr("Opacity of the dispersion overlay in the preview."));

  opacityValueLabel_ = new QLabel(QStringLiteral("50%"), this);
  opacityValueLabel_->setMinimumWidth(40);
  opacityValueLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  auto *opacityRow = new QWidget(this);
  auto *opacityLayout = new QHBoxLayout(opacityRow);
  opacityLayout->setContentsMargins(0, 0, 0, 0);
  opacityLayout->addWidget(opacitySlider_, 1);
  opacityLayout->addWidget(opacityValueLabel_);
  form->addRow(tr("Overlay opacity:"), opacityRow);

  showPreviewCheck_ = new QCheckBox(tr("Show preview"), this);
  showPreviewCheck_->setChecked(true);
  form->addRow(showPreviewCheck_);

  previewModeCombo_ = new QComboBox(this);
  previewModeCombo_->addItem(tr("Original"),
                             static_cast<int>(PreviewMode::Original));
  previewModeCombo_->addItem(tr("Dispersion overlay"),
                             static_cast<int>(PreviewMode::DispersionOverlay));
  previewModeCombo_->addItem(tr("Comparison"),
                             static_cast<int>(PreviewMode::Comparison));
  form->addRow(tr("Preview mode:"), previewModeCombo_);

  highlightChangesCheck_ = new QCheckBox(tr("Highlight changes"), this);
  highlightChangesCheck_->setChecked(false);
  form->addRow(highlightChangesCheck_);

  writeHeaderCheck_ =
      new QCheckBox(tr("Write payload size header (4 bytes)"), this);
  writeHeaderCheck_->setChecked(true);
  writeHeaderCheck_->setToolTip(
      tr("If enabled, embedding writes a 4-byte length prefix that lets "
         "extraction read the payload size automatically. The setting must "
         "match between embed and extract."));
  form->addRow(writeHeaderCheck_);

  layout->addLayout(form);
  layout->addStretch();

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this,
          &SettingsDialog::onAccepted);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);

  connect(opacitySlider_, &QSlider::valueChanged, this, [this](int value) {
    opacityValueLabel_->setText(QStringLiteral("%1%").arg(value));
  });
}

void SettingsDialog::onAccepted() {
  emit settingsChanged();
  accept();
}

bool SettingsDialog::isDarkTheme() const {
  return themeCombo_->currentData().toString() == QLatin1String("dark");
}

QString SettingsDialog::language() const {
  return langCombo_->currentData().toString();
}

int SettingsDialog::overlayOpacity() const { return opacitySlider_->value(); }

bool SettingsDialog::showPreview() const {
  return showPreviewCheck_->isChecked();
}

PreviewMode SettingsDialog::previewMode() const {
  return static_cast<PreviewMode>(previewModeCombo_->currentData().toInt());
}

bool SettingsDialog::highlightChanges() const {
  return highlightChangesCheck_->isChecked();
}

bool SettingsDialog::writeHeader() const {
  return writeHeaderCheck_->isChecked();
}

void SettingsDialog::setDarkTheme(bool dark) {
  const int idx = themeCombo_->findData(dark ? QStringLiteral("dark")
                                             : QStringLiteral("light"));
  if (idx >= 0)
    themeCombo_->setCurrentIndex(idx);
}

void SettingsDialog::setLanguage(const QString &lang) {
  const int idx = langCombo_->findData(lang);
  if (idx >= 0)
    langCombo_->setCurrentIndex(idx);
}

void SettingsDialog::setOverlayOpacity(int value) {
  opacitySlider_->setValue(value);
}

void SettingsDialog::setShowPreview(bool on) {
  showPreviewCheck_->setChecked(on);
}

void SettingsDialog::setPreviewMode(PreviewMode mode) {
  const int idx = previewModeCombo_->findData(static_cast<int>(mode));
  if (idx >= 0)
    previewModeCombo_->setCurrentIndex(idx);
}

void SettingsDialog::setHighlightChanges(bool on) {
  highlightChangesCheck_->setChecked(on);
}

void SettingsDialog::setWriteHeader(bool on) {
  writeHeaderCheck_->setChecked(on);
}