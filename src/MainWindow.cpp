#include "MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QActionGroup>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHeaderView>
#include <QInputDialog>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QWidget>
#include <QtMath>
#include "MapWidget.h"
#include "AlertService.h"
#include "HistoricalWeatherService.h"
#include "ApiKeyStore.h"
#include "RoadConditionsService.h"
#include "ForecastService.h"
#include "weatherservice.h"

#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    weatherService = new WeatherService(this);
    comparisonWeatherService = new WeatherService(this);
    alertService = new AlertService(this);
    historicalWeatherService = new HistoricalWeatherService(this);
    roadConditionsService = new RoadConditionsService(this);
    forecastService = new ForecastService(this);
    QString tomTomKeyError;
    if (!ApiKeyStore::loadNamed(QStringLiteral("tomTomTrafficApiKey"),
                                &tomTomApiKey, &tomTomKeyError)) {
        qWarning() << tomTomKeyError;
    }
    connect(weatherService, &WeatherService::weatherReady,
            this, &MainWindow::displayWeather);
    connect(weatherService, &WeatherService::weatherFailed, this, [this](const QString& message) {
        statusLabel->setText(QStringLiteral("Status: %1").arg(message));
        alertsLabel->setText(QStringLiteral("Weather alerts: Search a valid location to check alerts"));
        viewAlertsButton->setText(QStringLiteral("View Alerts"));
        viewAlertsButton->setEnabled(false);
    });
    connect(alertService, &AlertService::alertsReady, this,
            [this](const QVector<WeatherAlert>& alerts) {
        activeAlerts = alerts;
        if (alerts.isEmpty()) {
            alertsLabel->setText(QStringLiteral("Weather alerts: None active"));
            viewAlertsButton->setText(QStringLiteral("View Alerts"));
            viewAlertsButton->setEnabled(false);
            return;
        }

        alertsLabel->setText(QStringLiteral("Weather alerts: %1 active for %2")
                                 .arg(alerts.size())
                                 .arg(alertLocation));
        viewAlertsButton->setText(QStringLiteral("View Alerts (%1)").arg(alerts.size()));
        viewAlertsButton->setEnabled(true);
    });
    connect(alertService, &AlertService::alertsFailed, this, [this](const QString& message) {
        activeAlerts.clear();
        alertsLabel->setText(QStringLiteral("Weather alerts unavailable: %1").arg(message));
        viewAlertsButton->setText(QStringLiteral("View Alerts"));
        viewAlertsButton->setEnabled(false);
        qWarning() << message;
    });
    connect(comparisonWeatherService, &WeatherService::weatherReady, this,
            [this](const WeatherData& data) {
        if (!comparisonInProgress) {
            return;
        }

        comparisonResults.append(data);
        if (comparisonResults.size() == 1) {
            statusLabel->setText(QStringLiteral("Status: Loading %1...")
                                     .arg(comparisonCities.at(1)));
            comparisonWeatherService->fetchWeather(comparisonCities.at(1));
            return;
        }

        comparisonInProgress = false;
        compareButton->setEnabled(savedCities->selectedItems().size() == 2);
        statusLabel->setText("Status: Comparison ready");
        showWeatherComparison();
    });
    connect(comparisonWeatherService, &WeatherService::weatherFailed, this,
            [this](const QString& message) {
        if (!comparisonInProgress) {
            return;
        }

        comparisonInProgress = false;
        comparisonResults.clear();
        compareButton->setEnabled(savedCities->selectedItems().size() == 2);
        statusLabel->setText(QStringLiteral("Status: Comparison failed"));
        QMessageBox::warning(this, QStringLiteral("Could not compare cities"), message);
    });
    setWindowTitle("Weather App");
    resize(1100, 760);

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    auto* mainLayout = new QVBoxLayout(centralWidget);

    auto*headerLayout = new QHBoxLayout();

    QLabel* TitleLabel = new QLabel("Weather App", this);
    
    settingsButton = new QPushButton("Settings", this);
    auto* settingsMenu = new QMenu(settingsButton);
    auto* themeMenu = settingsMenu->addMenu("Theme");
    auto* themeGroup = new QActionGroup(themeMenu);
    themeGroup->setExclusive(true);
    const QStringList themeKeys{
        QStringLiteral("light"), QStringLiteral("dark"),
        QStringLiteral("pink"), QStringLiteral("blue"),
        QStringLiteral("darkPurple"), QStringLiteral("darkBlue")
    };
    const QStringList themeLabels{
        QStringLiteral("Light"), QStringLiteral("Dark"),
        QStringLiteral("Pink"), QStringLiteral("Blue"),
        QStringLiteral("Dark Purple"), QStringLiteral("Dark Blue")
    };
    QSettings settings("WeatherApp", "WeatherApp");
    const QString defaultCity = settings.value("defaultCity").toString().trimmed();
    QString selectedTheme = settings.value("theme", "light").toString();
    if (!themeKeys.contains(selectedTheme)) {
        selectedTheme = QStringLiteral("light");
    }

    auto* measurementMenu = settingsMenu->addMenu(QStringLiteral("Measurements"));
    auto* measurementGroup = new QActionGroup(measurementMenu);
    measurementGroup->setExclusive(true);
    QString selectedMeasurementSystem =
        settings.value("measurementSystem", "metric").toString();
    if (selectedMeasurementSystem != QStringLiteral("metric") &&
        selectedMeasurementSystem != QStringLiteral("imperial")) {
        selectedMeasurementSystem = QStringLiteral("metric");
    }
    useImperial = selectedMeasurementSystem == QStringLiteral("imperial");
    const QStringList measurementKeys{
        QStringLiteral("metric"), QStringLiteral("imperial")
    };
    const QStringList measurementLabels{
        QStringLiteral("Metric (mm, m/s)"), QStringLiteral("Imperial (in, mph)")
    };
    for (int i = 0; i < measurementKeys.size(); ++i) {
        QAction* action = measurementMenu->addAction(measurementLabels.at(i));
        action->setCheckable(true);
        action->setData(measurementKeys.at(i));
        action->setChecked(measurementKeys.at(i) == selectedMeasurementSystem);
        measurementGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, action]() {
            applyMeasurementSystem(action->data().toString() == QStringLiteral("imperial"));
        });
    }

    for (int i = 0; i < themeKeys.size(); ++i) {
        QAction* action = themeMenu->addAction(themeLabels.at(i));
        action->setCheckable(true);
        action->setData(themeKeys.at(i));
        action->setChecked(themeKeys.at(i) == selectedTheme);
        themeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, action]() {
            applyTheme(action->data().toString());
        });
    }

    auto* temperatureMenu = settingsMenu->addMenu("Temperature");
    auto* temperatureGroup = new QActionGroup(temperatureMenu);
    temperatureGroup->setExclusive(true);
    useFahrenheit = settings.value("temperatureUnit", "C").toString() == "F";
    QAction* celsiusAction = temperatureMenu->addAction("Celsius");
    QAction* fahrenheitAction = temperatureMenu->addAction("Fahrenheit");
    celsiusAction->setCheckable(true);
    fahrenheitAction->setCheckable(true);
    celsiusAction->setChecked(!useFahrenheit);
    fahrenheitAction->setChecked(useFahrenheit);
    temperatureGroup->addAction(celsiusAction);
    temperatureGroup->addAction(fahrenheitAction);
    settingsButton->setMenu(settingsMenu);

    connect(celsiusAction, &QAction::triggered, this, [this]() {
        applyTemperatureUnit(false);
    });
    connect(fahrenheitAction, &QAction::triggered, this, [this]() {
        applyTemperatureUnit(true);
    });
    settingsMenu->addSeparator();
    auto* defaultCityMenu = settingsMenu->addMenu(QStringLiteral("Default city"));
    QAction* setDefaultCityAction =
        defaultCityMenu->addAction(QStringLiteral("Set default city..."));
    QAction* clearDefaultCityAction =
        defaultCityMenu->addAction(QStringLiteral("Clear default city"));
    clearDefaultCityAction->setEnabled(!defaultCity.isEmpty());
    connect(setDefaultCityAction, &QAction::triggered, this,
            [this, clearDefaultCityAction]() {
        QSettings currentSettings("WeatherApp", "WeatherApp");
        bool accepted = false;
        const QString city = QInputDialog::getText(
            this, QStringLiteral("Set default city"),
            QStringLiteral("Enter the city to load when Weather App starts:"),
            QLineEdit::Normal, currentSettings.value("defaultCity").toString(), &accepted)
                                .trimmed();
        if (!accepted) {
            return;
        }
        if (city.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("Default city required"),
                                     QStringLiteral("Enter a city name or cancel."));
            return;
        }

        currentSettings.setValue("defaultCity", city);
        currentSettings.sync();
        if (currentSettings.status() != QSettings::NoError) {
            QMessageBox::warning(
                this, QStringLiteral("Could not save default city"),
                QStringLiteral("Could not write the default city to application settings."));
            return;
        }
        clearDefaultCityAction->setEnabled(true);
        cityInput->setText(city);
        onSearchClicked();
    });
    connect(clearDefaultCityAction, &QAction::triggered, this,
            [this, clearDefaultCityAction]() {
        QSettings currentSettings("WeatherApp", "WeatherApp");
        currentSettings.remove("defaultCity");
        currentSettings.sync();
        if (currentSettings.status() != QSettings::NoError) {
            QMessageBox::warning(
                this, QStringLiteral("Could not clear default city"),
                QStringLiteral("Could not update application settings."));
            return;
        }
        clearDefaultCityAction->setEnabled(false);
    });
    settingsMenu->addSeparator();
    QAction* configureApiKeyAction = settingsMenu->addAction("OpenWeather API key...");
    clearApiKeyAction = settingsMenu->addAction("Remove saved API key");
    clearApiKeyAction->setEnabled(weatherService->hasSavedApiKey());
    connect(configureApiKeyAction, &QAction::triggered,
            this, &MainWindow::onConfigureApiKey);
    connect(clearApiKeyAction, &QAction::triggered,
            this, &MainWindow::onClearApiKey);
    settingsMenu->addSeparator();
    QAction* configureTrafficKeyAction =
        settingsMenu->addAction(QStringLiteral("TomTom Traffic API key..."));
    QAction* clearTrafficKeyAction =
        settingsMenu->addAction(QStringLiteral("Remove saved TomTom key"));
    clearTrafficKeyAction->setEnabled(!tomTomApiKey.isEmpty());
    connect(configureTrafficKeyAction, &QAction::triggered, this, [this, clearTrafficKeyAction]() {
        bool accepted = false;
        const QString key = QInputDialog::getText(
            this, QStringLiteral("TomTom Traffic API key"),
            QStringLiteral("Enter your TomTom API key. It will be encrypted for this Windows account."),
            QLineEdit::Password, QString(), &accepted);
        if (!accepted) {
            return;
        }
        QString error;
        if (!ApiKeyStore::saveNamed(QStringLiteral("tomTomTrafficApiKey"), key, &error)) {
            QMessageBox::warning(this, QStringLiteral("Could not save TomTom key"), error);
            return;
        }
        tomTomApiKey = key.trimmed();
        clearTrafficKeyAction->setEnabled(true);
        roadConditionsButton->setEnabled(hasCurrentCoordinates);
    });
    connect(clearTrafficKeyAction, &QAction::triggered, this, [this, clearTrafficKeyAction]() {
        QString error;
        if (!ApiKeyStore::clearNamed(QStringLiteral("tomTomTrafficApiKey"), &error)) {
            QMessageBox::warning(this, QStringLiteral("Could not remove TomTom key"), error);
            return;
        }
        tomTomApiKey.clear();
        clearTrafficKeyAction->setEnabled(false);
        roadConditionsButton->setEnabled(hasCurrentCoordinates);
    });
    applyTheme(selectedTheme);

    headerLayout->addWidget(TitleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(settingsButton);

    mainLayout->addLayout(headerLayout);

    auto* contentLayout = new QHBoxLayout();
    mainLayout->addLayout(contentLayout, 1);

    auto* sidebar = new QWidget(centralWidget);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 8, 12, 0);

    auto* searchLabel = new QLabel("Search by city", sidebar);
    sidebarLayout->addWidget(searchLabel);

    cityInput = new QLineEdit(sidebar);
    cityInput->setPlaceholderText("Enter city name");
    sidebarLayout->addWidget(cityInput);

    searchButton = new QPushButton("Search", sidebar);
    sidebarLayout->addWidget(searchButton);

    compareButton = new QPushButton("Compare", sidebar);
    compareButton->setEnabled(false);
    compareButton->setToolTip(QStringLiteral(
        "Select exactly two saved cities (Ctrl-click to select multiple), then compare them."));
    sidebarLayout->addWidget(compareButton);

    QLabel* savedCitiesLabel = new QLabel("Saved Cities", sidebar);
    savedCities = new QListWidget(sidebar);
    savedCities->setSelectionMode(QAbstractItemView::ExtendedSelection);
    const QStringList storedCities = settings.value("savedCities").toStringList();
    const bool initializeSavedCities = !settings.contains("savedCities");
    if (!initializeSavedCities) {
        for (const QString& city : storedCities) {
            const QString trimmedCity = city.trimmed();
            if (!trimmedCity.isEmpty()) {
                savedCities->addItem(trimmedCity);
            }
        }
    } else {
        savedCities->addItem("New York");
        savedCities->addItem("London");
    }
    sidebarLayout->addWidget(savedCitiesLabel);
    sidebarLayout->addWidget(savedCities, 1);

    loadCityButton = new QPushButton("Load Selected", sidebar);
    loadCityButton->setEnabled(false);
    sidebarLayout->addWidget(loadCityButton);

    saveButton = new QPushButton("Save City", sidebar);
    sidebarLayout->addWidget(saveButton);
    removeCityButton = new QPushButton("Remove Selected", sidebar);
    removeCityButton->setEnabled(false);
    sidebarLayout->addWidget(removeCityButton);

    auto* weatherPanel = new QFrame(centralWidget);
    weatherPanel->setObjectName("weatherPanel");
    weatherPanel->setFrameShape(QFrame::StyledPanel);
    auto* weatherLayout = new QVBoxLayout(weatherPanel);
    weatherLayout->setContentsMargins(24, 18, 24, 18);

    cityLabel = new QLabel("City: ", weatherPanel);
    tempLabel = new QLabel("Temperature: ", weatherPanel);
    conditionLabel = new QLabel("Condition: ", weatherPanel);

    cityLabel->setAlignment(Qt::AlignCenter);
    tempLabel->setAlignment(Qt::AlignCenter);
    conditionLabel->setAlignment(Qt::AlignCenter);
    cityLabel->setStyleSheet("font-size: 20px; font-weight: bold;");
    tempLabel->setStyleSheet("font-size: 36px; font-weight: bold;");
    conditionLabel->setStyleSheet("font-size: 18px;");

    weatherLayout->addWidget(cityLabel);
    weatherLayout->addWidget(tempLabel);
    weatherLayout->addWidget(conditionLabel);

    auto* detailsLayout = new QHBoxLayout();
    humidityLabel = new QLabel("Humidity: ", weatherPanel);
    windLabel = new QLabel("Wind: ", weatherPanel);
    humidityLabel->setAlignment(Qt::AlignCenter);
    windLabel->setAlignment(Qt::AlignCenter);
    detailsLayout->addWidget(humidityLabel);
    detailsLayout->addWidget(windLabel);
    weatherLayout->addLayout(detailsLayout);

    statusLabel = new QLabel("Status: Ready", weatherPanel);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setWordWrap(true);
    weatherLayout->addWidget(statusLabel);

    alertsLabel = new QLabel(QStringLiteral("Weather alerts: Search a U.S. location"),
                             weatherPanel);
    alertsLabel->setAlignment(Qt::AlignCenter);
    alertsLabel->setWordWrap(true);
    viewAlertsButton = new QPushButton(QStringLiteral("View Alerts"), weatherPanel);
    viewAlertsButton->setEnabled(false);
    weatherLayout->addWidget(alertsLabel);
    weatherLayout->addWidget(viewAlertsButton);
    connect(viewAlertsButton, &QPushButton::clicked,
            this, &MainWindow::showWeatherAlerts);

    historicalWeatherButton = new QPushButton(QStringLiteral("Historical Weather"), weatherPanel);
    historicalWeatherButton->setEnabled(false);
    historicalWeatherButton->setToolTip(
        QStringLiteral("View daily weather history for this location."));
    weatherLayout->addWidget(historicalWeatherButton);
    connect(historicalWeatherButton, &QPushButton::clicked,
            this, &MainWindow::showHistoricalWeather);

    roadConditionsButton = new QPushButton(QStringLiteral("Road & Travel Conditions"), weatherPanel);
    roadConditionsButton->setEnabled(false);
    roadConditionsButton->setToolTip(QStringLiteral(
        "Show TomTom traffic flow and reported incidents; configure a TomTom Traffic API key in Settings."));
    weatherLayout->addWidget(roadConditionsButton);
    connect(roadConditionsButton, &QPushButton::clicked,
            this, &MainWindow::showRoadConditions);

    forecastButton = new QPushButton(QStringLiteral("7-Day Forecast"), weatherPanel);
    forecastButton->setEnabled(false);
    weatherLayout->addWidget(forecastButton);
    connect(forecastButton, &QPushButton::clicked,
            this, &MainWindow::showForecast);

    auto* mapHeader = new QHBoxLayout();
    auto* mapTitle = new QLabel("Weather Radar & Location", weatherPanel);
    mapTitle->setStyleSheet("font-size: 16px; font-weight: bold;");
    mapHeader->addWidget(mapTitle);
    mapHeader->addStretch();

    auto* radarToggle = new QCheckBox("Radar", weatherPanel);
    radarToggle->setChecked(true);
    mapHeader->addWidget(radarToggle);
    auto* zoomOutButton = new QPushButton(QStringLiteral("\u2212"), weatherPanel);
    auto* zoomInButton = new QPushButton("+", weatherPanel);
    auto* centerButton = new QPushButton("Center on location", weatherPanel);
    mapHeader->addWidget(zoomOutButton);
    mapHeader->addWidget(zoomInButton);
    mapHeader->addWidget(centerButton);
    weatherLayout->addLayout(mapHeader);

    mapWidget = new MapWidget(weatherPanel);
    weatherLayout->addWidget(mapWidget, 1);
    connect(radarToggle, &QCheckBox::toggled, mapWidget, &MapWidget::setRadarEnabled);
    connect(zoomOutButton, &QPushButton::clicked, mapWidget, &MapWidget::zoomOut);
    connect(zoomInButton, &QPushButton::clicked, mapWidget, &MapWidget::zoomIn);
    connect(centerButton, &QPushButton::clicked, mapWidget, &MapWidget::centerOnLocation);
    if (initializeSavedCities) {
        QString error;
        if (!persistSavedCities(&error)) {
            qWarning() << error;
            statusLabel->setText(QStringLiteral("Status: Could not initialize saved cities: %1")
                                     .arg(error));
        }
    }

    contentLayout->addWidget(sidebar, 1);
    contentLayout->addWidget(weatherPanel, 2);

    connect(searchButton, &QPushButton::clicked, this, &MainWindow::onSearchClicked);
    connect(compareButton, &QPushButton::clicked, this, &MainWindow::onCompareClicked);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::onSaveClicked);
    connect(loadCityButton, &QPushButton::clicked,
            this, &MainWindow::onLoadCityClicked);
    connect(removeCityButton, &QPushButton::clicked,
            this, &MainWindow::onRemoveCityClicked);
    connect(savedCities, &QListWidget::itemSelectionChanged, this, [this]() {
        const int selectedCount = savedCities->selectedItems().size();
        loadCityButton->setEnabled(selectedCount == 1);
        removeCityButton->setEnabled(selectedCount > 0);
        compareButton->setEnabled(selectedCount == 2 && !comparisonInProgress);
    });
    connect(savedCities, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) {
        savedCities->clearSelection();
        item->setSelected(true);
        savedCities->setCurrentItem(item);
        onLoadCityClicked();
    });
    if (!defaultCity.isEmpty()) {
        cityInput->setText(defaultCity);
        onSearchClicked();
    }
}

