#include "MainWindow.h"

#include "CryptoPanel.h"
#include "HelpDialog.h"
#include "MaskingDialog.h"
#include "QtImageAdapter.h"
#include "SettingsDialog.h"

#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Crc32.h"
#include "rfp/crypto/CryptoRegistry.h"
#include "rfp/payload/PayloadCrypto.h"
#include "rfp/stego/Capacity.h"
#include "rfp/stego/StegoDecoder.h"
#include "rfp/stego/StegoDispersion.h"
#include "rfp/stego/StegoEncoder.h"
#include "rfp/stego/StegoSlots.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QProcess>
#include <QResizeEvent>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <span>
#include <vector>

// ===========================================================================
//  Anonymous namespace
// ===========================================================================
namespace {

QString crcToText(std::uint32_t crc) {
  return QStringLiteral("%1").arg(crc, 8, 16, QLatin1Char('0')).toUpper();
}

double computeAutoThreshold(const rfp::stego::ImageBuffer &image,
                            const rfp::stego::StegoParams &params) {
  if (!image.isValid())
    return 0.0;

  rfp::stego::DispersionCalculator calc(image, params);
  const std::size_t pixelCount = static_cast<std::size_t>(image.width) *
                                 static_cast<std::size_t>(image.height);

  std::vector<double> values;
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
  return values[idx < values.size() ? idx : values.size() - 1];
}

// Encodes %, ; and = so that free-form values (typically filesystem paths)
// can be stored inside a semicolon-separated key=value string.
QString escapeParamValue(const QString &value) {
  QString out = value;
  out.replace(QLatin1Char('%'), QStringLiteral("%25"));
  out.replace(QLatin1Char(';'), QStringLiteral("%3B"));
  out.replace(QLatin1Char('='), QStringLiteral("%3D"));
  return out;
}

QString unescapeParamValue(const QString &value) {
  QString out = value;
  out.replace(QStringLiteral("%3D"), QStringLiteral("="));
  out.replace(QStringLiteral("%3B"), QStringLiteral(";"));
  out.replace(QStringLiteral("%25"), QStringLiteral("%"));
  return out;
}

// Validated integer → enum conversion with a whitelist of accepted values.
template <typename Enum>
Enum enumFromIntOrDefault(int value, std::initializer_list<Enum> allowed,
                          Enum fallback) {
  for (const Enum candidate : allowed) {
    if (static_cast<int>(candidate) == value)
      return candidate;
  }
  return fallback;
}

bool isValidWindowSize(int value) {
  switch (value) {
  case 3:
  case 5:
  case 7:
  case 9:
  case 11:
  case 13:
    return true;
  default:
    return false;
  }
}

} // namespace

// ===========================================================================
//  Static helpers
// ===========================================================================

std::size_t MainWindow::cryptoOverheadBytes(rfp::crypto::CipherId id) noexcept {
  constexpr std::size_t kHeader = 16;
  constexpr std::size_t kSalt = 16;

  const auto spec = rfp::crypto::CryptoRegistry::cipherSpec(id);
  if (!spec)
    return kHeader + kSalt;

  std::size_t total = kHeader + kSalt + spec->ivSize + spec->tagSize;
  if (spec->kind == rfp::crypto::CipherKind::BlockCbc)
    total += 16;
  return total;
}

// ===========================================================================
//  Construction
// ===========================================================================

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setupUi();
  setupConnections();

  updateTimer_ = new QTimer(this);
  updateTimer_->setSingleShot(true);
  updateTimer_->setInterval(300);
  connect(updateTimer_, &QTimer::timeout, this, &MainWindow::doUpdate);

  connect(&previewWatcher_, &QFutureWatcher<RecomputeResult>::finished, this,
          &MainWindow::onPreviewReady);

  loadSettings();
  applySettings();

  QTimer::singleShot(0, this, &MainWindow::scheduleUpdate);

  setWindowTitle(QApplication::applicationDisplayName());
  resize(1200, 700);
  statusLabel_->setText(tr("Ready"));
}

MainWindow::~MainWindow() = default;

// ===========================================================================
//  setupUi
// ===========================================================================

