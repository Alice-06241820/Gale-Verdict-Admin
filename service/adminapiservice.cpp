#include "service/adminapiservice.h"

#include <QDate>

AdminApiService::AdminApiService(QObject *parent)
    : QObject(parent)
{
    seedMockData();
}

bool AdminApiService::login(const QString &account, const QString &password, QString *errorMessage)
{
    // TODO(后端)：替换为 POST /api/admin/login，成功后保存管理员 token。
    if (account.trimmed().isEmpty() || password.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "账号和密码不能为空";
        }
        return false;
    }

    if (account == "admin" && password == "123456") {
        return true;
    }

    if (errorMessage) {
        *errorMessage = "账号或密码错误";
    }
    return false;
}

RevenueSummary AdminApiService::revenueSummary() const
{
    // TODO(后端)：替换为 GET /api/admin/revenue/summary。
    return {386.50, 12480.80, 96842.20};
}

QList<RevenuePoint> AdminApiService::revenueTrend(int days) const
{
    // TODO(后端)：替换为 GET /api/admin/revenue/trend?days=7/30。
    QList<RevenuePoint> points;
    const QDate today = QDate::currentDate();
    for (int i = days - 1; i >= 0; --i) {
        const int phase = (days - i) % 6;
        const double base = days <= 7 ? 210.0 : 160.0;
        points.append({today.addDays(-i), base + phase * 42.5 + (i % 3) * 18.0});
    }
    return points;
}

DeviceStatusSummary AdminApiService::deviceStatusSummary() const
{
    // TODO(后端)：替换为 GET /api/admin/chargers/status-summary。
    DeviceStatusSummary summary;
    for (const StationInfo &station : stations_) {
        for (const ChargerInfo &charger : station.chargers) {
            if (charger.status == "在用") {
                ++summary.usingCount;
            } else if (charger.status == "故障") {
                ++summary.faultCount;
            } else {
                ++summary.idleCount;
            }
        }
    }
    return summary;
}

QList<ChargerInfo> AdminApiService::chargers() const
{
    // TODO(后端)：替换为 GET /api/admin/chargers。
    QList<ChargerInfo> result;
    for (const StationInfo &station : stations_) {
        result.append(station.chargers);
    }
    return result;
}

bool AdminApiService::restartCharger(const QString &chargerId, QString *message)
{
    // TODO(后端)：替换为 POST /api/admin/chargers/{id}/restart。
    for (StationInfo &station : stations_) {
        for (ChargerInfo &charger : station.chargers) {
            if (charger.id == chargerId) {
                if (charger.status == "在用") {
                    if (message) {
                        *message = "电桩正在使用中，暂不能重启";
                    }
                    return false;
                }
                charger.status = "空闲";
                if (message) {
                    *message = "重启指令已发送，设备状态已刷新";
                }
                return true;
            }
        }
    }

    if (message) {
        *message = "未找到目标电桩";
    }
    return false;
}

QList<StationInfo> AdminApiService::stations() const
{
    // TODO(后端)：替换为 GET /api/admin/stations。
    return stations_;
}

bool AdminApiService::addStation(const QString &name,
                                 const QString &address,
                                 double latitude,
                                 double longitude,
                                 int chargerCount,
                                 QString *message)
{
    // TODO(后端)：替换为 POST /api/admin/stations。
    const QString trimmedName = name.trimmed();
    const QString trimmedAddress = address.trimmed();
    if (trimmedName.isEmpty() || trimmedAddress.isEmpty()) {
        if (message) {
            *message = "站名和地址不能为空";
        }
        return false;
    }
    if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
        if (message) {
            *message = "经纬度范围不正确";
        }
        return false;
    }
    if (chargerCount <= 0 || chargerCount > 50) {
        if (message) {
            *message = "电桩数量需在 1 到 50 之间";
        }
        return false;
    }
    for (const StationInfo &station : stations_) {
        if (station.name == trimmedName) {
            if (message) {
                *message = "电站名称已存在";
            }
            return false;
        }
    }

    StationInfo station;
    station.id = stations_.isEmpty() ? 1 : stations_.last().id + 1;
    station.name = trimmedName;
    station.address = trimmedAddress;
    station.latitude = latitude;
    station.longitude = longitude;
    station.price = 1.28;
    for (int i = 1; i <= chargerCount; ++i) {
        ChargerInfo charger;
        charger.id = QString("S%1-C%2").arg(station.id, 3, 10, QChar('0')).arg(i, 2, 10, QChar('0'));
        charger.stationName = station.name;
        charger.type = i % 2 == 0 ? "快充" : "慢充";
        charger.powerKw = i % 2 == 0 ? 120.0 : 60.0;
        charger.status = "空闲";
        station.chargers.append(charger);
    }
    stations_.append(station);

    if (message) {
        *message = "新增电站成功";
    }
    return true;
}