void MainWindow::applyTheme(const QString& theme)
{
    QSettings settings("WeatherApp", "WeatherApp");
    settings.setValue("theme", theme);

    if (theme == QStringLiteral("dark")) {
        qApp->setStyleSheet(
            "QMainWindow, QWidget { background-color: #20242b; color: #e8eaed; }"
            "QLineEdit, QListWidget, QMenu { background-color: #2b3038; color: #e8eaed; "
            "border: 1px solid #49515c; }"
            "QPushButton { background-color: #343b46; color: #f1f3f4; "
            "border: 1px solid #505966; border-radius: 4px; padding: 5px 10px; }"
            "QPushButton:hover { background-color: #414a57; }"
            "QListWidget::item:selected, QMenu::item:selected { background-color: #426b9b; }");
    } else if (theme == QStringLiteral("darkPurple")) {
        qApp->setStyleSheet(
            "QMainWindow, QWidget { background-color: #211a2b; color: #eee8f5; }"
            "QLineEdit, QListWidget, QMenu { background-color: #30263d; color: #eee8f5; "
            "border: 1px solid #57476b; }"
            "QPushButton { background-color: #3a2e4a; color: #f5efff; "
            "border: 1px solid #695582; border-radius: 4px; padding: 5px 10px; }"
            "QPushButton:hover { background-color: #4a3a5e; }"
            "QListWidget::item:selected, QMenu::item:selected { background-color: #684a85; }");
    } else if (theme == QStringLiteral("darkBlue")) {
        qApp->setStyleSheet(
            "QMainWindow, QWidget { background-color: #172330; color: #e6eef7; }"
            "QLineEdit, QListWidget, QMenu { background-color: #223244; color: #e6eef7; "
            "border: 1px solid #3b536d; }"
            "QPushButton { background-color: #293d52; color: #eff6ff; "
            "border: 1px solid #496681; border-radius: 4px; padding: 5px 10px; }"
            "QPushButton:hover { background-color: #354e68; }"
            "QListWidget::item:selected, QMenu::item:selected { background-color: #35648e; }");
    } else if (theme == QStringLiteral("pink")) {
        qApp->setStyleSheet(
            "QMainWindow, QWidget { background-color: #fff1f5; color: #54243a; }"
            "QLineEdit, QListWidget, QMenu { background-color: #fffafd; color: #54243a; "
            "border: 1px solid #e8b7ca; }"
            "QPushButton { background-color: #f8d7e3; color: #54243a; "
            "border: 1px solid #dfa1ba; border-radius: 4px; padding: 5px 10px; }"
            "QPushButton:hover { background-color: #f2c0d2; }"
            "QListWidget::item:selected, QMenu::item:selected { background-color: #e9a9c2; }");
    } else if (theme == QStringLiteral("blue")) {
        qApp->setStyleSheet(
            "QMainWindow, QWidget { background-color: #edf6ff; color: #183650; }"
            "QLineEdit, QListWidget, QMenu { background-color: #fbfdff; color: #183650; "
            "border: 1px solid #a9cbe8; }"
            "QPushButton { background-color: #d3e8fa; color: #183650; "
            "border: 1px solid #8bb9df; border-radius: 4px; padding: 5px 10px; }"
            "QPushButton:hover { background-color: #bbdcf5; }"
            "QListWidget::item:selected, QMenu::item:selected { background-color: #9dc9ed; }");
    } else {
        qApp->setStyleSheet(
            "QMainWindow, QWidget { background-color: #f5f7fa; color: #202124; }"
            "QLineEdit, QListWidget, QMenu { background-color: #ffffff; color: #202124; "
            "border: 1px solid #c8cdd4; }"
            "QPushButton { background-color: #e9edf2; color: #202124; "
            "border: 1px solid #c8cdd4; border-radius: 4px; padding: 5px 10px; }"
            "QPushButton:hover { background-color: #dce3eb; }"
            "QListWidget::item:selected, QMenu::item:selected { background-color: #cfe2fa; }");
    }
}

