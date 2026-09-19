#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QSlider>

/// Preview mode selection shared between SettingsDialog and MainWindow.
enum class PreviewMode {
  Original = 0,
  DispersionOverlay = 1,
  Comparison = 2,
};

class SettingsDialog : public QDialog {
  Q_OBJECT
public:
  explicit SettingsDialog(QWidget *parent = nullptr);

  [[nodiscard]] bool isDarkTheme() const;
  [[nodiscard]] QString language() const;
  [[nodiscard]] int overlayOpacity() const;
  [[nodiscard]] bool showPreview() const;
  [[nodiscard]] PreviewMode previewMode() const;
  [[nodiscard]] bool highlightChanges() const;
  [[nodiscard]] bool writeHeader() const;

  void setDarkTheme(bool dark);
  void setLanguage(const QString &lang);
  void setOverlayOpacity(int value);
  void setShowPreview(bool on);
  void setPreviewMode(PreviewMode mode);
  void setHighlightChanges(bool on);
  void setWriteHeader(bool on);

signals:
  void settingsChanged();

private slots:
  void onAccepted();

private:
  QComboBox *themeCombo_ = nullptr;
  QComboBox *langCombo_ = nullptr;
  QSlider *opacitySlider_ = nullptr;
  QLabel *opacityValueLabel_ = nullptr;
  QCheckBox *showPreviewCheck_ = nullptr;
  QComboBox *previewModeCombo_ = nullptr;
  QCheckBox *highlightChangesCheck_ = nullptr;
  QCheckBox *writeHeaderCheck_ = nullptr;
};