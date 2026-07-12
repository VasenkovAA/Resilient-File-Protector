#include "MainWindow.h"

#include "QtImageAdapter.h"
#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Crc32.h"
#include "rfp/stego/Capacity.h"
#include "rfp/stego/StegoDecoder.h"
#include "rfp/stego/StegoDispersion.h"
#include "rfp/stego/StegoEncoder.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <limits>
#include <span>
#include <vector>

namespace {

QString crcToText(std::uint32_t crc) {
  return QStringLiteral("%1").arg(crc, 8, 16, QLatin1Char('0')).toUpper();
}

// Вычисляет автоматический порог как 70-й процентиль всех дисперсий по всем
// пикселям и каналам
double computeAutoThreshold(const rfp::stego::ImageBuffer &image,
                            const rfp::stego::StegoParams &params) {
  if (!image.isValid())
    return 0.0;
  rfp::stego::DispersionCalculator calc(image, params);
  std::vector<double> values;
  const auto pixelCount = static_cast<std::size_t>(image.width) *
                          static_cast<std::size_t>(image.height);
  values.reserve(pixelCount * image.channels);
  for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
    for (std::uint8_t channel = 0; channel < image.channels; ++channel) {
      values.push_back(calc.getDispersion(pixel, channel));
    }
  }
  if (values.empty())
    return 0.0;
  std::sort(values.begin(), values.end());
  const std::size_t idx =
      static_cast<std::size_t>(static_cast<double>(values.size()) * 0.7);
  return values[idx];
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("R.F.P. - Resilient File Protector"));
  resize(900, 700);

  auto *central = new QWidget(this);
  auto *rootLayout = new QVBoxLayout(central);

  // --- Группа "Images" ---
  auto *filesGroup = new QGroupBox(QStringLiteral("Images"), central);
  auto *filesLayout = new QGridLayout(filesGroup);

  inputImageEdit_ = new QLineEdit(filesGroup);
  outputImageEdit_ = new QLineEdit(filesGroup);
  auto *browseInputButton =
      new QPushButton(QStringLiteral("Browse..."), filesGroup);
  auto *browseOutputButton =
      new QPushButton(QStringLiteral("Browse..."), filesGroup);

  filesLayout->addWidget(new QLabel(QStringLiteral("Input image:"), filesGroup),
                         0, 0);
  filesLayout->addWidget(inputImageEdit_, 0, 1);
  filesLayout->addWidget(browseInputButton, 0, 2);
  filesLayout->addWidget(
      new QLabel(QStringLiteral("Output image:"), filesGroup), 1, 0);
  filesLayout->addWidget(outputImageEdit_, 1, 1);
  filesLayout->addWidget(browseOutputButton, 1, 2);

  connect(browseInputButton, &QPushButton::clicked, this,
          &MainWindow::browseInputImage);
  connect(browseOutputButton, &QPushButton::clicked, this,
          &MainWindow::browseOutputImage);

  // --- Группа "Steganography parameters" ---
  auto *paramsGroup =
      new QGroupBox(QStringLiteral("Steganography parameters"), central);
  auto *paramsLayout = new QFormLayout(paramsGroup);

  // Основные параметры
  bitsPerChannelSpin_ = new QSpinBox(paramsGroup);
  bitsPerChannelSpin_->setRange(1, 4);
  bitsPerChannelSpin_->setValue(1);

  seedSpin_ = new QSpinBox(paramsGroup);
  seedSpin_->setRange(0, std::numeric_limits<int>::max());
  seedSpin_->setValue(0);

  payloadSizeSpin_ = new QSpinBox(paramsGroup);
  payloadSizeSpin_->setRange(1, 100000000);
  payloadSizeSpin_->setValue(1);

  auto *channelsWidget = new QWidget(paramsGroup);
  auto *channelsLayout = new QHBoxLayout(channelsWidget);
  channelsLayout->setContentsMargins(0, 0, 0, 0);

  redCheck_ = new QCheckBox(QStringLiteral("R"), channelsWidget);
  greenCheck_ = new QCheckBox(QStringLiteral("G"), channelsWidget);
  blueCheck_ = new QCheckBox(QStringLiteral("B"), channelsWidget);
  alphaCheck_ = new QCheckBox(QStringLiteral("A"), channelsWidget);
  redCheck_->setChecked(true);
  greenCheck_->setChecked(true);
  blueCheck_->setChecked(true);
  alphaCheck_->setChecked(false);

  channelsLayout->addWidget(redCheck_);
  channelsLayout->addWidget(greenCheck_);
  channelsLayout->addWidget(blueCheck_);
  channelsLayout->addWidget(alphaCheck_);
  channelsLayout->addStretch();

  paramsLayout->addRow(QStringLiteral("Bits per channel:"),
                       bitsPerChannelSpin_);
  paramsLayout->addRow(QStringLiteral("Seed (0 = sequential):"), seedSpin_);
  paramsLayout->addRow(QStringLiteral("Payload size for extraction (bytes):"),
                       payloadSizeSpin_);
  paramsLayout->addRow(QStringLiteral("Channels:"), channelsWidget);

  // Новые параметры умного выбора
  auto *smartGroup =
      new QGroupBox(QStringLiteral("Smart selection"), paramsGroup);
  auto *smartLayout = new QFormLayout(smartGroup);

  modeCombo_ = new QComboBox(smartGroup);
  modeCombo_->addItem(QStringLiteral("Uniform"),
                      static_cast<int>(rfp::stego::SlotSelectionMode::Uniform));
  modeCombo_->addItem(QStringLiteral("Smart (dispersion)"),
                      static_cast<int>(rfp::stego::SlotSelectionMode::Smart));
  modeCombo_->setCurrentIndex(0);

  windowSizeCombo_ = new QComboBox(smartGroup);
  for (int s : {3, 5, 7, 9, 11, 13}) {
    windowSizeCombo_->addItem(QString::number(s), s);
  }
  windowSizeCombo_->setCurrentIndex(0);

  metricCombo_ = new QComboBox(smartGroup);
  metricCombo_->addItem(
      QStringLiteral("Luminance"),
      static_cast<int>(rfp::stego::DispersionMetric::Luminance));
  metricCombo_->addItem(
      QStringLiteral("Per-channel"),
      static_cast<int>(rfp::stego::DispersionMetric::PerChannel));
  metricCombo_->addItem(QStringLiteral("Sum"),
                        static_cast<int>(rfp::stego::DispersionMetric::Sum));

  thresholdEdit_ = new QLineEdit(smartGroup);
  thresholdEdit_->setText(QStringLiteral("0.0"));
  thresholdEdit_->setValidator(
      new QDoubleValidator(0.0, 100000.0, 2, thresholdEdit_));

  autoThresholdButton_ = new QPushButton(QStringLiteral("Auto"), smartGroup);
  auto *thresholdWidget = new QWidget(smartGroup);
  auto *thresholdLayout = new QHBoxLayout(thresholdWidget);
  thresholdLayout->setContentsMargins(0, 0, 0, 0);
  thresholdLayout->addWidget(thresholdEdit_, 1);
  thresholdLayout->addWidget(autoThresholdButton_);

  shuffleAfterSortCheck_ =
      new QCheckBox(QStringLiteral("Apply shuffle after sorting"), smartGroup);
  shuffleAfterSortCheck_->setChecked(true);

  smartLayout->addRow(QStringLiteral("Mode:"), modeCombo_);
  smartLayout->addRow(QStringLiteral("Window size:"), windowSizeCombo_);
  smartLayout->addRow(QStringLiteral("Dispersion metric:"), metricCombo_);
  smartLayout->addRow(QStringLiteral("Threshold:"), thresholdWidget);
  smartLayout->addRow(QStringLiteral(""), shuffleAfterSortCheck_);

  paramsLayout->addRow(smartGroup);

  // --- Группа ёмкости ---
  capacityLabel_ = new QLabel(QStringLiteral("Capacity: not loaded"), central);

  // --- Группа "Text payload" ---
  auto *payloadGroup = new QGroupBox(QStringLiteral("Text payload"), central);
  auto *payloadLayout = new QVBoxLayout(payloadGroup);
  payloadEdit_ = new QPlainTextEdit(payloadGroup);
  payloadEdit_->setPlaceholderText(
      QStringLiteral("Text to hide or extracted text will appear here"));
  payloadLayout->addWidget(payloadEdit_);
  payloadLayout->addWidget(capacityLabel_);

  // --- Кнопки действий ---
  auto *actionsLayout = new QHBoxLayout;
  auto *embedButton = new QPushButton(QStringLiteral("Embed text"), central);
  auto *extractButton =
      new QPushButton(QStringLiteral("Extract text"), central);
  actionsLayout->addWidget(embedButton);
  actionsLayout->addWidget(extractButton);
  actionsLayout->addStretch();

  statusLabel_ = new QLabel(QStringLiteral("Ready"), central);
  statusLabel_->setWordWrap(true);

  rootLayout->addWidget(filesGroup);
  rootLayout->addWidget(paramsGroup);
  rootLayout->addWidget(payloadGroup, 1);
  rootLayout->addLayout(actionsLayout);
  rootLayout->addWidget(statusLabel_);

  setCentralWidget(central);

  // Сигналы для обновления ёмкости
  connect(bitsPerChannelSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &MainWindow::updateCapacityInfo);
  connect(seedSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &MainWindow::updateCapacityInfo);
  connect(redCheck_, &QCheckBox::toggled, this,
          &MainWindow::updateCapacityInfo);
  connect(greenCheck_, &QCheckBox::toggled, this,
          &MainWindow::updateCapacityInfo);
  connect(blueCheck_, &QCheckBox::toggled, this,
          &MainWindow::updateCapacityInfo);
  connect(alphaCheck_, &QCheckBox::toggled, this,
          &MainWindow::updateCapacityInfo);
  connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MainWindow::updateCapacityInfo);
  connect(windowSizeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &MainWindow::updateCapacityInfo);
  connect(metricCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &MainWindow::updateCapacityInfo);
  connect(thresholdEdit_, &QLineEdit::textChanged, this,
          &MainWindow::updateCapacityInfo);
  connect(shuffleAfterSortCheck_, &QCheckBox::toggled, this,
          &MainWindow::updateCapacityInfo);

  connect(autoThresholdButton_, &QPushButton::clicked, this, [this]() {
    if (!currentImage_.has_value()) {
      QMessageBox::warning(this, QStringLiteral("R.F.P."),
                           QStringLiteral("Load an input image first."));
      return;
    }
    auto params = collectParams();
    const double autoThr = computeAutoThreshold(currentImage_.value(), params);
    thresholdEdit_->setText(QString::number(autoThr, 'f', 2));
  });

  connect(embedButton, &QPushButton::clicked, this, &MainWindow::embedText);
  connect(extractButton, &QPushButton::clicked, this, &MainWindow::extractText);

  updateCapacityInfo();
}