void MainWindow::applyTemperatureUnit(bool fahrenheit)
{
    useFahrenheit = fahrenheit;
    QSettings settings("WeatherApp", "WeatherApp");
    settings.setValue("temperatureUnit", useFahrenheit ? "F" : "C");
    updateTemperatureDisplay();
}

void MainWindow::applyMeasurementSystem(bool imperial)
{
    useImperial = imperial;
    QSettings settings("WeatherApp", "WeatherApp");
    settings.setValue("measurementSystem",
                      useImperial ? QStringLiteral("imperial") : QStringLiteral("metric"));
    if (hasWeatherData) {
        const double windSpeed = useImperial
            ? currentWindSpeedMetersPerSecond * 2.2369362920544
            : currentWindSpeedMetersPerSecond;
        windLabel->setText(QStringLiteral("Wind: %1 %2")
                               .arg(windSpeed, 0, 'f', 1)
                               .arg(useImperial ? QStringLiteral("mph")
                                                : QStringLiteral("m/s")));
    }
}

void MainWindow::updateTemperatureDisplay()
{
    if (!hasWeatherData) {
        return;
    }

    const double displayTemperature = useFahrenheit
        ? (currentTemperatureCelsius * 9.0 / 5.0) + 32.0
        : currentTemperatureCelsius;
    tempLabel->setText(QStringLiteral("Temperature: %1 %2%3")
                           .arg(displayTemperature, 0, 'f', 1)
                           .arg(QChar(0x00B0))
                           .arg(useFahrenheit ? QStringLiteral("F") : QStringLiteral("C")));
}

