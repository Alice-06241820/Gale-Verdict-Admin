#include "pages/stationspage.h"

#include "model/adminmodels.h"
#include "service/adminapiservice.h"

#include <QAbstractItemView>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

StationsPage::StationsPage(AdminApiService *service, QWidget *parent)
    : QWidget(parent)
    , service_(service)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(16);

    auto *title = new QLabel("充电站管理", this);
    title->setObjectName("pageTitle");
    auto *subtitle = new QLabel("维护电站台账，查看站内电桩状态", this);
    subtitle->setObjectName("pageSubtitle");
    layout->addWidget(title);
    layout->addWidget(subtitle);

    auto *content = new QGridLayout();
    content->setSpacing(16);

    auto *stationPanel = new QWidget(this);
    stationPanel->setObjectName("sectionPanel");
    auto *stationLayout = new QVBoxLayout(stationPanel);
    stationLayout->setContentsMargins(18, 16, 18, 18);
    stationLayout->setSpacing(12);
    auto *stationTitle = new QLabel("站点列表", stationPanel);
    stationTitle->setObjectName("sectionTitle");
    stationLayout->addWidget(stationTitle);

    stationTable_ = new QTableWidget(stationPanel);
    stationTable_->setColumnCount(7);
    stationTable_->setHorizontalHeaderLabels({"电站ID", "站名", "地址", "纬度", "经度", "电桩总数", "在线率"});
    auto *stationHeader = stationTable_->horizontalHeader();
    stationHeader->setSectionResizeMode(QHeaderView::Interactive);
    stationHeader->setSectionResizeMode(1, QHeaderView::Stretch);
    stationHeader->setSectionResizeMode(2, QHeaderView::Stretch);
    stationHeader->setStretchLastSection(false);
    stationHeader->setMinimumSectionSize(72);
    stationHeader->setDefaultAlignment(Qt::AlignCenter);
    stationTable_->verticalHeader()->setVisible(false);
    stationTable_->setFrameShape(QFrame::NoFrame);
    stationTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    stationTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    stationTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    stationTable_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    stationTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    stationTable_->setShowGrid(false);
    stationTable_->setAlternatingRowColors(true);
    stationTable_->verticalHeader()->setDefaultSectionSize(40);
    stationTable_->setColumnWidth(0, 86);
    stationTable_->setColumnWidth(1, 180);
    stationTable_->setColumnWidth(2, 240);
    stationTable_->setColumnWidth(3, 112);
    stationTable_->setColumnWidth(4, 112);
    stationTable_->setColumnWidth(5, 104);
    stationTable_->setColumnWidth(6, 90);
    stationLayout->addWidget(stationTable_, 1);
    content->addWidget(stationPanel, 0, 0, 2, 1);

    auto *formPanel = new QWidget(this);
    formPanel->setObjectName("sectionPanel");
    auto *formLayout = new QFormLayout(formPanel);
    formLayout->setContentsMargins(18, 16, 18, 18);
    formLayout->setSpacing(10);

    auto *formTitle = new QLabel("新增充电站", formPanel);
    formTitle->setObjectName("sectionTitle");
    formLayout->addRow(formTitle);

    nameEdit_ = new QLineEdit(formPanel);
    nameEdit_->setPlaceholderText("例如：大学城快充站");
    addressEdit_ = new QLineEdit(formPanel);
    addressEdit_->setPlaceholderText("详细地址");
    latSpin_ = new QDoubleSpinBox(formPanel);
    latSpin_->setRange(-90.0, 90.0);
    latSpin_->setDecimals(6);
    latSpin_->setValue(39.980000);
    lngSpin_ = new QDoubleSpinBox(formPanel);
    lngSpin_->setRange(-180.0, 180.0);
    lngSpin_->setDecimals(6);
    lngSpin_->setValue(116.320000);
    chargerCountSpin_ = new QSpinBox(formPanel);
    chargerCountSpin_->setRange(1, 50);
    chargerCountSpin_->setValue(4);
    auto *submitButton = new QPushButton("新增电站", formPanel);
    submitButton->setObjectName("primaryButton");

    formLayout->addRow("站名", nameEdit_);
    formLayout->addRow("地址", addressEdit_);
    formLayout->addRow("纬度", latSpin_);
    formLayout->addRow("经度", lngSpin_);
    formLayout->addRow("电桩数量", chargerCountSpin_);
    formLayout->addRow(submitButton);
    content->addWidget(formPanel, 0, 1);

    auto *detailPanel = new QWidget(this);
    detailPanel->setObjectName("sectionPanel");
    auto *detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(18, 16, 18, 18);
    detailLayout->setSpacing(12);
    auto *detailTitle = new QLabel("站内电桩", detailPanel);
    detailTitle->setObjectName("sectionTitle");
    detailLayout->addWidget(detailTitle);

    detailTable_ = new QTableWidget(detailPanel);
    detailTable_->setColumnCount(5);
    detailTable_->setHorizontalHeaderLabels({"电桩编号", "类型", "功率(kW)", "状态", "累计时长(h)"});
    auto *detailHeader = detailTable_->horizontalHeader();
    detailHeader->setSectionResizeMode(QHeaderView::Interactive);
    detailHeader->setSectionResizeMode(0, QHeaderView::Stretch);
    detailHeader->setSectionResizeMode(4, QHeaderView::Stretch);
    detailHeader->setStretchLastSection(false);
    detailHeader->setMinimumSectionSize(72);
    detailHeader->setDefaultAlignment(Qt::AlignCenter);
    detailTable_->verticalHeader()->setVisible(false);
    detailTable_->setFrameShape(QFrame::NoFrame);
    detailTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detailTable_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    detailTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    detailTable_->setShowGrid(false);
    detailTable_->setAlternatingRowColors(true);
    detailTable_->verticalHeader()->setDefaultSectionSize(40);
    detailTable_->setColumnWidth(0, 110);
    detailTable_->setColumnWidth(1, 72);
    detailTable_->setColumnWidth(2, 94);
    detailTable_->setColumnWidth(3, 72);
    detailTable_->setColumnWidth(4, 118);
    detailLayout->addWidget(detailTable_, 1);
    content->addWidget(detailPanel, 1, 1);
    content->setColumnStretch(0, 5);
    content->setColumnStretch(1, 3);
    content->setRowStretch(0, 1);
    content->setRowStretch(1, 1);

    layout->addLayout(content, 1);

    connect(stationTable_, &QTableWidget::currentCellChanged, this, [this](int currentRow) {
        fillDetailTable(currentRow);
    });
    connect(submitButton, &QPushButton::clicked, this, &StationsPage::submitStation);

    refresh();
}

