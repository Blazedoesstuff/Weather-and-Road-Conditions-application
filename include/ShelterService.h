#ifndef SHELTER_SERVICE_H
#define SHELTER_SERVICE_H

#include "ShelterData.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QVector>

class ShelterService : public QObject
{
    Q_OBJECT

public:
    explicit ShelterService(QObject* parent = nullptr);

    void fetchNearbyShelters(double latitude, double longitude, int radiusMeters = 10000);

signals:
    void sheltersReady(const QVector<ShelterData>& shelters);
    void sheltersFailed(const QString& message);

private:
    void cancelCurrentRequest();

    QNetworkAccessManager networkManager;
    QNetworkReply* currentReply = nullptr;
};

#endif
