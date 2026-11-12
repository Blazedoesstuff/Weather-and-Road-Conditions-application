#include "HistoricalWeatherService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QtMath>

namespace {

bool readDailyValue(const QJsonArray& values, int index, double* result)
{
    if (result == nullptr || index < 0 || index >= values.size() ||
        !values.at(index).isDouble()) {
        return false;
    }

    const double value = values.at(index).toDouble();
    if (!qIsFinite(value)) {
        return false;
    }
    *result = value;
    return true;
}

}

HistoricalWeatherService::HistoricalWeatherService(QObject* parent)
    : QObject(parent)
{
}

void HistoricalWeatherService::fetchHistoricalWeather(double latitude, double longitude,
                                                        const QDate& startDate,
                                                        const QDate& endDate)
{
    cancelCurrentRequest();

    const QDate earliestDate(1940, 1, 1);
    const QDate latestAvailableDate = QDate::currentDate().addDays(-5);
    if (!qIsFinite(latitude) || !qIsFinite(longitude) ||
        latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0) {
        emit historicalWeatherFailed(QStringLiteral("The selected location has invalid coordinates."));
        return;
    }
    if (!startDate.isValid() || !endDate.isValid() ||
        startDate < earliestDate || endDate > latestAvailableDate ||
        startDate > endDate) {
        emit historicalWeatherFailed(QStringLiteral(
            "Choose a valid date range from January 1, 1940 through %1.")
            .arg(latestAvailableDate.toString(Qt::ISODate)));
        return;
    }

    QUrl url(QStringLiteral("https://archive-api.open-meteo.com/v1/archive"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(latitude, 'f', 6));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(longitude, 'f', 6));
    query.addQueryItem(QStringLiteral("start_date"), startDate.toString(Qt::ISODate));
    query.addQueryItem(QStringLiteral("end_date"), endDate.toString(Qt::ISODate));
    query.addQueryItem(QStringLiteral("daily"),
                       QStringLiteral("temperature_2m_max,temperature_2m_min,precipitation_sum,wind_speed_10m_max"));
    query.addQueryItem(QStringLiteral("temperature_unit"), QStringLiteral("celsius"));
    query.addQueryItem(QStringLiteral("wind_speed_unit"), QStringLiteral("ms"));
    query.addQueryItem(QStringLiteral("precipitation_unit"), QStringLiteral("mm"));
    query.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(20000);
    QNetworkReply* reply = networkManager.get(request);
    currentReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray response = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        reply->deleteLater();
        if (currentReply != reply) {
            return;
        }
        currentReply = nullptr;

        if (networkError != QNetworkReply::NoError) {
            emit historicalWeatherFailed(
                QStringLiteral("Could not load historical weather: %1").arg(networkErrorText));
            return;
        }

        QVector<HistoricalWeatherDay> days;
        QString errorMessage;
        if (!parseResponse(response, &days, &errorMessage)) {
            emit historicalWeatherFailed(errorMessage);
            return;
        }
        emit historicalWeatherReady(days);
    });
}

void HistoricalWeatherService::cancel()
{
    cancelCurrentRequest();
}

bool HistoricalWeatherService::parseResponse(const QByteArray& response,
                                              QVector<HistoricalWeatherDay>* days,
                                              QString* errorMessage)
{
    if (days == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("A daily result list is required to parse historical weather.");
        }
        return false;
    }
    days->clear();

    QJsonParseError jsonError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The historical weather service returned invalid data.");
        }
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonObject daily = root.value(QStringLiteral("daily")).toObject();
    const QJsonArray dates = daily.value(QStringLiteral("time")).toArray();
    const QJsonArray maximums = daily.value(QStringLiteral("temperature_2m_max")).toArray();
    const QJsonArray minimums = daily.value(QStringLiteral("temperature_2m_min")).toArray();
    const QJsonArray precipitation = daily.value(QStringLiteral("precipitation_sum")).toArray();
    const QJsonArray windSpeeds = daily.value(QStringLiteral("wind_speed_10m_max")).toArray();

    const QString apiReason = root.value(QStringLiteral("reason")).toString();
    if (!apiReason.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Historical weather service error: %1").arg(apiReason);
        }
        return false;
    }
    if (dates.isEmpty() || maximums.size() != dates.size() ||
        minimums.size() != dates.size() || precipitation.size() != dates.size() ||
        windSpeeds.size() != dates.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The historical weather service returned incomplete daily data.");
        }
        return false;
    }

    days->reserve(dates.size());
    for (int i = 0; i < dates.size(); ++i) {
        if (!dates.at(i).isString()) {
            days->clear();
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("The historical weather service returned an invalid date.");
            }
            return false;
        }

        HistoricalWeatherDay day;
        day.date = QDate::fromString(dates.at(i).toString(), Qt::ISODate);
        if (!day.date.isValid() ||
            !readDailyValue(maximums, i, &day.maximumTemperature) ||
            !readDailyValue(minimums, i, &day.minimumTemperature) ||
            !readDailyValue(precipitation, i, &day.precipitationMm) ||
            !readDailyValue(windSpeeds, i, &day.maximumWindSpeed)) {
            days->clear();
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("The historical weather service returned invalid daily values.");
            }
            return false;
        }
        days->append(day);
    }

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

void HistoricalWeatherService::cancelCurrentRequest()
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