void MainWindow::browseInputImage() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Select input image"), QString(),
      QStringLiteral("Images (*.png *.bmp *.tif *.tiff);;All files (*.*)"));

  if (!path.isEmpty()) {
    inputImageEdit_->setText(path);
    auto result = rfp::gui::loadImageBuffer(path);
    if (result) {
      currentImage_ = result.value();
    } else {
      currentImage_.reset();
      QMessageBox::warning(this, QStringLiteral("R.F.P."),
                           QString::fromStdString(result.error().message));
    }
    updateCapacityInfo();
  }
}

void MainWindow::browseOutputImage() {
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Select output image"), QString(),
      QStringLiteral("PNG image (*.png);;BMP image (*.bmp);;All files (*.*)"));

  if (!path.isEmpty()) {
    outputImageEdit_->setText(path);
  }
}

rfp::stego::StegoParams MainWindow::collectParams() const {
  rfp::stego::StegoParams params;
  params.bitsPerChannel =
      static_cast<std::uint8_t>(bitsPerChannelSpin_->value());
  params.seed = static_cast<std::uint32_t>(seedSpin_->value());
  params.useRedChannel = redCheck_->isChecked();
  params.useGreenChannel = greenCheck_->isChecked();
  params.useBlueChannel = blueCheck_->isChecked();
  params.useAlphaChannel = alphaCheck_->isChecked();

  params.mode = static_cast<rfp::stego::SlotSelectionMode>(
      modeCombo_->currentData().toInt());
  params.windowSize = windowSizeCombo_->currentData().toInt();
  params.metric = static_cast<rfp::stego::DispersionMetric>(
      metricCombo_->currentData().toInt());
  params.dispersionThreshold = thresholdEdit_->text().toDouble();
  params.applyShuffleAfterSort = shuffleAfterSortCheck_->isChecked();

  return params;
}

