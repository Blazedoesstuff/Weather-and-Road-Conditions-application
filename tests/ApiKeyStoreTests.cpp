#include "ApiKeyStore.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class ApiKeyStoreTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(settingsDirectory.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("WeatherAppTests"));
        QCoreApplication::setApplicationName(QStringLiteral("ApiKeyStoreTests"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           settingsDirectory.path());
    }

    void encryptsLoadsAndClearsKey()
    {
        const QString apiKey = QStringLiteral("test-only-api-key-value");
        QString error;
        QVERIFY2(ApiKeyStore::save(apiKey, &error), qPrintable(error));

        QSettings settings(QStringLiteral("WeatherApp"), QStringLiteral("WeatherApp"));
        settings.beginGroup(QStringLiteral("credentials"));
        const QByteArray storedValue =
            settings.value(QStringLiteral("openWeatherApiKey")).toByteArray();
        settings.endGroup();
        QVERIFY(!storedValue.isEmpty());
        QVERIFY(!storedValue.contains(apiKey.toUtf8()));

        QString loadedKey;
        QVERIFY2(ApiKeyStore::load(&loadedKey, &error), qPrintable(error));
        QCOMPARE(loadedKey, apiKey);

        QVERIFY2(ApiKeyStore::clear(&error), qPrintable(error));
        QVERIFY2(ApiKeyStore::load(&loadedKey, &error), qPrintable(error));
        QVERIFY(loadedKey.isEmpty());
    }

    void rejectsEmptyKey()
    {
        QString error;
        QVERIFY(!ApiKeyStore::save(QStringLiteral("  "), &error));
        QVERIFY(!error.isEmpty());
    }

    void namedCredentialsAreIndependent()
    {
        const QString weatherKey = QStringLiteral("open-weather-test-key");
        const QString trafficKey = QStringLiteral("tomtom-test-key");
        QString error;
        QVERIFY2(ApiKeyStore::save(weatherKey, &error), qPrintable(error));
        QVERIFY2(ApiKeyStore::saveNamed(QStringLiteral("tomTomTrafficApiKey"),
                                        trafficKey, &error), qPrintable(error));

        QString loadedKey;
        QVERIFY2(ApiKeyStore::load(&loadedKey, &error), qPrintable(error));
        QCOMPARE(loadedKey, weatherKey);
        QVERIFY2(ApiKeyStore::loadNamed(QStringLiteral("tomTomTrafficApiKey"),
                                        &loadedKey, &error), qPrintable(error));
        QCOMPARE(loadedKey, trafficKey);

        QVERIFY2(ApiKeyStore::clearNamed(QStringLiteral("tomTomTrafficApiKey"),
                                         &error), qPrintable(error));
        QVERIFY2(ApiKeyStore::load(&loadedKey, &error), qPrintable(error));
        QCOMPARE(loadedKey, weatherKey);
    }

private:
    QTemporaryDir settingsDirectory;
};

QTEST_GUILESS_MAIN(ApiKeyStoreTests)

#include "ApiKeyStoreTests.moc"
