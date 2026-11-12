#include "MapWidget.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>

namespace {

constexpr int tileSize = 256;
constexpr int minimumZoom = 3;
constexpr int maximumZoom = 7;
constexpr double pi = 3.14159265358979323846;

}

MapWidget::MapWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    diskCache = new QNetworkDiskCache(&networkManager);
    const QString cacheDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/map-tiles");
    diskCache->setCacheDirectory(cacheDirectory);
    diskCache->setMaximumCacheSize(256 * 1024 * 1024);
    networkManager.setCache(diskCache);

    centerWorld = project(latitude, longitude);
    fetchRadarMetadata();
    radarRefreshTimer = new QTimer(this);
    radarRefreshTimer->setInterval(10 * 60 * 1000);
    connect(radarRefreshTimer, &QTimer::timeout, this, &MapWidget::fetchRadarMetadata);
    radarRefreshTimer->start();
}

void MapWidget::setLocation(double newLatitude, double newLongitude, const QString& label)
{
    if (!qIsFinite(newLatitude) || !qIsFinite(newLongitude) ||
        newLatitude < -90.0 || newLatitude > 90.0 ||
        newLongitude < -180.0 || newLongitude > 180.0) {
        clearLocation();
        return;
    }

    latitude = newLatitude;
    longitude = newLongitude;
    locationLabel = label;
    hasLocation = true;
    zoom = maximumZoom;
    centerWorld = project(latitude, longitude);
    update();
}

void MapWidget::clearLocation()
{
    hasLocation = false;
    locationLabel.clear();
    latitude = 39.5;
    longitude = -98.35;
    zoom = 5;
    centerWorld = project(latitude, longitude);
    update();
}

void MapWidget::setRadarEnabled(bool enabled)
{
    radarLayerEnabled = enabled;
    update();
}

bool MapWidget::radarEnabled() const
{
    return radarLayerEnabled;
}

void MapWidget::zoomIn()
{
    if (zoom < maximumZoom) {
        ++zoom;
        centerWorld *= 2.0;
        update();
    }
}

void MapWidget::zoomOut()
{
    if (zoom > minimumZoom) {
        --zoom;
        centerWorld /= 2.0;
        update();
    }
}

void MapWidget::centerOnLocation()
{
    centerWorld = project(latitude, longitude);
    update();
}

QPointF MapWidget::project(double latitude, double longitude) const
{
    const double worldSize = tileSize * qPow(2.0, zoom);
    const double radians = qDegreesToRadians(boundedLatitude(latitude));
    const double x = (longitude + 180.0) / 360.0 * worldSize;
    const double y = (1.0 - std::asinh(std::tan(radians)) / pi) / 2.0 * worldSize;
    return QPointF(x, y);
}

double MapWidget::boundedLatitude(double latitude)
{
    return qBound(-85.05112878, latitude, 85.05112878);
}

void MapWidget::fetchRadarMetadata()
{
    if (radarMetadataReply != nullptr) {
        radarMetadataReply->abort();
        radarMetadataReply->deleteLater();
        radarMetadataReply = nullptr;
    }

    QNetworkRequest request(QUrl(QStringLiteral(
        "https://api.rainviewer.com/public/weather-maps.json")));
    request.setRawHeader("User-Agent", "WeatherApp/1.0");
    radarMetadataReply = networkManager.get(request);
    QNetworkReply* reply = radarMetadataReply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray response = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        reply->deleteLater();
        if (radarMetadataReply != reply) {
            return;
        }
        radarMetadataReply = nullptr;

        if (networkError != QNetworkReply::NoError) {
            radarStatus = QStringLiteral("Radar unavailable: %1").arg(networkErrorText);
            emit mapStatusChanged(radarStatus);
            update();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            radarStatus = QStringLiteral("Radar service returned invalid data.");
            emit mapStatusChanged(radarStatus);
            update();
            return;
        }

        const QJsonObject root = document.object();
        const QString host = root.value(QStringLiteral("host")).toString();
        const QJsonArray frames = root.value(QStringLiteral("radar"))
                                      .toObject().value(QStringLiteral("past")).toArray();
        if (!host.startsWith(QStringLiteral("https://")) ||
            frames.isEmpty() || !frames.last().isObject()) {
            radarStatus = QStringLiteral("No radar frames are currently available.");
            emit mapStatusChanged(radarStatus);
            update();
            return;
        }

        const QJsonObject latestFrame = frames.last().toObject();
        const QString path = latestFrame.value(QStringLiteral("path")).toString();
        if (!path.startsWith(QStringLiteral("/v2/radar/")) || path.contains(QStringLiteral(".."))) {
            radarStatus = QStringLiteral("Radar service returned an invalid frame.");
            emit mapStatusChanged(radarStatus);
            update();
            return;
        }

        radarHost = host;
        radarPath = path;
        const qint64 frameTime = latestFrame.value(QStringLiteral("time")).toVariant()
                                     .toLongLong();
        radarStatus = frameTime > 0
            ? QStringLiteral("Radar frame: %1 UTC").arg(
                  QDateTime::fromSecsSinceEpoch(frameTime, Qt::UTC)
                      .toString(QStringLiteral("yyyy-MM-dd hh:mm")))
            : QStringLiteral("Radar frame available");
        emit mapStatusChanged(radarStatus);
        update();
    });
}

