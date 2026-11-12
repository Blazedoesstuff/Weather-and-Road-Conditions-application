cmake_minimum_required(VERSION 3.16)
project(WeatherApp LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Network Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network Widgets)

add_executable(WeatherApp
    include/ApiKeyStore.h
    include/AlertData.h
    include/AlertService.h
    include/ForecastService.h
    include/HistoricalWeatherService.h
    include/MapWidget.h
    include/MainWindow.h
    include/RoadConditionsService.h
    include/ShelterService.h
    include/ShelterData.h
    include/weatherservice.h
    src/CacheService.cpp
    src/ApiKeyStore.cpp
    src/AlertService.cpp
    src/ForecastService.cpp
    src/HistoricalWeatherService.cpp
    src/LocationService.cpp
    src/Main.cpp
    src/MainWindow.cpp
    src/MapWidget.cpp
    src/RoadConditionsService.cpp
    src/ShelterService.cpp
    src/WeatherService.cpp
)

target_include_directories(WeatherApp PRIVATE include)
target_link_libraries(WeatherApp PRIVATE Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Widgets)
if(WIN32)
    target_link_libraries(WeatherApp PRIVATE Crypt32)
endif()

if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

    add_executable(CacheServiceTests
        include/CacheService.h
        src/CacheService.cpp
        tests/CacheServiceTests.cpp
    )
    target_include_directories(CacheServiceTests PRIVATE include)
    target_link_libraries(CacheServiceTests PRIVATE Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME CacheServiceTests COMMAND CacheServiceTests)

    add_executable(AlertServiceTests
        include/AlertData.h
        include/AlertService.h
        src/AlertService.cpp
        tests/AlertServiceTests.cpp
    )
    target_include_directories(AlertServiceTests PRIVATE include)
    target_link_libraries(AlertServiceTests PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Network
    )
    add_test(NAME AlertServiceTests COMMAND AlertServiceTests)

    add_executable(HistoricalWeatherServiceTests
        include/HistoricalWeatherService.h
        src/HistoricalWeatherService.cpp
        tests/HistoricalWeatherServiceTests.cpp
    )
    target_include_directories(HistoricalWeatherServiceTests PRIVATE include)
    target_link_libraries(HistoricalWeatherServiceTests PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Network
    )
    add_test(NAME HistoricalWeatherServiceTests COMMAND HistoricalWeatherServiceTests)

    add_executable(RoadConditionsServiceTests
        include/RoadConditionsService.h
        src/RoadConditionsService.cpp
        tests/RoadConditionsServiceTests.cpp
    )
    target_include_directories(RoadConditionsServiceTests PRIVATE include)
    target_link_libraries(RoadConditionsServiceTests PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Network
    )
    add_test(NAME RoadConditionsServiceTests COMMAND RoadConditionsServiceTests)

    get_target_property(QT_CORE_DLL Qt${QT_VERSION_MAJOR}::Core IMPORTED_LOCATION_DEBUG)
    if(NOT QT_CORE_DLL)
        get_target_property(QT_CORE_DLL Qt${QT_VERSION_MAJOR}::Core IMPORTED_LOCATION_RELEASE)
    endif()
    if(QT_CORE_DLL)
        get_filename_component(QT_BIN_DIR "${QT_CORE_DLL}" DIRECTORY)
        set_tests_properties(CacheServiceTests PROPERTIES
            ENVIRONMENT "PATH=${QT_BIN_DIR}"
        )
        set_tests_properties(AlertServiceTests PROPERTIES
            ENVIRONMENT "PATH=${QT_BIN_DIR}"
        )
        set_tests_properties(HistoricalWeatherServiceTests PROPERTIES
            ENVIRONMENT "PATH=${QT_BIN_DIR}"
        )
        set_tests_properties(RoadConditionsServiceTests PROPERTIES
            ENVIRONMENT "PATH=${QT_BIN_DIR}"
        )
    endif()

    if(WIN32)
        add_executable(ApiKeyStoreTests
            include/ApiKeyStore.h
            src/ApiKeyStore.cpp
            tests/ApiKeyStoreTests.cpp
        )
        target_include_directories(ApiKeyStoreTests PRIVATE include)
        target_link_libraries(ApiKeyStoreTests PRIVATE Qt${QT_VERSION_MAJOR}::Test Crypt32)
        add_test(NAME ApiKeyStoreTests COMMAND ApiKeyStoreTests)
        if(QT_CORE_DLL)
            set_tests_properties(ApiKeyStoreTests PROPERTIES
                ENVIRONMENT "PATH=${QT_BIN_DIR}"
            )
        endif()
    endif()
endif()
