#include "ForecastService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QtMath>

namespace {

QString weatherDescription(int code)
{
    if (code == 0) return QStringLiteral("Clear sky");
    if (code == 1) return QStringLiteral("Mainly clear");
    if (code == 2) return QStringLiteral("Partly cloudy");
    if (code == 3) return QStringLiteral("Overcast");
    if (code == 45 || code == 48) return QStringLiteral("Fog");
    if (code >= 51 && code <= 57) return QStringLiteral("Drizzle");
    if (code >= 61 && code <= 67) return QStringLiteral("Rain");
    if (code >= 71 && code <= 77) return QStringLiteral("Snow");
    if (code >= 80 && code <= 82) return QStringLiteral("Rain showers");
    if (code >= 85 && code <= 86) return QStringLiteral("Snow showers");
    if (code == 95 || code == 96 || code == 99) return QStringLiteral("Thunderstorm");
    return QStringLiteral("Unknown conditions");
}

bool readNumber(const QJsonArray& values, int index, double* result)
{
    if (index < 0 || index >= values.size() || !values.at(index).isDouble()) {
        return false;
    }

    *result = values.at(index).toDouble();
    return true;
}

}

ForecastService::ForecastService(QObject* parent)
    : QObject(parent)
{
}

void ForecastService::fetchForecast(const QString& cityName)
{
    cancelCurrentRequest();

    const QString city = cityName.trimmed();
    if (city.isEmpty()) {
        emit forecastFailed(QStringLiteral("Enter a city name to get a forecast."));
        return;
    }

    QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("name"), city);
    query.addQueryItem(QStringLiteral("count"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("language"), QStringLiteral("en"));
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(query);

    QNetworkReply* reply = networkManager.get(QNetworkRequest(url));
    currentReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, city]() {
        const QByteArray response = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        reply->deleteLater();
        if (currentReply != reply) {
            return;
        }
        currentReply = nullptr;

        if (networkError != QNetworkReply::NoError) {
            emit forecastFailed(QStringLiteral("Could not find the city: %1").arg(networkErrorText));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            emit forecastFailed(QStringLiteral("The location service returned invalid data."));
            return;
        }

        const QJsonArray results = document.object().value(QStringLiteral("results")).toArray();
        if (results.isEmpty() || !results.first().isObject()) {
            emit forecastFailed(QStringLiteral("No matching location was found."));
            return;
        }

        const QJsonObject location = results.first().toObject();
        const QJsonValue latitude = location.value(QStringLiteral("latitude"));
        const QJsonValue longitude = location.value(QStringLiteral("longitude"));
        if (!latitude.isDouble() || !longitude.isDouble()) {
            emit forecastFailed(QStringLiteral("The location service returned incomplete coordinates."));
            return;
        }

        fetchDailyForecast(city, latitude.toDouble(), longitude.toDouble());
    });
}

void ForecastService::fetchForecast(const QString& cityName, double latitude, double longitude)
{
    cancelCurrentRequest();
    const QString city = cityName.trimmed();
    if (city.isEmpty() || !qIsFinite(latitude) || !qIsFinite(longitude) ||
        latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0) {
        emit forecastFailed(QStringLiteral("A city and valid coordinates are required for a forecast."));
        return;
    }

    fetchDailyForecast(city, latitude, longitude);
}

void ForecastService::cancel()
{
    cancelCurrentRequest();
}

void ForecastService::cancelCurrentRequest()
{
    if (currentReply == nullptr) {
        return;
    }

    QNetworkReply* reply = currentReply;
    currentReply = nullptr;
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
}

void ForecastService::fetchDailyForecast(const QString& cityName, double latitude, double longitude)
{
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(latitude, 'f', 6));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(longitude, 'f', 6));
    query.addQueryItem(QStringLiteral("daily"),
                       QStringLiteral("weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,wind_speed_10m_max"));
    query.addQueryItem(QStringLiteral("forecast_days"), QStringLiteral("7"));
    query.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    url.setQuery(query);

    QNetworkReply* reply = networkManager.get(QNetworkRequest(url));
    currentReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, cityName]() {
        const QByteArray response = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        reply->deleteLater();
        if (currentReply != reply) {
            return;
        }
        currentReply = nullptr;

        if (networkError != QNetworkReply::NoError) {
            emit forecastFailed(QStringLiteral("Could not load the forecast: %1").arg(networkErrorText));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            emit forecastFailed(QStringLiteral("The forecast service returned invalid data."));
            return;
        }

        const QJsonObject daily = document.object().value(QStringLiteral("daily")).toObject();
        const QJsonArray dates = daily.value(QStringLiteral("time")).toArray();
        const QJsonArray weatherCodes = daily.value(QStringLiteral("weather_code")).toArray();
        const QJsonArray maximums = daily.value(QStringLiteral("temperature_2m_max")).toArray();
        const QJsonArray minimums = daily.value(QStringLiteral("temperature_2m_min")).toArray();
        const QJsonArray precipitation = daily.value(QStringLiteral("precipitation_probability_max")).toArray();
        const QJsonArray windSpeeds = daily.value(QStringLiteral("wind_speed_10m_max")).toArray();

        if (dates.size() != 7 || weatherCodes.size() != dates.size() ||
            maximums.size() != dates.size() || minimums.size() != dates.size() ||
            precipitation.size() != dates.size() || windSpeeds.size() != dates.size()) {
            emit forecastFailed(QStringLiteral("The forecast service returned incomplete daily data."));
            return;
        }

        QVector<WeatherData> forecast;
        forecast.reserve(dates.size());
        for (int i = 0; i < dates.size(); ++i) {
            if (!dates.at(i).isString() || !weatherCodes.at(i).isDouble()) {
                emit forecastFailed(QStringLiteral("The forecast service returned invalid daily data."));
                return;
            }

            const QDate date = QDate::fromString(dates.at(i).toString(), Qt::ISODate);
            double maximum = 0.0;
            double minimum = 0.0;
            double precipitationChance = 0.0;
            double windSpeed = 0.0;
            if (!date.isValid() || !readNumber(maximums, i, &maximum) ||
                !readNumber(minimums, i, &minimum) ||
                !readNumber(precipitation, i, &precipitationChance) ||
                !readNumber(windSpeeds, i, &windSpeed)) {
                emit forecastFailed(QStringLiteral("The forecast service returned invalid daily values."));
                return;
            }

            WeatherData day{};
            day.city = cityName;
            day.date = date;
            day.description = weatherDescription(weatherCodes.at(i).toInt());
            day.minimumTemperature = minimum;
            day.maximumTemperature = maximum;
            day.temperature = (minimum + maximum) / 2.0;
            day.precipitationProbability = static_cast<int>(precipitationChance);
            day.windSpeed = windSpeed;
            forecast.append(day);
        }

        emit forecastReady(forecast);
    });
}