#pragma once

#include "rfp/crypto/CryptoTypes.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QToolButton>

#include <cstdint>
#include <optional>

/// Panel with encryption/decryption controls.
///
/// Role::Embed   — checkbox + password + confirm + cipher/kdf/iterations.
/// Role::Extract — checkbox + password only (cipher/kdf come from payload).
class CryptoPanel : public QGroupBox {
    Q_OBJECT
public:
    enum class Role { Embed, Extract };

    explicit CryptoPanel(Role role, QWidget* parent = nullptr);

    [[nodiscard]] bool                   encryptionEnabled() const;
    [[nodiscard]] QString                password()          const;
    [[nodiscard]] rfp::crypto::CipherId  cipher()            const;
    [[nodiscard]] rfp::crypto::KdfId     kdf()               const;
    [[nodiscard]] std::uint32_t          iterations()        const;
    [[nodiscard]] std::optional<QString> validatedPassword() const;

    void setEncryptionEnabled(bool on);
    void setPassword(const QString& pw);
    void clearPassword();
    void clearConfirm();

    void setCipher(rfp::crypto::CipherId id);
    void setKdf(rfp::crypto::KdfId id);
    void setIterations(std::uint32_t n);

signals:
    void changed();

private slots:
    void onToggleEnabled(bool on);
    void onToggleReveal(bool reveal);

private:
    Role role_;

    QCheckBox*   enableCheck_    = nullptr;
    QLineEdit*   passwordEdit_   = nullptr;
    QLineEdit*   confirmEdit_    = nullptr;
    QToolButton* revealButton_   = nullptr;
    QLabel*      confirmLabel_   = nullptr;
    QComboBox*   cipherCombo_    = nullptr;
    QComboBox*   kdfCombo_       = nullptr;
    QSpinBox*    iterationsSpin_ = nullptr;
    QLabel*      hintLabel_      = nullptr;
};
