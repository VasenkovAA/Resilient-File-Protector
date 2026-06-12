#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>

#include "rfp/stego/StegoParams.h"

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void browseInputImage();
    void browseOutputImage();
    void embedText();
    void extractText();

private:
    [[nodiscard]] rfp::stego::StegoParams collectParams() const;
    void setStatus(const QString& text);

    QLineEdit* inputImageEdit_ = nullptr;
    QLineEdit* outputImageEdit_ = nullptr;
    QPlainTextEdit* payloadEdit_ = nullptr;
    QSpinBox* payloadSizeSpin_ = nullptr;
    QSpinBox* bitsPerChannelSpin_ = nullptr;
    QSpinBox* seedSpin_ = nullptr;
    QCheckBox* redCheck_ = nullptr;
    QCheckBox* greenCheck_ = nullptr;
    QCheckBox* blueCheck_ = nullptr;
    QCheckBox* alphaCheck_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};
