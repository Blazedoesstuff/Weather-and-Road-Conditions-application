#ifndef HISTORICAL_WEATHER_SERVICE_H
#define HISTORICAL_WEATHER_SERVICE_H

#include <QDate>
#include <QNetworkAccessManager>
#include <QObject>
#include <QVector>

class QNetworkReply;

struct HistoricalWeatherDay
{
    QDate date;
    double minimumTemperature = 0.0;
    double maximumTemperature = 0.0;
    double precipitationMm = 0.0;
    double maximumWindSpeed = 0.0;
};

class HistoricalWeatherService : public QObject
{
    Q_OBJECT

public:
    explicit HistoricalWeatherService(QObject* parent = nullptr);

    void fetchHistoricalWeather(double latitude, double longitude,
                                const QDate& startDate, const QDate& endDate);
    void cancel();

    static bool parseResponse(const QByteArray& response,
                              QVector<HistoricalWeatherDay>* days,
                              QString* errorMessage = nullptr);

signals:
    void historicalWeatherReady(const QVector<HistoricalWeatherDay>& days);
    void historicalWeatherFailed(const QString& message);

private:
    void cancelCurrentRequest();

    QNetworkAccessManager networkManager;
    QNetworkReply* currentReply = nullptr;
};

#endif