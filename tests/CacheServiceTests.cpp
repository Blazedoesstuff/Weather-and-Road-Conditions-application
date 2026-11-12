#include "CacheService.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include <limits>

class CacheServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(settingsDirectory.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("WeatherAppTests"));
        QCoreApplication::setApplicationName(QStringLiteral("CacheServiceTests"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           settingsDirectory.path());
    }

    void savesAndLoadsWeatherData()
    {
        CacheService cache;
        WeatherData input{};
        input.city = QStringLiteral("WeatherApp Cache Test City");
        input.country = QStringLiteral("US");
        input.description = QStringLiteral("Partly cloudy");
        input.temperature = 18.5;
        input.humidity = 61.0;
        input.windSpeed = 4.2;
        input.latitude = 40.7128;
        input.longitude = -74.0060;
        input.hasCoordinates = true;

        WeatherData day{};
        day.city = input.city;
        day.date = QDate(2026, 11, 10);
        day.minimumTemperature = 12.0;
        day.maximumTemperature = 21.0;
        day.precipitationProbability = 25;
        input.forecast.append(day);

        QString error;
        QVERIFY2(cache.saveWeatherData(input, &error), qPrintable(error));

        WeatherData output{};
        QVERIFY2(cache.getWeatherData(input.city.toLower(), &output, &error), qPrintable(error));
        QCOMPARE(output.city, input.city);
        QCOMPARE(output.country, input.country);
        QCOMPARE(output.description, input.description);
        QCOMPARE(output.temperature, input.temperature);
        QCOMPARE(output.humidity, input.humidity);
        QCOMPARE(output.windSpeed, input.windSpeed);
        QVERIFY(output.hasCoordinates);
        QCOMPARE(output.latitude, input.latitude);
        QCOMPARE(output.longitude, input.longitude);
        QCOMPARE(output.forecast.size(), 1);
        QCOMPARE(output.forecast.first().date, day.date);
        QCOMPARE(output.forecast.first().minimumTemperature, day.minimumTemperature);
        QCOMPARE(output.forecast.first().maximumTemperature, day.maximumTemperature);
        QCOMPARE(output.forecast.first().precipitationProbability, day.precipitationProbability);
        QVERIFY(output.isCached);
        QVERIFY2(cache.removeWeatherData(input.city, &error), qPrintable(error));
    }

    void reportsCacheMiss()
    {
        CacheService cache;
        WeatherData output{};
        QString error;

        QVERIFY(!cache.getWeatherData(QStringLiteral("WeatherApp Missing Cache Test City"),
                                      &output, &error));
        QVERIFY(error.isEmpty());
    }

    void rejectsInvalidArguments()
    {
        CacheService cache;
        QString error;
        QVERIFY(!cache.getWeatherData(QStringLiteral("Some City"), nullptr, &error));
        QVERIFY(!error.isEmpty());

        WeatherData invalid{};
        invalid.city = QStringLiteral("WeatherApp Invalid Cache Test City");
        invalid.temperature = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!cache.saveWeatherData(invalid, &error));
        QVERIFY(!error.isEmpty());
    }

    void expiresEntries()
    {
        CacheService cache(1);
        WeatherData input{};
        input.city = QStringLiteral("WeatherApp Expiring Cache Test City");
        input.description = QStringLiteral("Clear");

        QString error;
        QVERIFY2(cache.saveWeatherData(input, &error), qPrintable(error));
        QTest::qWait(1100);

        WeatherData output{};
        QVERIFY(!cache.getWeatherData(input.city, &output, &error));
        QVERIFY(error.isEmpty());
    }

private:
    QTemporaryDir settingsDirectory;
};

QTEST_GUILESS_MAIN(CacheServiceTests)

#include "CacheServiceTests.moc"
