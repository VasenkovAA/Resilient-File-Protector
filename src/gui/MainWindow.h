#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QFutureWatcher>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>

#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoParams.h"
#include <optional>
#include <vector>

class SettingsDialog;
class MaskingDialog;
class HelpDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void browseInputImage();
    void browseOutputImage();
    void embedText();
    void extractText();
    void onTextChanged();
    void onFullscreen();
    void showSettings();
    void showMasking();
    void showHelp();
    void applySettings();
    void onEmbedFinished();
    void onExtractFinished();
    void onMaskingFinished();

    void copyEmbedParams();
    void pasteEmbedParams();
    void copyExtractParams();
    void pasteExtractParams();

    void doUpdate();
    void onPreviewReady();

private:
    void setupUi();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void showImage(const QImage &image, bool fit = true);
    void setStatus(const QString &text, int timeout = 0);
    void setProgress(int value, int maximum = 0);
    void updateStats(const QString &text);
    void runEmbed(const QString &input, const QString &output, const QByteArray &data);
    void runExtract(const QString &input, size_t payloadSize);
    void runMasking(const QString &dir, const QString &ext, int count,
                    bool recursive, const QString &exclude);
    void updateMiniPreview();

    void scheduleUpdate();

    [[nodiscard]] rfp::stego::StegoParams collectParams(bool forExtract = false) const;
    [[nodiscard]] QImage imageBufferToQImage(const rfp::stego::ImageBuffer &buffer) const;

    // ---- Статические помощники: вызываются из фонового потока, без this ----
    static QImage generateDispersionOverlay(const rfp::stego::ImageBuffer &buffer,
                                            const rfp::stego::StegoParams &params,
                                            int overlayOpacity,
                                            double &outMin, double &outMax, double &outMean);
    static QImage generateComparisonView(const QImage &original, const QImage &modified,
                                         bool highlightChanges);
    static QImage generateChangesMask(const QImage &original, const QImage &modified);
    static QColor dispersionToColor(double value, double minVal, double maxVal);

    [[nodiscard]] QString serializeFull(const rfp::stego::StegoParams &params,
                                        const QString &inputPath,
                                        const QString &outputPath) const;
    [[nodiscard]] bool deserializeFull(const QString &str,
                                       rfp::stego::StegoParams &params,
                                       QString &inputPath,
                                       QString &outputPath) const;

    struct RecomputeResult {
        QString capacityText;
        QString usageText;
        QImage previewImage;
        QString previewStats;
        bool previewValid = false;
    };

    // UI
    QTabWidget *tabWidget_ = nullptr;
    QWidget *embedTab_ = nullptr;
    QLineEdit *inputImageEdit_ = nullptr;
    QLineEdit *outputImageEdit_ = nullptr;
    QPlainTextEdit *payloadEdit_ = nullptr;
    QLabel *usageLabel_ = nullptr;
    QLabel *capacityLabel_ = nullptr;

    QSpinBox *embedBitsSpin_ = nullptr;
    QSpinBox *embedSeedSpin_ = nullptr;
    QCheckBox *embedRed_ = nullptr, *embedGreen_ = nullptr, *embedBlue_ = nullptr, *embedAlpha_ = nullptr;
    QComboBox *embedModeCombo_ = nullptr;
    QComboBox *embedWindowCombo_ = nullptr;
    QComboBox *embedMetricCombo_ = nullptr;
    QLineEdit *embedThresholdEdit_ = nullptr;
    QPushButton *embedAutoThresholdBtn_ = nullptr;
    QCheckBox *embedShuffleCheck_ = nullptr;

    QSpinBox *extractBitsSpin_ = nullptr;
    QSpinBox *extractSeedSpin_ = nullptr;
    QCheckBox *extractRed_ = nullptr, *extractGreen_ = nullptr, *extractBlue_ = nullptr, *extractAlpha_ = nullptr;
    QComboBox *extractModeCombo_ = nullptr;
    QComboBox *extractWindowCombo_ = nullptr;
    QComboBox *extractMetricCombo_ = nullptr;
    QLineEdit *extractThresholdEdit_ = nullptr;
    QPushButton *extractAutoThresholdBtn_ = nullptr;
    QCheckBox *extractShuffleCheck_ = nullptr;

    QWidget *extractTab_ = nullptr;
    QLineEdit *inputImageExtractEdit_ = nullptr;
    QPlainTextEdit *extractedTextEdit_ = nullptr;
    QSpinBox *payloadSizeSpin_ = nullptr;
    QCheckBox *autoDetectSizeCheck_ = nullptr;

    QGraphicsView *previewView_ = nullptr;
    QGraphicsScene *previewScene_ = nullptr;
    QLabel *miniPreviewLabel_ = nullptr;
    QLabel *statsLabel_ = nullptr;
    QProgressBar *progressBar_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QPushButton *settingsButton_ = nullptr;
    QPushButton *maskingButton_ = nullptr;
    QPushButton *fullscreenButton_ = nullptr;
    QPushButton *helpButton_ = nullptr;
    QPushButton *copyParamsBtn_ = nullptr;
    QPushButton *pasteParamsBtn_ = nullptr;

    std::optional<rfp::stego::ImageBuffer> currentImage_;
    std::optional<rfp::stego::ImageBuffer> modifiedImage_;
    std::optional<QImage> currentQImage_;
    std::optional<QImage> modifiedQImage_;

    bool showPreview_ = true;
    int previewMode_ = 0;
    int overlayOpacity_ = 50;
    bool highlightChanges_ = false;
    bool darkTheme_ = false;
    QString language_ = "en";
    bool writeHeader_ = true;

    QFutureWatcher<rfp::core::Result<rfp::stego::ImageBuffer>> embedWatcher_;
    QFutureWatcher<rfp::core::Result<rfp::core::ByteBuffer>> extractWatcher_;
    QFutureWatcher<void> maskingWatcher_;
    QFutureWatcher<RecomputeResult> previewWatcher_;
    bool embedding_ = false, extracting_ = false;

    bool recomputeInProgress_ = false;
    bool recomputePending_ = false;

    QSettings settings_;
    SettingsDialog *settingsDialog_ = nullptr;
    MaskingDialog *maskingDialog_ = nullptr;
    HelpDialog *helpDialog_ = nullptr;

    QTimer *updateTimer_ = nullptr;
};