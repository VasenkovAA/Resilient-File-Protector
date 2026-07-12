#pragma once

#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>

#include "rfp/stego/ImageBuffer.h"
#include "rfp/stego/StegoParams.h"

#include <optional>
#include <vector>

#include "HelpDialog.h"

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);

protected:
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void browseInputImage();
  void browseOutputImage();
  void embedText();
  void extractText();
  void onTextChanged();
  void onFullscreen();
  void onHelp();
  void onLanguageChanged(QAction *action);

private:
  void setupUi();
  void updateCapacityInfo();
  void updatePreview();
  void updateMiniPreview();
  void updateUsageInfo();
  void showImage(QGraphicsScene *scene, const QImage &image);
  void setStatus(const QString &text);
  void setProgress(int value, int maximum = 0);
  void updateStats(const QString &text);

  void createMenuBar();
  void loadLanguageSetting();
  void saveLanguageSetting(const QString &lang);
  void applyTooltips();

  [[nodiscard]] rfp::stego::StegoParams collectParams() const;
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

  QLineEdit *inputImageEdit_ = nullptr;
  QLineEdit *outputImageEdit_ = nullptr;
  QPlainTextEdit *payloadEdit_ = nullptr;
  QSpinBox *payloadSizeSpin_ = nullptr;

  QSpinBox *bitsPerChannelSpin_ = nullptr;
  QSpinBox *seedSpin_ = nullptr;
  QCheckBox *redCheck_ = nullptr;
  QCheckBox *greenCheck_ = nullptr;
  QCheckBox *blueCheck_ = nullptr;
  QCheckBox *alphaCheck_ = nullptr;

  QComboBox *modeCombo_ = nullptr;
  QComboBox *windowSizeCombo_ = nullptr;
  QComboBox *metricCombo_ = nullptr;
  QLineEdit *thresholdEdit_ = nullptr;
  QPushButton *autoThresholdButton_ = nullptr;
  QCheckBox *shuffleAfterSortCheck_ = nullptr;

  QLabel *miniPreviewLabel_ = nullptr;

  QGraphicsView *previewView_ = nullptr;
  QGraphicsScene *previewScene_ = nullptr;
  QPushButton *fullscreenButton_ = nullptr;

  QCheckBox *showPreviewCheck_ = nullptr;
  QComboBox *previewModeCombo_ = nullptr;
  QSlider *overlayOpacitySlider_ = nullptr;
  QCheckBox *highlightChangesCheck_ = nullptr;
  QLabel *statsLabel_ = nullptr;

  QLabel *capacityLabel_ = nullptr;
  QLabel *usageLabel_ = nullptr;

  QLabel *statusLabel_ = nullptr;
  QProgressBar *progressBar_ = nullptr;

  QPushButton *embedButton_ = nullptr;
  QPushButton *extractButton_ = nullptr;

  std::optional<rfp::stego::ImageBuffer> currentImage_;
  std::optional<rfp::stego::ImageBuffer> modifiedImage_;

  mutable std::optional<std::vector<double>> cachedDispersions_;
  mutable rfp::stego::StegoParams cachedParams_;
  mutable bool dispersionCacheValid_ = false;

  QPointer<HelpDialog> helpDialog_;
  QActionGroup *languageGroup_ = nullptr;
  QString currentLanguage_ = "en";
};