#include "MainWindow.h"
#include "QtImageAdapter.h"
#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Crc32.h"
#include "rfp/stego/Capacity.h"
#include "rfp/stego/StegoDecoder.h"
#include "rfp/stego/StegoDispersion.h"
#include "rfp/stego/StegoEncoder.h"
#include "rfp/stego/StegoSlots.h"

#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

namespace {

QString crcToText(std::uint32_t crc) {
  return QStringLiteral("%1").arg(crc, 8, 16, QLatin1Char('0')).toUpper();
}

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
  resize(1200, 700);
  setupUi();

  connect(bitsPerChannelSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &MainWindow::updatePreview);
  connect(seedSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &MainWindow::updatePreview);
  connect(redCheck_, &QCheckBox::toggled, this, &MainWindow::updatePreview);
  connect(greenCheck_, &QCheckBox::toggled, this, &MainWindow::updatePreview);
  connect(blueCheck_, &QCheckBox::toggled, this, &MainWindow::updatePreview);
  connect(alphaCheck_, &QCheckBox::toggled, this, &MainWindow::updatePreview);
  connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MainWindow::updatePreview);
  connect(windowSizeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &MainWindow::updatePreview);
  connect(metricCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &MainWindow::updatePreview);
  connect(thresholdEdit_, &QLineEdit::textChanged, this,
          &MainWindow::updatePreview);
  connect(shuffleAfterSortCheck_, &QCheckBox::toggled, this,
          &MainWindow::updatePreview);
  connect(showPreviewCheck_, &QCheckBox::toggled, this,
          &MainWindow::updatePreview);
  connect(previewModeCombo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MainWindow::updatePreview);
  connect(overlayOpacitySlider_, &QSlider::valueChanged, this,
          &MainWindow::updatePreview);
  connect(highlightChangesCheck_, &QCheckBox::toggled, this,
          &MainWindow::updatePreview);

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

  connect(fullscreenButton_, &QPushButton::clicked, this,
          &MainWindow::onFullscreen);
  connect(payloadEdit_, &QPlainTextEdit::textChanged, this,
          &MainWindow::onTextChanged);

  updateCapacityInfo();
  updateUsageInfo();
  updateStats(QStringLiteral("Ready"));
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  if (previewScene_ && previewScene_->itemsBoundingRect().isValid()) {
    previewView_->fitInView(previewScene_->itemsBoundingRect(),
                            Qt::KeepAspectRatio);
  }
  updateMiniPreview();
}

