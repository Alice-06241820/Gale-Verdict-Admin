#ifndef ADMINAPISERVICE_H
#define ADMINAPISERVICE_H

#include "model/adminmodels.h"

#include <QObject>

class AdminApiService : public QObject
{
    Q_OBJECT

public:
    explicit AdminApiService(QObject *parent = nullptr);

    bool login(const QString &account, const QString &password, QString *errorMessage = nullptr);

    RevenueSummary revenueSummary() const;
    QList<RevenuePoint> revenueTrend(int days) const;
    DeviceStatusSummary deviceStatusSummary() const;

    QList<ChargerInfo> chargers() const;
    bool restartCharger(const QString &chargerId, QString *message = nullptr);

    QList<StationInfo> stations() const;
    bool addStation(const QString &name,
                    const QString &address,
                    double latitude,
                    double longitude,
                    int chargerCount,
                    QString *message = nullptr);

    QList<UserInfo> users(const QString &phoneKeyword = QString()) const;
    bool setUserFrozen(int userId, bool frozen, QString *message = nullptr);

private:
    void seedMockData();
    int countOnlineChargers(const StationInfo &station) const;

    QList<StationInfo> stations_;
    QList<UserInfo> users_;
};

#endif // ADMINAPISERVICE_H