void MainWindow::setupUi() {
  auto *central = new QWidget(this);
  auto *mainLayout = new QVBoxLayout(central);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  auto *toolBar = addToolBar(tr("Tools"));
  toolBar->setObjectName(QStringLiteral("mainToolBar"));

  settingsButton_ = new QPushButton(tr("Settings"), this);
  maskingButton_ = new QPushButton(tr("Masking"), this);
  fullscreenButton_ = new QPushButton(tr("Fullscreen"), this);
  helpButton_ = new QPushButton(tr("Help"), this);
  toolBar->addWidget(settingsButton_);
  toolBar->addWidget(maskingButton_);
  toolBar->addWidget(fullscreenButton_);
  toolBar->addWidget(helpButton_);

  tabWidget_ = new QTabWidget(this);

  // ========================================================================
  //  Embed tab
  // ========================================================================
  embedTab_ = new QWidget(this);
  auto *embedLayout = new QHBoxLayout(embedTab_);

  auto *embedLeft = new QWidget(embedTab_);
  auto *embedLeftLayout = new QVBoxLayout(embedLeft);

  // ---- Files ----
  auto *filesGroup = new QGroupBox(tr("Images"), embedLeft);
  auto *filesLayout = new QGridLayout(filesGroup);

  inputImageEdit_ = new QLineEdit(filesGroup);
  outputImageEdit_ = new QLineEdit(filesGroup);

  browseInputBtn_ = new QPushButton(tr("Browse..."), filesGroup);
  browseOutputBtn_ = new QPushButton(tr("Browse..."), filesGroup);

  miniPreviewLabel_ = new QLabel(filesGroup);
  miniPreviewLabel_->setFixedHeight(120);
  miniPreviewLabel_->setAlignment(Qt::AlignCenter);
  miniPreviewLabel_->setFrameShape(QFrame::StyledPanel);
  miniPreviewLabel_->setText(tr("No image"));

  filesLayout->addWidget(new QLabel(tr("Input:"), filesGroup), 0, 0);
  filesLayout->addWidget(inputImageEdit_, 0, 1);
  filesLayout->addWidget(browseInputBtn_, 0, 2);
  filesLayout->addWidget(new QLabel(tr("Output:"), filesGroup), 1, 0);
  filesLayout->addWidget(outputImageEdit_, 1, 1);
  filesLayout->addWidget(browseOutputBtn_, 1, 2);
  filesLayout->addWidget(miniPreviewLabel_, 0, 3, 2, 1);
  embedLeftLayout->addWidget(filesGroup);

  // ---- Payload ----
  auto *payloadGroup = new QGroupBox(tr("Text payload"), embedLeft);
  auto *payloadLayout = new QVBoxLayout(payloadGroup);

  payloadEdit_ = new QPlainTextEdit(payloadGroup);
  payloadEdit_->setPlaceholderText(tr("Enter text to hide..."));
  payloadLayout->addWidget(payloadEdit_);

  usageLabel_ = new QLabel(tr("Usage: 0 bytes"), payloadGroup);
  usageLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  payloadLayout->addWidget(usageLabel_);

  embedLeftLayout->addWidget(payloadGroup, 1);

  // ---- Encryption ----
  embedCryptoPanel_ = new CryptoPanel(CryptoPanel::Role::Embed, embedLeft);
  embedLeftLayout->addWidget(embedCryptoPanel_);

  // ---- Steganography parameters ----
  auto *embedParamsGroup =
      new QGroupBox(tr("Steganography parameters (Embed)"), embedLeft);
  auto *embedParamsLayout = new QFormLayout(embedParamsGroup);

  embedBitsSpin_ = new QSpinBox(embedParamsGroup);
  embedBitsSpin_->setRange(1, 4);
  embedBitsSpin_->setValue(1);

  embedSeedSpin_ = new QSpinBox(embedParamsGroup);
  embedSeedSpin_->setRange(0, std::numeric_limits<int>::max());
  embedSeedSpin_->setValue(0);

  auto *channelsWidgetEmbed = new QWidget(embedParamsGroup);
  auto *channelsLayoutEmbed = new QHBoxLayout(channelsWidgetEmbed);
  channelsLayoutEmbed->setContentsMargins(0, 0, 0, 0);

  embedRed_ = new QCheckBox(tr("R"), channelsWidgetEmbed);
  embedGreen_ = new QCheckBox(tr("G"), channelsWidgetEmbed);
  embedBlue_ = new QCheckBox(tr("B"), channelsWidgetEmbed);
  embedAlpha_ = new QCheckBox(tr("A"), channelsWidgetEmbed);
  embedRed_->setChecked(true);
  embedGreen_->setChecked(true);
  embedBlue_->setChecked(true);
  embedAlpha_->setChecked(false);

  channelsLayoutEmbed->addWidget(embedRed_);
  channelsLayoutEmbed->addWidget(embedGreen_);
  channelsLayoutEmbed->addWidget(embedBlue_);
  channelsLayoutEmbed->addWidget(embedAlpha_);
  channelsLayoutEmbed->addStretch();

  embedParamsLayout->addRow(tr("Bits per channel:"), embedBitsSpin_);
  embedParamsLayout->addRow(tr("Seed (0 = sequential):"), embedSeedSpin_);
  embedParamsLayout->addRow(tr("Channels:"), channelsWidgetEmbed);

  auto *embedSmartGroup =
      new QGroupBox(tr("Smart selection"), embedParamsGroup);
  auto *embedSmartLayout = new QFormLayout(embedSmartGroup);

  embedModeCombo_ = new QComboBox(embedSmartGroup);
  embedModeCombo_->addItem(
      tr("Uniform"), static_cast<int>(rfp::stego::SlotSelectionMode::Uniform));
  embedModeCombo_->addItem(
      tr("Smart (dispersion)"),
      static_cast<int>(rfp::stego::SlotSelectionMode::Dispersion));

  embedWindowCombo_ = new QComboBox(embedSmartGroup);
  for (int s : {3, 5, 7, 9, 11, 13})
    embedWindowCombo_->addItem(QString::number(s), s);

  embedMetricCombo_ = new QComboBox(embedSmartGroup);
  embedMetricCombo_->addItem(
      tr("Luminance"),
      static_cast<int>(rfp::stego::DispersionMetric::Luminance));
  embedMetricCombo_->addItem(
      tr("Per-channel"),
      static_cast<int>(rfp::stego::DispersionMetric::PerChannel));
  embedMetricCombo_->addItem(
      tr("Sum"), static_cast<int>(rfp::stego::DispersionMetric::Sum));

  embedThresholdEdit_ = new QLineEdit(embedSmartGroup);
  embedThresholdEdit_->setText(QStringLiteral("0.0"));
  embedThresholdEdit_->setValidator(
      new QDoubleValidator(0.0, 100000.0, 2, embedThresholdEdit_));

  embedAutoThresholdBtn_ = new QPushButton(tr("Auto"), embedSmartGroup);

  auto *thresholdWidgetEmbed = new QWidget(embedSmartGroup);
  auto *thresholdLayoutEmbed = new QHBoxLayout(thresholdWidgetEmbed);
  thresholdLayoutEmbed->setContentsMargins(0, 0, 0, 0);
  thresholdLayoutEmbed->addWidget(embedThresholdEdit_, 1);
  thresholdLayoutEmbed->addWidget(embedAutoThresholdBtn_);

  embedShuffleCheck_ =
      new QCheckBox(tr("Apply shuffle after sorting"), embedSmartGroup);
  embedShuffleCheck_->setChecked(true);

  embedSmartLayout->addRow(tr("Mode:"), embedModeCombo_);
  embedSmartLayout->addRow(tr("Window size:"), embedWindowCombo_);
  embedSmartLayout->addRow(tr("Dispersion metric:"), embedMetricCombo_);
  embedSmartLayout->addRow(tr("Threshold:"), thresholdWidgetEmbed);
  embedSmartLayout->addRow(embedShuffleCheck_);

  embedParamsLayout->addRow(embedSmartGroup);

  auto *embedButtonsLayout = new QHBoxLayout;
  copyEmbedBtn_ = new QPushButton(tr("Copy params"), embedParamsGroup);
  pasteEmbedBtn_ = new QPushButton(tr("Paste params"), embedParamsGroup);
  embedButtonsLayout->addWidget(copyEmbedBtn_);
  embedButtonsLayout->addWidget(pasteEmbedBtn_);
  embedButtonsLayout->addStretch();
  embedParamsLayout->addRow(embedButtonsLayout);

  capacityLabel_ = new QLabel(tr("Capacity: not loaded"), embedParamsGroup);
  capacityLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  embedParamsLayout->addRow(capacityLabel_);

  embedLeftLayout->addWidget(embedParamsGroup);

  auto *actionsLayout = new QHBoxLayout;
  embedBtn_ = new QPushButton(tr("Embed"), embedLeft);
  actionsLayout->addWidget(embedBtn_);
  actionsLayout->addStretch();
  embedLeftLayout->addLayout(actionsLayout);

  // ---- Preview ----
  auto *embedRight = new QWidget(embedTab_);
  auto *embedRightLayout = new QVBoxLayout(embedRight);

  previewView_ = new QGraphicsView(embedRight);
  previewScene_ = new QGraphicsScene(this);
  previewView_->setScene(previewScene_);
  previewView_->setRenderHint(QPainter::Antialiasing);
  previewView_->setBackgroundBrush(Qt::darkGray);
  previewView_->setAlignment(Qt::AlignCenter);
  previewView_->setDragMode(QGraphicsView::ScrollHandDrag);
  previewView_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  previewView_->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
  embedRightLayout->addWidget(previewView_);

  statsLabel_ = new QLabel(embedRight);
  statsLabel_->setWordWrap(true);
  statsLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  embedRightLayout->addWidget(statsLabel_);

  embedLayout->addWidget(embedLeft, 1);
  embedLayout->addWidget(embedRight, 2);
  tabWidget_->addTab(embedTab_, tr("Embed"));

  // ========================================================================
  //  Extract tab
  // ========================================================================
  extractTab_ = new QWidget(this);
  auto *extractLayout = new QHBoxLayout(extractTab_);

  auto *extractLeft = new QWidget(extractTab_);
  auto *extractLeftLayout = new QVBoxLayout(extractLeft);

  // ---- File ----
  auto *extractFilesGroup = new QGroupBox(tr("Image"), extractLeft);
  auto *extractFilesLayout = new QGridLayout(extractFilesGroup);

  inputImageExtractEdit_ = new QLineEdit(extractFilesGroup);
  browseExtractBtn_ = new QPushButton(tr("Browse..."), extractFilesGroup);

  extractFilesLayout->addWidget(new QLabel(tr("Input:"), extractFilesGroup), 0,
                                0);
  extractFilesLayout->addWidget(inputImageExtractEdit_, 0, 1);
  extractFilesLayout->addWidget(browseExtractBtn_, 0, 2);
  extractLeftLayout->addWidget(extractFilesGroup);

  // ---- Extraction options ----
  auto *extractParamsGroup =
      new QGroupBox(tr("Extraction options"), extractLeft);
  auto *extractParamsLayout = new QFormLayout(extractParamsGroup);

  autoDetectSizeCheck_ = new QCheckBox(
      tr("Header written at embed time (auto-detect payload size)"),
      extractParamsGroup);
  autoDetectSizeCheck_->setChecked(true);
  autoDetectSizeCheck_->setEnabled(false);
  autoDetectSizeCheck_->setToolTip(
      tr("This reflects the 'Write payload size header' setting in Settings. "
         "Change it there."));
  extractParamsLayout->addRow(autoDetectSizeCheck_);

  payloadSizeSpin_ = new QSpinBox(extractParamsGroup);
  payloadSizeSpin_->setRange(1, 100'000'000);
  payloadSizeSpin_->setValue(1024);
  payloadSizeSpin_->setToolTip(
      tr("Used only when the embed did NOT write a size header. "
         "Must exactly match the length of the embedded payload."));
  extractParamsLayout->addRow(tr("Payload size (bytes):"), payloadSizeSpin_);

  extractLeftLayout->addWidget(extractParamsGroup);

  // ---- Decryption ----
  extractCryptoPanel_ =
      new CryptoPanel(CryptoPanel::Role::Extract, extractLeft);
  extractLeftLayout->addWidget(extractCryptoPanel_);

  // ---- Steganography parameters ----
  auto *extractParamsGroup2 =
      new QGroupBox(tr("Steganography parameters (Extract)"), extractLeft);
  auto *extractParamsLayout2 = new QFormLayout(extractParamsGroup2);

  extractBitsSpin_ = new QSpinBox(extractParamsGroup2);
  extractBitsSpin_->setRange(1, 4);
  extractBitsSpin_->setValue(1);

  extractSeedSpin_ = new QSpinBox(extractParamsGroup2);
  extractSeedSpin_->setRange(0, std::numeric_limits<int>::max());
  extractSeedSpin_->setValue(0);

  auto *channelsWidgetExtract = new QWidget(extractParamsGroup2);
  auto *channelsLayoutExtract = new QHBoxLayout(channelsWidgetExtract);
  channelsLayoutExtract->setContentsMargins(0, 0, 0, 0);

  extractRed_ = new QCheckBox(tr("R"), channelsWidgetExtract);
  extractGreen_ = new QCheckBox(tr("G"), channelsWidgetExtract);
  extractBlue_ = new QCheckBox(tr("B"), channelsWidgetExtract);
  extractAlpha_ = new QCheckBox(tr("A"), channelsWidgetExtract);
  extractRed_->setChecked(true);
  extractGreen_->setChecked(true);
  extractBlue_->setChecked(true);
  extractAlpha_->setChecked(false);

  channelsLayoutExtract->addWidget(extractRed_);
  channelsLayoutExtract->addWidget(extractGreen_);
  channelsLayoutExtract->addWidget(extractBlue_);
  channelsLayoutExtract->addWidget(extractAlpha_);
  channelsLayoutExtract->addStretch();

  extractParamsLayout2->addRow(tr("Bits per channel:"), extractBitsSpin_);
  extractParamsLayout2->addRow(tr("Seed (0 = sequential):"), extractSeedSpin_);
  extractParamsLayout2->addRow(tr("Channels:"), channelsWidgetExtract);

  auto *extractSmartGroup =
      new QGroupBox(tr("Smart selection"), extractParamsGroup2);
  auto *extractSmartLayout = new QFormLayout(extractSmartGroup);

  extractModeCombo_ = new QComboBox(extractSmartGroup);
  extractModeCombo_->addItem(
      tr("Uniform"), static_cast<int>(rfp::stego::SlotSelectionMode::Uniform));
  extractModeCombo_->addItem(
      tr("Smart (dispersion)"),
      static_cast<int>(rfp::stego::SlotSelectionMode::Dispersion));

  extractWindowCombo_ = new QComboBox(extractSmartGroup);
  for (int s : {3, 5, 7, 9, 11, 13})
    extractWindowCombo_->addItem(QString::number(s), s);

  extractMetricCombo_ = new QComboBox(extractSmartGroup);
  extractMetricCombo_->addItem(
      tr("Luminance"),
      static_cast<int>(rfp::stego::DispersionMetric::Luminance));
  extractMetricCombo_->addItem(
      tr("Per-channel"),
      static_cast<int>(rfp::stego::DispersionMetric::PerChannel));
  extractMetricCombo_->addItem(
      tr("Sum"), static_cast<int>(rfp::stego::DispersionMetric::Sum));

  extractThresholdEdit_ = new QLineEdit(extractSmartGroup);
  extractThresholdEdit_->setText(QStringLiteral("0.0"));
  extractThresholdEdit_->setValidator(
      new QDoubleValidator(0.0, 100000.0, 2, extractThresholdEdit_));

  extractAutoThresholdBtn_ = new QPushButton(tr("Auto"), extractSmartGroup);

  auto *thresholdWidgetExtract = new QWidget(extractSmartGroup);
  auto *thresholdLayoutExtract = new QHBoxLayout(thresholdWidgetExtract);
  thresholdLayoutExtract->setContentsMargins(0, 0, 0, 0);
  thresholdLayoutExtract->addWidget(extractThresholdEdit_, 1);
  thresholdLayoutExtract->addWidget(extractAutoThresholdBtn_);

  extractShuffleCheck_ =
      new QCheckBox(tr("Apply shuffle after sorting"), extractSmartGroup);
  extractShuffleCheck_->setChecked(true);

  extractSmartLayout->addRow(tr("Mode:"), extractModeCombo_);
  extractSmartLayout->addRow(tr("Window size:"), extractWindowCombo_);
  extractSmartLayout->addRow(tr("Dispersion metric:"), extractMetricCombo_);
  extractSmartLayout->addRow(tr("Threshold:"), thresholdWidgetExtract);
  extractSmartLayout->addRow(extractShuffleCheck_);

  extractParamsLayout2->addRow(extractSmartGroup);

  auto *extractButtonsLayout = new QHBoxLayout;
  copyParamsBtn_ = new QPushButton(tr("Copy params"), extractParamsGroup2);
  pasteParamsBtn_ = new QPushButton(tr("Paste params"), extractParamsGroup2);
  extractButtonsLayout->addWidget(copyParamsBtn_);
  extractButtonsLayout->addWidget(pasteParamsBtn_);
  extractButtonsLayout->addStretch();
  extractParamsLayout2->addRow(extractButtonsLayout);

  extractLeftLayout->addWidget(extractParamsGroup2);

  auto *extractActionsLayout = new QHBoxLayout;
  extractBtn_ = new QPushButton(tr("Extract"), extractLeft);
  extractActionsLayout->addWidget(extractBtn_);
  extractActionsLayout->addStretch();
  extractLeftLayout->addLayout(extractActionsLayout);

  extractedTextEdit_ = new QPlainTextEdit(extractLeft);
  extractedTextEdit_->setReadOnly(true);
  extractedTextEdit_->setPlaceholderText(tr("Extracted text will appear here"));
  extractLeftLayout->addWidget(extractedTextEdit_, 1);

  // ---- Preview (shared scene with Embed tab) ----
  auto *extractRight = new QWidget(extractTab_);
  auto *extractRightLayout = new QVBoxLayout(extractRight);

  auto *extractPreview = new QGraphicsView(extractRight);
  extractPreview->setScene(previewScene_);
  extractPreview->setRenderHint(QPainter::Antialiasing);
  extractPreview->setBackgroundBrush(Qt::darkGray);
  extractPreview->setAlignment(Qt::AlignCenter);
  extractPreview->setDragMode(QGraphicsView::ScrollHandDrag);
  extractPreview->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  extractPreview->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
  extractRightLayout->addWidget(extractPreview);

  extractLayout->addWidget(extractLeft, 1);
  extractLayout->addWidget(extractRight, 2);
  tabWidget_->addTab(extractTab_, tr("Extract"));

  mainLayout->addWidget(tabWidget_);

  // ---- Status bar ----
  statusLabel_ = new QLabel(tr("Ready"), this);
  statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  statusLabel_->setMinimumWidth(300);
  statusBar()->addWidget(statusLabel_, 1);

  progressBar_ = new QProgressBar(this);
  progressBar_->setRange(0, 0);
  progressBar_->setTextVisible(false);
  progressBar_->setFixedWidth(160);
  progressBar_->setVisible(false);
  statusBar()->addPermanentWidget(progressBar_);

  setCentralWidget(central);
}

