#ifndef ALERT_SERVICE_H
#define ALERT_SERVICE_H

#include "AlertData.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QVector>

class QNetworkReply;

class AlertService : public QObject
{
    Q_OBJECT

public:
    explicit AlertService(QObject* parent = nullptr);

    void fetchAlerts(double latitude, double longitude);
    void cancel();
    static bool parseAlertsResponse(const QByteArray& response,
                                    QVector<WeatherAlert>* alerts,
                                    QString* errorMessage = nullptr);

signals:
    void alertsReady(const QVector<WeatherAlert>& alerts);
    void alertsFailed(const QString& message);

private:
    void cancelCurrentRequest();

    QNetworkAccessManager networkManager;
    QNetworkReply* currentReply = nullptr;
};

#endif
