#include "RoadConditionsService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QtMath>

namespace {

bool parseJsonObject(const QByteArray& response, QJsonObject* object)
{
    if (object == nullptr) {
        return false;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(response, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    *object = document.object();
    return true;
}

QString requestError(const QByteArray& response, int statusCode,
                     QNetworkReply::NetworkError networkError,
                     const QString& operation, const QString& reasonPhrase)
{
    if (statusCode == 401 || statusCode == 403) {
        const QString explanation = statusCode == 401
            ? QStringLiteral("TomTom rejected the API key")
            : QStringLiteral("TomTom denied access; verify the key's Traffic API permissions");
        return QStringLiteral("%1: %2 (HTTP %3).")
            .arg(operation, explanation).arg(statusCode);
    }

    QJsonObject errorObject;
    if (parseJsonObject(response, &errorObject)) {
        for (const QString& key : {QStringLiteral("detailedError"),
                                   QStringLiteral("errorText"),
                                   QStringLiteral("message"),
                                   QStringLiteral("error")}) {
            const QJsonValue value = errorObject.value(key);
            if (value.isString() && !value.toString().trimmed().isEmpty()) {
                return QStringLiteral("%1: %2").arg(operation, value.toString().trimmed());
            }
            if (value.isObject()) {
                const QJsonObject details = value.toObject();
                const QString code = details.value(QStringLiteral("code")).toString().trimmed();
                const QString message = details.value(QStringLiteral("message")).toString().trimmed();
                if (!code.isEmpty() || !message.isEmpty()) {
                    const QString detail = code.isEmpty()
                        ? message
                        : (message.isEmpty() ? code : QStringLiteral("%1: %2").arg(code, message));
                    return QStringLiteral("%1: %2").arg(operation, detail);
                }
            }
        }
    }

    if (statusCode > 0) {
        const QString reason = reasonPhrase.trimmed();
        return reason.isEmpty()
            ? QStringLiteral("%1 failed (HTTP %2).").arg(operation).arg(statusCode)
            : QStringLiteral("%1 failed (HTTP %2: %3).")
                  .arg(operation).arg(statusCode).arg(reason);
    }
    return QStringLiteral("%1 failed (network error %2).")
        .arg(operation).arg(static_cast<int>(networkError));
}

}

RoadConditionsService::RoadConditionsService(QObject* parent)
    : QObject(parent)
{
}

void RoadConditionsService::fetchConditions(double latitude, double longitude,
                                             const QString& requestedApiKey)
{
    cancelCurrentRequest();

    const QString key = requestedApiKey.trimmed();
    if (key.isEmpty()) {
        emit roadConditionsFailed(QStringLiteral(
            "TomTom traffic API key is not configured. Add it in Settings."));
        return;
    }
    if (!qIsFinite(latitude) || !qIsFinite(longitude) ||
        latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0) {
        emit roadConditionsFailed(QStringLiteral("The selected location has invalid coordinates."));
        return;
    }

    apiKey = key;
    pendingConditions = RoadConditions{};
    QUrl url(QStringLiteral(
        "https://api.tomtom.com/traffic/services/4/flowSegmentData/absolute/10/json"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("point"),
                       QStringLiteral("%1,%2").arg(latitude, 0, 'f', 6)
                           .arg(longitude, 0, 'f', 6));
    query.addQueryItem(QStringLiteral("unit"), QStringLiteral("kmph"));
    query.addQueryItem(QStringLiteral("key"), apiKey);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);
    QNetworkReply* reply = networkManager.get(request);
    currentReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, latitude, longitude]() {
        const QByteArray response = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString reasonPhrase =
            reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        reply->deleteLater();
        if (currentReply != reply) {
            return;
        }
        currentReply = nullptr;

        const auto continueWithIncidents = [this, latitude, longitude](const QString& error) {
            pendingConditions.trafficFlowError = error;
            fetchIncidents(latitude, longitude);
        };
        QJsonObject root;
        if (networkError != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
            continueWithIncidents(requestError(response, statusCode, networkError,
                                               QStringLiteral("Traffic flow"), reasonPhrase));
            return;
        }
        if (!parseJsonObject(response, &root)) {
            continueWithIncidents(QStringLiteral("TomTom returned invalid traffic-flow data."));
            return;
        }

        const QJsonObject flow = root.value(QStringLiteral("flowSegmentData")).toObject();
        const QJsonValue currentSpeed = flow.value(QStringLiteral("currentSpeed"));
        const QJsonValue freeFlowSpeed = flow.value(QStringLiteral("freeFlowSpeed"));
        if (!currentSpeed.isDouble() || !freeFlowSpeed.isDouble() ||
            !qIsFinite(currentSpeed.toDouble()) || !qIsFinite(freeFlowSpeed.toDouble())) {
            continueWithIncidents(QStringLiteral(
                "TomTom returned no traffic-flow segment at this location."));
            return;
        }
        pendingConditions.hasTrafficFlow = true;
        pendingConditions.currentSpeedKph = currentSpeed.toDouble();
        pendingConditions.freeFlowSpeedKph = freeFlowSpeed.toDouble();
        pendingConditions.roadClosed = flow.value(QStringLiteral("roadClosure")).toBool();
        pendingConditions.roadName = flow.value(QStringLiteral("frc")).toString();
        fetchIncidents(latitude, longitude);
    });
}

