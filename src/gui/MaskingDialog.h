#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>

/// Non-modal dialog that collects a directory, extension and count for the
/// "access masking" utility. Emits maskingRequested() when the user presses
/// Run; the dialog stays open until the caller explicitly closes it, so the
/// log remains visible.
class MaskingDialog : public QDialog {
  Q_OBJECT
public:
  explicit MaskingDialog(QWidget *parent = nullptr);

  void setDefaultDirectory(const QString &dir);
  void setExcludeFile(const QString &file);

  void appendLog(const QString &text);

  /// Enables or disables the Run button and shows a "working" indicator.
  /// Caller must call setBusy(true) before starting and setBusy(false)
  /// after the operation finishes.
  void setBusy(bool busy);

signals:
  void maskingRequested(const QString &dir, const QString &ext, int count,
                        bool recursive, const QString &exclude);

private slots:
  void onBrowse();
  void onRun();

private:
  QLineEdit *dirEdit_ = nullptr;
  QLineEdit *extEdit_ = nullptr;
  QSpinBox *countSpin_ = nullptr;
  QCheckBox *recursiveCheck_ = nullptr;
  QPlainTextEdit *logEdit_ = nullptr;
  QPushButton *runButton_ = nullptr;
  QString excludeFile_;
};