void MainWindow::setupUi() {
  auto *central = new QWidget(this);
  auto *mainLayout = new QHBoxLayout(central);

  auto *leftPanel = new QWidget(central);
  auto *leftLayout = new QVBoxLayout(leftPanel);

  auto *filesGroup = new QGroupBox(QStringLiteral("Images"), leftPanel);
  auto *filesLayout = new QGridLayout(filesGroup);

  inputImageEdit_ = new QLineEdit(filesGroup);
  outputImageEdit_ = new QLineEdit(filesGroup);
  auto *browseInputButton =
      new QPushButton(QStringLiteral("Browse..."), filesGroup);
  auto *browseOutputButton =
      new QPushButton(QStringLiteral("Browse..."), filesGroup);

  miniPreviewLabel_ = new QLabel(filesGroup);
  miniPreviewLabel_->setFixedHeight(120);
  miniPreviewLabel_->setScaledContents(false);
  miniPreviewLabel_->setAlignment(Qt::AlignCenter);
  miniPreviewLabel_->setStyleSheet(
      "QLabel { background-color: #333; border: 1px solid #555; }");
  miniPreviewLabel_->setText(QStringLiteral("No image"));

  filesLayout->addWidget(new QLabel(QStringLiteral("Input image:"), filesGroup),
                         0, 0);
  filesLayout->addWidget(inputImageEdit_, 0, 1);
  filesLayout->addWidget(browseInputButton, 0, 2);
  filesLayout->addWidget(
      new QLabel(QStringLiteral("Output image:"), filesGroup), 1, 0);
  filesLayout->addWidget(outputImageEdit_, 1, 1);
  filesLayout->addWidget(browseOutputButton, 1, 2);
  filesLayout->addWidget(miniPreviewLabel_, 0, 3, 2, 1);

  connect(browseInputButton, &QPushButton::clicked, this,
          &MainWindow::browseInputImage);
  connect(browseOutputButton, &QPushButton::clicked, this,
          &MainWindow::browseOutputImage);

  auto *paramsGroup =
      new QGroupBox(QStringLiteral("Steganography parameters"), leftPanel);
  auto *paramsLayout = new QFormLayout(paramsGroup);

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

  capacityLabel_ =
      new QLabel(QStringLiteral("Capacity: not loaded"), leftPanel);

  auto *payloadGroup = new QGroupBox(QStringLiteral("Text payload"), leftPanel);
  auto *payloadLayout = new QVBoxLayout(payloadGroup);
  payloadEdit_ = new QPlainTextEdit(payloadGroup);
  payloadEdit_->setPlaceholderText(
      QStringLiteral("Text to hide or extracted text will appear here"));
  payloadLayout->addWidget(payloadEdit_);
  usageLabel_ =
      new QLabel(QStringLiteral("Usage: 0 / 0 bytes (0%)"), payloadGroup);
  payloadLayout->addWidget(usageLabel_);
  payloadLayout->addWidget(capacityLabel_);

  auto *actionsLayout = new QHBoxLayout;
  auto *embedButton = new QPushButton(QStringLiteral("Embed text"), leftPanel);
  auto *extractButton =
      new QPushButton(QStringLiteral("Extract text"), leftPanel);
  actionsLayout->addWidget(embedButton);
  actionsLayout->addWidget(extractButton);
  actionsLayout->addStretch();

  auto *previewControlsGroup =
      new QGroupBox(QStringLiteral("Preview controls"), leftPanel);
  auto *previewControlsLayout = new QFormLayout(previewControlsGroup);
  showPreviewCheck_ =
      new QCheckBox(QStringLiteral("Show preview"), previewControlsGroup);
  showPreviewCheck_->setChecked(true);

  previewModeCombo_ = new QComboBox(previewControlsGroup);
  previewModeCombo_->addItem(QStringLiteral("Original"));
  previewModeCombo_->addItem(QStringLiteral("Dispersion overlay"));
  previewModeCombo_->addItem(QStringLiteral("Comparison (after embed)"));
  previewModeCombo_->setCurrentIndex(0);

  overlayOpacitySlider_ = new QSlider(Qt::Horizontal, previewControlsGroup);
  overlayOpacitySlider_->setRange(0, 100);
  overlayOpacitySlider_->setValue(50);

  highlightChangesCheck_ = new QCheckBox(
      QStringLiteral("Highlight changed pixels"), previewControlsGroup);
  highlightChangesCheck_->setChecked(false);

  previewControlsLayout->addRow(showPreviewCheck_);
  previewControlsLayout->addRow(QStringLiteral("Mode:"), previewModeCombo_);
  previewControlsLayout->addRow(QStringLiteral("Opacity:"),
                                overlayOpacitySlider_);
  previewControlsLayout->addRow(highlightChangesCheck_);

  statsLabel_ = new QLabel(previewControlsGroup);
  statsLabel_->setWordWrap(true);
  previewControlsLayout->addRow(QStringLiteral("Stats:"), statsLabel_);

  leftLayout->addWidget(filesGroup);
  leftLayout->addWidget(paramsGroup);
  leftLayout->addWidget(payloadGroup, 1);
  leftLayout->addLayout(actionsLayout);
  leftLayout->addWidget(previewControlsGroup);

  auto *rightPanel = new QWidget(central);
  auto *rightLayout = new QVBoxLayout(rightPanel);

  auto *previewHeader = new QWidget(rightPanel);
  auto *headerLayout = new QHBoxLayout(previewHeader);
  headerLayout->setContentsMargins(0, 0, 0, 0);
  QLabel *previewTitle = new QLabel(QStringLiteral("Preview"), previewHeader);
  fullscreenButton_ =
      new QPushButton(QStringLiteral("Fullscreen"), previewHeader);
  headerLayout->addWidget(previewTitle);
  headerLayout->addStretch();
  headerLayout->addWidget(fullscreenButton_);

  previewView_ = new QGraphicsView(rightPanel);
  previewScene_ = new QGraphicsScene(this);
  previewView_->setScene(previewScene_);
  previewView_->setRenderHint(QPainter::Antialiasing);
  previewView_->setBackgroundBrush(Qt::darkGray);
  previewView_->setAlignment(Qt::AlignCenter);
  previewView_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  rightLayout->addWidget(previewHeader);
  rightLayout->addWidget(previewView_, 1);

  auto *splitter = new QSplitter(Qt::Horizontal, central);
  splitter->addWidget(leftPanel);
  splitter->addWidget(rightPanel);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 2);

  mainLayout->addWidget(splitter);
  setCentralWidget(central);

  statusLabel_ = new QLabel(QStringLiteral("Ready"), this);
  progressBar_ = new QProgressBar(this);
  progressBar_->setVisible(false);
  progressBar_->setRange(0, 100);
  statusBar()->addWidget(statusLabel_, 1);
  statusBar()->addWidget(progressBar_);

  connect(embedButton, &QPushButton::clicked, this, &MainWindow::embedText);
  connect(extractButton, &QPushButton::clicked, this, &MainWindow::extractText);
}

