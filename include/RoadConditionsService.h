#ifndef ROAD_CONDITIONS_SERVICE_H
#define ROAD_CONDITIONS_SERVICE_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QVector>

class QNetworkReply;

struct RoadIncident
{
    QString description;
    QString road;
    QString from;
    QString to;
    QString category;
    int delaySeconds = 0;
};

struct RoadConditions
{
    QString roadName;
    QString trafficFlowError;
    QString incidentError;
    double currentSpeedKph = 0.0;
    double freeFlowSpeedKph = 0.0;
    bool hasTrafficFlow = false;
    bool roadClosed = false;
    QVector<RoadIncident> incidents;
};

class RoadConditionsService : public QObject
{
    Q_OBJECT

public:
    explicit RoadConditionsService(QObject* parent = nullptr);

    void fetchConditions(double latitude, double longitude, const QString& apiKey);
    void cancel();

signals:
    void roadConditionsReady(const RoadConditions& conditions);
    void roadConditionsFailed(const QString& message);

private:
    void fetchIncidents(double latitude, double longitude);
    void cancelCurrentRequest();

    QNetworkAccessManager networkManager;
    QNetworkReply* currentReply = nullptr;
    RoadConditions pendingConditions;
    QString apiKey;
};

#endif
