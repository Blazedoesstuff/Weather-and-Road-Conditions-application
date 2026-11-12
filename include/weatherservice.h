#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include "ApiKeyStore.h"
#include "CacheService.h"
#include "WeatherData.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

class WeatherService : public QObject
{
    Q_OBJECT

public:
    explicit WeatherService(QObject* parent = nullptr);

    bool setApiKey(const QString& apiKey, QString* errorMessage = nullptr);
    bool clearSavedApiKey(QString* errorMessage = nullptr);
    bool hasApiKey() const;
    bool hasSavedApiKey() const;
    void fetchWeather(const QString& cityName);
    WeatherData getWeatherData() const;
    double getTemperature() const;
    QString getCondition() const;
    double getHumidity() const;

signals:
    void weatherReady(const WeatherData& data);
    void weatherFailed(const QString& message);

private:
    void cancelCurrentRequest();

    WeatherData weatherData;
    CacheService cacheService;
    QString savedApiKey;
    QNetworkAccessManager networkManager;
    QNetworkReply* currentReply = nullptr;
};

#endif