void StationsPage::refresh()
{
    fillStationTable();
    fillDetailTable(stationTable_->currentRow());
}

void StationsPage::fillStationTable()
{
    const QList<StationInfo> rows = service_->stations();
    stationTable_->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        const StationInfo &station = rows[row];
        int online = 0;
        for (const ChargerInfo &charger : station.chargers) {
            if (charger.status != "故障") {
                ++online;
            }
        }
        const double onlineRate = station.chargers.isEmpty() ? 0.0 : online * 100.0 / station.chargers.size();
        const QStringList values = {
            QString::number(station.id),
            station.name,
            station.address,
            QString::number(station.latitude, 'f', 6),
            QString::number(station.longitude, 'f', 6),
            QString::number(station.chargers.size()),
            QString("%1%").arg(onlineRate, 0, 'f', 1),
        };
        for (int col = 0; col < values.size(); ++col) {
            auto *item = new QTableWidgetItem(values[col]);
            item->setTextAlignment(Qt::AlignCenter);
            item->setToolTip(values[col]);
            stationTable_->setItem(row, col, item);
        }
    }
    if (rows.size() > 0 && stationTable_->currentRow() < 0) {
        stationTable_->selectRow(0);
    }
}

void StationsPage::fillDetailTable(int stationRow)
{
    const QList<StationInfo> stations = service_->stations();
    if (stationRow < 0 || stationRow >= stations.size()) {
        detailTable_->setRowCount(0);
        return;
    }

    const QList<ChargerInfo> chargers = stations[stationRow].chargers;
    detailTable_->setRowCount(chargers.size());
    for (int row = 0; row < chargers.size(); ++row) {
        const ChargerInfo &charger = chargers[row];
        const QStringList values = {
            charger.id,
            charger.type,
            QString::number(charger.powerKw, 'f', 0),
            charger.status,
            QString::number(charger.totalHours, 'f', 1),
        };
        for (int col = 0; col < values.size(); ++col) {
            auto *item = new QTableWidgetItem(values[col]);
            item->setTextAlignment(Qt::AlignCenter);
            item->setToolTip(values[col]);
            detailTable_->setItem(row, col, item);
        }
    }
}

void StationsPage::submitStation()
{
    QString message;
    const bool ok = service_->addStation(nameEdit_->text(),
                                         addressEdit_->text(),
                                         latSpin_->value(),
                                         lngSpin_->value(),
                                         chargerCountSpin_->value(),
                                         &message);
    QMessageBox::information(this, ok ? "新增成功" : "新增失败", message);
    if (ok) {
        nameEdit_->clear();
        addressEdit_->clear();
        refresh();
    }
}
