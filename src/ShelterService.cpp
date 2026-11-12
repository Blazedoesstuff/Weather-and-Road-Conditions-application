#include "ShelterService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QtMath>

#include <algorithm>

namespace {

double distanceInKilometers(double latitude1, double longitude1,
                            double latitude2, double longitude2)
{
    constexpr double earthRadiusKm = 6371.0;
    constexpr double radians = 0.017453292519943295;
    const double lat1 = latitude1 * radians;
    const double lat2 = latitude2 * radians;
    const double deltaLat = (latitude2 - latitude1) * radians;
    const double deltaLon = (longitude2 - longitude1) * radians;
    const double haversine = qPow(qSin(deltaLat / 2.0), 2.0) +
        qCos(lat1) * qCos(lat2) * qPow(qSin(deltaLon / 2.0), 2.0);
    return 2.0 * earthRadiusKm * qAsin(qSqrt(qBound(0.0, haversine, 1.0)));
}

QString shelterAddress(const QJsonObject& tags)
{
    const QString fullAddress = tags.value(QStringLiteral("addr:full")).toString().trimmed();
    if (!fullAddress.isEmpty()) {
        return fullAddress;
    }

    QStringList parts;
    const QString houseNumber = tags.value(QStringLiteral("addr:housenumber")).toString().trimmed();
    const QString street = tags.value(QStringLiteral("addr:street")).toString().trimmed();
    const QString streetAddress = (houseNumber + QLatin1Char(' ') + street).trimmed();
    if (!streetAddress.isEmpty()) {
        parts.append(streetAddress);
    }

    const QString city = tags.value(QStringLiteral("addr:city")).toString().trimmed();
    const QString state = tags.value(QStringLiteral("addr:state")).toString().trimmed();
    QStringList localityParts;
    if (!city.isEmpty()) {
        localityParts.append(city);
    }
    if (!state.isEmpty()) {
        localityParts.append(state);
    }
    const QString locality = localityParts.join(QStringLiteral(", "));
    if (!locality.isEmpty()) {
        parts.append(locality);
    }

    return parts.join(QStringLiteral(", "));
}

}

ShelterService::ShelterService(QObject* parent)
    : QObject(parent)
{
}

void ShelterService::fetchNearbyShelters(double latitude, double longitude, int radiusMeters)
{
    cancelCurrentRequest();

    if (!qIsFinite(latitude) || !qIsFinite(longitude) ||
        latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0) {
        emit sheltersFailed(QStringLiteral("The selected location has invalid coordinates."));
        return;
    }
    if (radiusMeters < 1000 || radiusMeters > 50000) {
        emit sheltersFailed(QStringLiteral("Search radius must be between 1 and 50 kilometers."));
        return;
    }

    const QString query = QStringLiteral(
        "[out:json][timeout:25];"
        "("
        "node[\"amenity\"=\"shelter\"](around:%1,%2,%3);"
        "way[\"amenity\"=\"shelter\"](around:%1,%2,%3);"
        "relation[\"amenity\"=\"shelter\"](around:%1,%2,%3);"
        "node[\"emergency\"=\"shelter\"](around:%1,%2,%3);"
        "way[\"emergency\"=\"shelter\"](around:%1,%2,%3);"
        "relation[\"emergency\"=\"shelter\"](around:%1,%2,%3);"
        ");out center tags;"
    ).arg(radiusMeters)
     .arg(latitude, 0, 'f', 6)
     .arg(longitude, 0, 'f', 6);

    QUrl url(QStringLiteral("https://overpass-api.de/api/interpreter"));
    QUrlQuery body;
    body.addQueryItem(QStringLiteral("data"), query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("User-Agent", "WeatherApp/1.0");
    QNetworkReply* reply = networkManager.post(request, body.toString(QUrl::FullyEncoded).toUtf8());
    currentReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, latitude, longitude]() {
        const QByteArray response = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        reply->deleteLater();
        if (currentReply != reply) {
            return;
        }
        currentReply = nullptr;

        if (networkError != QNetworkReply::NoError) {
            emit sheltersFailed(QStringLiteral("Could not load nearby shelter locations: %1")
                                    .arg(networkErrorText));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            emit sheltersFailed(QStringLiteral("The shelter data service returned invalid data."));
            return;
        }

        const QJsonObject responseObject = document.object();
        const QString serviceRemark = responseObject.value(QStringLiteral("remark")).toString();
        if (!serviceRemark.isEmpty()) {
            emit sheltersFailed(QStringLiteral("The shelter data service could not complete the search: %1")
                                    .arg(serviceRemark));
            return;
        }
        if (!responseObject.value(QStringLiteral("elements")).isArray()) {
            emit sheltersFailed(QStringLiteral("The shelter data service returned an unexpected response."));
            return;
        }

        const QJsonArray elements = responseObject.value(QStringLiteral("elements")).toArray();
        QVector<ShelterData> shelters;
        shelters.reserve(elements.size());
        for (const QJsonValue& value : elements) {
            if (!value.isObject()) {
                continue;
            }

            const QJsonObject element = value.toObject();
            const QJsonObject tags = element.value(QStringLiteral("tags")).toObject();
            const QJsonValue latitudeValue = element.value(QStringLiteral("lat"));
            const QJsonValue longitudeValue = element.value(QStringLiteral("lon"));
            double shelterLatitude = 0.0;
            double shelterLongitude = 0.0;
            if (latitudeValue.isDouble() && longitudeValue.isDouble()) {
                shelterLatitude = latitudeValue.toDouble();
                shelterLongitude = longitudeValue.toDouble();
            } else {
                const QJsonObject center = element.value(QStringLiteral("center")).toObject();
                const QJsonValue centerLatitude = center.value(QStringLiteral("lat"));
                const QJsonValue centerLongitude = center.value(QStringLiteral("lon"));
                if (!centerLatitude.isDouble() || !centerLongitude.isDouble()) {
                    continue;
                }
                shelterLatitude = centerLatitude.toDouble();
                shelterLongitude = centerLongitude.toDouble();
            }
            if (!qIsFinite(shelterLatitude) || !qIsFinite(shelterLongitude) ||
                shelterLatitude < -90.0 || shelterLatitude > 90.0 ||
                shelterLongitude < -180.0 || shelterLongitude > 180.0) {
                continue;
            }

            ShelterData shelter;
            shelter.id = element.value(QStringLiteral("type")).toString() +
                QLatin1Char('/') + QString::number(element.value(QStringLiteral("id")).toVariant().toLongLong());
            shelter.name = tags.value(QStringLiteral("name")).toString().trimmed();
            if (shelter.name.isEmpty()) {
                shelter.name = QStringLiteral("Unnamed shelter location");
            }
            shelter.address = shelterAddress(tags);
            shelter.shelterType = tags.value(QStringLiteral("shelter_type")).toString();
            if (shelter.shelterType.isEmpty()) {
                shelter.shelterType = tags.value(QStringLiteral("social_facility")).toString();
            }
            shelter.source = QStringLiteral("OpenStreetMap");
            shelter.availability = QStringLiteral("Not reported; verify with local authorities");
            shelter.latitude = shelterLatitude;
            shelter.longitude = shelterLongitude;
            shelter.distanceKm = distanceInKilometers(latitude, longitude,
                                                      shelterLatitude, shelterLongitude);
            shelters.append(shelter);
        }

        std::sort(shelters.begin(), shelters.end(),
                  [](const ShelterData& left, const ShelterData& right) {
                      return left.distanceKm < right.distanceKm;
                  });
        emit sheltersReady(shelters);
    });
}

void ShelterService::cancelCurrentRequest()
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