void MainWindow::setStatus(const QString &text) { statusLabel_->setText(text); }

void MainWindow::updateCapacityInfo() {
  if (!currentImage_.has_value()) {
    capacityLabel_->setText(QStringLiteral("Capacity: not loaded"));
    return;
  }

  const auto params = collectParams();
  const auto capBytes =
      rfp::stego::capacityBytes(currentImage_.value(), params);
  const auto capBits = rfp::stego::capacityBits(currentImage_.value(), params);

  QString info =
      QStringLiteral("Capacity: %1 bytes (%2 bits)").arg(capBytes).arg(capBits);
  if (params.mode == rfp::stego::SlotSelectionMode::Smart) {
    rfp::stego::StegoParams uniformParams = params;
    uniformParams.mode = rfp::stego::SlotSelectionMode::Uniform;
    const auto totalBytes =
        rfp::stego::capacityBytes(currentImage_.value(), uniformParams);
    if (totalBytes > 0) {
      const double percent =
          (static_cast<double>(capBytes) / totalBytes) * 100.0;
      info +=
          QStringLiteral(" (using %1% of available)").arg(percent, 0, 'f', 1);
    }
  }
  capacityLabel_->setText(info);
}

void MainWindow::embedText() {
  if (inputImageEdit_->text().isEmpty() || outputImageEdit_->text().isEmpty()) {
    QMessageBox::warning(
        this, QStringLiteral("R.F.P."),
        QStringLiteral("Specify input and output image paths."));
    return;
  }

  const QString text = payloadEdit_->toPlainText();
  const QByteArray utf8 = text.toUtf8();
  if (utf8.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("R.F.P."),
                         QStringLiteral("Text payload is empty."));
    return;
  }

  auto imageResult = rfp::gui::loadImageBuffer(inputImageEdit_->text());
  if (!imageResult) {
    QMessageBox::warning(this, QStringLiteral("R.F.P."),
                         QString::fromStdString(imageResult.error().message));
    return;
  }
  currentImage_ = imageResult.value();

  const auto params = collectParams();
  const auto capacity = rfp::stego::capacityBytes(imageResult.value(), params);
  if (static_cast<std::size_t>(utf8.size()) > capacity) {
    QMessageBox::warning(
        this, QStringLiteral("R.F.P."),
        QStringLiteral(
            "Payload is too large. Capacity: %1 bytes, payload: %2 bytes.")
            .arg(capacity)
            .arg(utf8.size()));
    return;
  }

  const auto payload = rfp::core::ByteBuffer(utf8.begin(), utf8.end());
  auto encodedResult = rfp::stego::StegoEncoder::embedBytes(imageResult.value(),
                                                            payload, params);
  if (!encodedResult) {
    QMessageBox::warning(this, QStringLiteral("R.F.P."),
                         QString::fromStdString(encodedResult.error().message));
    return;
  }

  auto saveResult = rfp::gui::saveImageBuffer(encodedResult.value(),
                                              outputImageEdit_->text());
  if (!saveResult) {
    QMessageBox::warning(this, QStringLiteral("R.F.P."),
                         QString::fromStdString(saveResult.error().message));
    return;
  }

  const auto payloadSize = utf8.size();
  if (payloadSize > static_cast<std::size_t>(payloadSizeSpin_->maximum())) {
    payloadSizeSpin_->setValue(payloadSizeSpin_->maximum());
  } else {
    payloadSizeSpin_->setValue(static_cast<int>(payloadSize));
  }

  const auto crc = rfp::core::crc32(
      std::span<const rfp::core::Byte>(payload.data(), payload.size()));

  QString paramsStr = QStringLiteral("Mode: %1, Bits: %2, Seed: %3, Channels: ")
                          .arg(modeCombo_->currentText())
                          .arg(bitsPerChannelSpin_->value())
                          .arg(seedSpin_->value());
  QString ch;
  if (redCheck_->isChecked())
    ch += 'R';
  if (greenCheck_->isChecked())
    ch += 'G';
  if (blueCheck_->isChecked())
    ch += 'B';
  if (alphaCheck_->isChecked())
    ch += 'A';
  if (ch.isEmpty())
    ch = "none";
  paramsStr += ch;

  if (params.mode == rfp::stego::SlotSelectionMode::Smart) {
    paramsStr +=
        QStringLiteral(", Window: %1, Metric: %2, Threshold: %3, Shuffle: %4")
            .arg(windowSizeCombo_->currentText())
            .arg(metricCombo_->currentText())
            .arg(thresholdEdit_->text())
            .arg(shuffleAfterSortCheck_->isChecked() ? "on" : "off");
  }

  setStatus(QStringLiteral("Embedded %1 bytes. CRC32: %2. Parameters: %3")
                .arg(utf8.size())
                .arg(crcToText(crc))
                .arg(paramsStr));
}

