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
#include <QSignalBlocker>
#include <QDialog>

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QProcess>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QDebug>

#include <algorithm>
#include <cstring>
#include <limits>
#include <numeric>
#include <sstream>

// ============================================================================
//  Anonymous namespace
// ============================================================================
namespace {

QString crcToText(std::uint32_t crc) {
    return QStringLiteral("%1").arg(crc, 8, 16, QLatin1Char('0')).toUpper();
}

double computeAutoThreshold(const rfp::stego::ImageBuffer &image,
                            const rfp::stego::StegoParams &params) {
    if (!image.isValid()) return 0.0;
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
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t idx = static_cast<std::size_t>(static_cast<double>(values.size()) * 0.7);
    return values[idx];
}

} // namespace

// ============================================================================
//  Crypto overhead (static)
// ============================================================================
std::size_t MainWindow::cryptoOverheadBytes(rfp::crypto::CipherId id) noexcept
{
    //  Header (16B) + salt (16B) + iv + tag + CBC padding (16B worst case).
    constexpr std::size_t kHeader = 16;
    constexpr std::size_t kSalt   = 16;

    const auto spec = rfp::crypto::CryptoRegistry::cipherSpec(id);
    if (!spec) return kHeader + kSalt;

    std::size_t total = kHeader + kSalt + spec->ivSize + spec->tagSize;
    if (spec->kind == rfp::crypto::CipherKind::BlockCbc)
        total += 16;
    return total;
}

// ============================================================================
//  Constructor / Destructor
// ============================================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), settings_("RFP", "RFP-GUI") {
    setupUi();
    setupConnections();

    updateTimer_ = new QTimer(this);
    updateTimer_->setSingleShot(true);
    updateTimer_->setInterval(300);
    connect(updateTimer_, &QTimer::timeout, this, &MainWindow::doUpdate);

    connect(&previewWatcher_, &QFutureWatcher<RecomputeResult>::finished,
            this, &MainWindow::onPreviewReady);

    loadSettings();
    applySettings();
    QTimer::singleShot(0, this, &MainWindow::scheduleUpdate);

    setWindowTitle(tr("R.F.P. - Resilient File Protector"));
    resize(1200, 700);
    if (statusLabel_) statusLabel_->setText(tr("Ready"));
}

MainWindow::~MainWindow() {
    saveSettings();
}

