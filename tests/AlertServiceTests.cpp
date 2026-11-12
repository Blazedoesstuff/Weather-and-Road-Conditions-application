#include "AlertService.h"

#include <QTest>

class AlertServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesActiveAlert();
    void parsesEmptyAlertCollection();
    void rejectsInvalidResponse();
};

void AlertServiceTests::parsesActiveAlert()
{
    const QByteArray response = R"({
        "type":"FeatureCollection",
        "features":[{
            "type":"Feature",
            "properties":{
                "event":"Severe Thunderstorm Warning",
                "headline":"Severe thunderstorms expected",
                "severity":"Severe",
                "urgency":"Immediate",
                "certainty":"Observed",
                "areaDesc":"Example County",
                "description":"Large hail and damaging winds are expected.",
                "instruction":"Move indoors.",
                "effective":"2026-11-09T20:00:00-05:00",
                "expires":"2026-11-09T21:00:00-05:00"
            }
        }]
    })";

    QVector<WeatherAlert> alerts;
    QString error;
    QVERIFY2(AlertService::parseAlertsResponse(response, &alerts, &error), qPrintable(error));
    QCOMPARE(alerts.size(), 1);
    QCOMPARE(alerts.first().event, QStringLiteral("Severe Thunderstorm Warning"));
    QCOMPARE(alerts.first().severity, QStringLiteral("Severe"));
    QCOMPARE(alerts.first().areaDescription, QStringLiteral("Example County"));
    QCOMPARE(alerts.first().instruction, QStringLiteral("Move indoors."));
    QVERIFY(alerts.first().effective.isValid());
    QVERIFY(alerts.first().expires.isValid());
    QVERIFY(error.isEmpty());
}

void AlertServiceTests::parsesEmptyAlertCollection()
{
    QVector<WeatherAlert> alerts;
    QVERIFY(AlertService::parseAlertsResponse(
        QByteArrayLiteral("{\"features\":[]}"), &alerts));
    QVERIFY(alerts.isEmpty());
}

void AlertServiceTests::rejectsInvalidResponse()
{
    QVector<WeatherAlert> alerts;
    QString error;
    QVERIFY(!AlertService::parseAlertsResponse(QByteArrayLiteral("{}"), &alerts, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!AlertService::parseAlertsResponse(QByteArrayLiteral("not json"), &alerts, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!AlertService::parseAlertsResponse(QByteArrayLiteral("{\"features\":[]}"), nullptr, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_APPLESS_MAIN(AlertServiceTests)

#include "AlertServiceTests.moc"
