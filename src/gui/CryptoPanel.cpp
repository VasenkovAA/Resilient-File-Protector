#include "CryptoPanel.h"

#include "rfp/crypto/CryptoRegistry.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

CryptoPanel::CryptoPanel(Role role, QWidget* parent)
    : QGroupBox(parent), role_(role)
{
    setTitle(role == Role::Embed ? tr("Encryption")
                                 : tr("Decryption"));
    setCheckable(true);
    setChecked(false);

    auto* outer = new QVBoxLayout(this);
    auto* form  = new QFormLayout;
    outer->addLayout(form);

    // ---- Password row ----
    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText(tr("Password"));

    revealButton_ = new QToolButton(this);
    revealButton_->setText(QString::fromUtf8("\xF0\x9F\x91\x81")); // 👁
    revealButton_->setCheckable(true);
    revealButton_->setToolTip(tr("Show / hide password"));

    auto* pwRow = new QWidget(this);
    auto* pwLay = new QHBoxLayout(pwRow);
    pwLay->setContentsMargins(0, 0, 0, 0);
    pwLay->addWidget(passwordEdit_, 1);
    pwLay->addWidget(revealButton_);
    form->addRow(tr("Password:"), pwRow);

    // ---- Confirm row (Embed only) ----
    if (role_ == Role::Embed) {
        confirmEdit_ = new QLineEdit(this);
        confirmEdit_->setEchoMode(QLineEdit::Password);
        confirmEdit_->setPlaceholderText(tr("Repeat password"));
        confirmLabel_ = new QLabel(tr("Confirm:"), this);
        form->addRow(confirmLabel_, confirmEdit_);
    }

    // ---- Cipher / KDF / iterations (Embed only) ----
    if (role_ == Role::Embed) {
        cipherCombo_ = new QComboBox(this);
        for (const auto& spec : rfp::crypto::CryptoRegistry::availableCiphers())
            cipherCombo_->addItem(QString::fromLatin1(spec.displayName),
                                  static_cast<int>(spec.id));
        if (auto idx = cipherCombo_->findData(
                static_cast<int>(rfp::crypto::CipherId::Aes256Gcm));
            idx >= 0) {
            cipherCombo_->setCurrentIndex(idx);
        }
        form->addRow(tr("Cipher:"), cipherCombo_);

        kdfCombo_ = new QComboBox(this);
        for (const auto& spec : rfp::crypto::CryptoRegistry::availableKdfs()) {
            // Skip Argon2id — not implemented yet.
            if (spec.id == rfp::crypto::KdfId::Argon2id) continue;
            kdfCombo_->addItem(QString::fromLatin1(spec.displayName),
                               static_cast<int>(spec.id));
        }
        if (auto idx = kdfCombo_->findData(
                static_cast<int>(rfp::crypto::KdfId::Pbkdf2HmacSha256));
            idx >= 0) {
            kdfCombo_->setCurrentIndex(idx);
        }
        form->addRow(tr("KDF:"), kdfCombo_);

        iterationsSpin_ = new QSpinBox(this);
        iterationsSpin_->setRange(1000, 100'000'000);
        iterationsSpin_->setSingleStep(1000);
        iterationsSpin_->setValue(100'000);
        iterationsSpin_->setGroupSeparatorShown(true);
        form->addRow(tr("KDF iterations:"), iterationsSpin_);
    }

    // ---- Hint ----
    hintLabel_ = new QLabel(this);
    hintLabel_->setWordWrap(true);
    hintLabel_->setStyleSheet("color: #666; font-style: italic;");
    if (role_ == Role::Embed) {
        hintLabel_->setText(tr("Password is never written to disk or settings. "
                               "Cipher, KDF and iterations are stored inside "
                               "the encrypted payload."));
    } else {
        hintLabel_->setText(tr("Cipher, KDF and iterations are read from the "
                               "payload — only the password is required."));
    }
    outer->addWidget(hintLabel_);

    // ---- Wire up ----
    connect(this, &QGroupBox::toggled, this, &CryptoPanel::onToggleEnabled);
    connect(passwordEdit_, &QLineEdit::textChanged, this, &CryptoPanel::changed);
    connect(revealButton_, &QToolButton::toggled, this, &CryptoPanel::onToggleReveal);
    if (confirmEdit_)
        connect(confirmEdit_, &QLineEdit::textChanged, this, &CryptoPanel::changed);
    if (cipherCombo_)
        connect(cipherCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &CryptoPanel::changed);
    if (kdfCombo_)
        connect(kdfCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &CryptoPanel::changed);
    if (iterationsSpin_)
        connect(iterationsSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &CryptoPanel::changed);

    onToggleEnabled(false);
}

bool CryptoPanel::encryptionEnabled() const { return isChecked(); }

QString CryptoPanel::password() const { return passwordEdit_->text(); }

rfp::crypto::CipherId CryptoPanel::cipher() const
{
    if (!cipherCombo_) return rfp::crypto::CipherId::Aes256Gcm;
    return static_cast<rfp::crypto::CipherId>(cipherCombo_->currentData().toInt());
}

rfp::crypto::KdfId CryptoPanel::kdf() const
{
    if (!kdfCombo_) return rfp::crypto::KdfId::Pbkdf2HmacSha256;
    return static_cast<rfp::crypto::KdfId>(kdfCombo_->currentData().toInt());
}

std::uint32_t CryptoPanel::iterations() const
{
    return iterationsSpin_ ? static_cast<std::uint32_t>(iterationsSpin_->value())
                           : 100'000u;
}

std::optional<QString> CryptoPanel::validatedPassword() const
{
    if (!encryptionEnabled()) return std::nullopt;
    if (passwordEdit_->text().isEmpty()) return std::nullopt;
    if (role_ == Role::Embed &&
        passwordEdit_->text() != confirmEdit_->text()) return std::nullopt;
    return passwordEdit_->text();
}

void CryptoPanel::setEncryptionEnabled(bool on) { setChecked(on); }

void CryptoPanel::setPassword(const QString& pw)
{
    QSignalBlocker b(passwordEdit_);
    passwordEdit_->setText(pw);
    if (confirmEdit_) {
        QSignalBlocker b2(confirmEdit_);
        confirmEdit_->setText(pw);
    }
}

void CryptoPanel::clearPassword()
{
    QSignalBlocker b(passwordEdit_);
    passwordEdit_->clear();
}

void CryptoPanel::clearConfirm()
{
    if (!confirmEdit_) return;
    QSignalBlocker b(confirmEdit_);
    confirmEdit_->clear();
}

void CryptoPanel::setCipher(rfp::crypto::CipherId id)
{
    if (!cipherCombo_) return;
    if (auto idx = cipherCombo_->findData(static_cast<int>(id)); idx >= 0)
        cipherCombo_->setCurrentIndex(idx);
}

void CryptoPanel::setKdf(rfp::crypto::KdfId id)
{
    if (!kdfCombo_) return;
    if (auto idx = kdfCombo_->findData(static_cast<int>(id)); idx >= 0)
        kdfCombo_->setCurrentIndex(idx);
}

void CryptoPanel::setIterations(std::uint32_t n)
{
    if (iterationsSpin_) iterationsSpin_->setValue(static_cast<int>(n));
}

void CryptoPanel::onToggleEnabled(bool on)
{
    const bool enableInner = on;
    passwordEdit_->setEnabled(enableInner);
    revealButton_->setEnabled(enableInner);
    if (confirmEdit_)   confirmEdit_->setEnabled(enableInner);
    if (confirmLabel_)  confirmLabel_->setEnabled(enableInner);
    if (cipherCombo_)   cipherCombo_->setEnabled(enableInner);
    if (kdfCombo_)      kdfCombo_->setEnabled(enableInner);
    if (iterationsSpin_)iterationsSpin_->setEnabled(enableInner);
    hintLabel_->setEnabled(enableInner);
    emit changed();
}

void CryptoPanel::onToggleReveal(bool reveal)
{
    passwordEdit_->setEchoMode(reveal ? QLineEdit::Normal : QLineEdit::Password);
    if (confirmEdit_)
        confirmEdit_->setEchoMode(reveal ? QLineEdit::Normal : QLineEdit::Password);
}