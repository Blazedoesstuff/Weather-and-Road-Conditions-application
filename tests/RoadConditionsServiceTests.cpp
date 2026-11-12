#include "RoadConditionsService.h"

#include <QSignalSpy>
#include <QTest>

class RoadConditionsServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMissingApiKey();
    void rejectsInvalidCoordinates();
};

void RoadConditionsServiceTests::rejectsMissingApiKey()
{
    RoadConditionsService service;
    QSignalSpy failureSpy(&service, &RoadConditionsService::roadConditionsFailed);
    service.fetchConditions(40.7128, -74.0060, QString());

    QCOMPARE(failureSpy.size(), 1);
    QVERIFY(failureSpy.first().first().toString().contains(QStringLiteral("API key")));
}

void RoadConditionsServiceTests::rejectsInvalidCoordinates()
{
    RoadConditionsService service;
    QSignalSpy failureSpy(&service, &RoadConditionsService::roadConditionsFailed);
    service.fetchConditions(95.0, -74.0060, QStringLiteral("test-key"));

    QCOMPARE(failureSpy.size(), 1);
    QVERIFY(failureSpy.first().first().toString().contains(QStringLiteral("coordinates")));
}

QTEST_GUILESS_MAIN(RoadConditionsServiceTests)

#include "RoadConditionsServiceTests.moc"
