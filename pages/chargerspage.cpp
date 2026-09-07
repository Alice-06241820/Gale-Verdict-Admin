#include "pages/chargerspage.h"

#include "model/adminmodels.h"
#include "service/adminapiservice.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

ChargersPage::ChargersPage(AdminApiService *service, QWidget *parent)
    : QWidget(parent)
    , service_(service)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(16);

    auto *title = new QLabel("充电桩管理", this);
    title->setObjectName("pageTitle");
    auto *subtitle = new QLabel("查看电桩运行状态，向空闲或故障设备发送重启指令", this);
    subtitle->setObjectName("pageSubtitle");
    layout->addWidget(title);
    layout->addWidget(subtitle);

    auto *panel = new QWidget(this);
    panel->setObjectName("sectionPanel");
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(18, 16, 18, 18);
    panelLayout->setSpacing(12);

    auto *panelTitle = new QLabel("电桩列表", panel);
    panelTitle->setObjectName("sectionTitle");
    panelLayout->addWidget(panelTitle);

    auto *toolbar = new QHBoxLayout();
    messageLabel_ = new QLabel("请选择一条电桩记录", panel);
    messageLabel_->setObjectName("hintText");
    auto *refreshButton = new QPushButton("刷新", panel);
    restartButton_ = new QPushButton("远程重启", panel);
    restartButton_->setObjectName("primaryButton");
    toolbar->addWidget(messageLabel_);
    toolbar->addStretch();
    toolbar->addWidget(refreshButton);
    toolbar->addWidget(restartButton_);
    panelLayout->addLayout(toolbar);

    table_ = new QTableWidget(panel);
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({"电桩编号", "所属电站", "类型", "功率(kW)", "状态", "累计次数", "累计时长(h)"});
    auto *header = table_->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(72);
    header->setDefaultAlignment(Qt::AlignCenter);
    table_->verticalHeader()->setVisible(false);
    table_->setFrameShape(QFrame::NoFrame);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setDefaultSectionSize(40);
    table_->setColumnWidth(0, 126);
    table_->setColumnWidth(1, 210);
    table_->setColumnWidth(2, 88);
    table_->setColumnWidth(3, 104);
    table_->setColumnWidth(4, 88);
    table_->setColumnWidth(5, 112);
    table_->setColumnWidth(6, 136);
    panelLayout->addWidget(table_, 1);
    layout->addWidget(panel, 1);

    connect(refreshButton, &QPushButton::clicked, this, &ChargersPage::refresh);
    connect(restartButton_, &QPushButton::clicked, this, [this]() {
        const QString id = selectedChargerId();
        if (id.isEmpty()) {
            messageLabel_->setText("请先选择需要重启的电桩");
            return;
        }
        QString message;
        const bool ok = service_->restartCharger(id, &message);
        QMessageBox::information(this, ok ? "操作成功" : "操作失败", message);
        refresh();
    });

    refresh();
}

void ChargersPage::refresh()
{
    const QList<ChargerInfo> rows = service_->chargers();
    table_->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        const ChargerInfo &charger = rows[row];
        const QStringList values = {
            charger.id,
            charger.stationName,
            charger.type,
            QString::number(charger.powerKw, 'f', 0),
            charger.status,
            QString::number(charger.totalSessions),
            QString::number(charger.totalHours, 'f', 1),
        };
        for (int col = 0; col < values.size(); ++col) {
            auto *item = new QTableWidgetItem(values[col]);
            item->setTextAlignment(Qt::AlignCenter);
            if (col == 4) {
                item->setData(Qt::UserRole, charger.status);
            }
            item->setToolTip(values[col]);
            table_->setItem(row, col, item);
        }
    }
    messageLabel_->setText(QString("共 %1 个电桩").arg(rows.size()));
}

QString ChargersPage::selectedChargerId() const
{
    const int row = table_->currentRow();
    if (row < 0 || !table_->item(row, 0)) {
        return QString();
    }
    return table_->item(row, 0)->text();
}
