#ifndef MAP_WIDGET_H
#define MAP_WIDGET_H

#include <QHash>
#include <QImage>
#include <QNetworkAccessManager>
#include <QPoint>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QWidget>

class QNetworkDiskCache;
class QNetworkReply;
class QMouseEvent;
class QPaintEvent;
class QTimer;
class QWheelEvent;

class MapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapWidget(QWidget* parent = nullptr);

    void setLocation(double latitude, double longitude, const QString& label);
    void clearLocation();
    void setRadarEnabled(bool enabled);
    bool radarEnabled() const;
    void zoomIn();
    void zoomOut();
    void centerOnLocation();

signals:
    void mapStatusChanged(const QString& message);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void fetchRadarMetadata();
    void requestTile(const QString& url, const QString& key);
    QPointF project(double latitude, double longitude) const;
    static double boundedLatitude(double latitude);

    QNetworkAccessManager networkManager;
    QNetworkReply* radarMetadataReply = nullptr;
    QNetworkDiskCache* diskCache = nullptr;
    QTimer* radarRefreshTimer = nullptr;
    QHash<QString, QImage> tiles;
    QSet<QString> pendingTiles;
    QPointF centerWorld;
    QPoint lastMousePosition;
    int zoom = 5;
    double latitude = 39.5;
    double longitude = -98.35;
    bool hasLocation = false;
    bool radarLayerEnabled = true;
    bool dragging = false;
    QString locationLabel;
    QString radarHost;
    QString radarPath;
    QString radarStatus = QStringLiteral("Loading radar frames...");
};

#endif
