#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>

#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoParams.h"

#include <optional>

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);

private slots:
  void browseInputImage();
  void browseOutputImage();
  void embedText();
  void extractText();

private:
  void updateCapacityInfo();
  [[nodiscard]] rfp::stego::StegoParams collectParams() const;
  void setStatus(const QString &text);

  QLineEdit *inputImageEdit_ = nullptr;
  QLineEdit *outputImageEdit_ = nullptr;
  QPlainTextEdit *payloadEdit_ = nullptr;
  QSpinBox *payloadSizeSpin_ = nullptr;

  // Основные параметры
  QSpinBox *bitsPerChannelSpin_ = nullptr;
  QSpinBox *seedSpin_ = nullptr;
  QCheckBox *redCheck_ = nullptr;
  QCheckBox *greenCheck_ = nullptr;
  QCheckBox *blueCheck_ = nullptr;
  QCheckBox *alphaCheck_ = nullptr;

  // Новые параметры для умного выбора
  QComboBox *modeCombo_ = nullptr;
  QComboBox *windowSizeCombo_ = nullptr;
  QComboBox *metricCombo_ = nullptr;
  QLineEdit *thresholdEdit_ = nullptr;
  QPushButton *autoThresholdButton_ = nullptr;
  QCheckBox *shuffleAfterSortCheck_ = nullptr;

  QLabel *statusLabel_ = nullptr;
  QLabel *capacityLabel_ = nullptr;

  std::optional<rfp::stego::ImageBuffer> currentImage_;
};