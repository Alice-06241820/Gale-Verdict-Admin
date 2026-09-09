#include "service/adminapiservice.h"

#include <QDate>
#include <QEventLoop>
#include <QHash>
#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace {

QString jsonErrorMessage(const QJsonDocument &doc, const QString &fallback)
{
    if (doc.isObject()) {
        const QJsonObject object = doc.object();
        const QString code = object.value("code").toString();
        if (code == "point_working") {
            return "电桩正在使用中，后端当前禁止修改类型和功率";
        }
        if (code == "unauthorized") {
            return "登录已失效，请重新登录管理员账号";
        }
        if (code == "forbidden") {
            return "当前账号不是管理员，后端拒绝了本次操作";
        }
        if (code == "not_found") {
            return "后端没有找到这个电桩，请刷新列表后重试";
        }
        const QString message = object.value("message").toString();
        if (!message.isEmpty()) {
            return message;
        }
    }
    return fallback;
}

} // namespace

AdminApiService::AdminApiService(QObject *parent)
    : QObject(parent)
{
    seedMockData();
}

bool AdminApiService::login(const QString &account, const QString &password, QString *errorMessage)
{
    if (account.trimmed().isEmpty() || password.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "账号和密码不能为空";
        }
        return false;
    }

    QJsonObject body;
    body["username"] = account.trimmed();
    body["password"] = password;

    bool ok = false;
    const QJsonDocument doc = postJson("/api/users/login", body, &ok, errorMessage);
    if (!ok || !doc.isObject()) {
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = "登录失败，请检查后端是否正在运行";
        }
        return false;
    }

    const QJsonObject user = doc.object();
    if (user.value("role").toString() != "admin") {
        if (errorMessage) {
            *errorMessage = "当前账号不是管理员，请先让后端把该账号 role 改为 admin";
        }
        return false;
    }

    token_ = user.value("token").toString();
    if (token_.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "后端没有返回登录 token";
        }
        return false;
    }

    return true;
}

RevenueSummary AdminApiService::revenueSummary() const
{
    bool ok = false;
    const QJsonDocument doc = getJson("/api/admin/revenue/summary", &ok);
    if (ok && doc.isObject()) {
        const QJsonObject data = doc.object();
        return {data.value("today").toDouble(),
                data.value("month").toDouble(),
                data.value("total").toDouble()};
    }
    return {386.50, 12480.80, 96842.20};
}