// ===========================================================================
//  setupConnections
// ===========================================================================

void MainWindow::setupConnections() {
  // ---- Toolbar ----
  connect(settingsButton_, &QPushButton::clicked, this,
          &MainWindow::showSettings);
  connect(maskingButton_, &QPushButton::clicked, this,
          &MainWindow::showMasking);
  connect(fullscreenButton_, &QPushButton::clicked, this,
          &MainWindow::onFullscreen);
  connect(helpButton_, &QPushButton::clicked, this, &MainWindow::showHelp);

  // ---- Embed tab ----
  connect(browseInputBtn_, &QPushButton::clicked, this,
          &MainWindow::browseInputImage);
  connect(browseOutputBtn_, &QPushButton::clicked, this,
          &MainWindow::browseOutputImage);
  connect(embedBtn_, &QPushButton::clicked, this, &MainWindow::embedText);
  connect(copyEmbedBtn_, &QPushButton::clicked, this,
          &MainWindow::copyEmbedParams);
  connect(pasteEmbedBtn_, &QPushButton::clicked, this,
          &MainWindow::pasteEmbedParams);

  // ---- Extract tab ----
  connect(browseExtractBtn_, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select input image"), QString(),
        tr("Images (*.png *.bmp *.tif *.tiff *.ppm *.pgm);;All files (*.*)"));
    if (!path.isEmpty())
      inputImageExtractEdit_->setText(path);
  });
  connect(extractBtn_, &QPushButton::clicked, this, &MainWindow::extractText);
  connect(copyParamsBtn_, &QPushButton::clicked, this,
          &MainWindow::copyExtractParams);
  connect(pasteParamsBtn_, &QPushButton::clicked, this,
          &MainWindow::pasteExtractParams);

  // ---- Payload / params -> schedule update ----
  connect(payloadEdit_, &QPlainTextEdit::textChanged, this,
          &MainWindow::scheduleUpdate);

  const auto schedule = [this]() { scheduleUpdate(); };

  connect(embedBitsSpin_, &QSpinBox::valueChanged, this, schedule);
  connect(embedSeedSpin_, &QSpinBox::valueChanged, this, schedule);
  connect(embedRed_, &QCheckBox::toggled, this, schedule);
  connect(embedGreen_, &QCheckBox::toggled, this, schedule);
  connect(embedBlue_, &QCheckBox::toggled, this, schedule);
  connect(embedAlpha_, &QCheckBox::toggled, this, schedule);
  connect(embedModeCombo_, &QComboBox::currentIndexChanged, this, schedule);
  connect(embedWindowCombo_, &QComboBox::currentIndexChanged, this, schedule);
  connect(embedMetricCombo_, &QComboBox::currentIndexChanged, this, schedule);
  connect(embedThresholdEdit_, &QLineEdit::textChanged, this, schedule);
  connect(embedShuffleCheck_, &QCheckBox::toggled, this, schedule);

  connect(extractBitsSpin_, &QSpinBox::valueChanged, this, schedule);
  connect(extractSeedSpin_, &QSpinBox::valueChanged, this, schedule);
  connect(extractRed_, &QCheckBox::toggled, this, schedule);
  connect(extractGreen_, &QCheckBox::toggled, this, schedule);
  connect(extractBlue_, &QCheckBox::toggled, this, schedule);
  connect(extractAlpha_, &QCheckBox::toggled, this, schedule);
  connect(extractModeCombo_, &QComboBox::currentIndexChanged, this, schedule);
  connect(extractWindowCombo_, &QComboBox::currentIndexChanged, this, schedule);
  connect(extractMetricCombo_, &QComboBox::currentIndexChanged, this, schedule);
  connect(extractThresholdEdit_, &QLineEdit::textChanged, this, schedule);
  connect(extractShuffleCheck_, &QCheckBox::toggled, this, schedule);

  // ---- Crypto panels ----
  connect(embedCryptoPanel_, &CryptoPanel::changed, this,
          &MainWindow::scheduleUpdate);
  connect(extractCryptoPanel_, &CryptoPanel::changed, this,
          &MainWindow::scheduleUpdate);

  // ---- Async watchers ----
  connect(&embedWatcher_,
          &QFutureWatcher<rfp::core::Result<rfp::stego::ImageBuffer>>::finished,
          this, &MainWindow::onEmbedFinished);
  connect(&extractWatcher_,
          &QFutureWatcher<rfp::core::Result<rfp::core::ByteBuffer>>::finished,
          this, &MainWindow::onExtractFinished);

  // ---- Auto-threshold (Embed) ----
  connect(embedAutoThresholdBtn_, &QPushButton::clicked, this, [this]() {
    if (!currentImage_) {
      QMessageBox::warning(this, tr("R.F.P."),
                           tr("Load an input image first."));
      return;
    }
    const auto params = collectParams(ParamsRole::Embed);
    const auto imageCopy = currentImage_.value();

    beginBusy(tr("Computing threshold (Embed)..."));

    auto *watcher = new QFutureWatcher<double>(this);
    connect(watcher, &QFutureWatcher<double>::finished, this,
            [this, watcher]() {
              const double thr = watcher->result();
              embedThresholdEdit_->setText(QString::number(thr, 'f', 2));
              watcher->deleteLater();
              endBusy();
              setStatus(tr("Auto threshold: %1").arg(thr, 0, 'f', 2), 3000);
              scheduleUpdate();
            });
    watcher->setFuture(QtConcurrent::run([imageCopy, params]() {
      return computeAutoThreshold(imageCopy, params);
    }));
  });

  // ---- Auto-threshold (Extract) ----
  connect(extractAutoThresholdBtn_, &QPushButton::clicked, this, [this]() {
    if (!currentImage_) {
      QMessageBox::warning(this, tr("R.F.P."),
                           tr("Load an input image first."));
      return;
    }
    const auto params = collectParams(ParamsRole::Extract);
    const auto imageCopy = currentImage_.value();

    beginBusy(tr("Computing threshold (Extract)..."));

    auto *watcher = new QFutureWatcher<double>(this);
    connect(watcher, &QFutureWatcher<double>::finished, this,
            [this, watcher]() {
              const double thr = watcher->result();
              extractThresholdEdit_->setText(QString::number(thr, 'f', 2));
              watcher->deleteLater();
              endBusy();
              setStatus(tr("Auto threshold: %1").arg(thr, 0, 'f', 2), 3000);
              scheduleUpdate();
            });
    watcher->setFuture(QtConcurrent::run([imageCopy, params]() {
      return computeAutoThreshold(imageCopy, params);
    }));
  });

  connect(qApp, &QCoreApplication::aboutToQuit, this,
          &MainWindow::saveSettings);
}

// ===========================================================================
//  loadSettings / saveSettings / applySettings
// ===========================================================================

void MainWindow::loadSettings() {
  settings_.beginGroup(QStringLiteral("MainWindow"));
  if (settings_.contains(QStringLiteral("size"))) {
    const QSize sz = settings_.value(QStringLiteral("size")).toSize();
    if (sz.isValid())
      resize(sz);
  }
  if (settings_.contains(QStringLiteral("pos"))) {
    const QPoint pt = settings_.value(QStringLiteral("pos")).toPoint();
    if (!pt.isNull())
      move(pt);
  }
  settings_.endGroup();

  settings_.beginGroup(QStringLiteral("ParamsEmbed"));
  {
    const QSignalBlocker b1(embedBitsSpin_);
    const QSignalBlocker b2(embedSeedSpin_);
    const QSignalBlocker b3(embedRed_);
    const QSignalBlocker b4(embedGreen_);
    const QSignalBlocker b5(embedBlue_);
    const QSignalBlocker b6(embedAlpha_);
    const QSignalBlocker b7(embedModeCombo_);
    const QSignalBlocker b8(embedWindowCombo_);
    const QSignalBlocker b9(embedMetricCombo_);
    const QSignalBlocker b10(embedThresholdEdit_);
    const QSignalBlocker b11(embedShuffleCheck_);

    embedBitsSpin_->setValue(
        settings_.value(QStringLiteral("bits"), 1).toInt());
    embedSeedSpin_->setValue(
        settings_.value(QStringLiteral("seed"), 0).toInt());
    embedRed_->setChecked(
        settings_.value(QStringLiteral("red"), true).toBool());
    embedGreen_->setChecked(
        settings_.value(QStringLiteral("green"), true).toBool());
    embedBlue_->setChecked(
        settings_.value(QStringLiteral("blue"), true).toBool());
    embedAlpha_->setChecked(
        settings_.value(QStringLiteral("alpha"), false).toBool());
    embedModeCombo_->setCurrentIndex(
        settings_.value(QStringLiteral("mode"), 0).toInt());
    embedWindowCombo_->setCurrentIndex(
        settings_.value(QStringLiteral("window"), 0).toInt());
    embedMetricCombo_->setCurrentIndex(
        settings_.value(QStringLiteral("metric"), 0).toInt());
    embedThresholdEdit_->setText(
        settings_.value(QStringLiteral("threshold"), QStringLiteral("0.0"))
            .toString());
    embedShuffleCheck_->setChecked(
        settings_.value(QStringLiteral("shuffle"), true).toBool());
  }
  settings_.endGroup();

  settings_.beginGroup(QStringLiteral("ParamsExtract"));
  {
    const QSignalBlocker b1(extractBitsSpin_);
    const QSignalBlocker b2(extractSeedSpin_);
    const QSignalBlocker b3(extractRed_);
    const QSignalBlocker b4(extractGreen_);
    const QSignalBlocker b5(extractBlue_);
    const QSignalBlocker b6(extractAlpha_);
    const QSignalBlocker b7(extractModeCombo_);
    const QSignalBlocker b8(extractWindowCombo_);
    const QSignalBlocker b9(extractMetricCombo_);
    const QSignalBlocker b10(extractThresholdEdit_);
    const QSignalBlocker b11(extractShuffleCheck_);

    extractBitsSpin_->setValue(
        settings_.value(QStringLiteral("bits"), 1).toInt());
    extractSeedSpin_->setValue(
        settings_.value(QStringLiteral("seed"), 0).toInt());
    extractRed_->setChecked(
        settings_.value(QStringLiteral("red"), true).toBool());
    extractGreen_->setChecked(
        settings_.value(QStringLiteral("green"), true).toBool());
    extractBlue_->setChecked(
        settings_.value(QStringLiteral("blue"), true).toBool());
    extractAlpha_->setChecked(
        settings_.value(QStringLiteral("alpha"), false).toBool());
    extractModeCombo_->setCurrentIndex(
        settings_.value(QStringLiteral("mode"), 0).toInt());
    extractWindowCombo_->setCurrentIndex(
        settings_.value(QStringLiteral("window"), 0).toInt());
    extractMetricCombo_->setCurrentIndex(
        settings_.value(QStringLiteral("metric"), 0).toInt());
    extractThresholdEdit_->setText(
        settings_.value(QStringLiteral("threshold"), QStringLiteral("0.0"))
            .toString());
    extractShuffleCheck_->setChecked(
        settings_.value(QStringLiteral("shuffle"), true).toBool());
  }
  settings_.endGroup();

  settings_.beginGroup(QStringLiteral("CryptoEmbed"));
  {
    embedCryptoPanel_->setEncryptionEnabled(
        settings_.value(QStringLiteral("enabled"), false).toBool());

    const int cipherInt =
        settings_
            .value(QStringLiteral("cipher"),
                   static_cast<int>(rfp::crypto::CipherId::Aes256Gcm))
            .toInt();
    embedCryptoPanel_->setCipher(static_cast<rfp::crypto::CipherId>(cipherInt));

    const int kdfInt =
        settings_
            .value(QStringLiteral("kdf"),
                   static_cast<int>(rfp::crypto::KdfId::Pbkdf2HmacSha256))
            .toInt();
    embedCryptoPanel_->setKdf(static_cast<rfp::crypto::KdfId>(kdfInt));

    embedCryptoPanel_->setIterations(static_cast<std::uint32_t>(
        settings_.value(QStringLiteral("iterations"), 100000).toUInt()));
  }
  settings_.endGroup();

  settings_.beginGroup(QStringLiteral("CryptoExtract"));
  {
    extractCryptoPanel_->setEncryptionEnabled(
        settings_.value(QStringLiteral("enabled"), false).toBool());
  }
  settings_.endGroup();

  settings_.beginGroup(QStringLiteral("Extract"));
  {
    const QSignalBlocker b(payloadSizeSpin_);
    payloadSizeSpin_->setValue(
        settings_.value(QStringLiteral("payloadSize"), 1024).toInt());
  }
  settings_.endGroup();

  settings_.beginGroup(QStringLiteral("Preview"));
  showPreview_ = settings_.value(QStringLiteral("showPreview"), true).toBool();
  previewMode_ = static_cast<PreviewMode>(
      settings_.value(QStringLiteral("mode"), 0).toInt());
  overlayOpacity_ = settings_.value(QStringLiteral("opacity"), 50).toInt();
  highlightChanges_ =
      settings_.value(QStringLiteral("highlight"), false).toBool();
  darkTheme_ = settings_.value(QStringLiteral("darkTheme"), false).toBool();
  language_ = settings_.value(QStringLiteral("language"), QStringLiteral("en"))
                  .toString();
  writeHeader_ = settings_.value(QStringLiteral("writeHeader"), true).toBool();
  settings_.endGroup();
}

