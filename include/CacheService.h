#ifndef CACHE_SERVICE_H
#define CACHE_SERVICE_H

#include "WeatherData.h"

#include <QString>

class CacheService
{
public:
    explicit CacheService(int cacheDurationSeconds = 1800);

    bool getWeatherData(const QString& cityName, WeatherData* data,
                        QString* errorMessage = nullptr) const;
    bool saveWeatherData(const WeatherData& data,
                         QString* errorMessage = nullptr) const;
    bool removeWeatherData(const QString& cityName,
                           QString* errorMessage = nullptr) const;

private:
    QString cacheKey(const QString& cityName) const;

    int cacheDurationSeconds;
};

#endif