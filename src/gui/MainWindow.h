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
  void updatePreview();
  void updateCapacityInfo();
  void updateUsageInfo();
  void updateMiniPreview();
  void applySettings();
  void onEmbedFinished();
  void onExtractFinished();
  void onDispersionFinished();
  void onMaskingFinished();
  void onAutoThreshold();
  void updateExtractParamsInfo();


  void copyEmbedParams();
  void pasteEmbedParams();
  void copyExtractParams();
  void pasteExtractParams();

private:
  void setupUi();
  void setupConnections();
  void loadSettings();
  void saveSettings();
  void showImage(const QImage &image, bool fit = true);
  void setStatus(const QString &text, int timeout = 0);
  void setProgress(int value, int maximum = 0);
  void updateStats(const QString &text);
  void runEmbed(const QString &input, const QString &output,
                const QByteArray &data);
  void runExtract(const QString &input, size_t payloadSize);
  void runDispersion();
  void runMasking(const QString &dir, const QString &ext, int count,
                  bool recursive, const QString &exclude);

  [[nodiscard]] rfp::stego::StegoParams
  collectParams(bool forExtract = false) const;
  [[nodiscard]] QImage
  imageBufferToQImage(const rfp::stego::ImageBuffer &buffer) const;
  QImage generateDispersionOverlay(const rfp::stego::ImageBuffer &buffer,
                                   const rfp::stego::StegoParams &params,
                                   double &outMin, double &outMax,
                                   double &outMean);
  [[nodiscard]] QImage generateComparisonView(const QImage &original,
                                              const QImage &modified,
                                              bool highlightChanges) const;
  [[nodiscard]] QImage generateChangesMask(const QImage &original,
                                           const QImage &modified) const;
  [[nodiscard]] QColor dispersionToColor(double value, double minVal,
                                         double maxVal) const;


  [[nodiscard]] QString serializeFull(const rfp::stego::StegoParams &params,
                                      const QString &inputPath,
                                      const QString &outputPath) const;
  [[nodiscard]] bool deserializeFull(const QString &str,
                                     rfp::stego::StegoParams &params,
                                     QString &inputPath,
                                     QString &outputPath) const;


  QTabWidget *tabWidget_;
  QWidget *embedTab_;
  QLineEdit *inputImageEdit_;
  QLineEdit *outputImageEdit_;
  QPlainTextEdit *payloadEdit_;
  QLabel *usageLabel_;
  QLabel *capacityLabel_;


  QSpinBox *embedBitsSpin_;
  QSpinBox *embedSeedSpin_;
  QCheckBox *embedRed_, *embedGreen_, *embedBlue_, *embedAlpha_;
  QComboBox *embedModeCombo_;
  QComboBox *embedWindowCombo_;
  QComboBox *embedMetricCombo_;
  QLineEdit *embedThresholdEdit_;
  QPushButton *embedAutoThresholdBtn_;
  QCheckBox *embedShuffleCheck_;


  QSpinBox *extractBitsSpin_;
  QSpinBox *extractSeedSpin_;
  QCheckBox *extractRed_, *extractGreen_, *extractBlue_, *extractAlpha_;
  QComboBox *extractModeCombo_;
  QComboBox *extractWindowCombo_;
  QComboBox *extractMetricCombo_;
  QLineEdit *extractThresholdEdit_;
  QPushButton *extractAutoThresholdBtn_;
  QCheckBox *extractShuffleCheck_;

  QWidget *extractTab_;
  QLineEdit *inputImageExtractEdit_;
  QPlainTextEdit *extractedTextEdit_;
  QSpinBox *payloadSizeSpin_;
  QCheckBox *autoDetectSizeCheck_;

  QGraphicsView *previewView_;
  QGraphicsScene *previewScene_;
  QLabel *miniPreviewLabel_;
  QLabel *statsLabel_;
  QProgressBar *progressBar_;
  QLabel *statusLabel_;
  QPushButton *settingsButton_;
  QPushButton *maskingButton_;
  QPushButton *fullscreenButton_;
  QPushButton *helpButton_;
  QPushButton *copyParamsBtn_;
  QPushButton *pasteParamsBtn_;


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

  mutable std::optional<std::vector<double>> cachedDispersions_;
  mutable rfp::stego::StegoParams cachedParams_;
  mutable bool dispersionCacheValid_ = false;

  QFutureWatcher<rfp::core::Result<rfp::stego::ImageBuffer>> embedWatcher_;
  QFutureWatcher<rfp::core::Result<rfp::core::ByteBuffer>> extractWatcher_;
  QFutureWatcher<void> maskingWatcher_;
  QFutureWatcher<void> dispersionWatcher_;
  bool embedding_ = false, extracting_ = false;

  QSettings settings_;
  SettingsDialog *settingsDialog_ = nullptr;
  MaskingDialog *maskingDialog_ = nullptr;
  HelpDialog *helpDialog_ = nullptr;
};