void MainWindow::saveSettings() {
  settings_.beginGroup(QStringLiteral("MainWindow"));
  settings_.setValue(QStringLiteral("size"), size());
  settings_.setValue(QStringLiteral("pos"), pos());
  settings_.endGroup();

  settings_.beginGroup(QStringLiteral("ParamsEmbed"));
  settings_.setValue(QStringLiteral("bits"), embedBitsSpin_->value());
  settings_.setValue(QStringLiteral("seed"), embedSeedSpin_->value());
  settings_.setValue(QStringLiteral("red"), embedRed_->isChecked());
  settings_.setValue(QStringLiteral("green"), embedGreen_->isChecked());
  settings_.setValue(QStringLiteral("blue"), embedBlue_->isChecked());
  settings_.setValue(QStringLiteral("alpha"), embedAlpha_->isChecked());
  settings_.setValue(QStringLiteral("mode"), embedModeCombo_->currentIndex());
  settings_.setValue(QStringLiteral("window"),
                     embedWindowCombo_->currentIndex());
  settings_.setValue(QStringLiteral("metric"),
                     embedMetricCombo_->currentIndex());
  settings_.setValue(QStringLiteral("threshold"), embedThresholdEdit_->text());
  settings_.setValue(QStringLiteral("shuffle"),
                     embedShuffleCheck_->isChecked());
  settings_.endGroup();

  settings_.beginGroup(QStringLiteral("ParamsExtract"));
  settings_.setValue(QStringLiteral("bits"), extractBitsSpin_->value());
  settings_.setValue(QStringLiteral("seed"), extractSeedSpin_->value());
  settings_.setValue(QStringLiteral("red"), extractRed_->isChecked());
  settings_.setValue(QStringLiteral("green"), extractGreen_->isChecked());
  settings_.setValue(QStringLiteral("blue"), extractBlue_->isChecked());
  settings_.setValue(QStringLiteral("alpha"), extractAlpha_->isChecked());
  settings_.setValue(QStringLiteral("mode"), extractModeCombo_->currentIndex());
  settings_.setValue(QStringLiteral("window"),
                     extractWindowCombo_->currentIndex());
  settings_.setValue(QStringLiteral("metric"),
                     extractMetricCombo_->currentIndex());
  settings_.setValue(QStringLiteral("threshold"),
                     extractThresholdEdit_->text());
  settings_.setValue(QStringLiteral("shuffle"),
                     extractShuffleCheck_->isChecked());
  settings_.endGroup();

  settings_.beginGroup(QStringLiteral("CryptoEmbed"));
  settings_.setValue(QStringLiteral("enabled"),
                     embedCryptoPanel_->encryptionEnabled());
  settings_.setValue(QStringLiteral("cipher"),
                     static_cast<int>(embedCryptoPanel_->cipher()));
  settings_.setValue(QStringLiteral("kdf"),
                     static_cast<int>(embedCryptoPanel_->kdf()));
  settings_.setValue(QStringLiteral("iterations"),
                     static_cast<uint>(embedCryptoPanel_->iterations()));
  settings_.endGroup();

  settings_.beginGroup(QStringLiteral("CryptoExtract"));
  settings_.setValue(QStringLiteral("enabled"),
                     extractCryptoPanel_->encryptionEnabled());
  settings_.endGroup();

  settings_.beginGroup(QStringLiteral("Extract"));
  settings_.setValue(QStringLiteral("payloadSize"), payloadSizeSpin_->value());
  settings_.endGroup();

  settings_.beginGroup(QStringLiteral("Preview"));
  settings_.setValue(QStringLiteral("showPreview"), showPreview_);
  settings_.setValue(QStringLiteral("mode"), static_cast<int>(previewMode_));
  settings_.setValue(QStringLiteral("opacity"), overlayOpacity_);
  settings_.setValue(QStringLiteral("highlight"), highlightChanges_);
  settings_.setValue(QStringLiteral("darkTheme"), darkTheme_);
  settings_.setValue(QStringLiteral("language"), language_);
  settings_.setValue(QStringLiteral("writeHeader"), writeHeader_);
  settings_.endGroup();
}

void MainWindow::applySettings() {
  if (darkTheme_) {
    QPalette p;
    p.setColor(QPalette::Window, QColor(53, 53, 53));
    p.setColor(QPalette::WindowText, Qt::white);
    p.setColor(QPalette::Base, QColor(25, 25, 25));
    p.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    p.setColor(QPalette::ToolTipBase, Qt::white);
    p.setColor(QPalette::ToolTipText, Qt::white);
    p.setColor(QPalette::Text, Qt::white);
    p.setColor(QPalette::Button, QColor(53, 53, 53));
    p.setColor(QPalette::ButtonText, Qt::white);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(42, 130, 218));
    p.setColor(QPalette::Highlight, QColor(42, 130, 218));
    p.setColor(QPalette::HighlightedText, Qt::black);
    qApp->setPalette(p);

    qApp->setStyleSheet(
        QStringLiteral("QGroupBox { border: 1px solid #555; }"
                       "QLineEdit, QPlainTextEdit, QSpinBox, QComboBox { "
                       "background-color: #3c3c3c; }"
                       "QPushButton { background-color: #3c3c3c; }"
                       "QPushButton:hover { background-color: #4a4a4a; }"
                       "QGraphicsView { background-color: #1e1e1e; }"));
  } else {
    qApp->setPalette(qApp->style()->standardPalette());
    qApp->setStyleSheet(QString());
  }

  syncHeaderUiFromSettings();
  scheduleUpdate();
}

// ===========================================================================
//  Events
// ===========================================================================

void MainWindow::closeEvent(QCloseEvent *event) {
  saveSettings();
  event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  if (previewView_ && previewScene_ &&
      previewScene_->itemsBoundingRect().isValid()) {
    previewView_->fitInView(previewScene_->itemsBoundingRect(),
                            Qt::KeepAspectRatio);
  }
  updateMiniPreview();
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
  if (event->modifiers() & Qt::ControlModifier) {
    switch (event->key()) {
    case Qt::Key_O:
      browseInputImage();
      break;
    case Qt::Key_E:
      embedText();
      break;
    case Qt::Key_D:
      extractText();
      break;
    case Qt::Key_S:
      saveSettings();
      break;
    default:
      break;
    }
  } else if (event->key() == Qt::Key_F11) {
    onFullscreen();
  } else if (event->key() == Qt::Key_Escape) {
    if (previewView_ && previewView_->isFullScreen())
      previewView_->setWindowState(Qt::WindowNoState);
  }
  QMainWindow::keyPressEvent(event);
}

// ===========================================================================
//  Browse / Embed / Extract
// ===========================================================================

void MainWindow::browseInputImage() {
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Select input image"), QString(),
      tr("Images (*.png *.bmp *.tif *.tiff *.ppm *.pgm);;All files (*.*)"));
  if (path.isEmpty())
    return;

  inputImageEdit_->setText(path);
  beginBusy(tr("Loading image..."));

  (void)QtConcurrent::run([this, path]() {
    const auto result = rfp::gui::loadImageBuffer(path);
    QMetaObject::invokeMethod(this, [this, result, path]() {
      if (result) {
        currentImage_ = result.value();
        currentQImage_ = imageBufferToQImage(currentImage_.value());
        modifiedImage_.reset();
        modifiedQImage_.reset();
        updateMiniPreview();
        showImage(currentQImage_.value());
        updateStats(tr("Resolution: %1×%2, Channels: %3")
                        .arg(currentImage_->width)
                        .arg(currentImage_->height)
                        .arg(currentImage_->channels));
        endBusy();
        setStatus(tr("Loaded: %1").arg(path), 4000);
        scheduleUpdate();
      } else {
        endBusy();
        QMessageBox::warning(this, tr("R.F.P."),
                             QString::fromStdString(result.error().message));
        setStatus(tr("Error loading image"), 5000);
      }
    });
  });
}

void MainWindow::browseOutputImage() {
  const QString path = QFileDialog::getSaveFileName(
      this, tr("Select output image"), QString(),
      tr("PNG (*.png);;BMP (*.bmp);;PPM (*.ppm);;All files (*.*)"));
  if (!path.isEmpty())
    outputImageEdit_->setText(path);
}

void MainWindow::embedText() {
  if (embedding_)
    return;

  if (inputImageEdit_->text().isEmpty() || outputImageEdit_->text().isEmpty()) {
    QMessageBox::warning(this, tr("R.F.P."),
                         tr("Specify input and output image paths."));
    return;
  }

  const bool encrypting = embedCryptoPanel_->encryptionEnabled();
  if (encrypting) {
    if (embedCryptoPanel_->password().isEmpty()) {
      QMessageBox::warning(
          this, tr("R.F.P."),
          tr("Encryption is enabled but the password is empty."));
      return;
    }
    if (!embedCryptoPanel_->validatedPassword().has_value()) {
      QMessageBox::warning(this, tr("R.F.P."), tr("Passwords do not match."));
      return;
    }
  }

  const QByteArray utf8 = payloadEdit_->toPlainText().toUtf8();
  if (utf8.isEmpty()) {
    QMessageBox::warning(this, tr("R.F.P."), tr("Text payload is empty."));
    return;
  }
  if (!currentImage_) {
    QMessageBox::warning(this, tr("R.F.P."), tr("Load an input image first."));
    return;
  }

  const auto params = collectParams(ParamsRole::Embed);
  const auto capacity =
      rfp::stego::capacityBytes(currentImage_.value(), params);

  std::size_t needed = static_cast<std::size_t>(utf8.size());
  if (encrypting)
    needed += cryptoOverheadBytes(embedCryptoPanel_->cipher());
  if (writeHeader_)
    needed += 4;

  if (needed > capacity) {
    QMessageBox::warning(
        this, tr("R.F.P."),
        tr("Payload too large. Need %1 bytes, capacity is %2 bytes.")
            .arg(needed)
            .arg(capacity));
    return;
  }

  beginBusy(tr("Embedding..."));
  embedding_ = true;
  runEmbed(inputImageEdit_->text(), outputImageEdit_->text(), utf8);
}

