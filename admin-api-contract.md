# 运营管理端接口约定

Base URL: `http://127.0.0.1:5555`

所有请求和响应默认使用：

```text
Content-Type: application/json
```

除管理员登录接口外，管理端接口都建议携带：

```text
Authorization: Bearer <admin-token>
```

## 通用错误格式

```json
{
  "code": "invalid_request",
  "message": "请求参数不正确"
}
```

建议错误码：

| code | HTTP | 说明 |
|---|---:|---|
| `invalid_request` | 400 | 参数缺失、格式错误或校验失败 |
| `invalid_credentials` | 401 | 管理员账号或密码错误 |
| `unauthorized` | 401 | 未登录或 token 无效 |
| `forbidden` | 403 | 当前账号无操作权限 |
| `not_found` | 404 | 目标用户、电站或电桩不存在 |
| `conflict` | 409 | 状态冲突，例如重启正在使用的电桩 |
| `internal_error` | 500 | 服务端内部错误 |

## 状态值约定

电桩状态：

```text
空闲 / 在用 / 故障
```

用户状态：

```text
正常 / 冻结
```

订单状态：

```text
待充电 / 充电中 / 待结算 / 已完成
```

## 1. 管理员登录

```text
POST /api/admin/login
```

说明：管理员登录。默认账号 `admin`，默认密码 `123456`。

请求：

```json
{
  "account": "admin",
  "password": "123456"
}
```

成功响应：

```json
{
  "id": 1,
  "account": "admin",
  "token": "admin-token-example"
}
```

错误：

- `400 invalid_request`：账号或密码为空
- `401 invalid_credentials`：账号或密码错误

## 2. 营收总览

```text
GET /api/admin/revenue/summary
```

说明：返回今日营收、本月营收、总营收。只统计状态为 `已完成` 的订单。

成功响应：

```json
{
  "todayRevenue": 386.5,
  "monthRevenue": 12480.8,
  "totalRevenue": 96842.2
}
```

错误：

- `401 unauthorized`：管理员未登录或 token 无效

## 3. 营收趋势

```text
GET /api/admin/revenue/trend?days=7
```

说明：返回最近 7 天或 30 天的每日营收。没有订单的日期补 0。只统计状态为 `已完成` 的订单。

查询参数：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `days` | number | 是 | 只支持 `7` 或 `30` |

成功响应：

```json
{
  "days": 7,
  "points": [
    {
      "date": "2026-09-01",
      "amount": 180.5
    },
    {
      "date": "2026-09-02",
      "amount": 0
    }
  ]
}
```

错误：

- `400 invalid_request`：`days` 不是 7 或 30
- `401 unauthorized`：管理员未登录或 token 无效

## 4. 电桩状态统计

```text
GET /api/admin/chargers/status-summary
```

说明：统计空闲、在用、故障电桩数量和占比。

成功响应：

```json
{
  "total": 10,
  "idleCount": 5,
  "usingCount": 3,
  "faultCount": 2,
  "idleRate": 50.0,
  "usingRate": 30.0,
  "faultRate": 20.0
}
```

错误：

- `401 unauthorized`：管理员未登录或 token 无效

## 5. 电桩列表

```text
GET /api/admin/chargers
```

说明：返回所有电桩列表。

成功响应：

```json
{
  "chargers": [
    {
      "id": "HT-001",
      "stationId": 101,
      "stationName": "海棠湾快充站",
      "type": "快充",
      "powerKw": 120.0,
      "status": "空闲",
      "totalSessions": 284,
      "totalHours": 738.5
    }
  ]
}
```

字段说明：

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` | string | 电桩编号 |
| `stationId` | number | 所属电站 ID |
| `stationName` | string | 所属电站名称 |
| `type` | string | 电桩类型，例如 `快充`、`慢充` |
| `powerKw` | number | 功率，单位 kW |
| `status` | string | `空闲`、`在用`、`故障` |
| `totalSessions` | number | 累计充电次数 |
| `totalHours` | number | 累计充电时长，单位小时 |

错误：

- `401 unauthorized`：管理员未登录或 token 无效

## 6. 远程重启电桩

```text
POST /api/admin/chargers/{chargerId}/restart
```

说明：对指定电桩发送远程重启指令，并返回操作结果和最新状态。

路径参数：

| 参数 | 类型 | 说明 |
|---|---|---|
| `chargerId` | string | 电桩编号，例如 `HT-001` |

请求：

```json
{}
```

成功响应：

```json
{
  "success": true,
  "message": "重启指令已发送",
  "charger": {
    "id": "HT-001",
    "status": "空闲"
  }
}
```

错误：

- `401 unauthorized`：管理员未登录或 token 无效
- `404 not_found`：电桩不存在
- `409 conflict`：电桩正在使用中，暂不能重启

## 7. 电站列表

```text
GET /api/admin/stations
```

说明：返回所有充电站列表。

成功响应：

```json
{
  "stations": [
    {
      "id": 101,
      "name": "海棠湾快充站",
      "address": "软件园北街 18 号",
      "latitude": 39.9821,
      "longitude": 116.3074,
      "price": 1.36,
      "chargerTotal": 3,
      "onlineRate": 100.0
    }
  ]
}
```

字段说明：

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` | number | 电站 ID |
| `name` | string | 电站名称 |
| `address` | string | 详细地址 |
| `latitude` | number | 纬度 |
| `longitude` | number | 经度 |
| `price` | number | 充电单价 |
| `chargerTotal` | number | 电桩总数 |
| `onlineRate` | number | 在线率百分比，0 到 100 |

