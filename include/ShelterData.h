#ifndef SHELTER_DATA_H
#define SHELTER_DATA_H

#include <QString>

struct ShelterData
{
    QString id;
    QString name;
    QString address;
    QString shelterType;
    QString source;
    QString availability;
    double latitude = 0.0;
    double longitude = 0.0;
    double distanceKm = 0.0;
};

#endif
