#ifndef FORECAST_SERVICE_H
#define FORECAST_SERVICE_H

#include "WeatherData.h"

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ForecastService : public QObject
{
    Q_OBJECT

public:
    explicit ForecastService(QObject* parent = nullptr);

    void fetchForecast(const QString& cityName);
    void fetchForecast(const QString& cityName, double latitude, double longitude);
    void cancel();

signals:
    void forecastReady(const QVector<WeatherData>& forecast);
    void forecastFailed(const QString& message);

private:
    void cancelCurrentRequest();
    void fetchDailyForecast(const QString& cityName, double latitude, double longitude);

    QNetworkAccessManager networkManager;
    QNetworkReply* currentReply = nullptr;
};

#endif
