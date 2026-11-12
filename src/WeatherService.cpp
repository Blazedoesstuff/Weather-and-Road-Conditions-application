#include "weatherservice.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QtMath>

WeatherService::WeatherService(QObject* parent)
    : QObject(parent)
{
    QString error;
    if (!ApiKeyStore::load(&savedApiKey, &error)) {
        qWarning() << error;
    }
}

bool WeatherService::setApiKey(const QString& apiKey, QString* errorMessage)
{
    const QString normalizedKey = apiKey.trimmed();
    if (!ApiKeyStore::save(normalizedKey, errorMessage)) {
        return false;
    }
    savedApiKey = normalizedKey;
    return true;
}

bool WeatherService::clearSavedApiKey(QString* errorMessage)
{
    if (!ApiKeyStore::clear(errorMessage)) {
        return false;
    }
    savedApiKey.clear();
    return true;
}

bool WeatherService::hasApiKey() const
{
    return !qgetenv("WEATHER_API_KEY").trimmed().isEmpty() || !savedApiKey.isEmpty();
}

bool WeatherService::hasSavedApiKey() const
{
    return !savedApiKey.isEmpty();
}

void WeatherService::fetchWeather(const QString& cityName)
{
    cancelCurrentRequest();

    const QString city = cityName.trimmed();
    if (city.isEmpty()) {
        emit weatherFailed(QStringLiteral("Enter a city name."));
        return;
    }

    QByteArray apiKey = qgetenv("WEATHER_API_KEY").trimmed();
    if (apiKey.isEmpty()) {
        apiKey = savedApiKey.toUtf8();
    }
    if (apiKey.isEmpty()) {
        emit weatherFailed(QStringLiteral(
            "OpenWeather API key is not configured. Add it in Settings."));
        return;
    }

    QUrl url(QStringLiteral("https://api.openweathermap.org/data/2.5/weather"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), city);
    query.addQueryItem(QStringLiteral("appid"), QString::fromUtf8(apiKey));
    query.addQueryItem(QStringLiteral("units"), QStringLiteral("metric"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    currentReply = networkManager.get(request);
    QNetworkReply* reply = currentReply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, city]() {
        const QByteArray response = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString networkErrorText = reply->errorString();
        reply->deleteLater();
        if (currentReply != reply) {
            return;
        }
        currentReply = nullptr;

        const auto reportRefreshFailure = [this, &city](const QString& message) {
            WeatherData cachedData{};
            QString cacheError;
            if (cacheService.getWeatherData(city, &cachedData, &cacheError)) {
                weatherData = cachedData;
                emit weatherReady(weatherData);
                qWarning() << message << "Showing cached weather.";
                return;
            }
            if (!cacheError.isEmpty()) {
                qWarning() << cacheError;
            }
            emit weatherFailed(message);
        };

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            reportRefreshFailure(networkError != QNetworkReply::NoError
                ? QStringLiteral("Could not load weather: %1").arg(networkErrorText)
                : QStringLiteral("OpenWeather returned invalid data."));
            return;
        }

        const QJsonObject result = document.object();
        if (networkError != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
            const QString apiMessage = result.value(QStringLiteral("message")).toString();
            if (statusCode == 401) {
                reportRefreshFailure(QStringLiteral(
                    "OpenWeather rejected the API key. Check that it is valid and activated."));
            } else if (statusCode == 404) {
                reportRefreshFailure(QStringLiteral("OpenWeather could not find that city."));
            } else if (!apiMessage.isEmpty()) {
                reportRefreshFailure(QStringLiteral("OpenWeather error: %1").arg(apiMessage));
            } else {
                reportRefreshFailure(QStringLiteral("Could not load weather: %1").arg(networkErrorText));
            }
            return;
        }

        const QJsonObject main = result.value(QStringLiteral("main")).toObject();
        const QJsonObject wind = result.value(QStringLiteral("wind")).toObject();
        const QJsonArray conditions = result.value(QStringLiteral("weather")).toArray();
        const QJsonObject system = result.value(QStringLiteral("sys")).toObject();
        const QJsonValue temperature = main.value(QStringLiteral("temp"));
        const QJsonValue humidity = main.value(QStringLiteral("humidity"));
        const QJsonValue pressure = main.value(QStringLiteral("pressure"));
        const QJsonValue windSpeed = wind.value(QStringLiteral("speed"));
        if (!temperature.isDouble() || !humidity.isDouble() ||
            !pressure.isDouble() || !windSpeed.isDouble() ||
            conditions.isEmpty() || !conditions.first().isObject() ||
            !qIsFinite(temperature.toDouble()) || !qIsFinite(humidity.toDouble()) ||
            !qIsFinite(pressure.toDouble()) || !qIsFinite(windSpeed.toDouble())) {
            reportRefreshFailure(QStringLiteral("OpenWeather returned incomplete weather data."));
            return;
        }

        const QJsonObject condition = conditions.first().toObject();
        const QString description = condition.value(QStringLiteral("description")).toString();
        if (description.isEmpty()) {
            reportRefreshFailure(QStringLiteral("OpenWeather returned incomplete weather data."));
            return;
        }

        WeatherData freshData{};
        freshData.city = result.value(QStringLiteral("name")).toString(city);
        freshData.country = system.value(QStringLiteral("country")).toString();
        freshData.description = description;
        freshData.temperature = temperature.toDouble();
        freshData.humidity = humidity.toDouble();
        freshData.windSpeed = windSpeed.toDouble();
        freshData.pressure = qRound(pressure.toDouble());
        const QJsonObject coordinates = result.value(QStringLiteral("coord")).toObject();
        const QJsonValue latitude = coordinates.value(QStringLiteral("lat"));
        const QJsonValue longitude = coordinates.value(QStringLiteral("lon"));
        if (latitude.isDouble() && longitude.isDouble() &&
            qIsFinite(latitude.toDouble()) && qIsFinite(longitude.toDouble()) &&
            latitude.toDouble() >= -90.0 && latitude.toDouble() <= 90.0 &&
            longitude.toDouble() >= -180.0 && longitude.toDouble() <= 180.0) {
            freshData.latitude = latitude.toDouble();
            freshData.longitude = longitude.toDouble();
            freshData.hasCoordinates = true;
        }
        const QJsonValue visibility = result.value(QStringLiteral("visibility"));
        if (visibility.isDouble() && qIsFinite(visibility.toDouble()) &&
            visibility.toDouble() >= 0.0 && visibility.toDouble() <= 1000000.0) {
            freshData.visibility = qRound(visibility.toDouble());
        }

        QString saveError;
        if (!cacheService.saveWeatherData(freshData, &saveError)) {
            qWarning() << saveError;
        }
        weatherData = freshData;
        emit weatherReady(weatherData);
    });
}

void WeatherService::cancelCurrentRequest()
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

WeatherData WeatherService::getWeatherData() const
{
    return weatherData;
}

double WeatherService::getTemperature() const
{
    return weatherData.temperature;
}

QString WeatherService::getCondition() const
{
    return weatherData.description;
}

double WeatherService::getHumidity() const
{
    return weatherData.humidity;
}