// ============================================================================
//  setupUi
// ============================================================================
void MainWindow::setupUi() {
    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto *toolBar = addToolBar(tr("Tools"));
    toolBar->setObjectName("mainToolBar");
    settingsButton_ = new QPushButton(tr("Settings"), this);
    maskingButton_ = new QPushButton(tr("Masking"), this);
    fullscreenButton_ = new QPushButton(tr("Fullscreen"), this);
    helpButton_ = new QPushButton(tr("Help"), this);
    toolBar->addWidget(settingsButton_);
    toolBar->addWidget(maskingButton_);
    toolBar->addWidget(fullscreenButton_);
    toolBar->addWidget(helpButton_);

    tabWidget_ = new QTabWidget(this);

    // ---------- Embed tab ----------
    embedTab_ = new QWidget(this);
    auto *embedLayout = new QHBoxLayout(embedTab_);

    auto *embedLeft = new QWidget(embedTab_);
    auto *embedLeftLayout = new QVBoxLayout(embedLeft);

    auto *filesGroup = new QGroupBox(tr("Images"), embedLeft);
    auto *filesLayout = new QGridLayout(filesGroup);
    inputImageEdit_ = new QLineEdit(filesGroup);
    outputImageEdit_ = new QLineEdit(filesGroup);
    auto *browseInputBtn = new QPushButton(tr("Browse..."), filesGroup);
    browseInputBtn->setObjectName("browseInputBtn");
    auto *browseOutputBtn = new QPushButton(tr("Browse..."), filesGroup);
    browseOutputBtn->setObjectName("browseOutputBtn");
    miniPreviewLabel_ = new QLabel(filesGroup);
    miniPreviewLabel_->setFixedHeight(120);
    miniPreviewLabel_->setScaledContents(false);
    miniPreviewLabel_->setAlignment(Qt::AlignCenter);
    miniPreviewLabel_->setStyleSheet(
        "QLabel { background-color: #333; border: 1px solid #555; }");
    miniPreviewLabel_->setText(tr("No image"));

    filesLayout->addWidget(new QLabel(tr("Input:"), filesGroup), 0, 0);
    filesLayout->addWidget(inputImageEdit_, 0, 1);
    filesLayout->addWidget(browseInputBtn, 0, 2);
    filesLayout->addWidget(new QLabel(tr("Output:"), filesGroup), 1, 0);
    filesLayout->addWidget(outputImageEdit_, 1, 1);
    filesLayout->addWidget(browseOutputBtn, 1, 2);
    filesLayout->addWidget(miniPreviewLabel_, 0, 3, 2, 1);
    embedLeftLayout->addWidget(filesGroup);

    auto *payloadGroup = new QGroupBox(tr("Text payload"), embedLeft);
    auto *payloadLayout = new QVBoxLayout(payloadGroup);
    payloadEdit_ = new QPlainTextEdit(payloadGroup);
    payloadEdit_->setPlaceholderText(tr("Enter text to hide..."));
    payloadLayout->addWidget(payloadEdit_);
    usageLabel_ = new QLabel(tr("Usage: 0 bytes"), payloadGroup);
    usageLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    payloadLayout->addWidget(usageLabel_);
    embedLeftLayout->addWidget(payloadGroup, 1);

    // Encryption panel (Embed)
    embedCryptoPanel_ = new CryptoPanel(CryptoPanel::Role::Embed, embedLeft);
    embedLeftLayout->addWidget(embedCryptoPanel_);

    auto *embedParamsGroup = new QGroupBox(tr("Steganography parameters (Embed)"), embedLeft);
    embedParamsGroup->setCheckable(true);
    embedParamsGroup->setChecked(true);
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

    auto *embedSmartGroup = new QGroupBox(tr("Smart selection"), embedParamsGroup);
    auto *embedSmartLayout = new QFormLayout(embedSmartGroup);
    embedModeCombo_ = new QComboBox(embedSmartGroup);
    embedModeCombo_->addItem(tr("Uniform"), static_cast<int>(rfp::stego::SlotSelectionMode::Uniform));
    embedModeCombo_->addItem(tr("Smart (dispersion)"), static_cast<int>(rfp::stego::SlotSelectionMode::Dispersion));
    embedModeCombo_->setCurrentIndex(0);

    embedWindowCombo_ = new QComboBox(embedSmartGroup);
    for (int s : {3, 5, 7, 9, 11, 13}) embedWindowCombo_->addItem(QString::number(s), s);
    embedWindowCombo_->setCurrentIndex(0);

    embedMetricCombo_ = new QComboBox(embedSmartGroup);
    embedMetricCombo_->addItem(tr("Luminance"), static_cast<int>(rfp::stego::DispersionMetric::Luminance));
    embedMetricCombo_->addItem(tr("Per-channel"), static_cast<int>(rfp::stego::DispersionMetric::PerChannel));
    embedMetricCombo_->addItem(tr("Sum"), static_cast<int>(rfp::stego::DispersionMetric::Sum));

    embedThresholdEdit_ = new QLineEdit(embedSmartGroup);
    embedThresholdEdit_->setText("0.0");
    embedThresholdEdit_->setValidator(new QDoubleValidator(0.0, 100000.0, 2, embedThresholdEdit_));
    embedAutoThresholdBtn_ = new QPushButton(tr("Auto"), embedSmartGroup);
    auto *thresholdWidgetEmbed = new QWidget(embedSmartGroup);
    auto *thresholdLayoutEmbed = new QHBoxLayout(thresholdWidgetEmbed);
    thresholdLayoutEmbed->setContentsMargins(0, 0, 0, 0);
    thresholdLayoutEmbed->addWidget(embedThresholdEdit_, 1);
    thresholdLayoutEmbed->addWidget(embedAutoThresholdBtn_);

    embedShuffleCheck_ = new QCheckBox(tr("Apply shuffle after sorting"), embedSmartGroup);
    embedShuffleCheck_->setChecked(true);

    embedSmartLayout->addRow(tr("Mode:"), embedModeCombo_);
    embedSmartLayout->addRow(tr("Window size:"), embedWindowCombo_);
    embedSmartLayout->addRow(tr("Dispersion metric:"), embedMetricCombo_);
    embedSmartLayout->addRow(tr("Threshold:"), thresholdWidgetEmbed);
    embedSmartLayout->addRow(embedShuffleCheck_);

    embedParamsLayout->addRow(embedSmartGroup);

    auto *embedButtonsLayout = new QHBoxLayout;
    auto *copyEmbedBtn = new QPushButton(tr("Copy params"), embedParamsGroup);
    copyEmbedBtn->setObjectName("copyEmbedBtn");
    auto *pasteEmbedBtn = new QPushButton(tr("Paste params"), embedParamsGroup);
    pasteEmbedBtn->setObjectName("pasteEmbedBtn");
    embedButtonsLayout->addWidget(copyEmbedBtn);
    embedButtonsLayout->addWidget(pasteEmbedBtn);
    embedButtonsLayout->addStretch();
    embedParamsLayout->addRow(embedButtonsLayout);

    capacityLabel_ = new QLabel(tr("Capacity: not loaded"), embedParamsGroup);
    capacityLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    embedParamsLayout->addRow(capacityLabel_);

    embedLeftLayout->addWidget(embedParamsGroup);

    auto *actionsLayout = new QHBoxLayout;
    auto *embedBtn = new QPushButton(tr("Embed"), embedLeft);
    embedBtn->setObjectName("embedBtn");
    embedBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    actionsLayout->addWidget(embedBtn);
    actionsLayout->addStretch();
    embedLeftLayout->addLayout(actionsLayout);

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

    // ---------- Extract tab ----------
    extractTab_ = new QWidget(this);
    auto *extractLayout = new QHBoxLayout(extractTab_);

    auto *extractLeft = new QWidget(extractTab_);
    auto *extractLeftLayout = new QVBoxLayout(extractLeft);

    auto *extractFilesGroup = new QGroupBox(tr("Image"), extractLeft);
    auto *extractFilesLayout = new QGridLayout(extractFilesGroup);
    inputImageExtractEdit_ = new QLineEdit(extractFilesGroup);
    auto *browseExtractBtn = new QPushButton(tr("Browse..."), extractFilesGroup);
    browseExtractBtn->setObjectName("browseExtractBtn");
    extractFilesLayout->addWidget(new QLabel(tr("Input:"), extractFilesGroup), 0, 0);
    extractFilesLayout->addWidget(inputImageExtractEdit_, 0, 1);
    extractFilesLayout->addWidget(browseExtractBtn, 0, 2);
    extractLeftLayout->addWidget(extractFilesGroup);

    auto *extractParamsGroup = new QGroupBox(tr("Extraction options"), extractLeft);
    auto *extractParamsLayout = new QFormLayout(extractParamsGroup);

    // Auto-detect is a visual mirror of Settings → "Write payload size header".
    autoDetectSizeCheck_ = new QCheckBox(
        tr("Header written at embed time (auto-detect payload size)"),
        extractParamsGroup);
    autoDetectSizeCheck_->setChecked(true);
    autoDetectSizeCheck_->setEnabled(false);   // read-only mirror
    autoDetectSizeCheck_->setToolTip(tr(
        "This reflects the 'Write payload size header' setting in Settings. "
        "Change it there."));
    extractParamsLayout->addRow(autoDetectSizeCheck_);

    payloadSizeSpin_ = new QSpinBox(extractParamsGroup);
    payloadSizeSpin_->setRange(1, 100000000);
    payloadSizeSpin_->setValue(1024);
    payloadSizeSpin_->setToolTip(tr(
        "Used only when the embed did NOT write a size header. "
        "Must exactly match the length of the embedded payload."));
    extractParamsLayout->addRow(tr("Payload size (bytes):"), payloadSizeSpin_);

    extractLeftLayout->addWidget(extractParamsGroup);

    // Decryption panel (Extract)
    extractCryptoPanel_ = new CryptoPanel(CryptoPanel::Role::Extract, extractLeft);
    extractLeftLayout->addWidget(extractCryptoPanel_);

    auto *extractParamsGroup2 = new QGroupBox(tr("Steganography parameters (Extract)"), extractLeft);
    extractParamsGroup2->setCheckable(true);
    extractParamsGroup2->setChecked(true);
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

    auto *extractSmartGroup = new QGroupBox(tr("Smart selection"), extractParamsGroup2);
    auto *extractSmartLayout = new QFormLayout(extractSmartGroup);
    extractModeCombo_ = new QComboBox(extractSmartGroup);
    extractModeCombo_->addItem(tr("Uniform"), static_cast<int>(rfp::stego::SlotSelectionMode::Uniform));
    extractModeCombo_->addItem(tr("Smart (dispersion)"), static_cast<int>(rfp::stego::SlotSelectionMode::Dispersion));
    extractModeCombo_->setCurrentIndex(0);

    extractWindowCombo_ = new QComboBox(extractSmartGroup);
    for (int s : {3, 5, 7, 9, 11, 13}) extractWindowCombo_->addItem(QString::number(s), s);
    extractWindowCombo_->setCurrentIndex(0);

    extractMetricCombo_ = new QComboBox(extractSmartGroup);
    extractMetricCombo_->addItem(tr("Luminance"), static_cast<int>(rfp::stego::DispersionMetric::Luminance));
    extractMetricCombo_->addItem(tr("Per-channel"), static_cast<int>(rfp::stego::DispersionMetric::PerChannel));
    extractMetricCombo_->addItem(tr("Sum"), static_cast<int>(rfp::stego::DispersionMetric::Sum));

    extractThresholdEdit_ = new QLineEdit(extractSmartGroup);
    extractThresholdEdit_->setText("0.0");
    extractThresholdEdit_->setValidator(new QDoubleValidator(0.0, 100000.0, 2, extractThresholdEdit_));
    extractAutoThresholdBtn_ = new QPushButton(tr("Auto"), extractSmartGroup);
    auto *thresholdWidgetExtract = new QWidget(extractSmartGroup);
    auto *thresholdLayoutExtract = new QHBoxLayout(thresholdWidgetExtract);
    thresholdLayoutExtract->setContentsMargins(0, 0, 0, 0);
    thresholdLayoutExtract->addWidget(extractThresholdEdit_, 1);
    thresholdLayoutExtract->addWidget(extractAutoThresholdBtn_);

    extractShuffleCheck_ = new QCheckBox(tr("Apply shuffle after sorting"), extractSmartGroup);
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
    auto *extractBtn = new QPushButton(tr("Extract"), extractLeft);
    extractBtn->setObjectName("extractBtn");
    extractBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; }");
    extractActionsLayout->addWidget(extractBtn);
    extractActionsLayout->addStretch();
    extractLeftLayout->addLayout(extractActionsLayout);

    extractedTextEdit_ = new QPlainTextEdit(extractLeft);
    extractedTextEdit_->setReadOnly(true);
    extractedTextEdit_->setPlaceholderText(tr("Extracted text will appear here"));
    extractLeftLayout->addWidget(extractedTextEdit_, 1);

    auto *extractRight = new QWidget(extractTab_);
    auto *extractRightLayout = new QVBoxLayout(extractRight);
    QGraphicsView *extractPreview = new QGraphicsView(extractRight);
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

    // Статусбар: слева — текст, справа — индикатор занятости
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