void MainWindow::onSearchClicked()
{
    const QString cityName = cityInput->text().trimmed();
    if (cityName.isEmpty()) {
        statusLabel->setText("Status: Enter a city name");
        return;
    }

    statusLabel->setText("Status: Loading weather...");
    alertService->cancel();
    historicalWeatherService->cancel();
    roadConditionsService->cancel();
    forecastService->cancel();
    hasCurrentCoordinates = false;
    historicalWeatherButton->setEnabled(false);
    roadConditionsButton->setEnabled(false);
    forecastButton->setEnabled(false);
    activeAlerts.clear();
    alertsLabel->setText(QStringLiteral("Weather alerts: Waiting for updated location..."));
    viewAlertsButton->setText(QStringLiteral("View Alerts"));
    viewAlertsButton->setEnabled(false);
    cityLabel->setText("City: ");
    tempLabel->setText("Temperature: ");
    conditionLabel->setText("Condition: ");
    humidityLabel->setText("Humidity: ");
    windLabel->setText("Wind: ");
    hasWeatherData = false;
    weatherService->fetchWeather(cityName);
}

void MainWindow::displayWeather(const WeatherData& data)
{
    currentTemperatureCelsius = data.temperature;
    currentWindSpeedMetersPerSecond = data.windSpeed;
    hasWeatherData = true;
    const QString location = data.country.isEmpty()
        ? data.city
        : QStringLiteral("%1, %2").arg(data.city, data.country);
    currentWeatherLocation = location;
    cityLabel->setText("City: " + location);
    if (data.hasCoordinates) {
        currentLatitude = data.latitude;
        currentLongitude = data.longitude;
        hasCurrentCoordinates = true;
        historicalWeatherButton->setEnabled(true);
        roadConditionsButton->setEnabled(true);
        forecastButton->setEnabled(true);
        mapWidget->setLocation(data.latitude, data.longitude, location);
        alertLocation = location;
        activeAlerts.clear();
        alertsLabel->setText(QStringLiteral("Weather alerts: Checking for %1...")
                                 .arg(location));
        viewAlertsButton->setText(QStringLiteral("View Alerts"));
        viewAlertsButton->setEnabled(false);
        alertService->fetchAlerts(data.latitude, data.longitude);
    } else {
        hasCurrentCoordinates = false;
        historicalWeatherButton->setEnabled(false);
        roadConditionsButton->setEnabled(false);
        forecastButton->setEnabled(false);
        alertService->cancel();
        mapWidget->clearLocation();
        activeAlerts.clear();
        alertsLabel->setText(QStringLiteral("Weather alerts: Location coordinates unavailable"));
        viewAlertsButton->setText(QStringLiteral("View Alerts"));
        viewAlertsButton->setEnabled(false);
        statusLabel->setText("Status: Weather loaded; location coordinates unavailable");
    }
    updateTemperatureDisplay();
    conditionLabel->setText("Condition: " + data.description);
    humidityLabel->setText("Humidity: " + QString::number(data.humidity) + " %");
    const double displayWindSpeed = useImperial
        ? data.windSpeed * 2.2369362920544
        : data.windSpeed;
    windLabel->setText(QStringLiteral("Wind: %1 %2")
                           .arg(displayWindSpeed, 0, 'f', 1)
                           .arg(useImperial ? QStringLiteral("mph")
                                            : QStringLiteral("m/s")));
    if (data.hasCoordinates) {
        statusLabel->setText(data.isCached
                                 ? "Status: Weather loaded from cache"
                                 : "Status: Weather loaded");
    }
}