void MainWindow::setProgress(int value, int maximum) {
  if (maximum <= 0) {
    progressBar_->setVisible(false);
    return;
  }
  progressBar_->setVisible(true);
  progressBar_->setRange(0, maximum);
  progressBar_->setValue(value);
}

void MainWindow::setStatus(const QString &text) { statusLabel_->setText(text); }

void MainWindow::updateStats(const QString &text) {
  statsLabel_->setText(text);
}

void MainWindow::browseInputImage() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Select input image"), QString(),
      QStringLiteral("Images (*.png *.bmp *.tif *.tiff);;All files (*.*)"));

  if (!path.isEmpty()) {
    inputImageEdit_->setText(path);
    setProgress(0, 0);
    setStatus(QStringLiteral("Loading image..."));
    auto result = rfp::gui::loadImageBuffer(path);
    if (result) {
      currentImage_ = result.value();
      modifiedImage_.reset();
      updateMiniPreview();
      showImage(previewScene_, imageBufferToQImage(currentImage_.value()));
      setStatus(QStringLiteral("Loaded: %1").arg(path));
      updateStats(QStringLiteral("Resolution: %1×%2, Channels: %3")
                      .arg(currentImage_->width)
                      .arg(currentImage_->height)
                      .arg(currentImage_->channels));
    } else {
      currentImage_.reset();
      modifiedImage_.reset();
      miniPreviewLabel_->setText(QStringLiteral("No image"));
      previewScene_->clear();
      QMessageBox::warning(this, QStringLiteral("R.F.P."),
                           QString::fromStdString(result.error().message));
      setStatus(QStringLiteral("Error loading image"));
    }
    updateCapacityInfo();
    updateUsageInfo();
    updatePreview();
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

QImage
MainWindow::imageBufferToQImage(const rfp::stego::ImageBuffer &buffer) const {
  if (!buffer.isValid())
    return QImage();
  const QImage::Format fmt =
      (buffer.channels == 4) ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
  QImage img(static_cast<int>(buffer.width), static_cast<int>(buffer.height),
             fmt);
  const auto rowBytes =
      static_cast<std::size_t>(buffer.width) * buffer.channels;
  for (int y = 0; y < img.height(); ++y) {
    const auto *src =
        buffer.pixels.data() + static_cast<std::size_t>(y) * rowBytes;
    auto *dst = img.scanLine(y);
    std::memcpy(dst, src, rowBytes);
  }
  return img;
}

void MainWindow::showImage(QGraphicsScene *scene, const QImage &image) {
  if (!scene)
    return;
  scene->clear();
  if (image.isNull())
    return;
  QPixmap pix = QPixmap::fromImage(image);
  scene->addPixmap(pix);
  if (previewView_) {
    previewView_->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
  }
}

void MainWindow::updateMiniPreview() {
  if (!currentImage_.has_value()) {
    miniPreviewLabel_->setPixmap(QPixmap());
    miniPreviewLabel_->setText(QStringLiteral("No image"));
    return;
  }
  QImage img = imageBufferToQImage(currentImage_.value());
  if (img.isNull()) {
    miniPreviewLabel_->setText(QStringLiteral("Invalid image"));
    return;
  }
  QPixmap pix = QPixmap::fromImage(img);
  int labelHeight = miniPreviewLabel_->height();
  if (labelHeight <= 0)
    labelHeight = 120;
  QPixmap scaled = pix.scaledToHeight(labelHeight, Qt::SmoothTransformation);
  miniPreviewLabel_->setPixmap(scaled);
  miniPreviewLabel_->setText(QString());
}

void MainWindow::updateUsageInfo() {
  if (!currentImage_.has_value()) {
    usageLabel_->setText(QStringLiteral("Usage: no image"));
    return;
  }
  const auto params = collectParams();
  const auto capacity =
      rfp::stego::capacityBytes(currentImage_.value(), params);
  const QString text = payloadEdit_->toPlainText();
  const QByteArray utf8 = text.toUtf8();
  const std::size_t used = static_cast<std::size_t>(utf8.size());
  if (capacity == 0) {
    usageLabel_->setText(QStringLiteral("Usage: 0 bytes (capacity 0)"));
    return;
  }
  double percent = (static_cast<double>(used) / capacity) * 100.0;
  QString color = (used <= capacity) ? "green" : "red";
  usageLabel_->setText(
      QStringLiteral(
          "Usage: <span style=\"color:%1;\">%2 / %3 bytes (%4%)</span>")
          .arg(color)
          .arg(used)
          .arg(capacity)
          .arg(percent, 0, 'f', 1));
}

void MainWindow::onTextChanged() { updateUsageInfo(); }

QColor MainWindow::dispersionToColor(double value, double minVal,
                                     double maxVal) const {
  double norm = (maxVal > minVal) ? (value - minVal) / (maxVal - minVal) : 0.5;
  norm = std::clamp(norm, 0.0, 1.0);
  int r, g, b;
  if (norm < 0.5) {
    double t = norm / 0.5;
    r = 0;
    g = static_cast<int>(255 * t);
    b = static_cast<int>(255 * (1.0 - t));
  } else {
    double t = (norm - 0.5) / 0.5;
    r = static_cast<int>(255 * t);
    g = static_cast<int>(255 * (1.0 - t));
    b = 0;
  }
  return QColor(r, g, b);
}

QImage
MainWindow::generateDispersionOverlay(const rfp::stego::ImageBuffer &buffer,
                                      const rfp::stego::StegoParams &params,
                                      double &outMin, double &outMax,
                                      double &outMean) {
  QImage overlay(static_cast<int>(buffer.width),
                 static_cast<int>(buffer.height), QImage::Format_ARGB32);
  overlay.fill(Qt::transparent);

  if (!buffer.isValid() ||
      params.mode != rfp::stego::SlotSelectionMode::Smart) {
    return overlay;
  }

  if (!dispersionCacheValid_ || cachedParams_ != params) {
    setProgress(0, 100);
    setStatus(QStringLiteral("Computing dispersion..."));
    rfp::stego::DispersionCalculator calc(buffer, params);
    const auto pixelCount = static_cast<std::size_t>(buffer.width) *
                            static_cast<std::size_t>(buffer.height);
    std::vector<double> disp;
    disp.reserve(pixelCount);
    double sum = 0.0;
    double minVal = std::numeric_limits<double>::max();
    double maxVal = -std::numeric_limits<double>::max();

    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
      double totalDisp = 0.0;
      int channelCount = 0;
      for (std::uint8_t ch = 0; ch < buffer.channels; ++ch) {
        if (rfp::stego::detail::channelEnabled(ch, params)) {
          totalDisp += calc.getDispersion(pixel, ch);
          ++channelCount;
        }
      }
      double pixelDisp = (channelCount > 0) ? (totalDisp / channelCount) : 0.0;
      disp.push_back(pixelDisp);
      sum += pixelDisp;
      if (pixelDisp < minVal)
        minVal = pixelDisp;
      if (pixelDisp > maxVal)
        maxVal = pixelDisp;
      if (pixel % (pixelCount / 100 + 1) == 0) {
        setProgress(static_cast<int>(pixel * 100 / pixelCount), 100);
      }
    }

    cachedDispersions_ = std::move(disp);
    cachedParams_ = params;
    dispersionCacheValid_ = true;
    outMin = minVal;
    outMax = maxVal;
    outMean = sum / static_cast<double>(pixelCount);
    setProgress(0, 0);
    setStatus(QStringLiteral("Dispersion computed"));
  } else {
    const auto &disp = cachedDispersions_.value();
    outMin = *std::min_element(disp.begin(), disp.end());
    outMax = *std::max_element(disp.begin(), disp.end());
    outMean = std::accumulate(disp.begin(), disp.end(), 0.0) /
              static_cast<double>(disp.size());
  }

  const auto &disp = cachedDispersions_.value();
  const int width = overlay.width();
  const int height = overlay.height();
  const int alpha = static_cast<int>(overlayOpacitySlider_->value() * 2.55);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::size_t idx =
          static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
          static_cast<std::size_t>(x);
      double val = disp[idx];
      QColor color = dispersionToColor(val, outMin, outMax);
      color.setAlpha(alpha);
      overlay.setPixelColor(x, y, color);
    }
  }

  return overlay;
}

