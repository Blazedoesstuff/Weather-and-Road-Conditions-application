#include "HistoricalWeatherService.h"

#include <QTest>

class HistoricalWeatherServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesDailyHistory();
    void rejectsIncompleteHistory();
};

void HistoricalWeatherServiceTests::parsesDailyHistory()
{
    const QByteArray response = R"({
        "daily": {
            "time": ["2026-10-01", "2026-10-02"],
            "temperature_2m_max": [18.5, 20.0],
            "temperature_2m_min": [8.0, 9.5],
            "precipitation_sum": [1.2, 0.0],
            "wind_speed_10m_max": [7.4, 5.2]
        }
    })";

    QVector<HistoricalWeatherDay> days;
    QString error;
    QVERIFY2(HistoricalWeatherService::parseResponse(response, &days, &error),
             qPrintable(error));
    QCOMPARE(days.size(), 2);
    QCOMPARE(days.first().date, QDate(2026, 10, 1));
    QCOMPARE(days.first().maximumTemperature, 18.5);
    QCOMPARE(days.first().minimumTemperature, 8.0);
    QCOMPARE(days.first().precipitationMm, 1.2);
    QCOMPARE(days.last().maximumWindSpeed, 5.2);
    QVERIFY(error.isEmpty());
}

void HistoricalWeatherServiceTests::rejectsIncompleteHistory()
{
    QVector<HistoricalWeatherDay> days;
    QString error;
    QVERIFY(!HistoricalWeatherService::parseResponse(
        QByteArrayLiteral("{\"daily\":{\"time\":[\"2026-10-01\"]}}"), &days, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(days.isEmpty());

    QVERIFY(!HistoricalWeatherService::parseResponse(
        QByteArrayLiteral("{\"reason\":\"Date range is unavailable\"}"), &days, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!HistoricalWeatherService::parseResponse(
        QByteArrayLiteral("not json"), &days, &error));
}

QTEST_APPLESS_MAIN(HistoricalWeatherServiceTests)

#include "HistoricalWeatherServiceTests.moc"