void MainWindow::extractText() {
  if (inputImageEdit_->text().isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("R.F.P."),
                         QStringLiteral("Specify input image path."));
    return;
  }

  auto imageResult = rfp::gui::loadImageBuffer(inputImageEdit_->text());
  if (!imageResult) {
    QMessageBox::warning(this, QStringLiteral("R.F.P."),
                         QString::fromStdString(imageResult.error().message));
    return;
  }
  currentImage_ = imageResult.value();

  const auto params = collectParams();
  const auto payloadSize = static_cast<std::size_t>(payloadSizeSpin_->value());
  auto decodedResult = rfp::stego::StegoDecoder::extractBytes(
      imageResult.value(), payloadSize, params);
  if (!decodedResult) {
    QMessageBox::warning(this, QStringLiteral("R.F.P."),
                         QString::fromStdString(decodedResult.error().message));
    return;
  }

  const QByteArray decodedBytes(
      reinterpret_cast<const char *>(decodedResult.value().data()),
      static_cast<qsizetype>(decodedResult.value().size()));

  payloadEdit_->setPlainText(QString::fromUtf8(decodedBytes));

  const auto crc = rfp::core::crc32(std::span<const rfp::core::Byte>(
      decodedResult.value().data(), decodedResult.value().size()));
  setStatus(QStringLiteral("Extracted %1 bytes. CRC32: %2.")
                .arg(payloadSize)
                .arg(crcToText(crc)));
}