QImage MainWindow::generateChangesMask(const QImage &original,
                                       const QImage &modified) const {
  if (original.size() != modified.size()) {
    return QImage();
  }
  QImage mask(original.size(), QImage::Format_ARGB32);
  mask.fill(Qt::transparent);

  QColor highlightColor(255, 0, 0, 128);
  for (int y = 0; y < original.height(); ++y) {
    for (int x = 0; x < original.width(); ++x) {
      if (original.pixel(x, y) != modified.pixel(x, y)) {
        mask.setPixelColor(x, y, highlightColor);
      }
    }
  }
  return mask;
}

QImage MainWindow::generateComparisonView(const QImage &original,
                                          const QImage &modified,
                                          bool highlightChanges) const {
  if (original.isNull() || modified.isNull()) {
    return QImage();
  }

  const int width = original.width() + modified.width() + 10;
  const int height = std::max(original.height(), modified.height());
  QImage combined(width, height, QImage::Format_RGB888);
  combined.fill(Qt::lightGray);

  QPainter painter(&combined);
  painter.drawImage(0, 0, original);
  painter.drawImage(original.width() + 10, 0, modified);

  if (highlightChanges) {
    QImage mask = generateChangesMask(original, modified);
    if (!mask.isNull()) {
      painter.drawImage(original.width() + 10, 0, mask);
    }
  }
  painter.end();
  return combined;
}

