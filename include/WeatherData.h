#ifndef WEATHER_DATA_H
#define WEATHER_DATA_H
#include <QDate>
#include <QString>
#include <QVector>

struct WeatherData {
    QString city;
    QString country;
    QString description;
    double temperature = 0.0;
    double humidity = 0.0;
    double windSpeed = 0.0;
    double latitude = 0.0;
    double longitude = 0.0;
    bool hasCoordinates = false;
    int pressure = 0;
    int visibility = 0;
    int uvIndex = 0;

    QDate date;
    double minimumTemperature = 0.0;
    double maximumTemperature = 0.0;
    int precipitationProbability = -1;

    QVector<WeatherData> forecast;

    bool isCached = false;
};

#endif