void MainWindow::showHistoricalWeather()
{
    if (!hasCurrentCoordinates) {
        return;
    }

    const QDate earliestDate(1940, 1, 1);
    const QDate latestAvailableDate = QDate::currentDate().addDays(-5);
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Historical Weather - %1").arg(currentWeatherLocation));
    dialog.resize(780, 520);
    auto* layout = new QVBoxLayout(&dialog);
    auto* dateLayout = new QHBoxLayout();
    auto* startDate = new QDateEdit(&dialog);
    auto* endDate = new QDateEdit(&dialog);
    startDate->setCalendarPopup(true);
    endDate->setCalendarPopup(true);
    startDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    endDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    startDate->setDateRange(earliestDate, latestAvailableDate);
    endDate->setDateRange(earliestDate, latestAvailableDate);
    const QDate defaultEndDate = latestAvailableDate;
    startDate->setDate(defaultEndDate.addDays(-6) < earliestDate
                           ? earliestDate
                           : defaultEndDate.addDays(-6));
    endDate->setDate(defaultEndDate);
    dateLayout->addWidget(new QLabel(QStringLiteral("From:"), &dialog));
    dateLayout->addWidget(startDate);
    dateLayout->addWidget(new QLabel(QStringLiteral("To:"), &dialog));
    dateLayout->addWidget(endDate);
    auto* loadButton = new QPushButton(QStringLiteral("Load History"), &dialog);
    dateLayout->addWidget(loadButton);
    layout->addLayout(dateLayout);

    auto* status = new QLabel(
        QStringLiteral("Daily reanalysis data; recent days may not be available yet."),
        &dialog);
    status->setWordWrap(true);
    layout->addWidget(status);
    auto* table = new QTableWidget(0, 5, &dialog);
    table->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("Date"), QStringLiteral("High"), QStringLiteral("Low"),
        QStringLiteral("Precipitation"), QStringLiteral("Max Wind")
    });
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(table, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(&dialog, &QDialog::finished, historicalWeatherService,
            &HistoricalWeatherService::cancel);
    connect(historicalWeatherService, &HistoricalWeatherService::historicalWeatherFailed,
            &dialog, [status, loadButton](const QString& message) {
        status->setText(message);
        loadButton->setEnabled(true);
    });
    connect(historicalWeatherService, &HistoricalWeatherService::historicalWeatherReady,
            &dialog, [this, status, loadButton, table](const QVector<HistoricalWeatherDay>& days) {
        table->setRowCount(days.size());
        for (int row = 0; row < days.size(); ++row) {
            const HistoricalWeatherDay& day = days.at(row);
            const double high = useFahrenheit
                ? (day.maximumTemperature * 9.0 / 5.0) + 32.0
                : day.maximumTemperature;
            const double low = useFahrenheit
                ? (day.minimumTemperature * 9.0 / 5.0) + 32.0
                : day.minimumTemperature;
            const QString unit = useFahrenheit ? QStringLiteral("°F") : QStringLiteral("°C");
            const double precipitation = useImperial
                ? day.precipitationMm / 25.4
                : day.precipitationMm;
            const double windSpeed = useImperial
                ? day.maximumWindSpeed * 2.2369362920544
                : day.maximumWindSpeed;
            const QStringList values{
                day.date.toString(Qt::ISODate),
                QStringLiteral("%1 %2").arg(high, 0, 'f', 1).arg(unit),
                QStringLiteral("%1 %2").arg(low, 0, 'f', 1).arg(unit),
                QStringLiteral("%1 %2").arg(precipitation, 0, 'f', 2)
                    .arg(useImperial ? QStringLiteral("in") : QStringLiteral("mm")),
                QStringLiteral("%1 %2").arg(windSpeed, 0, 'f', 1)
                    .arg(useImperial ? QStringLiteral("mph") : QStringLiteral("m/s"))
            };
            for (int column = 0; column < values.size(); ++column) {
                table->setItem(row, column, new QTableWidgetItem(values.at(column)));
            }
        }
        status->setText(QStringLiteral(
            "Loaded %1 days. Historical values are reanalysis estimates, not station readings.")
                            .arg(days.size()));
        loadButton->setEnabled(true);
    });
    connect(loadButton, &QPushButton::clicked, &dialog, [this, startDate, endDate,
                                                          loadButton, status, table]() {
        loadButton->setEnabled(false);
        status->setText(QStringLiteral("Loading historical weather..."));
        table->setRowCount(0);
        historicalWeatherService->fetchHistoricalWeather(
            currentLatitude, currentLongitude, startDate->date(), endDate->date());
    });
    dialog.exec();
}