void MainWindow::updatePreview() {
  if (!currentImage_.has_value()) {
    previewScene_->clear();
    return;
  }

  if (!showPreviewCheck_->isChecked()) {
    showImage(previewScene_, imageBufferToQImage(currentImage_.value()));
    updateStats(QStringLiteral("Resolution: %1×%2, Channels: %3")
                    .arg(currentImage_->width)
                    .arg(currentImage_->height)
                    .arg(currentImage_->channels));
    return;
  }

  const int mode = previewModeCombo_->currentIndex();
  QImage displayImage;

  switch (mode) {
  case 0:
    displayImage = imageBufferToQImage(currentImage_.value());
    updateStats(QStringLiteral("Resolution: %1×%2, Channels: %3")
                    .arg(currentImage_->width)
                    .arg(currentImage_->height)
                    .arg(currentImage_->channels));
    break;
  case 1: {
    auto params = collectParams();
    if (params.mode == rfp::stego::SlotSelectionMode::Smart) {
      double minVal, maxVal, meanVal;
      QImage overlay = generateDispersionOverlay(currentImage_.value(), params,
                                                 minVal, maxVal, meanVal);
      QImage base = imageBufferToQImage(currentImage_.value());
      if (!overlay.isNull() && !base.isNull()) {
        QPainter painter(&base);
        painter.drawImage(0, 0, overlay);
        painter.end();
      }
      displayImage = base;
      updateStats(QStringLiteral("Dispersion: min=%1, max=%2, mean=%3")
                      .arg(minVal, 0, 'f', 2)
                      .arg(maxVal, 0, 'f', 2)
                      .arg(meanVal, 0, 'f', 2));
    } else {
      displayImage = imageBufferToQImage(currentImage_.value());
      updateStats(
          QStringLiteral("Dispersion overlay available only in Smart mode"));
    }
  } break;
  case 2: {
    if (!modifiedImage_.has_value()) {
      displayImage = imageBufferToQImage(currentImage_.value());
      updateStats(
          QStringLiteral("No modified image available. Embed data first."));
      break;
    }
    QImage orig = imageBufferToQImage(currentImage_.value());
    QImage mod = imageBufferToQImage(modifiedImage_.value());
    const bool highlight = highlightChangesCheck_->isChecked();
    displayImage = generateComparisonView(orig, mod, highlight);

    if (orig.size() == mod.size()) {
      int changedPixels = 0;
      for (int y = 0; y < orig.height(); ++y) {
        for (int x = 0; x < orig.width(); ++x) {
          if (orig.pixel(x, y) != mod.pixel(x, y)) {
            ++changedPixels;
          }
        }
      }
      const int totalPixels = orig.width() * orig.height();
      const double percent =
          (static_cast<double>(changedPixels) / totalPixels) * 100.0;
      updateStats(QStringLiteral("Changed pixels: %1 / %2 (%3%)")
                      .arg(changedPixels)
                      .arg(totalPixels)
                      .arg(percent, 0, 'f', 2));
    } else {
      updateStats(
          QStringLiteral("Images have different sizes, cannot compare"));
    }
  } break;
  default:
    displayImage = imageBufferToQImage(currentImage_.value());
    break;
  }

  showImage(previewScene_, displayImage);
}

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

  setProgress(0, 0);
  setStatus(QStringLiteral("Embedding..."));

  auto imageResult = rfp::gui::loadImageBuffer(inputImageEdit_->text());
  if (!imageResult) {
    QMessageBox::warning(this, QStringLiteral("R.F.P."),
                         QString::fromStdString(imageResult.error().message));
    setStatus(QStringLiteral("Embedding failed"));
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
    setStatus(QStringLiteral("Embedding failed: payload too large"));
    return;
  }

  const auto payload = rfp::core::ByteBuffer(utf8.begin(), utf8.end());
  auto encodedResult = rfp::stego::StegoEncoder::embedBytes(imageResult.value(),
                                                            payload, params);
  if (!encodedResult) {
    QMessageBox::warning(this, QStringLiteral("R.F.P."),
                         QString::fromStdString(encodedResult.error().message));
    setStatus(QStringLiteral("Embedding failed"));
    return;
  }

  modifiedImage_ = encodedResult.value();

  auto saveResult = rfp::gui::saveImageBuffer(encodedResult.value(),
                                              outputImageEdit_->text());
  if (!saveResult) {
    QMessageBox::warning(this, QStringLiteral("R.F.P."),
                         QString::fromStdString(saveResult.error().message));
    setStatus(QStringLiteral("Save failed"));
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

  const auto totalCapacity =
      rfp::stego::capacityBytes(currentImage_.value(), params);
  const double percentUsed =
      (static_cast<double>(payloadSize) / totalCapacity) * 100.0;
  updateStats(QStringLiteral("Used: %1 bytes (%2%)")
                  .arg(payloadSize)
                  .arg(percentUsed, 0, 'f', 1));

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

  updatePreview();
  updateUsageInfo();
  setProgress(0, 0);
}