void MainWindow::runEmbed(const QString &input, const QString &output,
                          const QByteArray &data) {
  pendingEmbedOutput_ = output;

  rfp::core::ByteBuffer inner;
  const bool encrypting = embedCryptoPanel_->encryptionEnabled();

  if (encrypting) {
    const auto ep = collectEncryptParams(embedCryptoPanel_->password());
    auto encResult = rfp::payload::encrypt(
        std::span<const rfp::core::Byte>(
            reinterpret_cast<const rfp::core::Byte *>(data.constData()),
            static_cast<std::size_t>(data.size())),
        ep);

    if (!encResult) {
      embedding_ = false;
      endBusy();
      QMessageBox::warning(
          this, tr("R.F.P."),
          tr("Encryption failed: %1")
              .arg(QString::fromStdString(encResult.error().message)));
      setStatus(tr("Encryption failed"), 5000);
      return;
    }
    inner = std::move(encResult.value());
  } else {
    inner.assign(reinterpret_cast<const rfp::core::Byte *>(data.constData()),
                 reinterpret_cast<const rfp::core::Byte *>(data.constData()) +
                     data.size());
  }

  rfp::core::ByteBuffer payload;
  if (writeHeader_) {
    const auto n = static_cast<std::uint32_t>(inner.size());
    payload.reserve(4 + inner.size());
    payload.push_back(static_cast<rfp::core::Byte>((n >> 24) & 0xFF));
    payload.push_back(static_cast<rfp::core::Byte>((n >> 16) & 0xFF));
    payload.push_back(static_cast<rfp::core::Byte>((n >> 8) & 0xFF));
    payload.push_back(static_cast<rfp::core::Byte>(n & 0xFF));
    payload.insert(payload.end(), inner.begin(), inner.end());
  } else {
    payload = std::move(inner);
  }

  const auto params = collectParams(ParamsRole::Embed);

  embedWatcher_.setFuture(QtConcurrent::run(
      [input, payload, params]() -> rfp::core::Result<rfp::stego::ImageBuffer> {
        auto imageResult = rfp::gui::loadImageBuffer(input);
        if (!imageResult)
          return imageResult.error();
        return rfp::stego::StegoEncoder::embedBytes(imageResult.value(),
                                                    payload, params);
      }));
}

void MainWindow::onEmbedFinished() {
  embedding_ = false;

  const auto result = embedWatcher_.result();
  if (!result) {
    endBusy();
    QMessageBox::warning(this, tr("R.F.P."),
                         QString::fromStdString(result.error().message));
    setStatus(tr("Embedding failed"), 5000);
    return;
  }

  modifiedImage_ = result.value();
  modifiedQImage_ = imageBufferToQImage(modifiedImage_.value());

  beginBusy(tr("Saving image..."));

  const auto saveResult =
      rfp::gui::saveImageBuffer(modifiedImage_.value(), pendingEmbedOutput_);
  if (!saveResult) {
    endBusy();
    QMessageBox::warning(this, tr("R.F.P."),
                         QString::fromStdString(saveResult.error().message));
    setStatus(tr("Save failed"), 5000);
    return;
  }

  const QByteArray originalData = payloadEdit_->toPlainText().toUtf8();
  const bool encrypted = embedCryptoPanel_->encryptionEnabled();

  updateStats(
      tr("Embedded %1 bytes%2%3.")
          .arg(originalData.size())
          .arg(encrypted ? tr(" (encrypted)") : QString())
          .arg(writeHeader_ ? tr(" (header included)") : tr(" (no header)")));

  const auto crc = rfp::core::crc32(std::span<const rfp::core::Byte>(
      reinterpret_cast<const rfp::core::Byte *>(originalData.constData()),
      static_cast<std::size_t>(originalData.size())));

  if (encrypted)
    embedCryptoPanel_->clearConfirm();

  endBusy();
  setStatus(tr("Embedded %1 bytes%2. CRC32: %3")
                .arg(originalData.size())
                .arg(encrypted ? tr(" (encrypted)") : QString())
                .arg(crcToText(crc)),
            8000);
  scheduleUpdate();
}

void MainWindow::extractText() {
  if (extracting_)
    return;

  if (inputImageExtractEdit_->text().isEmpty()) {
    QMessageBox::warning(this, tr("R.F.P."), tr("Specify input image path."));
    return;
  }

  const bool decrypting = extractCryptoPanel_->encryptionEnabled();
  if (decrypting && extractCryptoPanel_->password().isEmpty()) {
    QMessageBox::warning(
        this, tr("R.F.P."),
        tr("Decryption is enabled but the password is empty."));
    return;
  }

  beginBusy(tr("Extracting..."));
  extracting_ = true;
  runExtract(inputImageExtractEdit_->text());
}

void MainWindow::runExtract(const QString &input) {
  const auto params = collectParams(ParamsRole::Extract);

  const bool hasHeader = writeHeader_;
  const std::size_t manualSize =
      static_cast<std::size_t>(payloadSizeSpin_->value());

  const bool decrypting = extractCryptoPanel_->encryptionEnabled();
  const std::string password =
      decrypting ? extractCryptoPanel_->password().toStdString()
                 : std::string{};

  extractWatcher_.setFuture(QtConcurrent::run(
      [input, params, hasHeader, manualSize, decrypting,
       password]() -> rfp::core::Result<rfp::core::ByteBuffer> {
        auto imageResult = rfp::gui::loadImageBuffer(input);
        if (!imageResult)
          return imageResult.error();
        const auto &image = imageResult.value();

        const std::size_t capacity = rfp::stego::capacityBytes(image, params);
        if (capacity == 0) {
          return rfp::core::Error{
              rfp::core::ErrorCode::InvalidImageBuffer,
              "Image has zero capacity for these parameters"};
        }

        auto frameResult =
            rfp::stego::StegoDecoder::extractBytes(image, capacity, params);
        if (!frameResult)
          return frameResult.error();
        const auto &frame = frameResult.value();

        std::size_t payloadStart = 0;
        std::size_t payloadSize = 0;

        if (hasHeader) {
          // Layout produced by MainWindow::runEmbed:
          //   frame = [ size header: 4 bytes big-endian ][ inner: size bytes ]
          // The header stores the length of `inner`, i.e. of the
          // payload *without* the header itself.
          if (frame.size() < 4) {
            return rfp::core::Error{rfp::core::ErrorCode::InvalidImageBuffer,
                                    "Frame too short to contain a size header"};
          }
          payloadSize = (static_cast<std::uint32_t>(frame[0]) << 24) |
                        (static_cast<std::uint32_t>(frame[1]) << 16) |
                        (static_cast<std::uint32_t>(frame[2]) << 8) |
                        static_cast<std::uint32_t>(frame[3]);
          payloadStart = 4;

          // Overflow-safe check: never compute payloadStart + payloadSize.
          if (payloadSize == 0 || payloadSize > frame.size() - payloadStart) {
            return rfp::core::Error{
                rfp::core::ErrorCode::InvalidImageBuffer,
                "Invalid payload size in header. "
                "'Write payload size header' must match between "
                "embed and extract."};
          }
        } else {
          payloadSize = manualSize;
          if (payloadSize == 0 || payloadSize > frame.size()) {
            return rfp::core::Error{
                rfp::core::ErrorCode::InvalidArgument,
                "Manual payload size is out of range for this image."};
          }
        }

        rfp::core::ByteBuffer payload(
            frame.begin() + static_cast<std::ptrdiff_t>(payloadStart),
            frame.begin() +
                static_cast<std::ptrdiff_t>(payloadStart + payloadSize));
        if (!decrypting)
          return payload;

        rfp::payload::DecryptParams dp;
        dp.password = password;
        return rfp::payload::decrypt(payload, dp);
      }));
}

void MainWindow::onExtractFinished() {
  extracting_ = false;

  const auto result = extractWatcher_.result();
  if (!result) {
    endBusy();
    QMessageBox::warning(this, tr("R.F.P."),
                         QString::fromStdString(result.error().message));
    setStatus(tr("Extraction failed"), 5000);
    return;
  }

  const auto &data = result.value();
  const QByteArray decodedBytes(reinterpret_cast<const char *>(data.data()),
                                static_cast<qsizetype>(data.size()));
  extractedTextEdit_->setPlainText(QString::fromUtf8(decodedBytes));

  const auto crc = rfp::core::crc32(
      std::span<const rfp::core::Byte>(data.data(), data.size()));

  updateStats(tr("Extracted %1 bytes").arg(data.size()));
  endBusy();

  const bool decrypted = extractCryptoPanel_->encryptionEnabled();
  setStatus(tr("Extracted %1 bytes%2. CRC32: %3")
                .arg(data.size())
                .arg(decrypted ? tr(" (decrypted)") : QString())
                .arg(crcToText(crc)),
            8000);
}

// ===========================================================================
//  Debounced async recompute
// ===========================================================================

void MainWindow::scheduleUpdate() { updateTimer_->start(); }

