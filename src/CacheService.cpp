#include "CacheService.h"

#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QtMath>

namespace {

const QString cacheGroup = QStringLiteral("weatherCache");

QJsonObject serializeWeatherData(const WeatherData& data)
{
    QJsonObject object;
    object.insert(QStringLiteral("city"), data.city);
    object.insert(QStringLiteral("country"), data.country);
    object.insert(QStringLiteral("description"), data.description);
    object.insert(QStringLiteral("temperature"), data.temperature);
    object.insert(QStringLiteral("humidity"), data.humidity);
    object.insert(QStringLiteral("windSpeed"), data.windSpeed);
    object.insert(QStringLiteral("latitude"), data.latitude);
    object.insert(QStringLiteral("longitude"), data.longitude);
    object.insert(QStringLiteral("hasCoordinates"), data.hasCoordinates);
    object.insert(QStringLiteral("pressure"), data.pressure);
    object.insert(QStringLiteral("visibility"), data.visibility);
    object.insert(QStringLiteral("uvIndex"), data.uvIndex);
    object.insert(QStringLiteral("date"), data.date.toString(Qt::ISODate));
    object.insert(QStringLiteral("minimumTemperature"), data.minimumTemperature);
    object.insert(QStringLiteral("maximumTemperature"), data.maximumTemperature);
    object.insert(QStringLiteral("precipitationProbability"), data.precipitationProbability);

    QJsonArray forecast;
    for (const WeatherData& day : data.forecast) {
        forecast.append(serializeWeatherData(day));
    }
    object.insert(QStringLiteral("forecast"), forecast);
    return object;
}

bool readNumber(const QJsonObject& object, const QString& key, double* value)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isDouble() || !qIsFinite(jsonValue.toDouble())) {
        return false;
    }
    *value = jsonValue.toDouble();
    return true;
}

bool deserializeWeatherData(const QJsonObject& object, WeatherData* data, int depth)
{
    if (depth > 4) {
        return false;
    }

    const QJsonValue cityValue = object.value(QStringLiteral("city"));
    const QJsonValue forecastValue = object.value(QStringLiteral("forecast"));
    double pressure = 0.0;
    double visibility = 0.0;
    double uvIndex = 0.0;
    double precipitationProbability = 0.0;
    if (!cityValue.isString() ||
        !readNumber(object, QStringLiteral("temperature"), &data->temperature) ||
        !readNumber(object, QStringLiteral("humidity"), &data->humidity) ||
        !readNumber(object, QStringLiteral("windSpeed"), &data->windSpeed) ||
        !readNumber(object, QStringLiteral("pressure"), &pressure) ||
        !readNumber(object, QStringLiteral("visibility"), &visibility) ||
        !readNumber(object, QStringLiteral("uvIndex"), &uvIndex) ||
        !readNumber(object, QStringLiteral("minimumTemperature"), &data->minimumTemperature) ||
        !readNumber(object, QStringLiteral("maximumTemperature"), &data->maximumTemperature) ||
        !readNumber(object, QStringLiteral("precipitationProbability"), &precipitationProbability) ||
        !forecastValue.isArray()) {
        return false;
    }

    const QJsonValue hasCoordinatesValue = object.value(QStringLiteral("hasCoordinates"));
    if (hasCoordinatesValue.isBool() && hasCoordinatesValue.toBool()) {
        double latitude = 0.0;
        double longitude = 0.0;
        if (!readNumber(object, QStringLiteral("latitude"), &latitude) ||
            !readNumber(object, QStringLiteral("longitude"), &longitude) ||
            latitude < -90.0 || latitude > 90.0 ||
            longitude < -180.0 || longitude > 180.0) {
            return false;
        }
        data->latitude = latitude;
        data->longitude = longitude;
        data->hasCoordinates = true;
    }

    const QString date = object.value(QStringLiteral("date")).toString();
    const QDate parsedDate = date.isEmpty() ? QDate() : QDate::fromString(date, Qt::ISODate);
    if (!date.isEmpty() && !parsedDate.isValid()) {
        return false;
    }

    if (pressure < 0 || pressure > 100000 || qFloor(pressure) != pressure ||
        visibility < 0 || visibility > 1000000 || qFloor(visibility) != visibility ||
        uvIndex < 0 || uvIndex > 1000 || qFloor(uvIndex) != uvIndex ||
        precipitationProbability < -1 || precipitationProbability > 100 ||
        qFloor(precipitationProbability) != precipitationProbability) {
        return false;
    }

    data->city = cityValue.toString();
    data->country = object.value(QStringLiteral("country")).toString();
    data->description = object.value(QStringLiteral("description")).toString();
    data->pressure = static_cast<int>(pressure);
    data->visibility = static_cast<int>(visibility);
    data->uvIndex = static_cast<int>(uvIndex);
    data->date = parsedDate;
    data->precipitationProbability = static_cast<int>(precipitationProbability);
    data->isCached = true;

    const QJsonArray forecast = forecastValue.toArray();
    data->forecast.reserve(forecast.size());
    for (const QJsonValue& dayValue : forecast) {
        if (!dayValue.isObject()) {
            return false;
        }
        WeatherData day{};
        if (!deserializeWeatherData(dayValue.toObject(), &day, depth + 1)) {
            return false;
        }
        data->forecast.append(day);
    }
    return true;
}

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

}

