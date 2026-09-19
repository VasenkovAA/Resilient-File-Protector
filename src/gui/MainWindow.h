#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QFutureWatcher>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
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

#include "SettingsDialog.h"
#include "rfp/crypto/CryptoTypes.h"
#include "rfp/payload/PayloadCrypto.h"
#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoParams.h"

#include <cstddef>
#include <optional>

class MaskingDialog;
class HelpDialog;
class CryptoPanel;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

protected:
  void resizeEvent(QResizeEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  void browseInputImage();
  void browseOutputImage();
  void embedText();
  void extractText();
  void onFullscreen();
  void showSettings();
  void showMasking();
  void showHelp();
  void applySettings();
  void onEmbedFinished();
  void onExtractFinished();

  void copyEmbedParams();
  void pasteEmbedParams();
  void copyExtractParams();
  void pasteExtractParams();

  void doUpdate();
  void onPreviewReady();

private:
  enum class ParamsRole { Embed, Extract };

  struct RecomputeResult {
    QString capacityText;
    QString usageText;
    QImage previewImage;
    QString previewStats;
    bool previewValid = false;
  };

  void setupUi();
  void setupConnections();
  void loadSettings();
  void saveSettings();

  void showImage(const QImage &image, bool fit = true);
  void updateMiniPreview();
  void setStatus(const QString &text, int timeoutMs = 0);
  void updateStats(const QString &text);

  void beginBusy(const QString &message);
  void endBusy();

  void runEmbed(const QString &input, const QString &output,
                const QByteArray &data);
  void runExtract(const QString &input);
  void runMasking(const QString &dir, const QString &ext, int count,
                  bool recursive, const QString &exclude);

  void scheduleUpdate();

  [[nodiscard]] rfp::stego::StegoParams collectParams(ParamsRole role) const;
  [[nodiscard]] rfp::payload::EncryptParams
  collectEncryptParams(const QString &password) const;

  [[nodiscard]] QImage
  imageBufferToQImage(const rfp::stego::ImageBuffer &buffer) const;

  static QImage generateDispersionOverlay(const rfp::stego::ImageBuffer &buffer,
                                          const rfp::stego::StegoParams &params,
                                          int overlayOpacity, double &outMin,
                                          double &outMax, double &outMean);
  static QImage generateComparisonView(const QImage &original,
                                       const QImage &modified,
                                       bool highlightChanges);
  static QImage generateChangesMask(const QImage &original,
                                    const QImage &modified);
  [[nodiscard]] static qint64 countChangedPixels(const QImage &original,
                                                 const QImage &modified);
  static QColor dispersionToColor(double value, double minVal, double maxVal);

  static std::size_t cryptoOverheadBytes(rfp::crypto::CipherId id) noexcept;

  [[nodiscard]] QString serializeFull(const rfp::stego::StegoParams &params,
                                      const QString &inputPath,
                                      const QString &outputPath) const;
  [[nodiscard]] bool deserializeFull(const QString &str,
                                     rfp::stego::StegoParams &params,
                                     QString &inputPath, QString &outputPath);

  void applyParamsToEmbedUi(const rfp::stego::StegoParams &params);
  void applyParamsToExtractUi(const rfp::stego::StegoParams &params);
  void syncHeaderUiFromSettings();

  // ---- UI: Embed tab ----
  QTabWidget *tabWidget_ = nullptr;
  QWidget *embedTab_ = nullptr;
  QLineEdit *inputImageEdit_ = nullptr;
  QLineEdit *outputImageEdit_ = nullptr;
  QPlainTextEdit *payloadEdit_ = nullptr;
  QLabel *usageLabel_ = nullptr;
  QLabel *capacityLabel_ = nullptr;

  QSpinBox *embedBitsSpin_ = nullptr;
  QSpinBox *embedSeedSpin_ = nullptr;
  QCheckBox *embedRed_ = nullptr;
  QCheckBox *embedGreen_ = nullptr;
  QCheckBox *embedBlue_ = nullptr;
  QCheckBox *embedAlpha_ = nullptr;
  QComboBox *embedModeCombo_ = nullptr;
  QComboBox *embedWindowCombo_ = nullptr;
  QComboBox *embedMetricCombo_ = nullptr;
  QLineEdit *embedThresholdEdit_ = nullptr;
  QPushButton *embedAutoThresholdBtn_ = nullptr;
  QCheckBox *embedShuffleCheck_ = nullptr;
  CryptoPanel *embedCryptoPanel_ = nullptr;

  QPushButton *browseInputBtn_ = nullptr;
  QPushButton *browseOutputBtn_ = nullptr;
  QPushButton *embedBtn_ = nullptr;
  QPushButton *copyEmbedBtn_ = nullptr;
  QPushButton *pasteEmbedBtn_ = nullptr;

  // ---- UI: Extract tab ----
  QWidget *extractTab_ = nullptr;
  QLineEdit *inputImageExtractEdit_ = nullptr;
  QPlainTextEdit *extractedTextEdit_ = nullptr;
  QSpinBox *payloadSizeSpin_ = nullptr;
  QCheckBox *autoDetectSizeCheck_ = nullptr;

  QSpinBox *extractBitsSpin_ = nullptr;
  QSpinBox *extractSeedSpin_ = nullptr;
  QCheckBox *extractRed_ = nullptr;
  QCheckBox *extractGreen_ = nullptr;
  QCheckBox *extractBlue_ = nullptr;
  QCheckBox *extractAlpha_ = nullptr;
  QComboBox *extractModeCombo_ = nullptr;
  QComboBox *extractWindowCombo_ = nullptr;
  QComboBox *extractMetricCombo_ = nullptr;
  QLineEdit *extractThresholdEdit_ = nullptr;
  QPushButton *extractAutoThresholdBtn_ = nullptr;
  QCheckBox *extractShuffleCheck_ = nullptr;
  CryptoPanel *extractCryptoPanel_ = nullptr;

  QPushButton *browseExtractBtn_ = nullptr;
  QPushButton *extractBtn_ = nullptr;

  // ---- UI: preview / status ----
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

  // ---- State ----
  std::optional<rfp::stego::ImageBuffer> currentImage_;
  std::optional<rfp::stego::ImageBuffer> modifiedImage_;
  std::optional<QImage> currentQImage_;
  std::optional<QImage> modifiedQImage_;

  bool showPreview_ = true;
  PreviewMode previewMode_ = PreviewMode::Original;
  int overlayOpacity_ = 50;
  bool highlightChanges_ = false;
  bool darkTheme_ = false;
  QString language_ = QStringLiteral("en");
  bool writeHeader_ = true;

  QString pendingEmbedOutput_;
  QSize lastFittedSize_;

  // ---- Async ----
  QFutureWatcher<rfp::core::Result<rfp::stego::ImageBuffer>> embedWatcher_;
  QFutureWatcher<rfp::core::Result<rfp::core::ByteBuffer>> extractWatcher_;
  QFutureWatcher<RecomputeResult> previewWatcher_;

  bool embedding_ = false;
  bool extracting_ = false;

  bool recomputeInProgress_ = false;
  bool recomputePending_ = false;
  int busyCounter_ = 0;

  QSettings settings_;
  SettingsDialog *settingsDialog_ = nullptr;
  MaskingDialog *maskingDialog_ = nullptr;
  HelpDialog *helpDialog_ = nullptr;

  QTimer *updateTimer_ = nullptr;
};