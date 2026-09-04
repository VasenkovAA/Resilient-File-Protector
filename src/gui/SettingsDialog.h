#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QSlider>

class SettingsDialog : public QDialog {
  Q_OBJECT
public:
  explicit SettingsDialog(QWidget *parent = nullptr);

  bool isDarkTheme() const;
  QString language() const;
  int overlayOpacity() const;
  bool showPreview() const;
  int previewMode() const;
  bool highlightChanges() const;
  bool writeHeader() const;

signals:
  void settingsChanged();

private slots:
  void onAccepted();

private:
  QComboBox *themeCombo_;
  QComboBox *langCombo_;
  QSlider *opacitySlider_;
  QCheckBox *showPreviewCheck_;
  QComboBox *previewModeCombo_;
  QCheckBox *highlightChangesCheck_;
  QCheckBox *writeHeaderCheck_;
};