CacheService::CacheService(int cacheDurationSeconds)
    : cacheDurationSeconds(cacheDurationSeconds)
{
}

bool CacheService::getWeatherData(const QString& cityName, WeatherData* data,
                                  QString* errorMessage) const
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    if (data == nullptr) {
        setError(errorMessage, QStringLiteral("A destination for cached weather data is required."));
        return false;
    }

    const QString key = cacheKey(cityName);
    if (key.isEmpty()) {
        return false;
    }

    QSettings settings(QStringLiteral("WeatherApp"), QStringLiteral("WeatherApp"));
    settings.beginGroup(cacheGroup);
    const QVariant entryValue = settings.value(key);
    const QSettings::Status status = settings.status();
    settings.endGroup();
    if (status != QSettings::NoError) {
        setError(errorMessage, QStringLiteral("Could not read the weather cache."));
        return false;
    }
    if (!entryValue.isValid()) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(entryValue.toByteArray(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage, QStringLiteral("The saved weather cache entry is invalid."));
        return false;
    }

    const QJsonObject entry = document.object();
    const QJsonValue expiresValue = entry.value(QStringLiteral("expiresAt"));
    const QJsonValue dataValue = entry.value(QStringLiteral("data"));
    if (!expiresValue.isDouble() || !dataValue.isObject()) {
        setError(errorMessage, QStringLiteral("The saved weather cache entry is incomplete."));
        return false;
    }
    if (expiresValue.toDouble() <= QDateTime::currentMSecsSinceEpoch()) {
        removeWeatherData(cityName, errorMessage);
        return false;
    }

    WeatherData cachedData{};
    if (!deserializeWeatherData(dataValue.toObject(), &cachedData, 0) ||
        cachedData.city.compare(cityName.trimmed(), Qt::CaseInsensitive) != 0) {
        setError(errorMessage, QStringLiteral("The saved weather cache entry is invalid."));
        return false;
    }

    *data = cachedData;
    return true;
}

bool CacheService::saveWeatherData(const WeatherData& data, QString* errorMessage) const
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    if (cacheDurationSeconds <= 0 || data.city.trimmed().isEmpty() ||
        !qIsFinite(data.temperature) || !qIsFinite(data.humidity) ||
        !qIsFinite(data.windSpeed)) {
        setError(errorMessage, QStringLiteral("Weather data or cache lifetime is invalid."));
        return false;
    }

    QJsonObject entry;
    entry.insert(QStringLiteral("expiresAt"),
                 QDateTime::currentMSecsSinceEpoch() +
                     static_cast<qint64>(cacheDurationSeconds) * 1000);
    entry.insert(QStringLiteral("data"), serializeWeatherData(data));

    QSettings settings(QStringLiteral("WeatherApp"), QStringLiteral("WeatherApp"));
    settings.beginGroup(cacheGroup);
    settings.setValue(cacheKey(data.city), QJsonDocument(entry).toJson(QJsonDocument::Compact));
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setError(errorMessage, QStringLiteral("Could not save weather data to the cache."));
        return false;
    }
    return true;
}

bool CacheService::removeWeatherData(const QString& cityName, QString* errorMessage) const
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    const QString key = cacheKey(cityName);
    if (key.isEmpty()) {
        return true;
    }

    QSettings settings(QStringLiteral("WeatherApp"), QStringLiteral("WeatherApp"));
    settings.beginGroup(cacheGroup);
    settings.remove(key);
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setError(errorMessage, QStringLiteral("Could not remove the weather cache entry."));
        return false;
    }
    return true;
}

QString CacheService::cacheKey(const QString& cityName) const
{
    const QString normalizedCity = cityName.trimmed().toCaseFolded();
    if (normalizedCity.isEmpty()) {
        return {};
    }

    const QByteArray hash = QCryptographicHash::hash(normalizedCity.toUtf8(),
                                                      QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(hash);
}