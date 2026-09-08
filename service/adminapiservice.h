#ifndef ADMINAPISERVICE_H
#define ADMINAPISERVICE_H

#include "model/adminmodels.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
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
    QJsonDocument getJson(const QString &path, bool *ok, QString *errorMessage = nullptr) const;
    QJsonDocument postJson(const QString &path,
                           const QJsonObject &body,
                           bool *ok,
                           QString *errorMessage = nullptr) const;
    QJsonDocument sendJson(const QString &method,
                           const QString &path,
                           const QJsonObject *body,
                           bool *ok,
                           QString *errorMessage) const;
    QList<ChargerInfo> backendChargers(bool *ok = nullptr) const;
    QList<StationInfo> backendStations(bool *ok = nullptr) const;
    QString statusToText(const QString &status) const;
    QString pointTypeToText(const QString &type) const;
    QString userStatusToText(const QString &status) const;

    QString baseUrl_ = "http://127.0.0.1:5555";
    QString token_;
    mutable QNetworkAccessManager network_;
    QList<StationInfo> stations_;
    QList<UserInfo> users_;
};

#endif // ADMINAPISERVICE_H
