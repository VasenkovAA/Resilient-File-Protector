#include "MainWindow.h"

#include "QtImageAdapter.h"
#include "rfp/core/ByteBuffer.h"
#include "rfp/core/Crc32.h"
#include "rfp/stego/Capacity.h"
#include "rfp/stego/StegoDecoder.h"
#include "rfp/stego/StegoEncoder.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWidget>

#include <limits>
#include <span>

namespace {

QString crcToText(std::uint32_t crc)
{
    return QStringLiteral("%1").arg(crc, 8, 16, QLatin1Char('0')).toUpper();
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("R.F.P. - Resilient File Protector"));
    resize(820, 640);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);

    auto* filesGroup = new QGroupBox(QStringLiteral("Images"), central);
    auto* filesLayout = new QGridLayout(filesGroup);

    inputImageEdit_ = new QLineEdit(filesGroup);
    outputImageEdit_ = new QLineEdit(filesGroup);
    auto* browseInputButton = new QPushButton(QStringLiteral("Browse..."), filesGroup);
    auto* browseOutputButton = new QPushButton(QStringLiteral("Browse..."), filesGroup);

    filesLayout->addWidget(new QLabel(QStringLiteral("Input image:"), filesGroup), 0, 0);
    filesLayout->addWidget(inputImageEdit_, 0, 1);
    filesLayout->addWidget(browseInputButton, 0, 2);
    filesLayout->addWidget(new QLabel(QStringLiteral("Output image:"), filesGroup), 1, 0);
    filesLayout->addWidget(outputImageEdit_, 1, 1);
    filesLayout->addWidget(browseOutputButton, 1, 2);

    auto* paramsGroup = new QGroupBox(QStringLiteral("Steganography parameters"), central);
    auto* paramsLayout = new QFormLayout(paramsGroup);

    bitsPerChannelSpin_ = new QSpinBox(paramsGroup);
    bitsPerChannelSpin_->setRange(1, 4);
    bitsPerChannelSpin_->setValue(1);

    seedSpin_ = new QSpinBox(paramsGroup);
    seedSpin_->setRange(0, std::numeric_limits<int>::max());
    seedSpin_->setValue(0);

    payloadSizeSpin_ = new QSpinBox(paramsGroup);
    payloadSizeSpin_->setRange(1, 100000000);
    payloadSizeSpin_->setValue(1);

    auto* channelsWidget = new QWidget(paramsGroup);
    auto* channelsLayout = new QHBoxLayout(channelsWidget);
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

    paramsLayout->addRow(QStringLiteral("Bits per channel:"), bitsPerChannelSpin_);
    paramsLayout->addRow(QStringLiteral("Seed, 0 = sequential:"), seedSpin_);
    paramsLayout->addRow(QStringLiteral("Payload size for extraction, bytes:"), payloadSizeSpin_);
    paramsLayout->addRow(QStringLiteral("Channels:"), channelsWidget);

    auto* payloadGroup = new QGroupBox(QStringLiteral("Text payload"), central);
    auto* payloadLayout = new QVBoxLayout(payloadGroup);
    payloadEdit_ = new QPlainTextEdit(payloadGroup);
    payloadEdit_->setPlaceholderText(QStringLiteral("Text to hide or extracted text will appear here"));
    payloadLayout->addWidget(payloadEdit_);

    auto* actionsLayout = new QHBoxLayout;
    auto* embedButton = new QPushButton(QStringLiteral("Embed text"), central);
    auto* extractButton = new QPushButton(QStringLiteral("Extract text"), central);
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

    connect(browseInputButton, &QPushButton::clicked, this, &MainWindow::browseInputImage);
    connect(browseOutputButton, &QPushButton::clicked, this, &MainWindow::browseOutputImage);
    connect(embedButton, &QPushButton::clicked, this, &MainWindow::embedText);
    connect(extractButton, &QPushButton::clicked, this, &MainWindow::extractText);
}

void MainWindow::browseInputImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select input image"),
        QString(),
        QStringLiteral("Images (*.png *.bmp *.tif *.tiff);;All files (*.*)"));

    if (!path.isEmpty()) {
        inputImageEdit_->setText(path);
    }
}

void MainWindow::browseOutputImage()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Select output image"),
        QString(),
        QStringLiteral("PNG image (*.png);;BMP image (*.bmp);;All files (*.*)"));

    if (!path.isEmpty()) {
        outputImageEdit_->setText(path);
    }
}

