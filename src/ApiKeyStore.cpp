#include "ApiKeyStore.h"

#include <QSettings>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {

const QString settingsGroup = QStringLiteral("credentials");
const QString openWeatherSettingsKey = QStringLiteral("openWeatherApiKey");

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

}

bool ApiKeyStore::load(QString* apiKey, QString* errorMessage)
{
    return loadNamed(openWeatherSettingsKey, apiKey, errorMessage);
}

bool ApiKeyStore::loadNamed(const QString& settingsKey, QString* apiKey,
                            QString* errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    if (apiKey == nullptr) {
        setError(errorMessage, QStringLiteral("A destination for the API key is required."));
        return false;
    }
    apiKey->clear();

    QSettings settings(QStringLiteral("WeatherApp"), QStringLiteral("WeatherApp"));
    settings.beginGroup(settingsGroup);
    const QByteArray encrypted = settings.value(settingsKey).toByteArray();
    const QSettings::Status status = settings.status();
    settings.endGroup();
    if (status != QSettings::NoError) {
        setError(errorMessage, QStringLiteral("Could not read the saved API key."));
        return false;
    }
    if (encrypted.isEmpty()) {
        return true;
    }

#ifdef Q_OS_WIN
    const QByteArray encryptedBytes = QByteArray::fromBase64(encrypted);
    if (encryptedBytes.isEmpty() || encryptedBytes.toBase64() != encrypted) {
        setError(errorMessage, QStringLiteral("The saved API key is invalid."));
        return false;
    }

    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(encryptedBytes.constData()));
    input.cbData = static_cast<DWORD>(encryptedBytes.size());
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        setError(errorMessage, QStringLiteral(
            "Could not decrypt the saved API key for this Windows account."));
        return false;
    }

    const QByteArray plainText(reinterpret_cast<const char*>(output.pbData),
                               static_cast<qsizetype>(output.cbData));
    LocalFree(output.pbData);
    const QString decodedKey = QString::fromUtf8(plainText.constData(), plainText.size());
    if (decodedKey.isEmpty() || decodedKey.toUtf8() != plainText) {
        setError(errorMessage, QStringLiteral("The saved API key is invalid."));
        return false;
    }
    *apiKey = decodedKey;
    return true;
#else
    setError(errorMessage, QStringLiteral(
        "Encrypted API-key storage is only supported on Windows."));
    return false;
#endif
}

bool ApiKeyStore::save(const QString& apiKey, QString* errorMessage)
{
    return saveNamed(openWeatherSettingsKey, apiKey, errorMessage);
}

bool ApiKeyStore::saveNamed(const QString& settingsKey, const QString& apiKey,
                            QString* errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    const QByteArray plainText = apiKey.trimmed().toUtf8();
    if (plainText.isEmpty()) {
        setError(errorMessage, QStringLiteral("Enter an API key before saving."));
        return false;
    }

#ifdef Q_OS_WIN
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plainText.constData()));
    input.cbData = static_cast<DWORD>(plainText.size());
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"WeatherApp API key", nullptr,
                          nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        setError(errorMessage, QStringLiteral(
            "Windows could not encrypt the API key for this account."));
        return false;
    }

    const QByteArray encrypted(reinterpret_cast<const char*>(output.pbData),
                               static_cast<qsizetype>(output.cbData));
    LocalFree(output.pbData);

    QSettings settings(QStringLiteral("WeatherApp"), QStringLiteral("WeatherApp"));
    settings.beginGroup(settingsGroup);
    settings.setValue(settingsKey, encrypted.toBase64());
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setError(errorMessage, QStringLiteral("Could not save the encrypted API key."));
        return false;
    }
    return true;
#else
    setError(errorMessage, QStringLiteral(
        "Encrypted API-key storage is only supported on Windows."));
    return false;
#endif
}

bool ApiKeyStore::clear(QString* errorMessage)
{
    return clearNamed(openWeatherSettingsKey, errorMessage);
}

bool ApiKeyStore::clearNamed(const QString& settingsKey, QString* errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    QSettings settings(QStringLiteral("WeatherApp"), QStringLiteral("WeatherApp"));
    settings.beginGroup(settingsGroup);
    settings.remove(settingsKey);
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setError(errorMessage, QStringLiteral("Could not remove the saved API key."));
        return false;
    }
    return true;
}