QList<UserInfo> AdminApiService::users(const QString &phoneKeyword) const
{
    // TODO(后端)：替换为 GET /api/admin/users?phone=。
    if (phoneKeyword.trimmed().isEmpty()) {
        return users_;
    }

    QList<UserInfo> result;
    for (const UserInfo &user : users_) {
        if (user.phone.contains(phoneKeyword.trimmed())) {
            result.append(user);
        }
    }
    return result;
}

bool AdminApiService::setUserFrozen(int userId, bool frozen, QString *message)
{
    // TODO(后端)：替换为 POST /api/admin/users/{id}/status。
    for (UserInfo &user : users_) {
        if (user.id == userId) {
            const QString targetStatus = frozen ? "冻结" : "正常";
            if (user.status == targetStatus) {
                if (message) {
                    *message = frozen ? "用户已经是冻结状态" : "用户已经是正常状态";
                }
                return false;
            }
            user.status = targetStatus;
            if (message) {
                *message = frozen ? "用户已冻结" : "用户已解冻";
            }
            return true;
        }
    }

    if (message) {
        *message = "未找到目标用户";
    }
    return false;
}

void AdminApiService::seedMockData()
{
    stations_.clear();
    users_.clear();

    StationInfo north;
    north.id = 101;
    north.name = "海棠湾快充站";
    north.address = "软件园北街 18 号";
    north.latitude = 39.9821;
    north.longitude = 116.3074;
    north.price = 1.36;
    north.chargers = {
        {"HT-001", north.name, "快充", 120.0, "空闲", 284, 738.5},
        {"HT-002", north.name, "快充", 120.0, "在用", 311, 825.0},
        {"HT-003", north.name, "慢充", 60.0, "故障", 96, 210.5},
    };

    StationInfo east;
    east.id = 102;
    east.name = "云谷中心充电站";
    east.address = "云谷路 7 号 B 区";
    east.latitude = 39.9754;
    east.longitude = 116.3358;
    east.price = 1.22;
    east.chargers = {
        {"YG-001", east.name, "快充", 100.0, "空闲", 198, 540.0},
        {"YG-002", east.name, "慢充", 45.0, "空闲", 152, 421.0},
        {"YG-003", east.name, "快充", 100.0, "在用", 243, 688.0},
        {"YG-004", east.name, "快充", 100.0, "空闲", 176, 509.5},
    };

    StationInfo south;
    south.id = 103;
    south.name = "清河服务区充电站";
    south.address = "清河路 66 号";
    south.latitude = 39.9563;
    south.longitude = 116.3182;
    south.price = 1.18;
    south.chargers = {
        {"QH-001", south.name, "慢充", 60.0, "空闲", 132, 390.0},
        {"QH-002", south.name, "快充", 120.0, "故障", 88, 184.0},
        {"QH-003", south.name, "快充", 120.0, "在用", 265, 704.0},
    };

    stations_ = {north, east, south};

    users_ = {
        {1, "13800138000", "用户8000", 128.50, QDateTime(QDate(2026, 8, 28), QTime(9, 10)), "正常"},
        {2, "13900139000", "用户9000", 32.00, QDateTime(QDate(2026, 8, 30), QTime(15, 42)), "正常"},
        {3, "18600001234", "通勤车主", 0.80, QDateTime(QDate(2026, 9, 1), QTime(18, 20)), "冻结"},
        {4, "15700005678", "周末出行", 246.70, QDateTime(QDate(2026, 9, 3), QTime(11, 6)), "正常"},
    };
}

int AdminApiService::countOnlineChargers(const StationInfo &station) const
{
    int count = 0;
    for (const ChargerInfo &charger : station.chargers) {
        if (charger.status != "故障") {
            ++count;
        }
    }
    return count;
}
