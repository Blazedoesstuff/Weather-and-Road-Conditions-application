#ifndef ALERT_DATA_H
#define ALERT_DATA_H

#include <QDateTime>
#include <QString>

struct WeatherAlert
{
    QString event;
    QString headline;
    QString severity;
    QString urgency;
    QString certainty;
    QString areaDescription;
    QString description;
    QString instruction;
    QDateTime effective;
    QDateTime expires;
};

#endif
