#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QVector>
#include "AlertData.h"
#include "WeatherData.h"

class QLabel;
class QAction;
class QLineEdit;
class QListWidget;
class QPushButton;
class MapWidget;
class WeatherService;
class CacheService;
class AlertService;
class HistoricalWeatherService;
class RoadConditionsService;
class ForecastService;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    public:
        explicit MainWindow(QWidget *parent = nullptr);
    private:
        QLineEdit* cityInput;
        QPushButton* settingsButton;
        QPushButton* searchButton;
        QPushButton* compareButton;
        QPushButton* saveButton;
        QPushButton* loadCityButton;
        QPushButton* removeCityButton;
        QAction* clearApiKeyAction = nullptr;
        
        QLabel* cityLabel;
        QLabel* tempLabel;
        QLabel* conditionLabel;

        QListWidget* savedCities;
        QLabel* humidityLabel;
        QLabel* windLabel;
        QLabel* statusLabel;
        QLabel* alertsLabel;
        QPushButton* viewAlertsButton;
        QPushButton* historicalWeatherButton;
        QPushButton* roadConditionsButton;
        QPushButton* forecastButton;
        MapWidget* mapWidget;
        
        WeatherService* weatherService;
        WeatherService* comparisonWeatherService;
        AlertService* alertService;
        HistoricalWeatherService* historicalWeatherService;
        RoadConditionsService* roadConditionsService;
        ForecastService* forecastService;
        QVector<WeatherAlert> activeAlerts;
        QString alertLocation;
        QString currentWeatherLocation;
        double currentLatitude = 0.0;
        double currentLongitude = 0.0;
        bool hasCurrentCoordinates = false;
        QStringList comparisonCities;
        QVector<WeatherData> comparisonResults;
        bool comparisonInProgress = false;
        double currentTemperatureCelsius = 0.0;
        double currentWindSpeedMetersPerSecond = 0.0;
        bool hasWeatherData = false;
        bool useFahrenheit = false;
        bool useImperial = false;
        QString tomTomApiKey;
        void applyTheme(const QString& theme);
        void applyTemperatureUnit(bool fahrenheit);
        void applyMeasurementSystem(bool imperial);
        void updateTemperatureDisplay();
        void displayWeather(const WeatherData& data);
        void showWeatherAlerts();
        void showHistoricalWeather();
        void showRoadConditions();
        void showForecast();
        void showWeatherComparison();
        bool persistSavedCities(QString* errorMessage = nullptr) const;

    private slots:
        void onSearchClicked();
        void onCompareClicked();
        void onSaveClicked();
        void onLoadCityClicked();
        void onRemoveCityClicked();
        void onConfigureApiKey();
        void onClearApiKey();
};

#endif