void RoadConditionsService::fetchIncidents(double latitude, double longitude)
{
    constexpr double searchRadiusDegrees = 0.15;
    const double minLatitude = qMax(-90.0, latitude - searchRadiusDegrees);
    const double maxLatitude = qMin(90.0, latitude + searchRadiusDegrees);
    const double minLongitude = qMax(-180.0, longitude - searchRadiusDegrees);
    const double maxLongitude = qMin(180.0, longitude + searchRadiusDegrees);

    QUrl url(QStringLiteral("https://api.tomtom.com/traffic/services/5/incidentDetails"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("key"), apiKey);
    query.addQueryItem(QStringLiteral("bbox"),
                       QStringLiteral("%1,%2,%3,%4")
                           .arg(minLongitude, 0, 'f', 6)
                           .arg(minLatitude, 0, 'f', 6)
                           .arg(maxLongitude, 0, 'f', 6)
                           .arg(maxLatitude, 0, 'f', 6));
    query.addQueryItem(QStringLiteral("fields"),
                       QStringLiteral(
                           "{incidents{type,geometry{type,coordinates},properties{iconCategory,"
                           "magnitudeOfDelay,events{description,code,iconCategory},from,to,delay,"
                           "roadNumbers}}}"));
    query.addQueryItem(QStringLiteral("language"), QStringLiteral("en-GB"));
    query.addQueryItem(QStringLiteral("timeValidityFilter"), QStringLiteral("present"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);
    QNetworkReply* reply = networkManager.get(request);
    currentReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray response = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString reasonPhrase =
            reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        reply->deleteLater();
        if (currentReply != reply) {
            return;
        }
        currentReply = nullptr;

        QJsonObject root;
        if (networkError != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
            pendingConditions.incidentError =
                requestError(response, statusCode, networkError,
                             QStringLiteral("Road incidents"), reasonPhrase);
            if (pendingConditions.hasTrafficFlow) {
                emit roadConditionsReady(pendingConditions);
            } else {
                emit roadConditionsFailed(
                    pendingConditions.trafficFlowError + QLatin1Char('\n') +
                    pendingConditions.incidentError);
            }
            return;
        }
        if (!parseJsonObject(response, &root) ||
            !root.value(QStringLiteral("incidents")).isArray()) {
            pendingConditions.incidentError =
                QStringLiteral("TomTom returned invalid road-incident data.");
            if (pendingConditions.hasTrafficFlow) {
                emit roadConditionsReady(pendingConditions);
            } else {
                emit roadConditionsFailed(
                    pendingConditions.trafficFlowError + QLatin1Char('\n') +
                    pendingConditions.incidentError);
            }
            return;
        }

        const QJsonArray incidents = root.value(QStringLiteral("incidents")).toArray();
        for (const QJsonValue& value : incidents) {
            if (!value.isObject()) {
                continue;
            }
            const QJsonObject properties = value.toObject()
                                               .value(QStringLiteral("properties")).toObject();
            RoadIncident incident;
            incident.category = properties.value(QStringLiteral("iconCategory")).toVariant()
                                    .toString();
            incident.from = properties.value(QStringLiteral("from")).toString();
            incident.to = properties.value(QStringLiteral("to")).toString();
            incident.delaySeconds =
                properties.value(QStringLiteral("delay")).toInt();
            const QJsonArray roadNumbers = properties.value(QStringLiteral("roadNumbers")).toArray();
            QStringList roads;
            for (const QJsonValue& road : roadNumbers) {
                if (road.isString()) {
                    roads.append(road.toString());
                }
            }
            incident.road = roads.join(QStringLiteral(", "));
            const QJsonArray events = properties.value(QStringLiteral("events")).toArray();
            if (!events.isEmpty() && events.first().isObject()) {
                incident.description = events.first().toObject()
                                           .value(QStringLiteral("description")).toString();
            }
            if (incident.description.isEmpty()) {
                incident.description = QStringLiteral("Road incident");
            }
            pendingConditions.incidents.append(incident);
        }
        emit roadConditionsReady(pendingConditions);
    });
}

void RoadConditionsService::cancel()
{
    cancelCurrentRequest();
}

void RoadConditionsService::cancelCurrentRequest()
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