void MapWidget::requestTile(const QString& url, const QString& key)
{
    if (tiles.contains(key) || pendingTiles.contains(key)) {
        return;
    }

    pendingTiles.insert(key);
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent", "WeatherApp/1.0");
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferCache);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
    QNetworkReply* reply = networkManager.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key]() {
        const QByteArray bytes = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString error = reply->errorString();
        reply->deleteLater();
        pendingTiles.remove(key);
        if (networkError != QNetworkReply::NoError) {
            emit mapStatusChanged(QStringLiteral("Could not load a map tile: %1").arg(error));
            return;
        }

        QImage image;
        if (!image.loadFromData(bytes)) {
            emit mapStatusChanged(QStringLiteral("The map provider returned an invalid tile."));
            return;
        }
        tiles.insert(key, image);
        update();
    });
}

void MapWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), QColor(232, 238, 241));
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QPointF topLeft = centerWorld - QPointF(width() / 2.0, height() / 2.0);
    const int firstTileX = qFloor(topLeft.x() / tileSize);
    const int lastTileX = qFloor((topLeft.x() + width()) / tileSize);
    const int firstTileY = qFloor(topLeft.y() / tileSize);
    const int lastTileY = qFloor((topLeft.y() + height()) / tileSize);
    const int tileCount = 1 << zoom;
    bool hasAnyBaseTile = false;

    for (int tileY = firstTileY; tileY <= lastTileY; ++tileY) {
        if (tileY < 0 || tileY >= tileCount) {
            continue;
        }
        for (int tileX = firstTileX; tileX <= lastTileX; ++tileX) {
            const int wrappedX = ((tileX % tileCount) + tileCount) % tileCount;
            const QRectF target(tileX * tileSize - topLeft.x(),
                                tileY * tileSize - topLeft.y(),
                                tileSize, tileSize);
            const QString baseKey = QStringLiteral("osm:%1:%2:%3")
                .arg(zoom).arg(wrappedX).arg(tileY);
            const QString baseUrl = QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png")
                .arg(zoom).arg(wrappedX).arg(tileY);
            if (tiles.contains(baseKey)) {
                painter.drawImage(target, tiles.value(baseKey));
                hasAnyBaseTile = true;
            } else {
                painter.fillRect(target, QColor(225, 232, 235));
                requestTile(baseUrl, baseKey);
            }

            if (radarLayerEnabled && !radarPath.isEmpty()) {
                const QString radarKey = QStringLiteral("radar:%1:%2:%3:%4")
                    .arg(radarPath).arg(zoom).arg(wrappedX).arg(tileY);
                const QString radarUrl = radarHost + radarPath +
                    QStringLiteral("/256/%1/%2/%3/2/1_1.png")
                        .arg(zoom).arg(wrappedX).arg(tileY);
                if (tiles.contains(radarKey)) {
                    painter.save();
                    painter.setOpacity(0.68);
                    painter.drawImage(target, tiles.value(radarKey));
                    painter.restore();
                } else {
                    requestTile(radarUrl, radarKey);
                }
            }
        }
    }

    if (hasLocation) {
        const QPointF locationWorld = project(latitude, longitude);
        const QPointF locationPoint = locationWorld - topLeft;
        painter.setPen(QPen(Qt::white, 5));
        painter.setBrush(QColor(210, 45, 54));
        painter.drawEllipse(locationPoint, 9, 9);
        painter.setPen(QPen(QColor(135, 20, 30), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(locationPoint, 10, 10);
        if (!locationLabel.isEmpty()) {
            const QRectF labelRect(locationPoint.x() + 14, locationPoint.y() - 13, 180, 26);
            painter.fillRect(labelRect, QColor(255, 255, 255, 220));
            painter.setPen(QColor(25, 35, 45));
            painter.drawText(labelRect.adjusted(6, 0, -4, 0),
                             Qt::AlignVCenter | Qt::TextSingleLine, locationLabel);
        }
    }

    if (!hasAnyBaseTile) {
        painter.setPen(QColor(50, 60, 70));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Loading map tiles..."));
    }

    const QString attribution = QStringLiteral("© OpenStreetMap contributors  |  Radar: RainViewer");
    const QRect attributionRect(6, height() - 26, width() - 12, 20);
    painter.fillRect(attributionRect, QColor(255, 255, 255, 220));
    painter.setPen(QColor(35, 45, 55));
    painter.drawText(attributionRect.adjusted(5, 0, -5, 0),
                     Qt::AlignVCenter | Qt::AlignLeft, attribution);

    if (radarLayerEnabled && !radarStatus.isEmpty()) {
        painter.fillRect(QRect(6, 6, width() - 12, 24), QColor(255, 255, 255, 220));
        painter.drawText(QRect(12, 6, width() - 24, 24),
                         Qt::AlignVCenter | Qt::AlignLeft, radarStatus);
    }
}

void MapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        lastMousePosition = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    QWidget::mousePressEvent(event);
}

void MapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging) {
        const QPoint delta = event->pos() - lastMousePosition;
        centerWorld -= QPointF(delta);
        lastMousePosition = event->pos();
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void MapWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragging = false;
        unsetCursor();
    }
    QWidget::mouseReleaseEvent(event);
}

void MapWidget::wheelEvent(QWheelEvent* event)
{
    if (event->angleDelta().y() > 0) {
        zoomIn();
    } else if (event->angleDelta().y() < 0) {
        zoomOut();
    }
    event->accept();
}