void MainWindow::showForecast()
{
    if (!hasCurrentCoordinates) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("7-Day Forecast - %1").arg(currentWeatherLocation));
    dialog.resize(780, 420);
    auto* layout = new QVBoxLayout(&dialog);
    auto* status = new QLabel(QStringLiteral("Loading the 7-day forecast..."), &dialog);
    status->setWordWrap(true);
    layout->addWidget(status);
    auto* table = new QTableWidget(0, 6, &dialog);
    table->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("Date"), QStringLiteral("Conditions"), QStringLiteral("High"),
        QStringLiteral("Low"), QStringLiteral("Precipitation"), QStringLiteral("Max Wind")
    });
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(table, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(&dialog, &QDialog::finished, forecastService, &ForecastService::cancel);
    connect(forecastService, &ForecastService::forecastFailed, &dialog,
            [status](const QString& message) {
        status->setText(message);
    });
    connect(forecastService, &ForecastService::forecastReady, &dialog,
            [this, status, table](const QVector<WeatherData>& forecast) {
        table->setRowCount(forecast.size());
        for (int row = 0; row < forecast.size(); ++row) {
            const WeatherData& day = forecast.at(row);
            const double high = useFahrenheit
                ? (day.maximumTemperature * 9.0 / 5.0) + 32.0
                : day.maximumTemperature;
            const double low = useFahrenheit
                ? (day.minimumTemperature * 9.0 / 5.0) + 32.0
                : day.minimumTemperature;
            const double wind = useImperial
                ? day.windSpeed * 2.2369362920544
                : day.windSpeed;
            const QString temperatureUnit = useFahrenheit ? QStringLiteral("°F")
                                                          : QStringLiteral("°C");
            const QString windUnit = useImperial ? QStringLiteral("mph")
                                                 : QStringLiteral("m/s");
            const QString precipitation = day.precipitationProbability >= 0
                ? QStringLiteral("%1%").arg(day.precipitationProbability)
                : QStringLiteral("—");
            const QStringList values{
                day.date.toString(QStringLiteral("ddd, MMM d")),
                day.description,
                QStringLiteral("%1 %2").arg(high, 0, 'f', 1).arg(temperatureUnit),
                QStringLiteral("%1 %2").arg(low, 0, 'f', 1).arg(temperatureUnit),
                precipitation,
                QStringLiteral("%1 %2").arg(wind, 0, 'f', 1).arg(windUnit)
            };
            for (int column = 0; column < values.size(); ++column) {
                table->setItem(row, column, new QTableWidgetItem(values.at(column)));
            }
        }
        status->setText(QStringLiteral("Forecast loaded for the next seven days."));
    });
    forecastService->fetchForecast(currentWeatherLocation, currentLatitude, currentLongitude);
    dialog.exec();
}