// ============================================================================
//  setupConnections
// ============================================================================
void MainWindow::setupConnections() {
    auto *browseInputBtn = findChild<QPushButton *>("browseInputBtn");
    if (browseInputBtn)
        connect(browseInputBtn, &QPushButton::clicked, this, &MainWindow::browseInputImage);
    auto *browseOutputBtn = findChild<QPushButton *>("browseOutputBtn");
    if (browseOutputBtn)
        connect(browseOutputBtn, &QPushButton::clicked, this, &MainWindow::browseOutputImage);
    auto *browseExtractBtn = findChild<QPushButton *>("browseExtractBtn");
    if (browseExtractBtn)
        connect(browseExtractBtn, &QPushButton::clicked, this, [this]() {
            QString path = QFileDialog::getOpenFileName(
                this, tr("Select input image"), QString(),
                tr("Images (*.png *.bmp *.tif *.tiff *.ppm *.pgm);;All files (*.*)"));
            if (!path.isEmpty())
                inputImageExtractEdit_->setText(path);
        });

    auto *embedBtn = findChild<QPushButton *>("embedBtn");
    if (embedBtn)
        connect(embedBtn, &QPushButton::clicked, this, &MainWindow::embedText);
    auto *extractBtn = findChild<QPushButton *>("extractBtn");
    if (extractBtn)
        connect(extractBtn, &QPushButton::clicked, this, &MainWindow::extractText);

    connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::showSettings);
    connect(maskingButton_, &QPushButton::clicked, this, &MainWindow::showMasking);
    connect(fullscreenButton_, &QPushButton::clicked, this, &MainWindow::onFullscreen);
    connect(helpButton_, &QPushButton::clicked, this, &MainWindow::showHelp);

    // Auto-threshold (Embed)
    connect(embedAutoThresholdBtn_, &QPushButton::clicked, this, [this]() {
        if (!currentImage_) {
            QMessageBox::warning(this, tr("R.F.P."), tr("Load an input image first."));
            return;
        }
        auto params = collectParams(false);
        auto imageCopy = currentImage_.value();
        beginBusy(tr("Computing threshold (Embed)..."));

        auto *watcher = new QFutureWatcher<double>(this);
        connect(watcher, &QFutureWatcher<double>::finished, this, [this, watcher]() {
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

    // Auto-threshold (Extract)
    connect(extractAutoThresholdBtn_, &QPushButton::clicked, this, [this]() {
        if (!currentImage_) {
            QMessageBox::warning(this, tr("R.F.P."), tr("Load an input image first."));
            return;
        }
        auto params = collectParams(true);
        auto imageCopy = currentImage_.value();
        beginBusy(tr("Computing threshold (Extract)..."));

        auto *watcher = new QFutureWatcher<double>(this);
        connect(watcher, &QFutureWatcher<double>::finished, this, [this, watcher]() {
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

    auto *copyEmbedBtn = findChild<QPushButton *>("copyEmbedBtn");
    if (copyEmbedBtn)
        connect(copyEmbedBtn, &QPushButton::clicked, this, &MainWindow::copyEmbedParams);
    auto *pasteEmbedBtn = findChild<QPushButton *>("pasteEmbedBtn");
    if (pasteEmbedBtn)
        connect(pasteEmbedBtn, &QPushButton::clicked, this, &MainWindow::pasteEmbedParams);

    connect(copyParamsBtn_, &QPushButton::clicked, this, &MainWindow::copyExtractParams);
    connect(pasteParamsBtn_, &QPushButton::clicked, this, &MainWindow::pasteExtractParams);

    connect(payloadEdit_, &QPlainTextEdit::textChanged, this, &MainWindow::scheduleUpdate);

    auto updateSlot = [this]() { scheduleUpdate(); };
    connect(embedBitsSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, updateSlot);
    connect(embedSeedSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, updateSlot);
    connect(embedRed_, &QCheckBox::toggled, this, updateSlot);
    connect(embedGreen_, &QCheckBox::toggled, this, updateSlot);
    connect(embedBlue_, &QCheckBox::toggled, this, updateSlot);
    connect(embedAlpha_, &QCheckBox::toggled, this, updateSlot);
    connect(embedModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateSlot);
    connect(embedWindowCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateSlot);
    connect(embedMetricCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateSlot);
    connect(embedThresholdEdit_, &QLineEdit::textChanged, this, updateSlot);
    connect(embedShuffleCheck_, &QCheckBox::toggled, this, updateSlot);

    connect(extractBitsSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, updateSlot);
    connect(extractSeedSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, updateSlot);
    connect(extractRed_, &QCheckBox::toggled, this, updateSlot);
    connect(extractGreen_, &QCheckBox::toggled, this, updateSlot);
    connect(extractBlue_, &QCheckBox::toggled, this, updateSlot);
    connect(extractAlpha_, &QCheckBox::toggled, this, updateSlot);
    connect(extractModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateSlot);
    connect(extractWindowCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateSlot);
    connect(extractMetricCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateSlot);
    connect(extractThresholdEdit_, &QLineEdit::textChanged, this, updateSlot);
    connect(extractShuffleCheck_, &QCheckBox::toggled, this, updateSlot);

    // Crypto panels
    if (embedCryptoPanel_)
        connect(embedCryptoPanel_, &CryptoPanel::changed, this, &MainWindow::onEmbedCryptoChanged);
    if (extractCryptoPanel_)
        connect(extractCryptoPanel_, &CryptoPanel::changed, this, &MainWindow::onExtractCryptoChanged);

    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveSettings);
}

// ============================================================================
//  loadSettings / saveSettings / applySettings
// ============================================================================
void MainWindow::loadSettings() {
    settings_.beginGroup("MainWindow");
    if (settings_.contains("size")) {
        QSize sz = settings_.value("size").toSize();
        if (sz.isValid()) resize(sz);
    }
    if (settings_.contains("pos")) {
        QPoint pt = settings_.value("pos").toPoint();
        if (!pt.isNull()) move(pt);
    }
    settings_.endGroup();

    settings_.beginGroup("ParamsEmbed");
    {
        QSignalBlocker b1(embedBitsSpin_);
        QSignalBlocker b2(embedSeedSpin_);
        QSignalBlocker b3(embedRed_);
        QSignalBlocker b4(embedGreen_);
        QSignalBlocker b5(embedBlue_);
        QSignalBlocker b6(embedAlpha_);
        QSignalBlocker b7(embedModeCombo_);
        QSignalBlocker b8(embedWindowCombo_);
        QSignalBlocker b9(embedMetricCombo_);
        QSignalBlocker b10(embedThresholdEdit_);
        QSignalBlocker b11(embedShuffleCheck_);

        if (embedBitsSpin_)      embedBitsSpin_->setValue(settings_.value("bits", 1).toInt());
        if (embedSeedSpin_)      embedSeedSpin_->setValue(settings_.value("seed", 0).toInt());
        if (embedRed_)           embedRed_->setChecked(settings_.value("red", true).toBool());
        if (embedGreen_)         embedGreen_->setChecked(settings_.value("green", true).toBool());
        if (embedBlue_)          embedBlue_->setChecked(settings_.value("blue", true).toBool());
        if (embedAlpha_)         embedAlpha_->setChecked(settings_.value("alpha", false).toBool());
        if (embedModeCombo_)     embedModeCombo_->setCurrentIndex(settings_.value("mode", 0).toInt());
        if (embedWindowCombo_)   embedWindowCombo_->setCurrentIndex(settings_.value("window", 0).toInt());
        if (embedMetricCombo_)   embedMetricCombo_->setCurrentIndex(settings_.value("metric", 0).toInt());
        if (embedThresholdEdit_) embedThresholdEdit_->setText(settings_.value("threshold", "0.0").toString());
        if (embedShuffleCheck_)  embedShuffleCheck_->setChecked(settings_.value("shuffle", true).toBool());
    }
    settings_.endGroup();

    // Crypto (Embed) — settings only, no password.
    settings_.beginGroup("CryptoEmbed");
    if (embedCryptoPanel_) {
        embedCryptoPanel_->setEncryptionEnabled(
            settings_.value("enabled", false).toBool());
        const int cipherInt = settings_.value("cipher",
            static_cast<int>(rfp::crypto::CipherId::Aes256Gcm)).toInt();
        embedCryptoPanel_->setCipher(static_cast<rfp::crypto::CipherId>(cipherInt));
        const int kdfInt = settings_.value("kdf",
            static_cast<int>(rfp::crypto::KdfId::Pbkdf2HmacSha256)).toInt();
        embedCryptoPanel_->setKdf(static_cast<rfp::crypto::KdfId>(kdfInt));
        embedCryptoPanel_->setIterations(
            static_cast<std::uint32_t>(settings_.value("iterations", 100000).toUInt()));
    }
    settings_.endGroup();

    settings_.beginGroup("ParamsExtract");
    {
        QSignalBlocker b1(extractBitsSpin_);
        QSignalBlocker b2(extractSeedSpin_);
        QSignalBlocker b3(extractRed_);
        QSignalBlocker b4(extractGreen_);
        QSignalBlocker b5(extractBlue_);
        QSignalBlocker b6(extractAlpha_);
        QSignalBlocker b7(extractModeCombo_);
        QSignalBlocker b8(extractWindowCombo_);
        QSignalBlocker b9(extractMetricCombo_);
        QSignalBlocker b10(extractThresholdEdit_);
        QSignalBlocker b11(extractShuffleCheck_);

        if (extractBitsSpin_)      extractBitsSpin_->setValue(settings_.value("bits", 1).toInt());
        if (extractSeedSpin_)      extractSeedSpin_->setValue(settings_.value("seed", 0).toInt());
        if (extractRed_)           extractRed_->setChecked(settings_.value("red", true).toBool());
        if (extractGreen_)         extractGreen_->setChecked(settings_.value("green", true).toBool());
        if (extractBlue_)          extractBlue_->setChecked(settings_.value("blue", true).toBool());
        if (extractAlpha_)         extractAlpha_->setChecked(settings_.value("alpha", false).toBool());
        if (extractModeCombo_)     extractModeCombo_->setCurrentIndex(settings_.value("mode", 0).toInt());
        if (extractWindowCombo_)   extractWindowCombo_->setCurrentIndex(settings_.value("window", 0).toInt());
        if (extractMetricCombo_)   extractMetricCombo_->setCurrentIndex(settings_.value("metric", 0).toInt());
        if (extractThresholdEdit_) extractThresholdEdit_->setText(settings_.value("threshold", "0.0").toString());
        if (extractShuffleCheck_)  extractShuffleCheck_->setChecked(settings_.value("shuffle", true).toBool());
    }
    settings_.endGroup();

    // Crypto (Extract)
    settings_.beginGroup("CryptoExtract");
    if (extractCryptoPanel_) {
        extractCryptoPanel_->setEncryptionEnabled(
            settings_.value("enabled", false).toBool());
    }
    settings_.endGroup();

    settings_.beginGroup("Extract");
    {
        QSignalBlocker b1(payloadSizeSpin_);
        if (payloadSizeSpin_)
            payloadSizeSpin_->setValue(settings_.value("payloadSize", 1024).toInt());
    }
    settings_.endGroup();

    settings_.beginGroup("Preview");
    showPreview_      = settings_.value("showPreview", true).toBool();
    previewMode_      = settings_.value("mode", 0).toInt();
    overlayOpacity_   = settings_.value("opacity", 50).toInt();
    highlightChanges_ = settings_.value("highlight", false).toBool();
    darkTheme_        = settings_.value("darkTheme", false).toBool();
    language_         = settings_.value("language", "en").toString();
    writeHeader_      = settings_.value("writeHeader", true).toBool();
    settings_.endGroup();
}

void MainWindow::saveSettings() {
    settings_.beginGroup("MainWindow");
    settings_.setValue("size", size());
    settings_.setValue("pos", pos());
    settings_.endGroup();

    settings_.beginGroup("ParamsEmbed");
    settings_.setValue("bits", embedBitsSpin_->value());
    settings_.setValue("seed", embedSeedSpin_->value());
    settings_.setValue("red", embedRed_->isChecked());
    settings_.setValue("green", embedGreen_->isChecked());
    settings_.setValue("blue", embedBlue_->isChecked());
    settings_.setValue("alpha", embedAlpha_->isChecked());
    settings_.setValue("mode", embedModeCombo_->currentIndex());
    settings_.setValue("window", embedWindowCombo_->currentIndex());
    settings_.setValue("metric", embedMetricCombo_->currentIndex());
    settings_.setValue("threshold", embedThresholdEdit_->text());
    settings_.setValue("shuffle", embedShuffleCheck_->isChecked());
    settings_.endGroup();

    settings_.beginGroup("CryptoEmbed");
    if (embedCryptoPanel_) {
        settings_.setValue("enabled",    embedCryptoPanel_->encryptionEnabled());
        settings_.setValue("cipher",     static_cast<int>(embedCryptoPanel_->cipher()));
        settings_.setValue("kdf",        static_cast<int>(embedCryptoPanel_->kdf()));
        settings_.setValue("iterations", static_cast<uint>(embedCryptoPanel_->iterations()));
    }
    settings_.endGroup();

    settings_.beginGroup("ParamsExtract");
    settings_.setValue("bits", extractBitsSpin_->value());
    settings_.setValue("seed", extractSeedSpin_->value());
    settings_.setValue("red", extractRed_->isChecked());
    settings_.setValue("green", extractGreen_->isChecked());
    settings_.setValue("blue", extractBlue_->isChecked());
    settings_.setValue("alpha", extractAlpha_->isChecked());
    settings_.setValue("mode", extractModeCombo_->currentIndex());
    settings_.setValue("window", extractWindowCombo_->currentIndex());
    settings_.setValue("metric", extractMetricCombo_->currentIndex());
    settings_.setValue("threshold", extractThresholdEdit_->text());
    settings_.setValue("shuffle", extractShuffleCheck_->isChecked());
    settings_.endGroup();

    settings_.beginGroup("CryptoExtract");
    if (extractCryptoPanel_) {
        settings_.setValue("enabled", extractCryptoPanel_->encryptionEnabled());
    }
    settings_.endGroup();

    settings_.beginGroup("Extract");
    settings_.setValue("payloadSize", payloadSizeSpin_->value());
    settings_.endGroup();

    settings_.beginGroup("Preview");
    settings_.setValue("showPreview", showPreview_);
    settings_.setValue("mode", previewMode_);
    settings_.setValue("opacity", overlayOpacity_);
    settings_.setValue("highlight", highlightChanges_);
    settings_.setValue("darkTheme", darkTheme_);
    settings_.setValue("language", language_);
    settings_.setValue("writeHeader", writeHeader_);
    settings_.endGroup();
}

void MainWindow::applySettings() {
    if (darkTheme_) {
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        qApp->setPalette(darkPalette);
        qApp->setStyleSheet("QGroupBox { border: 1px solid #555; }"
                            "QLineEdit, QPlainTextEdit, QSpinBox, QComboBox { background-color: #3c3c3c; }"
                            "QPushButton { background-color: #3c3c3c; }"
                            "QPushButton:hover { background-color: #4a4a4a; }"
                            "QGraphicsView { background-color: #1e1e1e; }");
    } else {
        qApp->setPalette(qApp->style()->standardPalette());
        qApp->setStyleSheet("");
    }
    if (settingsDialog_) {
        showPreview_ = settingsDialog_->showPreview();
        previewMode_ = settingsDialog_->previewMode();
        overlayOpacity_ = settingsDialog_->overlayOpacity();
        highlightChanges_ = settingsDialog_->highlightChanges();
        writeHeader_ = settingsDialog_->writeHeader();
    }

    // Sync the auto-detect checkbox / manual-size spinner with header mode.
    if (autoDetectSizeCheck_) {
        QSignalBlocker b(autoDetectSizeCheck_);
        autoDetectSizeCheck_->setChecked(writeHeader_);
        autoDetectSizeCheck_->setEnabled(false);
    }
    if (payloadSizeSpin_) payloadSizeSpin_->setEnabled(!writeHeader_);

    scheduleUpdate();
}

// ============================================================================
//  Events
// ============================================================================
void MainWindow::closeEvent(QCloseEvent *event) {
    saveSettings();
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (previewView_ && previewScene_ && previewScene_->itemsBoundingRect().isValid())
        previewView_->fitInView(previewScene_->itemsBoundingRect(), Qt::KeepAspectRatio);
    updateMiniPreview();
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        switch (event->key()) {
        case Qt::Key_O: browseInputImage(); break;
        case Qt::Key_E: embedText(); break;
        case Qt::Key_D: extractText(); break;
        case Qt::Key_S: saveSettings(); break;
        default: break;
        }
    } else if (event->key() == Qt::Key_Escape) {
        if (previewView_ && previewView_->isFullScreen())
            previewView_->setWindowState(Qt::WindowNoState);
    } else if (event->key() == Qt::Key_F11) {
        onFullscreen();
    }
    QMainWindow::keyPressEvent(event);
}

// ============================================================================
//  Browse / Embed / Extract
// ============================================================================
void MainWindow::browseInputImage() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Select input image"), QString(),
        tr("Images (*.png *.bmp *.tif *.tiff *.ppm *.pgm);;All files (*.*)"));
    if (path.isEmpty()) return;
    inputImageEdit_->setText(path);

    beginBusy(tr("Loading image..."));

    QFuture<void> future = QtConcurrent::run([this, path]() {
        auto result = rfp::gui::loadImageBuffer(path);
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
    (void)future;
}

void MainWindow::browseOutputImage() {
    QString path = QFileDialog::getSaveFileName(
        this, tr("Select output image"), QString(),
        tr("PNG (*.png);;BMP (*.bmp);;PPM (*.ppm);;All files (*.*)"));
    if (!path.isEmpty())
        outputImageEdit_->setText(path);
}

void MainWindow::embedText() {
    if (embedding_) return;
    if (inputImageEdit_->text().isEmpty() || outputImageEdit_->text().isEmpty()) {
        QMessageBox::warning(this, tr("R.F.P."), tr("Specify input and output image paths."));
        return;
    }

    const bool encrypting =
        embedCryptoPanel_ && embedCryptoPanel_->encryptionEnabled();
    if (encrypting) {
        if (embedCryptoPanel_->password().isEmpty()) {
            QMessageBox::warning(this, tr("R.F.P."),
                tr("Encryption is enabled but the password is empty."));
            return;
        }
        if (!embedCryptoPanel_->validatedPassword().has_value()) {
            QMessageBox::warning(this, tr("R.F.P."), tr("Passwords do not match."));
            return;
        }
    }

    QString text = payloadEdit_->toPlainText();
    QByteArray utf8 = text.toUtf8();
    if (utf8.isEmpty()) {
        QMessageBox::warning(this, tr("R.F.P."), tr("Text payload is empty."));
        return;
    }
    if (!currentImage_) {
        QMessageBox::warning(this, tr("R.F.P."), tr("Load an input image first."));
        return;
    }
    auto params = collectParams(false);
    auto capacity = rfp::stego::capacityBytes(currentImage_.value(), params);

    std::size_t needed = static_cast<std::size_t>(utf8.size());
    if (encrypting)
        needed += MainWindow::cryptoOverheadBytes(embedCryptoPanel_->cipher());
    if (writeHeader_) needed += 4;

    if (needed > capacity) {
        QMessageBox::warning(this, tr("R.F.P."),
            tr("Payload too large. Need %1 bytes, capacity is %2 bytes.")
                .arg(needed).arg(capacity));
        return;
    }
    beginBusy(tr("Embedding..."));
    embedding_ = true;
    runEmbed(inputImageEdit_->text(), outputImageEdit_->text(), utf8);
}

void MainWindow::runEmbed(const QString &input, const QString &output, const QByteArray &data) {
    (void)output;   // saved later in onEmbedFinished

    // 1) Optional encryption.
    rfp::core::ByteBuffer inner;
    const bool encrypting =
        embedCryptoPanel_ && embedCryptoPanel_->encryptionEnabled();

    if (encrypting) {
        const QString pw = embedCryptoPanel_->password();
        auto ep = collectEncryptParams(pw);
        auto encResult = rfp::payload::encrypt(
            std::span<const rfp::core::Byte>(
                reinterpret_cast<const rfp::core::Byte*>(data.constData()),
                static_cast<std::size_t>(data.size())),
            ep);
        if (!encResult) {
            endBusy();
            embedding_ = false;
            QMessageBox::warning(this, tr("R.F.P."),
                tr("Encryption failed: %1")
                    .arg(QString::fromStdString(encResult.error().message)));
            setStatus(tr("Encryption failed"), 5000);
            return;
        }
        inner = std::move(encResult.value());
    } else {
        inner.assign(reinterpret_cast<const rfp::core::Byte*>(data.constData()),
                     reinterpret_cast<const rfp::core::Byte*>(data.constData()) +
                         data.size());
    }

    // 2) Optional 4-byte size header (byte-by-byte, avoids unaligned UB).
    rfp::core::ByteBuffer payload;
    if (writeHeader_) {
        const auto n = static_cast<std::uint32_t>(inner.size());
        payload.reserve(4 + inner.size());
        payload.push_back(static_cast<rfp::core::Byte>((n >> 24) & 0xFF));
        payload.push_back(static_cast<rfp::core::Byte>((n >> 16) & 0xFF));
        payload.push_back(static_cast<rfp::core::Byte>((n >>  8) & 0xFF));
        payload.push_back(static_cast<rfp::core::Byte>( n        & 0xFF));
        payload.insert(payload.end(), inner.begin(), inner.end());
    } else {
        payload = std::move(inner);
    }

    auto params = collectParams(false);

    auto future = QtConcurrent::run(
        [input, payload, params]() -> rfp::core::Result<rfp::stego::ImageBuffer> {
            auto imageResult = rfp::gui::loadImageBuffer(input);
            if (!imageResult) return imageResult.error();
            return rfp::stego::StegoEncoder::embedBytes(imageResult.value(), payload, params);
        });

    embedWatcher_.setFuture(future);
    connect(&embedWatcher_, &QFutureWatcher<rfp::core::Result<rfp::stego::ImageBuffer>>::finished,
            this, &MainWindow::onEmbedFinished);
}

void MainWindow::onEmbedFinished() {
    embedding_ = false;
    auto result = embedWatcher_.result();
    if (!result) {
        endBusy();
        QMessageBox::warning(this, tr("R.F.P."), QString::fromStdString(result.error().message));
        setStatus(tr("Embedding failed"), 5000);
        return;
    }
    modifiedImage_ = result.value();
    modifiedQImage_ = imageBufferToQImage(modifiedImage_.value());

    beginBusy(tr("Saving image..."));

    auto saveResult = rfp::gui::saveImageBuffer(modifiedImage_.value(), outputImageEdit_->text());
    if (!saveResult) {
        endBusy();
        QMessageBox::warning(this, tr("R.F.P."), QString::fromStdString(saveResult.error().message));
        setStatus(tr("Save failed"), 5000);
        return;
    }

    QByteArray originalData = payloadEdit_->toPlainText().toUtf8();
    const bool encrypted =
        embedCryptoPanel_ && embedCryptoPanel_->encryptionEnabled();

    updateStats(tr("Embedded %1 bytes%2%3.")
                    .arg(originalData.size())
                    .arg(encrypted ? tr(" (encrypted)") : QString())
                    .arg(writeHeader_ ? tr(" (header included)") : tr(" (no header)")));

    auto crc = rfp::core::crc32(std::span<const rfp::core::Byte>(
        reinterpret_cast<const rfp::core::Byte *>(originalData.constData()),
        static_cast<size_t>(originalData.size())));

    if (encrypted && embedCryptoPanel_) embedCryptoPanel_->clearConfirm();

    endBusy();
    setStatus(tr("Embedded %1 bytes%2. CRC32: %3")
                  .arg(originalData.size())
                  .arg(encrypted ? tr(" (encrypted)") : QString())
                  .arg(crcToText(crc)), 8000);
    scheduleUpdate();
}

void MainWindow::extractText() {
    if (extracting_) return;
    if (inputImageExtractEdit_->text().isEmpty()) {
        QMessageBox::warning(this, tr("R.F.P."), tr("Specify input image path."));
        return;
    }

    const bool decrypting =
        extractCryptoPanel_ && extractCryptoPanel_->encryptionEnabled();
    if (decrypting && extractCryptoPanel_->password().isEmpty()) {
        QMessageBox::warning(this, tr("R.F.P."),
            tr("Decryption is enabled but the password is empty."));
        return;
    }

    beginBusy(tr("Extracting..."));
    extracting_ = true;
    runExtract(inputImageExtractEdit_->text());
}

void MainWindow::runExtract(const QString &input) {
    auto params = collectParams(true);

    // Snapshot all mode-deciding settings on the UI thread.
    const bool hasHeader   = writeHeader_;
    const std::size_t manualSize =
        static_cast<std::size_t>(payloadSizeSpin_->value());

    const bool decrypting =
        extractCryptoPanel_ && extractCryptoPanel_->encryptionEnabled();
    const std::string password =
        decrypting ? extractCryptoPanel_->password().toStdString() : std::string{};

    auto future = QtConcurrent::run(
        [input, params, hasHeader, manualSize, decrypting, password]()
            -> rfp::core::Result<rfp::core::ByteBuffer>
        {
            auto imageResult = rfp::gui::loadImageBuffer(input);
            if (!imageResult) return imageResult.error();
            const auto& image = imageResult.value();

            // StegoDecoder always starts from slot 0. Read the WHOLE frame in
            // one call, then slice in memory. Two calls would re-read the
            // header and never reach the actual payload.
            const std::size_t capacity = rfp::stego::capacityBytes(image, params);
            if (capacity == 0)
                return rfp::core::Error{rfp::core::ErrorCode::InvalidImageBuffer,
                                        "Image has zero capacity for these parameters"};

            auto frameResult = rfp::stego::StegoDecoder::extractBytes(
                image, capacity, params);
            if (!frameResult) return frameResult.error();
            const auto& frame = frameResult.value();

            std::size_t payloadStart = 0;
            std::size_t payloadSize  = 0;

            if (hasHeader) {
                if (frame.size() < 4)
                    return rfp::core::Error{rfp::core::ErrorCode::InvalidImageBuffer,
                                            "Frame too short to contain a size header"};
                payloadSize =
                    (static_cast<std::uint32_t>(frame[0]) << 24) |
                    (static_cast<std::uint32_t>(frame[1]) << 16) |
                    (static_cast<std::uint32_t>(frame[2]) <<  8) |
                     static_cast<std::uint32_t>(frame[3]);
                payloadStart = 4;

                if (payloadSize == 0 ||
                    payloadStart + payloadSize > frame.size()) {
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
                frame.begin() + static_cast<std::ptrdiff_t>(payloadStart + payloadSize));

            if (!decrypting) return payload;

            rfp::payload::DecryptParams dp;
            dp.password = password;
            return rfp::payload::decrypt(payload, dp);
        });

    extractWatcher_.setFuture(future);
    connect(&extractWatcher_,
            &QFutureWatcher<rfp::core::Result<rfp::core::ByteBuffer>>::finished,
            this, &MainWindow::onExtractFinished);
}

void MainWindow::onExtractFinished() {
    extracting_ = false;
    auto result = extractWatcher_.result();
    if (!result) {
        endBusy();
        QMessageBox::warning(this, tr("R.F.P."), QString::fromStdString(result.error().message));
        setStatus(tr("Extraction failed"), 5000);
        return;
    }
    const auto &data = result.value();
    QByteArray decodedBytes(reinterpret_cast<const char *>(data.data()),
                            static_cast<qsizetype>(data.size()));
    extractedTextEdit_->setPlainText(QString::fromUtf8(decodedBytes));
    auto crc = rfp::core::crc32(std::span<const rfp::core::Byte>(data.data(), data.size()));
    updateStats(tr("Extracted %1 bytes").arg(data.size()));
    endBusy();

    const bool decrypted =
        extractCryptoPanel_ && extractCryptoPanel_->encryptionEnabled();
    setStatus(tr("Extracted %1 bytes%2. CRC32: %3")
                  .arg(data.size())
                  .arg(decrypted ? tr(" (decrypted)") : QString())
                  .arg(crcToText(crc)), 8000);
}

void MainWindow::onTextChanged() {
    scheduleUpdate();
}

void MainWindow::onEmbedCryptoChanged()   { scheduleUpdate(); }
void MainWindow::onExtractCryptoChanged() { scheduleUpdate(); }

// ============================================================================
//  Debounced async recompute
// ============================================================================
void MainWindow::scheduleUpdate() {
    if (!updateTimer_) return;
    updateTimer_->start();
}

void MainWindow::doUpdate() {
    if (!updateTimer_) return;

    if (!currentImage_) {
        if (capacityLabel_) capacityLabel_->setText(tr("Capacity: not loaded"));
        if (usageLabel_)    usageLabel_->setText(tr("Usage: no image"));
        if (previewScene_)  previewScene_->clear();
        return;
    }

    if (recomputeInProgress_) {
        recomputePending_ = true;
        return;
    }

    const rfp::stego::StegoParams params = collectParams(false);
    const QString payloadText = payloadEdit_->toPlainText();
    const bool showPreview  = showPreview_;
    const int  previewMode  = previewMode_;
    const int  overlayOp    = overlayOpacity_;
    const bool highlight    = highlightChanges_;
    const bool writeHdr     = writeHeader_;
    const rfp::stego::ImageBuffer imageCopy = currentImage_.value();
    const QImage curQImg    = currentQImage_.value_or(QImage());
    const QImage modQImg    = modifiedQImage_.value_or(QImage());
    const bool hasModified  = modifiedQImage_.has_value();

    const bool encrypting =
        embedCryptoPanel_ && embedCryptoPanel_->encryptionEnabled();
    const std::size_t cryptoOverhead = encrypting
        ? MainWindow::cryptoOverheadBytes(embedCryptoPanel_->cipher())
        : 0;

    recomputeInProgress_ = true;
    beginBusy(tr("Computing capacity and preview..."));

    auto future = QtConcurrent::run(
        [params, payloadText, showPreview, previewMode, overlayOp,
         highlight, writeHdr, imageCopy, curQImg, modQImg, hasModified,
         encrypting, cryptoOverhead]() -> RecomputeResult {

        RecomputeResult res;

        const size_t capacity     = rfp::stego::capacityBytes(imageCopy, params);
        const size_t capacityBits = rfp::stego::capacityBits(imageCopy, params);

        QString info = tr("Capacity: %1 bytes (%2 bits)").arg(capacity).arg(capacityBits);
        if (params.mode == rfp::stego::SlotSelectionMode::Dispersion) {
            rfp::stego::StegoParams uniformParams = params;
            uniformParams.mode = rfp::stego::SlotSelectionMode::Uniform;
            const size_t uniformBytes = rfp::stego::capacityBytes(imageCopy, uniformParams);
            if (uniformBytes > 0) {
                const double percent =
                    static_cast<double>(capacity) / static_cast<double>(uniformBytes) * 100.0;
                info += tr(" (using %1% of available)").arg(percent, 0, 'f', 1);
            }
        }
        res.capacityText = info;

        const QByteArray utf8 = payloadText.toUtf8();
        std::size_t used = static_cast<std::size_t>(utf8.size());
        if (encrypting) used += cryptoOverhead;
        if (writeHdr)   used += 4;

        if (capacity == 0) {
            res.usageText = tr("Usage: 0 bytes (capacity 0)");
        } else {
            const double percent =
                static_cast<double>(used) / static_cast<double>(capacity) * 100.0;
            const QString color = (used <= capacity) ? "green" : "red";
            QString tag;
            if (encrypting) tag = tr(" (encrypted)");
            res.usageText =
                tr("Usage: <span style=\"color:%1;\">%2 / %3 bytes (%4%)</span>%5")
                    .arg(color).arg(used).arg(capacity)
                    .arg(percent, 0, 'f', 1)
                    .arg(tag);
        }

        if (!showPreview) return res;

        if (previewMode == 0) {
            if (!curQImg.isNull()) {
                res.previewImage = curQImg;
                res.previewValid = true;
            }
            return res;
        }
        if (curQImg.isNull()) return res;

        if (previewMode == 1) {
            double minVal = 0, maxVal = 0, meanVal = 0;
            const QImage overlay = MainWindow::generateDispersionOverlay(
                imageCopy, params, overlayOp, minVal, maxVal, meanVal);
            QImage result = curQImg;
            if (!overlay.isNull()) {
                QPainter painter(&result);
                painter.drawImage(0, 0, overlay);
                painter.end();
            }
            res.previewImage = result;
            res.previewStats = tr("Dispersion: min=%1, max=%2, mean=%3")
                                   .arg(minVal, 0, 'f', 2)
                                   .arg(maxVal, 0, 'f', 2)
                                   .arg(meanVal, 0, 'f', 2);
            res.previewValid = true;
        } else if (previewMode == 2) {
            if (!hasModified) {
                res.previewImage = curQImg;
                res.previewStats = tr("No modified image available. Embed data first.");
                res.previewValid = true;
                return res;
            }
            QImage comp = MainWindow::generateComparisonView(curQImg, modQImg, highlight);
            QString stats;
            if (curQImg.size() == modQImg.size()) {
                int changed = 0;
                for (int y = 0; y < curQImg.height(); ++y)
                    for (int x = 0; x < curQImg.width(); ++x)
                        if (curQImg.pixel(x, y) != modQImg.pixel(x, y))
                            ++changed;
                const double percent =
                    static_cast<double>(changed) /
                    (static_cast<double>(curQImg.width()) * curQImg.height()) * 100.0;
                stats = tr("Changed pixels: %1 / %2 (%3%)")
                            .arg(changed)
                            .arg(curQImg.width() * curQImg.height())
                            .arg(percent, 0, 'f', 2);
            } else {
                stats = tr("Image sizes differ");
            }
            res.previewImage = comp;
            res.previewStats = stats;
            res.previewValid = true;
        }
        return res;
    });

    previewWatcher_.setFuture(future);
}

void MainWindow::onPreviewReady() {
    recomputeInProgress_ = false;

    if (!previewWatcher_.isFinished()) return;

    const RecomputeResult res = previewWatcher_.result();

    if (!res.capacityText.isEmpty() && capacityLabel_)
        capacityLabel_->setText(res.capacityText);
    if (!res.usageText.isEmpty() && usageLabel_)
        usageLabel_->setText(res.usageText);

    if (res.previewValid && !res.previewImage.isNull()) {
        showImage(res.previewImage);
        updateStats(res.previewStats);
    } else {
        if (previewScene_) previewScene_->clear();
        updateStats("");
    }

    if (recomputePending_) {
        recomputePending_ = false;
        QTimer::singleShot(0, this, &MainWindow::doUpdate);
        return;
    }

    endBusy();
    setStatus(tr("Ready"), 2000);
}

// ============================================================================
//  Static helpers
// ============================================================================
QImage MainWindow::imageBufferToQImage(const rfp::stego::ImageBuffer &buffer) const {
    if (!buffer.isValid()) return QImage();
    QImage::Format fmt = (buffer.channels == 4) ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
    QImage img(static_cast<int>(buffer.width), static_cast<int>(buffer.height), fmt);
    const auto rowBytes = static_cast<std::size_t>(buffer.width) * buffer.channels;
    for (int y = 0; y < img.height(); ++y) {
        const auto *src = buffer.pixels.data() + static_cast<std::size_t>(y) * rowBytes;
        auto *dst = img.scanLine(y);
        std::memcpy(dst, src, rowBytes);
    }
    return img;
}

QImage MainWindow::generateDispersionOverlay(const rfp::stego::ImageBuffer &buffer,
                                             const rfp::stego::StegoParams &params,
                                             int overlayOpacity,
                                             double &outMin, double &outMax, double &outMean) {
    QImage overlay(static_cast<int>(buffer.width), static_cast<int>(buffer.height),
                   QImage::Format_ARGB32);
    overlay.fill(Qt::transparent);
    if (!buffer.isValid() || params.mode != rfp::stego::SlotSelectionMode::Dispersion)
        return overlay;

    rfp::stego::DispersionCalculator calc(buffer, params);
    const auto pixelCount = static_cast<std::size_t>(buffer.width) *
                            static_cast<std::size_t>(buffer.height);
    std::vector<double> disp;
    disp.reserve(pixelCount);
    double sum = 0;
    double minVal = std::numeric_limits<double>::max();
    double maxVal = -std::numeric_limits<double>::max();
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        double totalDisp = 0;
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
        if (pixelDisp < minVal) minVal = pixelDisp;
        if (pixelDisp > maxVal) maxVal = pixelDisp;
    }
    outMin = minVal;
    outMax = maxVal;
    outMean = sum / static_cast<double>(pixelCount);

    const int width = overlay.width();
    const int height = overlay.height();
    const int alpha = static_cast<int>(overlayOpacity * 2.55);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::size_t idx = static_cast<std::size_t>(y) * width + x;
            QColor color = dispersionToColor(disp[idx], outMin, outMax);
            color.setAlpha(alpha);
            overlay.setPixelColor(x, y, color);
        }
    }
    return overlay;
}

QImage MainWindow::generateComparisonView(const QImage &original, const QImage &modified,
                                          bool highlightChanges) {
    if (original.isNull() || modified.isNull()) return QImage();
    int width = original.width() + modified.width() + 10;
    int height = std::max(original.height(), modified.height());
    QImage combined(width, height, QImage::Format_RGB888);
    combined.fill(Qt::lightGray);
    QPainter painter(&combined);
    painter.drawImage(0, 0, original);
    painter.drawImage(original.width() + 10, 0, modified);
    if (highlightChanges) {
        QImage mask = generateChangesMask(original, modified);
        if (!mask.isNull())
            painter.drawImage(original.width() + 10, 0, mask);
    }
    painter.end();
    return combined;
}

QImage MainWindow::generateChangesMask(const QImage &original, const QImage &modified) {
    if (original.size() != modified.size()) return QImage();
    QImage mask(original.size(), QImage::Format_ARGB32);
    mask.fill(Qt::transparent);
    QColor highlightColor(255, 0, 0, 128);
    for (int y = 0; y < original.height(); ++y)
        for (int x = 0; x < original.width(); ++x)
            if (original.pixel(x, y) != modified.pixel(x, y))
                mask.setPixelColor(x, y, highlightColor);
    return mask;
}

QColor MainWindow::dispersionToColor(double value, double minVal, double maxVal) {
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

// ============================================================================
//  Serialization / Copy-Paste params
// ============================================================================
QString MainWindow::serializeFull(const rfp::stego::StegoParams &params,
                                  const QString &inputPath,
                                  const QString &outputPath) const {
    std::ostringstream oss;
    oss << "bits=" << static_cast<int>(params.bitsPerChannel)
        << ";seed=" << params.seed
        << ";channels=" << (params.useRedChannel ? "R" : "")
        << (params.useGreenChannel ? "G" : "")
        << (params.useBlueChannel ? "B" : "")
        << (params.useAlphaChannel ? "A" : "")
        << ";mode=" << static_cast<int>(params.mode)
        << ";window=" << params.windowSize
        << ";metric=" << static_cast<int>(params.metric)
        << ";threshold=" << params.dispersionThreshold
        << ";shuffle=" << (params.applyShuffleAfterSort ? 1 : 0)
        << ";header="  << (writeHeader_ ? 1 : 0);

    // Crypto settings (NOT the password).
    if (embedCryptoPanel_) {
        oss << ";encrypt=" << (embedCryptoPanel_->encryptionEnabled() ? 1 : 0)
            << ";cipher="  << static_cast<int>(embedCryptoPanel_->cipher())
            << ";kdf="     << static_cast<int>(embedCryptoPanel_->kdf())
            << ";iter="    << embedCryptoPanel_->iterations();
    }

    auto escape = [](const QString &s) {
        QString r = s;
        r.replace(';', "%3B");
        r.replace('=', "%3D");
        return r;
    };
    if (!inputPath.isEmpty())
        oss << ";input=" << escape(inputPath).toStdString();
    if (!outputPath.isEmpty())
        oss << ";output=" << escape(outputPath).toStdString();
    return QString::fromStdString(oss.str());
}

bool MainWindow::deserializeFull(const QString &str, rfp::stego::StegoParams &params,
                                 QString &inputPath, QString &outputPath) {
    params.bitsPerChannel = 1;
    params.seed = 0;
    params.useRedChannel = true;
    params.useGreenChannel = true;
    params.useBlueChannel = true;
    params.useAlphaChannel = false;
    params.mode = rfp::stego::SlotSelectionMode::Uniform;
    params.windowSize = 3;
    params.metric = rfp::stego::DispersionMetric::Luminance;
    params.dispersionThreshold = 0.0;
    params.applyShuffleAfterSort = true;
    inputPath.clear();
    outputPath.clear();

    QRegularExpression re(R"((\w+)=([^;]*))");
    QRegularExpressionMatchIterator it = re.globalMatch(str);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString key = match.captured(1);
        QString value = match.captured(2);
        if (key == "bits")
            params.bitsPerChannel = static_cast<std::uint8_t>(value.toInt());
        else if (key == "seed")
            params.seed = value.toUInt();
        else if (key == "channels") {
            params.useRedChannel = value.contains('R');
            params.useGreenChannel = value.contains('G');
            params.useBlueChannel = value.contains('B');
            params.useAlphaChannel = value.contains('A');
        } else if (key == "mode")
            params.mode = static_cast<rfp::stego::SlotSelectionMode>(value.toInt());
        else if (key == "window")
            params.windowSize = value.toInt();
        else if (key == "metric")
            params.metric = static_cast<rfp::stego::DispersionMetric>(value.toInt());
        else if (key == "threshold")
            params.dispersionThreshold = value.toDouble();
        else if (key == "shuffle")
            params.applyShuffleAfterSort = (value.toInt() != 0);
        else if (key == "header")
            writeHeader_ = (value.toInt() != 0);
        else if (key == "encrypt" && embedCryptoPanel_)
            embedCryptoPanel_->setEncryptionEnabled(value.toInt() != 0);
        else if (key == "cipher" && embedCryptoPanel_)
            embedCryptoPanel_->setCipher(static_cast<rfp::crypto::CipherId>(value.toInt()));
        else if (key == "kdf" && embedCryptoPanel_)
            embedCryptoPanel_->setKdf(static_cast<rfp::crypto::KdfId>(value.toInt()));
        else if (key == "iter" && embedCryptoPanel_)
            embedCryptoPanel_->setIterations(static_cast<std::uint32_t>(value.toUInt()));
        else if (key == "input") {
            QString s = value;
            s.replace("%3B", ";");
            s.replace("%3D", "=");
            inputPath = s;
        } else if (key == "output") {
            QString s = value;
            s.replace("%3B", ";");
            s.replace("%3D", "=");
            outputPath = s;
        }
    }
    return true;
}

void MainWindow::copyEmbedParams() {
    auto params = collectParams(false);
    QString str = serializeFull(params, inputImageEdit_->text(), outputImageEdit_->text());
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(str);
    setStatus(tr("Embedding parameters (with paths) copied to clipboard."), 2000);
}

void MainWindow::pasteEmbedParams() {
    QClipboard *clipboard = QApplication::clipboard();
    QString str = clipboard->text();
    if (str.isEmpty()) {
        setStatus(tr("Clipboard is empty."), 2000);
        return;
    }
    rfp::stego::StegoParams params;
    QString inputPath, outputPath;
    if (!deserializeFull(str, params, inputPath, outputPath)) {
        setStatus(tr("Failed to parse parameters from clipboard."), 2000);
        return;
    }

    {
        QSignalBlocker b1(embedBitsSpin_);
        QSignalBlocker b2(embedSeedSpin_);
        QSignalBlocker b3(embedRed_);
        QSignalBlocker b4(embedGreen_);
        QSignalBlocker b5(embedBlue_);
        QSignalBlocker b6(embedAlpha_);
        QSignalBlocker b7(embedModeCombo_);
        QSignalBlocker b8(embedWindowCombo_);
        QSignalBlocker b9(embedMetricCombo_);
        QSignalBlocker b10(embedThresholdEdit_);
        QSignalBlocker b11(embedShuffleCheck_);

        embedBitsSpin_->setValue(params.bitsPerChannel);
        embedSeedSpin_->setValue(params.seed);
        embedRed_->setChecked(params.useRedChannel);
        embedGreen_->setChecked(params.useGreenChannel);
        embedBlue_->setChecked(params.useBlueChannel);
        embedAlpha_->setChecked(params.useAlphaChannel);

        int modeIdx = embedModeCombo_->findData(static_cast<int>(params.mode));
        if (modeIdx >= 0) embedModeCombo_->setCurrentIndex(modeIdx);

        int winIdx = embedWindowCombo_->findData(params.windowSize);
        if (winIdx >= 0) embedWindowCombo_->setCurrentIndex(winIdx);

        int metricIdx = embedMetricCombo_->findData(static_cast<int>(params.metric));
        if (metricIdx >= 0) embedMetricCombo_->setCurrentIndex(metricIdx);

        embedThresholdEdit_->setText(QString::number(params.dispersionThreshold, 'f', 2));
        embedShuffleCheck_->setChecked(params.applyShuffleAfterSort);
    }

    if (!inputPath.isEmpty())  inputImageEdit_->setText(inputPath);
    if (!outputPath.isEmpty()) outputImageEdit_->setText(outputPath);

    // Reflect possibly-updated writeHeader_ in the Extract tab UI.
    if (autoDetectSizeCheck_) {
        QSignalBlocker b(autoDetectSizeCheck_);
        autoDetectSizeCheck_->setChecked(writeHeader_);
    }
    if (payloadSizeSpin_) payloadSizeSpin_->setEnabled(!writeHeader_);

    setStatus(tr("Embedding parameters pasted from clipboard."), 2000);
    scheduleUpdate();
}

void MainWindow::copyExtractParams() {
    auto params = collectParams(true);
    QString str = serializeFull(params, QString(), QString());
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(str);
    setStatus(tr("Extraction parameters copied to clipboard."), 2000);
}

void MainWindow::pasteExtractParams() {
    QClipboard *clipboard = QApplication::clipboard();
    QString str = clipboard->text();
    if (str.isEmpty()) {
        setStatus(tr("Clipboard is empty."), 2000);
        return;
    }
    rfp::stego::StegoParams params;
    QString inputPath, outputPath;
    if (!deserializeFull(str, params, inputPath, outputPath)) {
        setStatus(tr("Failed to parse parameters from clipboard."), 2000);
        return;
    }

    {
        QSignalBlocker b1(extractBitsSpin_);
        QSignalBlocker b2(extractSeedSpin_);
        QSignalBlocker b3(extractRed_);
        QSignalBlocker b4(extractGreen_);
        QSignalBlocker b5(extractBlue_);
        QSignalBlocker b6(extractAlpha_);
        QSignalBlocker b7(extractModeCombo_);
        QSignalBlocker b8(extractWindowCombo_);
        QSignalBlocker b9(extractMetricCombo_);
        QSignalBlocker b10(extractThresholdEdit_);
        QSignalBlocker b11(extractShuffleCheck_);

        extractBitsSpin_->setValue(params.bitsPerChannel);
        extractSeedSpin_->setValue(params.seed);
        extractRed_->setChecked(params.useRedChannel);
        extractGreen_->setChecked(params.useGreenChannel);
        extractBlue_->setChecked(params.useBlueChannel);
        extractAlpha_->setChecked(params.useAlphaChannel);

        int modeIdx = extractModeCombo_->findData(static_cast<int>(params.mode));
        if (modeIdx >= 0) extractModeCombo_->setCurrentIndex(modeIdx);

        int winIdx = extractWindowCombo_->findData(params.windowSize);
        if (winIdx >= 0) extractWindowCombo_->setCurrentIndex(winIdx);

        int metricIdx = extractMetricCombo_->findData(static_cast<int>(params.metric));
        if (metricIdx >= 0) extractMetricCombo_->setCurrentIndex(metricIdx);

        extractThresholdEdit_->setText(QString::number(params.dispersionThreshold, 'f', 2));
        extractShuffleCheck_->setChecked(params.applyShuffleAfterSort);
    }

    // Reflect header mode coming from the clipboard.
    if (autoDetectSizeCheck_) {
        QSignalBlocker b(autoDetectSizeCheck_);
        autoDetectSizeCheck_->setChecked(writeHeader_);
    }
    if (payloadSizeSpin_) payloadSizeSpin_->setEnabled(!writeHeader_);

    if (!outputPath.isEmpty()) {
        inputImageExtractEdit_->setText(outputPath);
        setStatus(tr("Extraction parameters pasted and input path set to output from copied data."), 2000);
    } else {
        setStatus(tr("Extraction parameters pasted (no path information)."), 2000);
    }
    scheduleUpdate();
}

// ============================================================================
//  Misc
// ============================================================================
void MainWindow::onMaskingFinished() {}

rfp::stego::StegoParams MainWindow::collectParams(bool forExtract) const {
    rfp::stego::StegoParams params;
    if (forExtract) {
        params.bitsPerChannel = static_cast<std::uint8_t>(extractBitsSpin_->value());
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
MainWindow::collectEncryptParams(const QString& password) const {
    rfp::payload::EncryptParams p;
    p.password = password.toStdString();
    if (embedCryptoPanel_) {
        p.cipher     = embedCryptoPanel_->cipher();
        p.kdf        = embedCryptoPanel_->kdf();
        p.iterations = embedCryptoPanel_->iterations();
    }
    return p;
}

void MainWindow::showImage(const QImage &image, bool fit) {
    if (!previewScene_ || !previewView_) return;
    previewScene_->clear();
    if (image.isNull()) return;
    QPixmap pix = QPixmap::fromImage(image);
    previewScene_->addPixmap(pix);
    if (fit)
        previewView_->fitInView(previewScene_->itemsBoundingRect(), Qt::KeepAspectRatio);
}

void MainWindow::updateMiniPreview() {
    if (!miniPreviewLabel_) return;
    if (!currentQImage_ || currentQImage_->isNull()) {
        miniPreviewLabel_->setPixmap(QPixmap());
        miniPreviewLabel_->setText(tr("No image"));
        return;
    }
    QPixmap pix = QPixmap::fromImage(currentQImage_.value());
    int h = miniPreviewLabel_->height();
    if (h <= 0) h = 120;
    miniPreviewLabel_->setPixmap(pix.scaledToHeight(h, Qt::SmoothTransformation));
    miniPreviewLabel_->setText(QString());
}

void MainWindow::setStatus(const QString &text, int timeout) {
    if (!statusLabel_) return;
    statusLabel_->setText(text);
    if (timeout > 0)
        QTimer::singleShot(timeout, this, [this]() {
            if (statusLabel_) statusLabel_->clear();
        });
}

void MainWindow::beginBusy(const QString &message) {
    ++busyCounter_;
    if (progressBar_) {
        progressBar_->setRange(0, 0);
        progressBar_->setVisible(true);
    }
    if (statusLabel_) {
        statusLabel_->setText(message);
    }
}

void MainWindow::endBusy() {
    if (busyCounter_ > 0)
        --busyCounter_;
    if (busyCounter_ == 0 && progressBar_) {
        progressBar_->setVisible(false);
    }
}

void MainWindow::updateStats(const QString &text) {
    if (!statsLabel_) return;
    statsLabel_->setText(text);
}

void MainWindow::onFullscreen() {
    if (!previewScene_) return;

    auto *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Preview"));
    dlg->setStyleSheet("background-color: black;");

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
            darkTheme_ = settingsDialog_->isDarkTheme();
            QString newLang = settingsDialog_->language();
            overlayOpacity_ = settingsDialog_->overlayOpacity();
            showPreview_ = settingsDialog_->showPreview();
            previewMode_ = settingsDialog_->previewMode();
            highlightChanges_ = settingsDialog_->highlightChanges();
            writeHeader_ = settingsDialog_->writeHeader();

            if (newLang != language_) {
                language_ = newLang;
                saveSettings();
                qApp->quit();
                QProcess::startDetached(qApp->arguments()[0], qApp->arguments());
                return;
            }
            applySettings();
        });
        connect(settingsDialog_, &SettingsDialog::settingsChanged, this,
                &MainWindow::scheduleUpdate);
    }
    settingsDialog_->show();
}

void MainWindow::showMasking() {
    if (!maskingDialog_) {
        maskingDialog_ = new MaskingDialog(this);
        connect(maskingDialog_, &MaskingDialog::maskingRequested, this,
                &MainWindow::runMasking);
    }
    if (currentImage_ && !inputImageEdit_->text().isEmpty()) {
        QFileInfo info(inputImageEdit_->text());
        maskingDialog_->setDefaultDirectory(info.absolutePath());
        maskingDialog_->setExcludeFile(inputImageEdit_->text());
    }
    maskingDialog_->show();
}

void MainWindow::showHelp() {
    if (!helpDialog_)
        helpDialog_ = new HelpDialog(this);
    helpDialog_->setLanguage(language_);
    helpDialog_->show();
}

void MainWindow::runMasking(const QString &dir, const QString &ext, int count,
                            bool recursive, const QString &exclude) {
    beginBusy(tr("Masking..."));
    auto future = QtConcurrent::run([dir, ext, count, recursive, exclude]() {
        QDir directory(dir);
        if (!directory.exists()) return;
        QStringList filters;
        if (!ext.isEmpty()) filters << "*." + ext;
        else filters << "*";
        QDir::Filters dirFilter = QDir::Files | QDir::Readable;
        if (recursive) dirFilter |= QDir::AllDirs;
        QStringList files = directory.entryList(filters, dirFilter, QDir::Name);
        if (!exclude.isEmpty())
            files.removeAll(QFileInfo(exclude).fileName());
        if (files.size() > count)
            files = files.mid(0, count);
        for (const QString &f : files) {
            QFile file(directory.absoluteFilePath(f));
            if (file.open(QIODevice::ReadOnly))
                file.close();
        }
    });
    maskingWatcher_.setFuture(future);
    connect(&maskingWatcher_, &QFutureWatcher<void>::finished, this, [this]() {
        endBusy();
        setStatus(tr("Masking completed"), 4000);
        if (maskingDialog_)
            maskingDialog_->appendLog(tr("Masking completed."));
    });
}
