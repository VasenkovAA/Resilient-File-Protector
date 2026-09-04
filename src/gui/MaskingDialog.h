#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>

class MaskingDialog : public QDialog {
  Q_OBJECT
public:
  explicit MaskingDialog(QWidget *parent = nullptr);

  void setDefaultDirectory(const QString &dir);
  void setExcludeFile(const QString &file);
  void appendLog(const QString &text);

signals:
  void maskingRequested(const QString &dir, const QString &ext, int count,
                        bool recursive, const QString &exclude);

private slots:
  void onBrowse();
  void onRun();

private:
  QLineEdit *dirEdit_;
  QLineEdit *extEdit_;
  QSpinBox *countSpin_;
  QCheckBox *recursiveCheck_;
  QPlainTextEdit *logEdit_;
  QString excludeFile_;
};