void MainWindow::doUpdate() {
  if (!currentImage_) {
    capacityLabel_->setText(tr("Capacity: not loaded"));
    usageLabel_->setText(tr("Usage: no image"));
    previewScene_->clear();
    lastFittedSize_ = QSize();
    return;
  }

  if (recomputeInProgress_) {
    recomputePending_ = true;
    return;
  }

  const auto params = collectParams(ParamsRole::Embed);
  const QString payload = payloadEdit_->toPlainText();

  const bool showPreview = showPreview_;
  const auto previewMode = previewMode_;
  const int overlayOp = overlayOpacity_;
  const bool highlight = highlightChanges_;
  const bool writeHdr = writeHeader_;
  const auto imageCopy = currentImage_.value();
  const QImage curQImg = currentQImage_.value_or(QImage());
  const QImage modQImg = modifiedQImage_.value_or(QImage());
  const bool hasModified = modifiedQImage_.has_value();

  const bool encrypting = embedCryptoPanel_->encryptionEnabled();
  const std::size_t cryptoOverhead =
      encrypting ? cryptoOverheadBytes(embedCryptoPanel_->cipher()) : 0;

  recomputeInProgress_ = true;
  beginBusy(tr("Computing capacity and preview..."));

  previewWatcher_.setFuture(QtConcurrent::run([params, payload, showPreview,
                                               previewMode, overlayOp,
                                               highlight, writeHdr, imageCopy,
                                               curQImg, modQImg, hasModified,
                                               encrypting, cryptoOverhead]()
                                                  -> RecomputeResult {
    RecomputeResult res;

    const std::size_t capacity = rfp::stego::capacityBytes(imageCopy, params);
    const std::size_t capacityBits =
        rfp::stego::capacityBits(imageCopy, params);

    QString info =
        tr("Capacity: %1 bytes (%2 bits)").arg(capacity).arg(capacityBits);
    if (params.mode == rfp::stego::SlotSelectionMode::Dispersion) {
      auto uniformParams = params;
      uniformParams.mode = rfp::stego::SlotSelectionMode::Uniform;
      const std::size_t uniformBytes =
          rfp::stego::capacityBytes(imageCopy, uniformParams);
      if (uniformBytes > 0) {
        const double percent = static_cast<double>(capacity) /
                               static_cast<double>(uniformBytes) * 100.0;
        info += tr(" (using %1% of available)").arg(percent, 0, 'f', 1);
      }
    }
    res.capacityText = info;

    const QByteArray utf8 = payload.toUtf8();
    std::size_t used = static_cast<std::size_t>(utf8.size());
    if (encrypting)
      used += cryptoOverhead;
    if (writeHdr)
      used += 4;

    if (capacity == 0) {
      res.usageText = tr("Usage: 0 bytes (capacity 0)");
    } else {
      const double percent =
          static_cast<double>(used) / static_cast<double>(capacity) * 100.0;
      const QString color =
          (used <= capacity) ? QStringLiteral("green") : QStringLiteral("red");
      const QString tag = encrypting ? tr(" (encrypted)") : QString();
      res.usageText =
          tr("Usage: <span style=\"color:%1;\">%2 / %3 bytes (%4%)</span>%5")
              .arg(color)
              .arg(used)
              .arg(capacity)
              .arg(percent, 0, 'f', 1)
              .arg(tag);
    }

    if (!showPreview)
      return res;
    if (curQImg.isNull())
      return res;

    switch (previewMode) {
    case PreviewMode::Original:
      res.previewImage = curQImg;
      res.previewValid = true;
      break;

    case PreviewMode::DispersionOverlay: {
      double minVal = 0.0;
      double maxVal = 0.0;
      double meanVal = 0.0;
      const QImage overlay = MainWindow::generateDispersionOverlay(
          imageCopy, params, overlayOp, minVal, maxVal, meanVal);

      QImage result = curQImg;
      if (!overlay.isNull()) {
        QPainter painter(&result);
        painter.drawImage(0, 0, overlay);
      }
      res.previewImage = result;
      res.previewStats = tr("Dispersion: min=%1, max=%2, mean=%3")
                             .arg(minVal, 0, 'f', 2)
                             .arg(maxVal, 0, 'f', 2)
                             .arg(meanVal, 0, 'f', 2);
      res.previewValid = true;
      break;
    }

    case PreviewMode::Comparison: {
      if (!hasModified) {
        res.previewImage = curQImg;
        res.previewStats = tr("No modified image available. Embed data first.");
        res.previewValid = true;
        break;
      }
      res.previewImage =
          MainWindow::generateComparisonView(curQImg, modQImg, highlight);

      if (curQImg.size() == modQImg.size()) {
        const qint64 changed = MainWindow::countChangedPixels(curQImg, modQImg);
        const double percent = static_cast<double>(changed) /
                               (static_cast<double>(curQImg.width()) *
                                static_cast<double>(curQImg.height())) *
                               100.0;
        res.previewStats =
            tr("Changed pixels: %1 / %2 (%3%)")
                .arg(changed)
                .arg(static_cast<qint64>(curQImg.width()) * curQImg.height())
                .arg(percent, 0, 'f', 2);
      } else {
        res.previewStats = tr("Image sizes differ");
      }
      res.previewValid = true;
      break;
    }
    }
    return res;
  }));
}

void MainWindow::onPreviewReady() {
  recomputeInProgress_ = false;

  const auto &res = previewWatcher_.result();

  if (!res.capacityText.isEmpty())
    capacityLabel_->setText(res.capacityText);
  if (!res.usageText.isEmpty())
    usageLabel_->setText(res.usageText);

  if (res.previewValid && !res.previewImage.isNull()) {
    const bool sizeChanged = (lastFittedSize_ != res.previewImage.size());
    showImage(res.previewImage, sizeChanged);
    if (sizeChanged)
      lastFittedSize_ = res.previewImage.size();
    updateStats(res.previewStats);
  } else {
    previewScene_->clear();
    lastFittedSize_ = QSize();
    updateStats(QString());
  }

  endBusy();

  if (recomputePending_) {
    recomputePending_ = false;
    QTimer::singleShot(0, this, &MainWindow::doUpdate);
    return;
  }

  setStatus(tr("Ready"), 2000);
}

// ===========================================================================
//  Static helpers
// ===========================================================================

QImage
MainWindow::imageBufferToQImage(const rfp::stego::ImageBuffer &buffer) const {
  if (!buffer.isValid())
    return QImage();

  const QImage::Format fmt =
      (buffer.channels == 4) ? QImage::Format_RGBA8888 : QImage::Format_RGB888;

  QImage img(static_cast<int>(buffer.width), static_cast<int>(buffer.height),
             fmt);
  if (img.isNull())
    return QImage();

  const std::size_t rowBytes =
      static_cast<std::size_t>(buffer.width) * buffer.channels;
  for (int y = 0; y < img.height(); ++y) {
    const auto *src =
        buffer.pixels.data() + static_cast<std::size_t>(y) * rowBytes;
    std::memcpy(img.scanLine(y), src, rowBytes);
  }
  return img;
}

QImage
MainWindow::generateDispersionOverlay(const rfp::stego::ImageBuffer &buffer,
                                      const rfp::stego::StegoParams &params,
                                      int overlayOpacity, double &outMin,
                                      double &outMax, double &outMean) {
  outMin = outMax = outMean = 0.0;

  if (!buffer.isValid())
    return QImage();
  if (params.mode != rfp::stego::SlotSelectionMode::Dispersion)
    return QImage();

  const int width = static_cast<int>(buffer.width);
  const int height = static_cast<int>(buffer.height);
  const auto pixelCount =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

  rfp::stego::DispersionCalculator calc(buffer, params);

  std::vector<double> disp;
  disp.reserve(pixelCount);

  double sum = 0.0;
  double minVal = std::numeric_limits<double>::max();
  double maxVal = -std::numeric_limits<double>::max();

  for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
    double total = 0.0;
    int channelCount = 0;
    for (std::uint8_t ch = 0; ch < buffer.channels; ++ch) {
      if (rfp::stego::detail::channelEnabled(ch, params)) {
        total += calc.getDispersion(pixel, ch);
        ++channelCount;
      }
    }
    const double pixelDisp = (channelCount > 0) ? (total / channelCount) : 0.0;

    disp.push_back(pixelDisp);
    sum += pixelDisp;
    if (pixelDisp < minVal)
      minVal = pixelDisp;
    if (pixelDisp > maxVal)
      maxVal = pixelDisp;
  }

  outMin = minVal;
  outMax = maxVal;
  outMean = (pixelCount > 0) ? (sum / static_cast<double>(pixelCount)) : 0.0;

  QImage overlay(width, height, QImage::Format_ARGB32);
  if (overlay.isNull())
    return QImage();

  const int alpha = static_cast<int>(overlayOpacity * 2.55);

  for (int y = 0; y < height; ++y) {
    auto *line = reinterpret_cast<QRgb *>(overlay.scanLine(y));
    const std::size_t rowOffset = static_cast<std::size_t>(y) * width;
    for (int x = 0; x < width; ++x) {
      const double v = disp[rowOffset + static_cast<std::size_t>(x)];
      const QColor c = dispersionToColor(v, minVal, maxVal);
      line[x] = qRgba(c.red(), c.green(), c.blue(), alpha);
    }
  }
  return overlay;
}

QImage MainWindow::generateComparisonView(const QImage &original,
                                          const QImage &modified,
                                          bool highlightChanges) {
  if (original.isNull() || modified.isNull())
    return QImage();

  const int width = original.width() + modified.width() + 10;
  const int height = std::max(original.height(), modified.height());

  QImage combined(width, height, QImage::Format_RGB888);
  if (combined.isNull())
    return QImage();
  combined.fill(Qt::lightGray);

  QPainter painter(&combined);
  painter.drawImage(0, 0, original);
  painter.drawImage(original.width() + 10, 0, modified);

  if (highlightChanges) {
    const QImage mask = generateChangesMask(original, modified);
    if (!mask.isNull())
      painter.drawImage(original.width() + 10, 0, mask);
  }
  painter.end();
  return combined;
}

QImage MainWindow::generateChangesMask(const QImage &original,
                                       const QImage &modified) {
  if (original.isNull() || modified.isNull())
    return QImage();
  if (original.size() != modified.size())
    return QImage();

  const QImage a = (original.format() == QImage::Format_RGB888 ||
                    original.format() == QImage::Format_RGBA8888 ||
                    original.format() == QImage::Format_ARGB32)
                       ? original
                       : original.convertToFormat(QImage::Format_ARGB32);
  const QImage b = (modified.format() == QImage::Format_RGB888 ||
                    modified.format() == QImage::Format_RGBA8888 ||
                    modified.format() == QImage::Format_ARGB32)
                       ? modified
                       : modified.convertToFormat(QImage::Format_ARGB32);

  if (a.format() != b.format()) {
    const QImage aa = a.convertToFormat(QImage::Format_ARGB32);
    const QImage bb = b.convertToFormat(QImage::Format_ARGB32);
    return generateChangesMask(aa, bb);
  }

  const int width = a.width();
  const int height = a.height();

  QImage mask(width, height, QImage::Format_ARGB32);
  if (mask.isNull())
    return QImage();
  mask.fill(Qt::transparent);

  const QRgb highlight = qRgba(255, 0, 0, 128);
  const int bytesPerPixel = a.depth() / 8;
  const int rowBytes = width * bytesPerPixel;

  for (int y = 0; y < height; ++y) {
    const auto *lineA = a.constScanLine(y);
    const auto *lineB = b.constScanLine(y);
    auto *lineMask = reinterpret_cast<QRgb *>(mask.scanLine(y));

    if (std::memcmp(lineA, lineB, static_cast<std::size_t>(rowBytes)) == 0)
      continue;

    if (bytesPerPixel == 4) {
      const auto *pixA = reinterpret_cast<const QRgb *>(lineA);
      const auto *pixB = reinterpret_cast<const QRgb *>(lineB);
      for (int x = 0; x < width; ++x) {
        if (pixA[x] != pixB[x])
          lineMask[x] = highlight;
      }
    } else {
      for (int x = 0; x < width; ++x) {
        const int off = x * bytesPerPixel;
        if (std::memcmp(lineA + off, lineB + off,
                        static_cast<std::size_t>(bytesPerPixel)) != 0) {
          lineMask[x] = highlight;
        }
      }
    }
  }
  return mask;
}

qint64 MainWindow::countChangedPixels(const QImage &original,
                                      const QImage &modified) {
  if (original.isNull() || modified.isNull())
    return 0;
  if (original.size() != modified.size())
    return 0;
  if (original.format() != modified.format())
    return 0;

  const int width = original.width();
  const int height = original.height();
  const int bytesPerPixel = original.depth() / 8;
  const int rowBytes = width * bytesPerPixel;

  qint64 changed = 0;
  for (int y = 0; y < height; ++y) {
    const auto *lineA = original.constScanLine(y);
    const auto *lineB = modified.constScanLine(y);

    if (std::memcmp(lineA, lineB, static_cast<std::size_t>(rowBytes)) == 0)
      continue;

    if (bytesPerPixel == 4) {
      const auto *pixA = reinterpret_cast<const QRgb *>(lineA);
      const auto *pixB = reinterpret_cast<const QRgb *>(lineB);
      for (int x = 0; x < width; ++x)
        if (pixA[x] != pixB[x])
          ++changed;
    } else {
      for (int x = 0; x < width; ++x) {
        const int off = x * bytesPerPixel;
        if (std::memcmp(lineA + off, lineB + off,
                        static_cast<std::size_t>(bytesPerPixel)) != 0) {
          ++changed;
        }
      }
    }
  }
  return changed;
}

