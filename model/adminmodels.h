#ifndef ADMINMODELS_H
#define ADMINMODELS_H

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QPointF>
#include <QString>
#include <QTime>

struct RevenueSummary
{
    double today = 0.0;
    double month = 0.0;
    double total = 0.0;
};

struct RevenuePoint
{
    QDate date;
    double amount = 0.0;
};

struct DeviceStatusSummary
{
    int usingCount = 0;
    int idleCount = 0;
    int faultCount = 0;

    int total() const { return usingCount + idleCount + faultCount; }
};

struct ChargerInfo
{
    QString id;
    QString stationName;
    QString type;
    double powerKw = 0.0;
    QString status;
    int totalSessions = 0;
    double totalHours = 0.0;
};

struct StationInfo
{
    int id = 0;
    QString name;
    QString address;
    double latitude = 0.0;
    double longitude = 0.0;
    double price = 0.0;
    QList<ChargerInfo> chargers;
};

struct UserInfo
{
    int id = 0;
    QString phone;
    QString nickname;
    double balance = 0.0;
    QDateTime registeredAt;
    QString status;
};

#endif // ADMINMODELS_H
