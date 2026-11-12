#ifndef API_KEY_STORE_H
#define API_KEY_STORE_H

#include <QString>

namespace ApiKeyStore
{

bool load(QString* apiKey, QString* errorMessage = nullptr);
bool save(const QString& apiKey, QString* errorMessage = nullptr);
bool clear(QString* errorMessage = nullptr);
bool loadNamed(const QString& settingsKey, QString* apiKey,
               QString* errorMessage = nullptr);
bool saveNamed(const QString& settingsKey, const QString& apiKey,
               QString* errorMessage = nullptr);
bool clearNamed(const QString& settingsKey, QString* errorMessage = nullptr);

}

#endif