QList<RevenuePoint> AdminApiService::revenueTrend(int days) const
{
    bool ok = false;
    const QJsonDocument doc = getJson(QString("/api/admin/revenue/trend?days=%1").arg(days), &ok);
    if (ok && doc.isArray()) {
        QList<RevenuePoint> result;
        const QJsonArray rows = doc.array();
        for (const QJsonValue &value : rows) {
            if (!value.isObject()) {
                continue;
            }
            const QJsonObject row = value.toObject();
            result.append({QDate::fromString(row.value("date").toString(), Qt::ISODate),
                           row.value("revenue").toDouble()});
        }
        return result;
    }

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
    bool statsOk = false;
    const QJsonDocument doc = getJson("/api/charging-points/statistics", &statsOk);
    if (statsOk && doc.isObject()) {
        const QJsonObject data = doc.object();
        DeviceStatusSummary summary;
        summary.usingCount = data.value("working").toObject().value("count").toInt();
        summary.idleCount = data.value("free").toObject().value("count").toInt();
        summary.faultCount = data.value("broken").toObject().value("count").toInt();
        return summary;
    }

    DeviceStatusSummary summary;
    bool ok = false;
    const QList<StationInfo> rows = backendStations(&ok);
    const QList<StationInfo> source = ok ? rows : stations_;
    for (const StationInfo &station : source) {
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
    bool ok = false;
    const QList<ChargerInfo> rows = backendChargers(&ok);
    if (ok) {
        return rows;
    }

    QList<ChargerInfo> fallback;
    for (const StationInfo &station : stations_) {
        fallback.append(station.chargers);
    }
    return fallback;
}

bool AdminApiService::restartCharger(const QString &chargerId, QString *message)
{
    bool ok = false;
    postJson(QString("/api/charging-points/%1/restart").arg(chargerId.trimmed()),
             QJsonObject(),
             &ok,
             message);
    if (ok) {
        if (message) {
            *message = "重启成功，电桩已恢复空闲";
        }
        return true;
    }

    if (!token_.isEmpty()) {
        return false;
    }

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

bool AdminApiService::updateChargerAttributes(const QString &chargerId,
                                              const QString &typeText,
                                              double powerKw,
                                              QString *message)
{
    const QString trimmedId = chargerId.trimmed();
    const QString apiType = pointTypeToApi(typeText);
    if (trimmedId.isEmpty()) {
        if (message) {
            *message = "请先选择需要修改的电桩";
        }
        return false;
    }
    if (apiType.isEmpty()) {
        if (message) {
            *message = "电桩类型必须是快充或慢充";
        }
        return false;
    }
    if (powerKw <= 0.0) {
        if (message) {
            *message = "功率必须大于 0";
        }
        return false;
    }

    if (!token_.isEmpty()) {
        QJsonObject body;
        body["type"] = apiType;
        body["power_kw"] = powerKw;

        bool ok = false;
        patchJson(QString("/api/charging-points/%1").arg(trimmedId), body, &ok, message);
        if (ok) {
            if (message) {
                *message = "电桩参数已保存";
            }
            return true;
        }
        return false;
    }

    for (StationInfo &station : stations_) {
        for (ChargerInfo &charger : station.chargers) {
            if (charger.id == trimmedId) {
                if (charger.status == "在用") {
                    if (message) {
                        *message = "电桩正在使用中，暂不能修改类型和功率";
                    }
                    return false;
                }
                charger.type = pointTypeToText(apiType);
                charger.powerKw = powerKw;
                if (message) {
                    *message = "电桩参数已保存";
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
    bool ok = false;
    const QList<StationInfo> rows = backendStations(&ok);
    if (ok) {
        return rows;
    }
    return stations_;
}

bool AdminApiService::addStation(const QString &name,
                                 const QString &address,
                                 double latitude,
                                 double longitude,
                                 int chargerCount,
                                 QString *message)
{
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

    QJsonObject body;
    body["name"] = trimmedName;
    body["latitude"] = latitude;
    body["longitude"] = longitude;
    body["total_points"] = chargerCount;

    bool ok = false;
    postJson("/api/charging-stations/register", body, &ok, message);
    if (ok) {
        if (message) {
            *message = "新增电站成功";
        }
        return true;
    }

    if (!token_.isEmpty()) {
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
    if (!token_.isEmpty()) {
        const QString keyword = phoneKeyword.trimmed();
        QString path = "/api/admin/users";
        if (!keyword.isEmpty()) {
            path += "?phone=" + QString::fromUtf8(QUrl::toPercentEncoding(keyword));
        }

        bool ok = false;
        const QJsonDocument doc = getJson(path, &ok);
        if (ok && doc.isArray()) {
            QList<UserInfo> result;
            const QJsonArray rows = doc.array();
            for (const QJsonValue &value : rows) {
                if (!value.isObject()) {
                    continue;
                }
                const QJsonObject row = value.toObject();
                UserInfo user;
                user.id = row.value("id").toInt();
                user.phone = row.value("phone").toString();
                user.nickname = row.value("username").toString();
                user.balance = row.value("balance").toDouble();
                user.registeredAt = QDateTime::fromSecsSinceEpoch(
                    static_cast<qint64>(row.value("created_at").toDouble()));
                user.status = userStatusToText(row.value("status").toString());
                result.append(user);
            }
            return result;
        }
    }

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
    if (!token_.isEmpty()) {
        QJsonObject body;
        body["status"] = frozen ? QString("frozen") : QString("active");

        bool ok = false;
        postJson(QString("/api/admin/users/%1/status").arg(userId), body, &ok, message);
        if (ok) {
            if (message) {
                *message = frozen ? "用户已冻结" : "用户已解冻";
            }
            return true;
        }
        return false;
    }

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

QJsonDocument AdminApiService::getJson(const QString &path, bool *ok, QString *errorMessage) const
{
    return sendJson("GET", path, nullptr, ok, errorMessage);
}

QJsonDocument AdminApiService::postJson(const QString &path,
                                        const QJsonObject &body,
                                        bool *ok,
                                        QString *errorMessage) const
{
    return sendJson("POST", path, &body, ok, errorMessage);
}

QJsonDocument AdminApiService::patchJson(const QString &path,
                                         const QJsonObject &body,
                                         bool *ok,
                                         QString *errorMessage) const
{
    return sendJson("PATCH", path, &body, ok, errorMessage);
}

QJsonDocument AdminApiService::sendJson(const QString &method,
                                        const QString &path,
                                        const QJsonObject *body,
                                        bool *ok,
                                        QString *errorMessage) const
{
    if (ok) {
        *ok = false;
    }
    if (errorMessage) {
        errorMessage->clear();
    }

    QNetworkRequest request(QUrl(baseUrl_ + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!token_.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + token_.toUtf8());
    }

    QNetworkReply *reply = nullptr;
    if (method == "GET") {
        reply = network_.get(request);
    } else if (method == "POST") {
        const QByteArray payload = body ? QJsonDocument(*body).toJson(QJsonDocument::Compact) : QByteArray("{}");
        reply = network_.post(request, payload);
    } else {
        const QByteArray payload = body ? QJsonDocument(*body).toJson(QJsonDocument::Compact) : QByteArray("{}");
        reply = network_.sendCustomRequest(request, method.toUtf8(), payload);
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(5000);
    loop.exec();

    if (!timer.isActive()) {
        reply->abort();
        reply->deleteLater();
        if (errorMessage) {
            *errorMessage = "连接后端超时，请确认后端服务已启动";
        }
        return QJsonDocument();
    }

    const QByteArray data = reply->readAll();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError networkError = reply->error();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (networkError != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
        if (errorMessage) {
            const QString fallback = statusCode > 0
                                         ? QString("后端返回错误：HTTP %1").arg(statusCode)
                                         : "无法连接后端，请确认服务已启动";
            *errorMessage = jsonErrorMessage(doc, fallback);
        }
        return doc;
    }

    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage) {
            *errorMessage = "后端返回的数据不是合法 JSON";
        }
        return QJsonDocument();
    }

    if (ok) {
        *ok = true;
    }
    return doc;
}

QList<ChargerInfo> AdminApiService::backendChargers(bool *ok) const
{
    if (ok) {
        *ok = false;
    }
    if (token_.isEmpty()) {
        return {};
    }

    bool requestOk = false;
    const QJsonDocument doc = getJson("/api/charging-points", &requestOk);
    if (!requestOk || !doc.isArray()) {
        return {};
    }

    QList<ChargerInfo> rows;
    const QJsonArray points = doc.array();
    for (const QJsonValue &value : points) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject pointJson = value.toObject();
        ChargerInfo charger;
        charger.id = QString::number(pointJson.value("id").toInt());
        charger.stationId = pointJson.value("charging_station_id").toInt();
        charger.stationName = pointJson.value("charging_station_name").toString();
        charger.type = pointTypeToText(pointJson.value("type").toString());
        charger.powerKw = pointJson.value("power_kw").toDouble();
        charger.status = statusToText(pointJson.value("status").toString());
        charger.totalSessions = pointJson.value("usage_count").toInt();
        charger.totalHours = pointJson.value("usage_duration_seconds").toDouble() / 3600.0;
        rows.append(charger);
    }

    if (ok) {
        *ok = true;
    }
    return rows;
}

QList<StationInfo> AdminApiService::backendStations(bool *ok) const
{
    if (ok) {
        *ok = false;
    }
    if (token_.isEmpty()) {
        return {};
    }

    bool stationsOk = false;
    const QJsonDocument doc = getJson("/api/charging-stations", &stationsOk);
    if (!stationsOk || !doc.isArray()) {
        return {};
    }

    bool chargersOk = false;
    const QList<ChargerInfo> chargers = backendChargers(&chargersOk);

    QHash<int, QList<ChargerInfo>> chargersByStation;
    if (chargersOk) {
        for (const ChargerInfo &charger : chargers) {
            chargersByStation[charger.stationId].append(charger);
        }
    }

    QList<StationInfo> rows;
    const QJsonArray stations = doc.array();
    for (const QJsonValue &value : stations) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject stationJson = value.toObject();
        StationInfo station;
        station.id = stationJson.value("id").toInt();
        station.name = stationJson.value("name").toString();
        station.address = "后端暂未返回地址";
        station.latitude = stationJson.value("latitude").toDouble();
        station.longitude = stationJson.value("longitude").toDouble();
        station.price = 0.0;

        if (chargersOk) {
            station.chargers = chargersByStation.value(station.id);
        } else {
            bool pointsOk = false;
            const QJsonDocument pointsDoc = getJson(QString("/api/charging-stations/%1/points").arg(station.id),
                                                    &pointsOk);
            if (pointsOk && pointsDoc.isArray()) {
                const QJsonArray points = pointsDoc.array();
                for (const QJsonValue &pointValue : points) {
                    if (!pointValue.isObject()) {
                        continue;
                    }
                    const QJsonObject pointJson = pointValue.toObject();
                    ChargerInfo charger;
                    charger.id = QString::number(pointJson.value("id").toInt());
                    charger.stationId = station.id;
                    charger.stationName = station.name;
                    charger.type = pointTypeToText(pointJson.value("type").toString());
                    charger.powerKw = pointJson.value("power_kw").toDouble();
                    charger.status = statusToText(pointJson.value("status").toString());
                    charger.totalSessions = 0;
                    charger.totalHours = 0.0;
                    station.chargers.append(charger);
                }
            }
        }
        rows.append(station);
    }

    if (ok) {
        *ok = true;
    }
    return rows;
}

QString AdminApiService::statusToText(const QString &status) const
{
    if (status == "working") {
        return "在用";
    }
    if (status == "broken") {
        return "故障";
    }
    return "空闲";
}

QString AdminApiService::pointTypeToText(const QString &type) const
{
    if (type == "DC") {
        return "快充";
    }
    if (type == "AC") {
        return "慢充";
    }
    return type.isEmpty() ? "未知" : type;
}

QString AdminApiService::pointTypeToApi(const QString &typeText) const
{
    if (typeText == "快充" || typeText == "DC") {
        return "DC";
    }
    if (typeText == "慢充" || typeText == "AC") {
        return "AC";
    }
    return QString();
}

QString AdminApiService::userStatusToText(const QString &status) const
{
    if (status == "frozen") {
        return "冻结";
    }
    return "正常";
}
