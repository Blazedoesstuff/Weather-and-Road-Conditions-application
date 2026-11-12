#include "AlertService.h"

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

QDateTime parseAlertDate(const QJsonObject& properties, const QString& key)
{
    const QString value = properties.value(key).toString();
    QDateTime dateTime = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value, Qt::ISODate);
    }
    return dateTime;
}

}

AlertService::AlertService(QObject* parent)
    : QObject(parent)
{
}

void AlertService::fetchAlerts(double latitude, double longitude)
{
    cancelCurrentRequest();

    if (!qIsFinite(latitude) || !qIsFinite(longitude) ||
        latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0) {
        emit alertsFailed(QStringLiteral("The selected location has invalid coordinates."));
        return;
    }

    QUrl url(QStringLiteral("https://api.weather.gov/alerts/active"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("point"),
                       QStringLiteral("%1,%2")
                           .arg(latitude, 0, 'f', 4)
                           .arg(longitude, 0, 'f', 4));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "WeatherApp/1.0");
    request.setRawHeader("Accept", "application/geo+json");
    request.setTransferTimeout(15000);

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
            emit alertsFailed(QStringLiteral("Could not load National Weather Service alerts: %1")
                                  .arg(networkErrorText));
            return;
        }

        QVector<WeatherAlert> alerts;
        QString parseError;
        if (!parseAlertsResponse(response, &alerts, &parseError)) {
            emit alertsFailed(parseError);
            return;
        }
        emit alertsReady(alerts);
    });
}

void AlertService::cancel()
{
    cancelCurrentRequest();
}

bool AlertService::parseAlertsResponse(const QByteArray& response,
                                      QVector<WeatherAlert>* alerts,
                                      QString* errorMessage)
{
    if (alerts == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("An alert list is required to parse alert data.");
        }
        return false;
    }
    alerts->clear();

    QJsonParseError jsonError;
    const QJsonDocument document = QJsonDocument::fromJson(response, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The National Weather Service returned invalid alert data.");
        }
        return false;
    }

    const QJsonValue featuresValue = document.object().value(QStringLiteral("features"));
    if (!featuresValue.isArray()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The National Weather Service returned an unexpected alert response.");
        }
        return false;
    }

    const QJsonArray features = featuresValue.toArray();
    alerts->reserve(features.size());
    for (const QJsonValue& featureValue : features) {
        if (!featureValue.isObject()) {
            continue;
        }

        const QJsonObject properties = featureValue.toObject()
                                           .value(QStringLiteral("properties")).toObject();
        WeatherAlert alert;
        alert.event = properties.value(QStringLiteral("event")).toString().trimmed();
        alert.headline = properties.value(QStringLiteral("headline")).toString().trimmed();
        if (alert.event.isEmpty() && alert.headline.isEmpty()) {
            continue;
        }
        alert.severity = properties.value(QStringLiteral("severity")).toString().trimmed();
        alert.urgency = properties.value(QStringLiteral("urgency")).toString().trimmed();
        alert.certainty = properties.value(QStringLiteral("certainty")).toString().trimmed();
        alert.areaDescription = properties.value(QStringLiteral("areaDesc")).toString().trimmed();
        alert.description = properties.value(QStringLiteral("description")).toString().trimmed();
        alert.instruction = properties.value(QStringLiteral("instruction")).toString().trimmed();
        alert.effective = parseAlertDate(properties, QStringLiteral("effective"));
        alert.expires = parseAlertDate(properties, QStringLiteral("expires"));
        alerts->append(alert);
    }

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

void AlertService::cancelCurrentRequest()
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