void MainWindow::showRoadConditions()
{
    if (!hasCurrentCoordinates) {
        return;
    }

    QString apiKey = qgetenv("TOMTOM_API_KEY").trimmed();
    if (apiKey.isEmpty()) {
        apiKey = tomTomApiKey;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Road & Travel Conditions - %1")
                              .arg(currentWeatherLocation));
    dialog.resize(640, 480);
    auto* layout = new QVBoxLayout(&dialog);
    auto* status = new QLabel(QStringLiteral("Loading live traffic and nearby incidents..."),
                              &dialog);
    status->setWordWrap(true);
    layout->addWidget(status);
    auto* details = new QTextBrowser(&dialog);
    details->setOpenExternalLinks(false);
    layout->addWidget(details, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(&dialog, &QDialog::finished, roadConditionsService,
            &RoadConditionsService::cancel);
    connect(roadConditionsService, &RoadConditionsService::roadConditionsFailed,
            &dialog, [status, details](const QString& message) {
        status->setText(message);
        details->clear();
    });
    connect(roadConditionsService, &RoadConditionsService::roadConditionsReady,
            &dialog, [this, status, details](const RoadConditions& conditions) {
        const double currentSpeed = useImperial
            ? conditions.currentSpeedKph * 0.62137119223733
            : conditions.currentSpeedKph;
        const double freeFlowSpeed = useImperial
            ? conditions.freeFlowSpeedKph * 0.62137119223733
            : conditions.freeFlowSpeedKph;
        const QString speedUnit = useImperial ? QStringLiteral("mph")
                                              : QStringLiteral("km/h");
        QStringList lines;
        if (conditions.hasTrafficFlow) {
            status->setText(QStringLiteral("%1: %2 / %3 %4; %5 incident(s) nearby")
                                .arg(conditions.roadClosed
                                         ? QStringLiteral("Road closure reported")
                                         : QStringLiteral("Traffic near location"),
                                     QString::number(currentSpeed, 'f', 1),
                                     QString::number(freeFlowSpeed, 'f', 1),
                                     speedUnit)
                                .arg(conditions.incidents.size()));
            lines.append(QStringLiteral("Current speed: %1 %2")
                             .arg(currentSpeed, 0, 'f', 1).arg(speedUnit));
            lines.append(QStringLiteral("Free-flow speed: %1 %2")
                             .arg(freeFlowSpeed, 0, 'f', 1).arg(speedUnit));
        } else {
            status->setText(QStringLiteral("Traffic flow unavailable; %1 incident(s) nearby")
                                .arg(conditions.incidents.size()));
            lines.append(conditions.trafficFlowError);
        }
        lines.append(QString());
        lines.append(QStringLiteral("Nearby incidents (TomTom):"));
        if (conditions.incidents.isEmpty()) {
            lines.append(QStringLiteral("No incidents reported in the search area."));
        }
        for (const RoadIncident& incident : conditions.incidents) {
            QString title = incident.description;
            if (!incident.road.isEmpty()) {
                title += QStringLiteral(" — %1").arg(incident.road);
            }
            if (incident.delaySeconds > 0) {
                title += QStringLiteral(" (about %1 min delay)")
                             .arg(qMax(1, qRound(incident.delaySeconds / 60.0)));
            }
            lines.append(QStringLiteral("- %1").arg(title));
            if (!incident.from.isEmpty() || !incident.to.isEmpty()) {
                lines.append(QStringLiteral("  %1%2%3")
                                 .arg(incident.from,
                                      incident.from.isEmpty() || incident.to.isEmpty()
                                          ? QString()
                                          : QStringLiteral(" to "),
                                      incident.to));
            }
        }
        if (!conditions.incidentError.isEmpty()) {
            lines.append(QString());
            lines.append(conditions.incidentError);
        }
        details->setPlainText(lines.join(QLatin1Char('\n')));
    });
    roadConditionsService->fetchConditions(currentLatitude, currentLongitude, apiKey);
    dialog.exec();
}

void MainWindow::showWeatherAlerts()
{
    if (activeAlerts.isEmpty()) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Weather Alerts - %1").arg(alertLocation));
    dialog.resize(780, 480);
    auto* layout = new QVBoxLayout(&dialog);
    auto* contentLayout = new QHBoxLayout();
    auto* alertList = new QListWidget(&dialog);
    auto* details = new QTextBrowser(&dialog);
    details->setOpenExternalLinks(false);
    contentLayout->addWidget(alertList, 1);
    contentLayout->addWidget(details, 2);
    layout->addLayout(contentLayout, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    for (int i = 0; i < activeAlerts.size(); ++i) {
        const WeatherAlert& alert = activeAlerts.at(i);
        const QString title = alert.headline.isEmpty() ? alert.event : alert.headline;
        alertList->addItem(title);
    }
    connect(alertList, &QListWidget::currentRowChanged, &dialog, [this, details](int row) {
        if (row < 0 || row >= activeAlerts.size()) {
            details->clear();
            return;
        }

        const WeatherAlert& alert = activeAlerts.at(row);
        QStringList lines;
        lines.append(alert.event.isEmpty() ? alert.headline : alert.event);
        if (!alert.severity.isEmpty()) {
            lines.append(QStringLiteral("Severity: %1").arg(alert.severity));
        }
        if (!alert.urgency.isEmpty()) {
            lines.append(QStringLiteral("Urgency: %1").arg(alert.urgency));
        }
        if (!alert.certainty.isEmpty()) {
            lines.append(QStringLiteral("Certainty: %1").arg(alert.certainty));
        }
        if (!alert.areaDescription.isEmpty()) {
            lines.append(QStringLiteral("Affected areas: %1").arg(alert.areaDescription));
        }
        if (alert.effective.isValid()) {
            lines.append(QStringLiteral("Effective: %1").arg(
                alert.effective.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm t"))));
        }
        if (alert.expires.isValid()) {
            lines.append(QStringLiteral("Expires: %1").arg(
                alert.expires.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm t"))));
        }
        lines.append(QString());
        lines.append(alert.description);
        if (!alert.instruction.isEmpty()) {
            lines.append(QString());
            lines.append(QStringLiteral("Instructions:"));
            lines.append(alert.instruction);
        }
        details->setPlainText(lines.join(QLatin1Char('\n')));
    });
    alertList->setCurrentRow(0);
    dialog.exec();
}

void MainWindow::onCompareClicked()
{
    const QList<QListWidgetItem*> selected = savedCities->selectedItems();
    if (selected.size() != 2) {
        statusLabel->setText("Status: Select exactly two saved cities to compare");
        return;
    }

    comparisonCities = QStringList{selected.at(0)->text(), selected.at(1)->text()};
    if (comparisonCities.at(0).compare(comparisonCities.at(1), Qt::CaseInsensitive) == 0) {
        statusLabel->setText("Status: Select two different cities to compare");
        return;
    }

    comparisonResults.clear();
    comparisonInProgress = true;
    compareButton->setEnabled(false);
    statusLabel->setText(QStringLiteral("Status: Loading %1...")
                             .arg(comparisonCities.first()));
    comparisonWeatherService->fetchWeather(comparisonCities.first());
}

void MainWindow::showWeatherComparison()
{
    if (comparisonResults.size() != 2) {
        return;
    }

    const WeatherData& first = comparisonResults.at(0);
    const WeatherData& second = comparisonResults.at(1);
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle("Weather Comparison");
    dialog->setMinimumWidth(560);
    auto* layout = new QVBoxLayout(dialog);
    auto* table = new QTableWidget(5, 3, dialog);
    table->setHorizontalHeaderLabels(
        QStringList{QStringLiteral("Weather"), first.city, second.city});
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    const auto temperatureText = [this](double celsius) {
        const double temperature = useFahrenheit ? (celsius * 9.0 / 5.0) + 32.0 : celsius;
        return QStringLiteral("%1 %2%3")
            .arg(temperature, 0, 'f', 1)
            .arg(QChar(0x00B0))
            .arg(useFahrenheit ? QStringLiteral("F") : QStringLiteral("C"));
    };
    const QStringList labels{
        QStringLiteral("Temperature"),
        QStringLiteral("Conditions"),
        QStringLiteral("Humidity"),
        QStringLiteral("Wind"),
        QStringLiteral("Pressure")
    };
    const QStringList firstValues{
        temperatureText(first.temperature),
        first.description,
        QStringLiteral("%1%").arg(first.humidity, 0, 'f', 0),
        QStringLiteral("%1 %2").arg(
            useImperial ? first.windSpeed * 2.2369362920544 : first.windSpeed, 0, 'f', 1)
            .arg(useImperial ? QStringLiteral("mph") : QStringLiteral("m/s")),
        QStringLiteral("%1 hPa").arg(first.pressure)
    };
    const QStringList secondValues{
        temperatureText(second.temperature),
        second.description,
        QStringLiteral("%1%").arg(second.humidity, 0, 'f', 0),
        QStringLiteral("%1 %2").arg(
            useImperial ? second.windSpeed * 2.2369362920544 : second.windSpeed, 0, 'f', 1)
            .arg(useImperial ? QStringLiteral("mph") : QStringLiteral("m/s")),
        QStringLiteral("%1 hPa").arg(second.pressure)
    };
    for (int row = 0; row < labels.size(); ++row) {
        table->setItem(row, 0, new QTableWidgetItem(labels.at(row)));
        table->setItem(row, 1, new QTableWidgetItem(firstValues.at(row)));
        table->setItem(row, 2, new QTableWidgetItem(secondValues.at(row)));
    }
    layout->addWidget(table);
    if (first.isCached || second.isCached) {
        auto* note = new QLabel(
            "At least one result is cached; its live refresh was unavailable.", dialog);
        note->setWordWrap(true);
        layout->addWidget(note);
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::clicked, dialog, &QDialog::accept);
    layout->addWidget(buttons);
    dialog->exec();
}

void MainWindow::onSaveClicked()
{
    const QString cityName = cityInput->text().trimmed();
    if (cityName.isEmpty()) {
        statusLabel->setText("Status: Enter a city name to save");
        return;
    }

    for (int i = 0; i < savedCities->count(); ++i) {
        if (savedCities->item(i)->text().compare(cityName, Qt::CaseInsensitive) == 0) {
            statusLabel->setText("Status: City is already saved");
            return;
        }
    }

    savedCities->addItem(cityName);
    QString error;
    if (!persistSavedCities(&error)) {
        delete savedCities->takeItem(savedCities->count() - 1);
        statusLabel->setText(QStringLiteral("Status: Could not save city: %1").arg(error));
        return;
    }
    statusLabel->setText("Status: City saved");
}

void MainWindow::onLoadCityClicked()
{
    const QList<QListWidgetItem*> selected = savedCities->selectedItems();
    if (selected.size() != 1) {
        statusLabel->setText("Status: Select one saved city to load");
        return;
    }

    cityInput->setText(selected.first()->text());
    onSearchClicked();
}

void MainWindow::onRemoveCityClicked()
{
    QList<QListWidgetItem*> selected = savedCities->selectedItems();
    if (selected.isEmpty()) {
        statusLabel->setText("Status: Select saved cities to remove");
        return;
    }

    QVector<QPair<int, QString>> removedCities;
    removedCities.reserve(selected.size());
    for (QListWidgetItem* item : selected) {
        removedCities.append(qMakePair(savedCities->row(item), item->text()));
    }
    std::sort(removedCities.begin(), removedCities.end(),
              [](const QPair<int, QString>& left, const QPair<int, QString>& right) {
                  return left.first > right.first;
              });
    for (const auto& removed : removedCities) {
        delete savedCities->takeItem(removed.first);
    }

    QString error;
    if (!persistSavedCities(&error)) {
        for (auto it = removedCities.crbegin(); it != removedCities.crend(); ++it) {
            savedCities->insertItem(it->first, it->second);
        }
        statusLabel->setText(QStringLiteral("Status: Could not remove cities: %1").arg(error));
        return;
    }
    statusLabel->setText(QStringLiteral("Status: Removed %1 saved city/cities")
                             .arg(removedCities.size()));
}

bool MainWindow::persistSavedCities(QString* errorMessage) const
{
    QStringList cities;
    for (int i = 0; i < savedCities->count(); ++i) {
        cities.append(savedCities->item(i)->text());
    }

    QSettings settings("WeatherApp", "WeatherApp");
    settings.setValue("savedCities", cities);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not write saved cities to application settings.");
        }
        return false;
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

void MainWindow::onConfigureApiKey()
{
    bool accepted = false;
    const QString apiKey = QInputDialog::getText(
        this,
        QStringLiteral("OpenWeather API key"),
        QStringLiteral("Enter your API key. It will be encrypted for this Windows account."),
        QLineEdit::Password,
        QString(),
        &accepted);
    if (!accepted) {
        return;
    }

    QString error;
    if (!weatherService->setApiKey(apiKey, &error) ||
        !comparisonWeatherService->setApiKey(apiKey, &error)) {
        QMessageBox::warning(this, QStringLiteral("Could not save API key"), error);
        return;
    }
    clearApiKeyAction->setEnabled(true);

    QMessageBox::information(
        this,
        QStringLiteral("API key saved"),
        qEnvironmentVariableIsSet("WEATHER_API_KEY")
            ? QStringLiteral("The key was saved securely. WEATHER_API_KEY is set and will take precedence.")
            : QStringLiteral("The key was encrypted and saved for this Windows account."));
}

void MainWindow::onClearApiKey()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        QStringLiteral("Remove saved API key"),
        QStringLiteral("Remove the API key saved for this Windows account?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    QString error;
    if (!weatherService->clearSavedApiKey(&error) ||
        !comparisonWeatherService->clearSavedApiKey(&error)) {
        QMessageBox::warning(this, QStringLiteral("Could not remove API key"), error);
        return;
    }
    clearApiKeyAction->setEnabled(false);

    QMessageBox::information(
        this,
        QStringLiteral("API key removed"),
        qEnvironmentVariableIsSet("WEATHER_API_KEY")
            ? QStringLiteral("The saved key was removed. WEATHER_API_KEY is still set and will be used.")
            : QStringLiteral("The saved key was removed."));
}
    