QColor MainWindow::dispersionToColor(double value, double minVal,
                                     double maxVal) {
  const double range = (maxVal > minVal) ? (maxVal - minVal) : 1.0;
  double norm = (value - minVal) / range;
  norm = std::clamp(norm, 0.0, 1.0);

  int r = 0;
  int g = 0;
  int b = 0;
  if (norm < 0.5) {
    const double t = norm / 0.5;
    r = 0;
    g = static_cast<int>(255.0 * t);
    b = static_cast<int>(255.0 * (1.0 - t));
  } else {
    const double t = (norm - 0.5) / 0.5;
    r = static_cast<int>(255.0 * t);
    g = static_cast<int>(255.0 * (1.0 - t));
    b = 0;
  }
  return QColor(r, g, b);
}

// ===========================================================================
//  Param serialization
// ===========================================================================

QString MainWindow::serializeFull(const rfp::stego::StegoParams &params,
                                  const QString &inputPath,
                                  const QString &outputPath) const {
  QString channels;
  if (params.useRedChannel)
    channels += QLatin1Char('R');
  if (params.useGreenChannel)
    channels += QLatin1Char('G');
  if (params.useBlueChannel)
    channels += QLatin1Char('B');
  if (params.useAlphaChannel)
    channels += QLatin1Char('A');

  QStringList parts;
  parts
      << QStringLiteral("v=1")
      << QStringLiteral("bits=%1").arg(static_cast<int>(params.bitsPerChannel))
      << QStringLiteral("seed=%1").arg(params.seed)
      << QStringLiteral("channels=%1").arg(channels)
      << QStringLiteral("mode=%1").arg(static_cast<int>(params.mode))
      << QStringLiteral("window=%1").arg(params.windowSize)
      << QStringLiteral("metric=%1").arg(static_cast<int>(params.metric))
      << QStringLiteral("threshold=%1").arg(params.dispersionThreshold)
      << QStringLiteral("shuffle=%1").arg(params.applyShuffleAfterSort ? 1 : 0)
      << QStringLiteral("header=%1").arg(writeHeader_ ? 1 : 0)
      << QStringLiteral("encrypt=%1")
             .arg(embedCryptoPanel_->encryptionEnabled() ? 1 : 0)
      << QStringLiteral("cipher=%1")
             .arg(static_cast<int>(embedCryptoPanel_->cipher()))
      << QStringLiteral("kdf=%1").arg(
             static_cast<int>(embedCryptoPanel_->kdf()))
      << QStringLiteral("iter=%1").arg(embedCryptoPanel_->iterations());

  if (!inputPath.isEmpty())
    parts << QStringLiteral("input=%1").arg(escapeParamValue(inputPath));
  if (!outputPath.isEmpty())
    parts << QStringLiteral("output=%1").arg(escapeParamValue(outputPath));

  return parts.join(QLatin1Char(';'));
}

bool MainWindow::deserializeFull(const QString &str,
                                 rfp::stego::StegoParams &params,
                                 QString &inputPath, QString &outputPath) {
  params.bitsPerChannel = 1;
  params.seed = 0;
  params.useRedChannel = true;
  params.useGreenChannel = true;
  params.useBlueChannel = true;
  params.useAlphaChannel = false;
  params.mode = rfp::stego::SlotSelectionMode::Uniform;
  params.windowSize = 5;
  params.metric = rfp::stego::DispersionMetric::Luminance;
  params.dispersionThreshold = 0.0;
  params.applyShuffleAfterSort = true;
  inputPath.clear();
  outputPath.clear();

  bool anyRecognised = false;

  const QStringList tokens = str.split(QLatin1Char(';'), Qt::SkipEmptyParts);
  for (const QString &token : tokens) {
    const int eq = token.indexOf(QLatin1Char('='));
    if (eq <= 0)
      continue;

    const QString key = token.left(eq);
    const QString value = token.mid(eq + 1);

    if (key == QLatin1String("bits")) {
      const int v = value.toInt();
      if (v >= 1 && v <= 4) {
        params.bitsPerChannel = static_cast<std::uint8_t>(v);
        anyRecognised = true;
      }
    } else if (key == QLatin1String("seed")) {
      params.seed = value.toUInt();
      anyRecognised = true;
    } else if (key == QLatin1String("channels")) {
      params.useRedChannel = value.contains(QLatin1Char('R'));
      params.useGreenChannel = value.contains(QLatin1Char('G'));
      params.useBlueChannel = value.contains(QLatin1Char('B'));
      params.useAlphaChannel = value.contains(QLatin1Char('A'));
      anyRecognised = true;
    } else if (key == QLatin1String("mode")) {
      params.mode =
          enumFromIntOrDefault(value.toInt(),
                               {rfp::stego::SlotSelectionMode::Uniform,
                                rfp::stego::SlotSelectionMode::Dispersion},
                               rfp::stego::SlotSelectionMode::Uniform);
      anyRecognised = true;
    } else if (key == QLatin1String("window")) {
      const int v = value.toInt();
      if (isValidWindowSize(v)) {
        params.windowSize = v;
        anyRecognised = true;
      }
    } else if (key == QLatin1String("metric")) {
      params.metric =
          enumFromIntOrDefault(value.toInt(),
                               {rfp::stego::DispersionMetric::Luminance,
                                rfp::stego::DispersionMetric::PerChannel,
                                rfp::stego::DispersionMetric::Sum},
                               rfp::stego::DispersionMetric::Luminance);
      anyRecognised = true;
    } else if (key == QLatin1String("threshold")) {
      const double v = value.toDouble();
      if (v >= 0.0) {
        params.dispersionThreshold = v;
        anyRecognised = true;
      }
    } else if (key == QLatin1String("shuffle")) {
      params.applyShuffleAfterSort = (value.toInt() != 0);
      anyRecognised = true;
    } else if (key == QLatin1String("header")) {
      writeHeader_ = (value.toInt() != 0);
      anyRecognised = true;
    } else if (key == QLatin1String("encrypt")) {
      embedCryptoPanel_->setEncryptionEnabled(value.toInt() != 0);
      anyRecognised = true;
    } else if (key == QLatin1String("cipher")) {
      embedCryptoPanel_->setCipher(enumFromIntOrDefault(
          value.toInt(),
          {rfp::crypto::CipherId::Aes128Gcm, rfp::crypto::CipherId::Aes256Gcm,
           rfp::crypto::CipherId::ChaCha20Poly1305,
           rfp::crypto::CipherId::Aes256Cbc, rfp::crypto::CipherId::Aes256Ctr},
          rfp::crypto::CipherId::Aes256Gcm));
      anyRecognised = true;
    } else if (key == QLatin1String("kdf")) {
      embedCryptoPanel_->setKdf(enumFromIntOrDefault(
          value.toInt(),
          {rfp::crypto::KdfId::Pbkdf2HmacSha256,
           rfp::crypto::KdfId::Pbkdf2HmacSha512, rfp::crypto::KdfId::Scrypt},
          rfp::crypto::KdfId::Pbkdf2HmacSha256));
      anyRecognised = true;
    } else if (key == QLatin1String("iter")) {
      const auto v = value.toUInt();
      if (v >= 1000u && v <= 100'000'000u) {
        embedCryptoPanel_->setIterations(v);
        anyRecognised = true;
      }
    } else if (key == QLatin1String("input")) {
      inputPath = unescapeParamValue(value);
      anyRecognised = true;
    } else if (key == QLatin1String("output")) {
      outputPath = unescapeParamValue(value);
      anyRecognised = true;
    }
  }

  return anyRecognised;
}

// ===========================================================================
//  Apply parsed parameters to the UI
// ===========================================================================

void MainWindow::applyParamsToEmbedUi(const rfp::stego::StegoParams &params) {
  const QSignalBlocker b1(embedBitsSpin_);
  const QSignalBlocker b2(embedSeedSpin_);
  const QSignalBlocker b3(embedRed_);
  const QSignalBlocker b4(embedGreen_);
  const QSignalBlocker b5(embedBlue_);
  const QSignalBlocker b6(embedAlpha_);
  const QSignalBlocker b7(embedModeCombo_);
  const QSignalBlocker b8(embedWindowCombo_);
  const QSignalBlocker b9(embedMetricCombo_);
  const QSignalBlocker b10(embedThresholdEdit_);
  const QSignalBlocker b11(embedShuffleCheck_);

  embedBitsSpin_->setValue(params.bitsPerChannel);
  embedSeedSpin_->setValue(static_cast<int>(params.seed));
  embedRed_->setChecked(params.useRedChannel);
  embedGreen_->setChecked(params.useGreenChannel);
  embedBlue_->setChecked(params.useBlueChannel);
  embedAlpha_->setChecked(params.useAlphaChannel);

  if (const int idx = embedModeCombo_->findData(static_cast<int>(params.mode));
      idx >= 0)
    embedModeCombo_->setCurrentIndex(idx);
  if (const int idx = embedWindowCombo_->findData(params.windowSize); idx >= 0)
    embedWindowCombo_->setCurrentIndex(idx);
  if (const int idx =
          embedMetricCombo_->findData(static_cast<int>(params.metric));
      idx >= 0)
    embedMetricCombo_->setCurrentIndex(idx);

  embedThresholdEdit_->setText(
      QString::number(params.dispersionThreshold, 'f', 2));
  embedShuffleCheck_->setChecked(params.applyShuffleAfterSort);
}

void MainWindow::applyParamsToExtractUi(const rfp::stego::StegoParams &params) {
  const QSignalBlocker b1(extractBitsSpin_);
  const QSignalBlocker b2(extractSeedSpin_);
  const QSignalBlocker b3(extractRed_);
  const QSignalBlocker b4(extractGreen_);
  const QSignalBlocker b5(extractBlue_);
  const QSignalBlocker b6(extractAlpha_);
  const QSignalBlocker b7(extractModeCombo_);
  const QSignalBlocker b8(extractWindowCombo_);
  const QSignalBlocker b9(extractMetricCombo_);
  const QSignalBlocker b10(extractThresholdEdit_);
  const QSignalBlocker b11(extractShuffleCheck_);

  extractBitsSpin_->setValue(params.bitsPerChannel);
  extractSeedSpin_->setValue(static_cast<int>(params.seed));
  extractRed_->setChecked(params.useRedChannel);
  extractGreen_->setChecked(params.useGreenChannel);
  extractBlue_->setChecked(params.useBlueChannel);
  extractAlpha_->setChecked(params.useAlphaChannel);

  if (const int idx =
          extractModeCombo_->findData(static_cast<int>(params.mode));
      idx >= 0)
    extractModeCombo_->setCurrentIndex(idx);
  if (const int idx = extractWindowCombo_->findData(params.windowSize);
      idx >= 0)
    extractWindowCombo_->setCurrentIndex(idx);
  if (const int idx =
          extractMetricCombo_->findData(static_cast<int>(params.metric));
      idx >= 0)
    extractMetricCombo_->setCurrentIndex(idx);

  extractThresholdEdit_->setText(
      QString::number(params.dispersionThreshold, 'f', 2));
  extractShuffleCheck_->setChecked(params.applyShuffleAfterSort);
}

// ===========================================================================
//  Copy / Paste
// ===========================================================================

void MainWindow::copyEmbedParams() {
  const auto params = collectParams(ParamsRole::Embed);
  const QString str =
      serializeFull(params, inputImageEdit_->text(), outputImageEdit_->text());
  QApplication::clipboard()->setText(str);
  setStatus(tr("Embedding parameters (with paths) copied to clipboard."), 2000);
}

