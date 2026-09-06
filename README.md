# Gale Verdict 运营管理端

独立 Qt Widgets 管理端项目，对应需求矩阵中“运营管理端”第 19-29 条。

## 当前实现

- 管理员登录：默认账号 `admin`，默认密码 `123456`。
- 经营总览：今日营收、本月营收、总营收、近 7 天 / 30 天趋势图。
- 设备状态监控：在用、空闲、故障电桩数量。
- 充电桩管理：列表查询、远程重启模拟。
- 充电站管理：列表查询、站内电桩详情、新增电站。
- 用户管理：手机号模糊搜索、冻结与解冻。

## 后端连接策略

目前后端 `API.md` 只有普通用户注册、登录、查询本人和充值接口，尚未提供管理端接口。因此本项目采用 mock 服务层：

- 页面只调用 `service/AdminApiService`。
- `AdminApiService` 现在返回本地模拟数据。
- 每个未来后端接入点都已用 `TODO(后端)` 标注。

后端补齐接口后，主要替换 `AdminApiService`，页面结构不用大改。

## Ubuntu 22.04 依赖

管理端使用 Qt Charts，因此在之前客户端依赖基础上还需要：

```bash
sudo apt install -y qt6-charts-dev
```

完整依赖可使用：

```bash
sudo apt install -y \
build-essential gdb git qtcreator \
qt6-base-dev qt6-base-dev-tools qmake6 \
qt6-tools-dev qt6-tools-dev-tools \
qt6-declarative-dev qt6-positioning-dev \
libqt6webchannel6-dev \
qt6-webengine-dev qt6-webengine-dev-tools \
qt6-charts-dev \
libgl1-mesa-dev libnss3 libxkbcommon-x11-0 libxcb-cursor0
```

## 构建

在 Qt Creator 中打开：

```text
Gale-Verdict-Admin.pro
```

选择 `Desktop Qt 6.2.4 GCC 64bit` Kit 后构建并运行。

命令行构建：

```bash
mkdir -p build-linux
cd build-linux
qmake6 ../Gale-Verdict-Admin.pro CONFIG+=debug
make -j$(nproc)
./Gale-Verdict-Admin
```