void MainWindow::extractText() {
  if (inputImageEdit_->text().isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("R.F.P."),
                         QStringLiteral("Specify input image path."));
    return;
  }

  setStatus(QStringLiteral("Extracting..."));
  auto imageResult = rfp::gui::loadImageBuffer(inputImageEdit_->text());
  if (!imageResult) {
    QMessageBox::warning(this, QStringLiteral("R.F.P."),
                         QString::fromStdString(imageResult.error().message));
    setStatus(QStringLiteral("Extraction failed"));
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
    setStatus(QStringLiteral("Extraction failed"));
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

  updateStats(QStringLiteral("Extracted %1 bytes").arg(payloadSize));
  updateUsageInfo();
}

void MainWindow::onFullscreen() {
  if (previewModeCombo_->currentIndex() == 2) {
    setStatus(QStringLiteral("Fullscreen not available in comparison mode"));
    return;
  }
  if (!previewScene_ || previewScene_->items().isEmpty()) {
    return;
  }
  QGraphicsPixmapItem *item =
      dynamic_cast<QGraphicsPixmapItem *>(previewScene_->items().first());
  if (!item)
    return;
  QPixmap pix = item->pixmap();
  if (pix.isNull())
    return;

  QDialog *dialog = new QDialog(this);
  dialog->setWindowTitle(QStringLiteral("Fullscreen Preview"));
  dialog->setWindowState(Qt::WindowFullScreen);
  QVBoxLayout *layout = new QVBoxLayout(dialog);
  QGraphicsView *view = new QGraphicsView(dialog);
  QGraphicsScene *scene = new QGraphicsScene(view);
  scene->addPixmap(pix);
  view->setScene(scene);
  view->setRenderHint(QPainter::Antialiasing);
  view->setBackgroundBrush(Qt::black);
  view->setAlignment(Qt::AlignCenter);
  view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  layout->addWidget(view);
  QPushButton *closeBtn = new QPushButton(QStringLiteral("Close"), dialog);
  connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
  layout->addWidget(closeBtn, 0, Qt::AlignCenter);
  dialog->exec();
}