错误：

- `401 unauthorized`：管理员未登录或 token 无效

## 8. 站内电桩详情

```text
GET /api/admin/stations/{stationId}/chargers
```

说明：返回指定电站下所有电桩的实时状态明细。

路径参数：

| 参数 | 类型 | 说明 |
|---|---|---|
| `stationId` | number | 电站 ID |

成功响应：

```json
{
  "stationId": 101,
  "stationName": "海棠湾快充站",
  "chargers": [
    {
      "id": "HT-001",
      "type": "快充",
      "powerKw": 120.0,
      "status": "空闲",
      "totalSessions": 284,
      "totalHours": 738.5
    }
  ]
}
```

错误：

- `401 unauthorized`：管理员未登录或 token 无效
- `404 not_found`：电站不存在

## 9. 新增充电站

```text
POST /api/admin/stations
```

说明：新增充电站，并按电桩数量初始化关联电桩。

请求：

```json
{
  "name": "大学城快充站",
  "address": "大学城东路 88 号",
  "latitude": 39.98,
  "longitude": 116.32,
  "price": 1.28,
  "chargerCount": 4
}
```

校验规则：

- `name`、`address` 不能为空
- `latitude` 范围为 -90 到 90
- `longitude` 范围为 -180 到 180
- `price` 必须大于 0
- `chargerCount` 必须大于 0
- 电站名称或位置不能重复

成功响应：

```json
{
  "id": 104,
  "name": "大学城快充站",
  "address": "大学城东路 88 号",
  "latitude": 39.98,
  "longitude": 116.32,
  "price": 1.28,
  "chargerTotal": 4,
  "onlineRate": 100.0
}
```

错误：

- `400 invalid_request`：参数缺失或格式错误
- `401 unauthorized`：管理员未登录或 token 无效
- `409 conflict`：电站名称或位置重复

## 10. 用户列表与手机号搜索

```text
GET /api/admin/users
GET /api/admin/users?phone=138
```

说明：返回用户列表。`phone` 可选，用于手机号模糊搜索。

查询参数：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `phone` | string | 否 | 手机号关键字 |

成功响应：

```json
{
  "users": [
    {
      "id": 1,
      "phone": "13800138000",
      "nickname": "用户8000",
      "balance": 128.5,
      "registeredAt": "2026-08-28T09:10:00",
      "status": "正常"
    }
  ]
}
```

字段说明：

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` | number | 用户 ID |
| `phone` | string | 手机号 |
| `nickname` | string | 昵称 |
| `balance` | number | 钱包余额 |
| `registeredAt` | string | 注册时间，ISO 时间字符串 |
| `status` | string | `正常` 或 `冻结` |

错误：

- `401 unauthorized`：管理员未登录或 token 无效

## 11. 冻结或解冻用户

```text
POST /api/admin/users/{userId}/status
```

说明：修改指定用户状态。冻结后用户不能继续登录或发起新的充电业务。

路径参数：

| 参数 | 类型 | 说明 |
|---|---|---|
| `userId` | number | 用户 ID |

请求：

```json
{
  "status": "冻结"
}
```

也可以解冻：

```json
{
  "status": "正常"
}
```

成功响应：

```json
{
  "id": 3,
  "phone": "18600001234",
  "nickname": "通勤车主",
  "balance": 0.8,
  "registeredAt": "2026-09-01T18:20:00",
  "status": "冻结"
}
```

错误：

- `400 invalid_request`：`status` 不是 `正常` 或 `冻结`
- `401 unauthorized`：管理员未登录或 token 无效
- `404 not_found`：用户不存在
- `409 conflict`：用户已经是目标状态

## 前端接入说明

管理端当前已经有 `AdminApiService` 服务层。后端接口完成后，前端主要把 mock 数据替换为 HTTP 请求，不需要大改页面。

建议后端接口完成后同步更新后端项目的 `API.md`，保持真实接口和文档一致。