void MainWindow::pasteEmbedParams() {
  const QString str = QApplication::clipboard()->text();
  if (str.isEmpty()) {
    setStatus(tr("Clipboard is empty."), 2000);
    return;
  }

  rfp::stego::StegoParams params;
  QString inputPath;
  QString outputPath;
  if (!deserializeFull(str, params, inputPath, outputPath)) {
    setStatus(tr("Failed to parse parameters from clipboard."), 2000);
    return;
  }

  applyParamsToEmbedUi(params);

  if (!inputPath.isEmpty())
    inputImageEdit_->setText(inputPath);
  if (!outputPath.isEmpty())
    outputImageEdit_->setText(outputPath);

  syncHeaderUiFromSettings();

  setStatus(tr("Embedding parameters pasted from clipboard."), 2000);
  scheduleUpdate();
}

void MainWindow::copyExtractParams() {
  const auto params = collectParams(ParamsRole::Extract);
  const QString str = serializeFull(params, QString(), QString());
  QApplication::clipboard()->setText(str);
  setStatus(tr("Extraction parameters copied to clipboard."), 2000);
}

void MainWindow::pasteExtractParams() {
  const QString str = QApplication::clipboard()->text();
  if (str.isEmpty()) {
    setStatus(tr("Clipboard is empty."), 2000);
    return;
  }

  rfp::stego::StegoParams params;
  QString inputPath;
  QString outputPath;
  if (!deserializeFull(str, params, inputPath, outputPath)) {
    setStatus(tr("Failed to parse parameters from clipboard."), 2000);
    return;
  }

  applyParamsToExtractUi(params);

  syncHeaderUiFromSettings();

  if (!outputPath.isEmpty()) {
    inputImageExtractEdit_->setText(outputPath);
    setStatus(tr("Extraction parameters pasted and input path set to output "
                 "from copied data."),
              2000);
  } else {
    setStatus(tr("Extraction parameters pasted (no path information)."), 2000);
  }
  scheduleUpdate();
}

void MainWindow::syncHeaderUiFromSettings() {
  {
    const QSignalBlocker b(autoDetectSizeCheck_);
    autoDetectSizeCheck_->setChecked(writeHeader_);
  }
  payloadSizeSpin_->setEnabled(!writeHeader_);
}

// ===========================================================================
//  Param collection
// ===========================================================================

rfp::stego::StegoParams MainWindow::collectParams(ParamsRole role) const {
  rfp::stego::StegoParams params;

  if (role == ParamsRole::Extract) {
    params.bitsPerChannel =
        static_cast<std::uint8_t>(extractBitsSpin_->value());
    params.seed = static_cast<std::uint32_t>(extractSeedSpin_->value());
    params.useRedChannel = extractRed_->isChecked();
    params.useGreenChannel = extractGreen_->isChecked();
    params.useBlueChannel = extractBlue_->isChecked();
    params.useAlphaChannel = extractAlpha_->isChecked();
    params.mode = static_cast<rfp::stego::SlotSelectionMode>(
        extractModeCombo_->currentData().toInt());
    params.windowSize = extractWindowCombo_->currentData().toInt();
    params.metric = static_cast<rfp::stego::DispersionMetric>(
        extractMetricCombo_->currentData().toInt());
    params.dispersionThreshold = extractThresholdEdit_->text().toDouble();
    params.applyShuffleAfterSort = extractShuffleCheck_->isChecked();
  } else {
    params.bitsPerChannel = static_cast<std::uint8_t>(embedBitsSpin_->value());
    params.seed = static_cast<std::uint32_t>(embedSeedSpin_->value());
    params.useRedChannel = embedRed_->isChecked();
    params.useGreenChannel = embedGreen_->isChecked();
    params.useBlueChannel = embedBlue_->isChecked();
    params.useAlphaChannel = embedAlpha_->isChecked();
    params.mode = static_cast<rfp::stego::SlotSelectionMode>(
        embedModeCombo_->currentData().toInt());
    params.windowSize = embedWindowCombo_->currentData().toInt();
    params.metric = static_cast<rfp::stego::DispersionMetric>(
        embedMetricCombo_->currentData().toInt());
    params.dispersionThreshold = embedThresholdEdit_->text().toDouble();
    params.applyShuffleAfterSort = embedShuffleCheck_->isChecked();
  }
  return params;
}

rfp::payload::EncryptParams
MainWindow::collectEncryptParams(const QString &password) const {
  rfp::payload::EncryptParams p;
  p.password = password.toStdString();
  p.cipher = embedCryptoPanel_->cipher();
  p.kdf = embedCryptoPanel_->kdf();
  p.iterations = embedCryptoPanel_->iterations();
  return p;
}

// ===========================================================================
//  Preview / status helpers
// ===========================================================================

void MainWindow::showImage(const QImage &image, bool fit) {
  previewScene_->clear();
  if (image.isNull())
    return;

  previewScene_->addPixmap(QPixmap::fromImage(image));

  if (fit) {
    previewView_->fitInView(previewScene_->itemsBoundingRect(),
                            Qt::KeepAspectRatio);
  }
}

void MainWindow::updateMiniPreview() {
  if (!currentQImage_ || currentQImage_->isNull()) {
    miniPreviewLabel_->setPixmap(QPixmap());
    miniPreviewLabel_->setText(tr("No image"));
    return;
  }

  const QPixmap pix = QPixmap::fromImage(currentQImage_.value());
  if (pix.isNull()) {
    miniPreviewLabel_->setPixmap(QPixmap());
    miniPreviewLabel_->setText(tr("No image"));
    return;
  }

  int h = miniPreviewLabel_->height();
  if (h <= 0)
    h = 120;
  miniPreviewLabel_->setPixmap(pix.scaledToHeight(h, Qt::SmoothTransformation));
  miniPreviewLabel_->setText(QString());
}

void MainWindow::setStatus(const QString &text, int timeoutMs) {
  statusLabel_->setText(text);
  if (timeoutMs > 0) {
    QTimer::singleShot(timeoutMs, this, [this, text]() {
      if (statusLabel_->text() == text)
        statusLabel_->clear();
    });
  }
}

void MainWindow::updateStats(const QString &text) {
  statsLabel_->setText(text);
}

void MainWindow::beginBusy(const QString &message) {
  ++busyCounter_;
  progressBar_->setRange(0, 0);
  progressBar_->setVisible(true);
  statusLabel_->setText(message);
}

void MainWindow::endBusy() {
  Q_ASSERT(busyCounter_ > 0);
  if (busyCounter_ > 0)
    --busyCounter_;
  if (busyCounter_ == 0)
    progressBar_->setVisible(false);
}

// ===========================================================================
//  Toolbar actions
// ===========================================================================

void MainWindow::onFullscreen() {
  auto *dlg = new QDialog(this);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowTitle(tr("Preview"));
  dlg->setStyleSheet(QStringLiteral("background-color: black;"));

  auto *view = new QGraphicsView(dlg);
  view->setScene(previewScene_);
  view->setRenderHint(QPainter::Antialiasing);
  view->setBackgroundBrush(Qt::black);
  view->setAlignment(Qt::AlignCenter);
  view->setDragMode(QGraphicsView::ScrollHandDrag);
  view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  view->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
  view->setFrameShape(QFrame::NoFrame);

  auto *layout = new QVBoxLayout(dlg);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(view);

  dlg->showFullScreen();
  if (previewScene_->itemsBoundingRect().isValid())
    view->fitInView(previewScene_->itemsBoundingRect(), Qt::KeepAspectRatio);

  setStatus(tr("Fullscreen: press Esc to exit"), 3000);
}

void MainWindow::showSettings() {
  if (!settingsDialog_) {
    settingsDialog_ = new SettingsDialog(this);

    connect(settingsDialog_, &SettingsDialog::settingsChanged, this, [this]() {
      const QString newLang = settingsDialog_->language();

      darkTheme_ = settingsDialog_->isDarkTheme();
      overlayOpacity_ = settingsDialog_->overlayOpacity();
      showPreview_ = settingsDialog_->showPreview();
      previewMode_ = settingsDialog_->previewMode();
      highlightChanges_ = settingsDialog_->highlightChanges();
      writeHeader_ = settingsDialog_->writeHeader();

      if (newLang != language_) {
        language_ = newLang;
        saveSettings();
        const QString exePath = QCoreApplication::applicationFilePath();
        qApp->quit();
        QProcess::startDetached(exePath, QStringList());
        return;
      }
      applySettings();
      scheduleUpdate();
    });
  }

  settingsDialog_->setDarkTheme(darkTheme_);
  settingsDialog_->setLanguage(language_);
  settingsDialog_->setOverlayOpacity(overlayOpacity_);
  settingsDialog_->setShowPreview(showPreview_);
  settingsDialog_->setPreviewMode(previewMode_);
  settingsDialog_->setHighlightChanges(highlightChanges_);
  settingsDialog_->setWriteHeader(writeHeader_);

  settingsDialog_->show();
  settingsDialog_->raise();
  settingsDialog_->activateWindow();
}

void MainWindow::showMasking() {
  if (!maskingDialog_) {
    maskingDialog_ = new MaskingDialog(this);
    connect(maskingDialog_, &MaskingDialog::maskingRequested, this,
            &MainWindow::runMasking);
  }
  if (!inputImageEdit_->text().isEmpty()) {
    const QFileInfo info(inputImageEdit_->text());
    maskingDialog_->setDefaultDirectory(info.absolutePath());
    maskingDialog_->setExcludeFile(inputImageEdit_->text());
  }
  maskingDialog_->show();
  maskingDialog_->raise();
  maskingDialog_->activateWindow();
}

void MainWindow::showHelp() {
  if (!helpDialog_)
    helpDialog_ = new HelpDialog(this);
  helpDialog_->setLanguage(language_);
  helpDialog_->show();
  helpDialog_->raise();
  helpDialog_->activateWindow();
}

// ===========================================================================
//  Access masking (utility)
// ===========================================================================

void MainWindow::runMasking(const QString &dir, const QString &ext, int count,
                            bool recursive, const QString &exclude) {
  beginBusy(tr("Masking..."));
  if (maskingDialog_)
    maskingDialog_->setBusy(true);

  (void)QtConcurrent::run([this, dir, ext, count, recursive, exclude]() {
    QDir directory(dir);
    if (!directory.exists()) {
      QMetaObject::invokeMethod(this, [this, dir]() {
        if (maskingDialog_) {
          maskingDialog_->setBusy(false);
          maskingDialog_->appendLog(
              tr("Directory does not exist: %1").arg(dir));
        }
        endBusy();
      });
      return;
    }

    QStringList filters;
    if (ext.isEmpty())
      filters << QStringLiteral("*");
    else
      filters << QStringLiteral("*.") + ext;

    QDir::Filters dirFilter = QDir::Files | QDir::Readable;
    if (recursive)
      dirFilter |= QDir::AllDirs;

    QStringList files = directory.entryList(filters, dirFilter, QDir::Name);
    if (!exclude.isEmpty())
      files.removeAll(QFileInfo(exclude).fileName());

    if (files.size() > count)
      files = files.mid(0, count);

    int touched = 0;
    for (const QString &f : files) {
      QFile file(directory.absoluteFilePath(f));
      if (!file.open(QIODevice::ReadOnly))
        continue;
      (void)file.read(1);
      file.close();
      ++touched;
    }

    QMetaObject::invokeMethod(this, [this, touched]() {
      if (maskingDialog_) {
        maskingDialog_->setBusy(false);
        maskingDialog_->appendLog(
            tr("Masking completed. Files touched: %1").arg(touched));
      }
      endBusy();
      setStatus(tr("Masking completed"), 4000);
    });
  });
}