rfp::stego::StegoParams MainWindow::collectParams() const
{
    rfp::stego::StegoParams params;
    params.bitsPerChannel = static_cast<std::uint8_t>(bitsPerChannelSpin_->value());
    params.seed = static_cast<std::uint32_t>(seedSpin_->value());
    params.useRedChannel = redCheck_->isChecked();
    params.useGreenChannel = greenCheck_->isChecked();
    params.useBlueChannel = blueCheck_->isChecked();
    params.useAlphaChannel = alphaCheck_->isChecked();
    return params;
}

void MainWindow::setStatus(const QString& text)
{
    statusLabel_->setText(text);
}

void MainWindow::embedText()
{
    if (inputImageEdit_->text().isEmpty() || outputImageEdit_->text().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("R.F.P."), QStringLiteral("Specify input and output image paths."));
        return;
    }

    const QString text = payloadEdit_->toPlainText();
    const QByteArray utf8 = text.toUtf8();
    if (utf8.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("R.F.P."), QStringLiteral("Text payload is empty."));
        return;
    }

    auto imageResult = rfp::gui::loadImageBuffer(inputImageEdit_->text());
    if (!imageResult) {
        QMessageBox::warning(this, QStringLiteral("R.F.P."), QString::fromStdString(imageResult.error().message));
        return;
    }

    const auto params = collectParams();
    const auto capacity = rfp::stego::capacityBytes(imageResult.value(), params);
    if (static_cast<std::size_t>(utf8.size()) > capacity) {
        QMessageBox::warning(
            this,
            QStringLiteral("R.F.P."),
            QStringLiteral("Payload is too large. Capacity: %1 bytes, payload: %2 bytes.")
                .arg(capacity)
                .arg(utf8.size()));
        return;
    }

    const auto payload = rfp::core::ByteBuffer(utf8.begin(), utf8.end());
    auto encodedResult = rfp::stego::StegoEncoder::embedBytes(imageResult.value(), payload, params);
    if (!encodedResult) {
        QMessageBox::warning(this, QStringLiteral("R.F.P."), QString::fromStdString(encodedResult.error().message));
        return;
    }

    auto saveResult = rfp::gui::saveImageBuffer(encodedResult.value(), outputImageEdit_->text());
    if (!saveResult) {
        QMessageBox::warning(this, QStringLiteral("R.F.P."), QString::fromStdString(saveResult.error().message));
        return;
    }

    const auto payloadSize = utf8.size();
    if (payloadSize > payloadSizeSpin_->maximum()) {
        payloadSizeSpin_->setValue(payloadSizeSpin_->maximum());
    } else {
        payloadSizeSpin_->setValue(static_cast<int>(payloadSize));
    }

    const auto crc = rfp::core::crc32(std::span<const rfp::core::Byte>(payload.data(), payload.size()));
    setStatus(QStringLiteral("Embedded %1 bytes. CRC32: %2. Remember extraction parameters and payload size.")
                  .arg(utf8.size())
                  .arg(crcToText(crc)));
}

void MainWindow::extractText()
{
    if (inputImageEdit_->text().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("R.F.P."), QStringLiteral("Specify input image path."));
        return;
    }

    auto imageResult = rfp::gui::loadImageBuffer(inputImageEdit_->text());
    if (!imageResult) {
        QMessageBox::warning(this, QStringLiteral("R.F.P."), QString::fromStdString(imageResult.error().message));
        return;
    }

    const auto params = collectParams();
    const auto payloadSize = static_cast<std::size_t>(payloadSizeSpin_->value());
    auto decodedResult = rfp::stego::StegoDecoder::extractBytes(imageResult.value(), payloadSize, params);
    if (!decodedResult) {
        QMessageBox::warning(this, QStringLiteral("R.F.P."), QString::fromStdString(decodedResult.error().message));
        return;
    }

    const QByteArray decodedBytes(
        reinterpret_cast<const char*>(decodedResult.value().data()),
        static_cast<qsizetype>(decodedResult.value().size()));

    payloadEdit_->setPlainText(QString::fromUtf8(decodedBytes));

    const auto crc = rfp::core::crc32(std::span<const rfp::core::Byte>(decodedResult.value().data(), decodedResult.value().size()));
    setStatus(QStringLiteral("Extracted %1 bytes. CRC32: %2.").arg(payloadSize).arg(crcToText(crc)));
}
