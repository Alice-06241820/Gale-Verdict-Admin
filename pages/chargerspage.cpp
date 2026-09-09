#include "pages/chargerspage.h"

#include "model/adminmodels.h"
#include "service/adminapiservice.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

QTableWidgetItem *centerItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    item->setToolTip(text);
    return item;
}

} // namespace

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
    table_->setColumnCount(8);
    table_->setHorizontalHeaderLabels({"电桩编号", "所属电站", "类型", "功率(kW)", "状态", "累计次数", "累计时长(h)", "操作"});
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
    table_->setColumnWidth(7, 96);
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
            QString(),
        };
        for (int col = 0; col < values.size(); ++col) {
            auto *item = centerItem(values[col]);
            if (col == 4) {
                item->setData(Qt::UserRole, charger.status);
            }
            table_->setItem(row, col, item);
        }

        auto *editButton = new QPushButton("编辑", table_);
        editButton->setObjectName("tableEditButton");
        editButton->setToolTip(charger.status == "在用" ? "电桩正在使用中，后端当前禁止修改参数" : "编辑本行电桩参数");
        connect(editButton, &QPushButton::clicked, this, [this, editButton]() {
            int targetRow = -1;
            for (int i = 0; i < table_->rowCount(); ++i) {
                if (table_->cellWidget(i, 7) == editButton) {
                    targetRow = i;
                    break;
                }
            }
            openChargerEditor(targetRow);
        });
        table_->setCellWidget(row, 7, editButton);
    }
    messageLabel_->setText(QString("共 %1 个电桩").arg(rows.size()));
    if (!rows.isEmpty() && (table_->currentRow() < 0 || table_->currentRow() >= rows.size())) {
        table_->selectRow(0);
    }
}

QString ChargersPage::selectedChargerId() const
{
    const int row = table_->currentRow();
    if (row < 0 || !table_->item(row, 0)) {
        return QString();
    }
    return table_->item(row, 0)->text();
}

void ChargersPage::openChargerEditor(int row)
{
    if (row < 0 || row >= table_->rowCount() || !table_->item(row, 0)) {
        messageLabel_->setText("请先选择需要修改的电桩");
        return;
    }

    const QString id = table_->item(row, 0)->text();
    const QString stationName = table_->item(row, 1) ? table_->item(row, 1)->text() : QString();
    const QString typeText = table_->item(row, 2) ? table_->item(row, 2)->text() : QString();
    const double powerKw = table_->item(row, 3) ? table_->item(row, 3)->text().toDouble() : 60.0;
    const QString status = table_->item(row, 4) ? table_->item(row, 4)->text() : QString();

    QDialog dialog(this);
    dialog.setObjectName("chargerEditDialog");
    dialog.setWindowTitle("编辑电桩信息");
    dialog.setModal(true);
    dialog.setMinimumWidth(380);

    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(22, 20, 22, 20);
    root->setSpacing(14);

    auto *title = new QLabel("编辑电桩信息", &dialog);
    title->setObjectName("dialogTitle");
    auto *subtitle = new QLabel(QString("电桩 %1 · %2").arg(id, stationName), &dialog);
    subtitle->setObjectName("dialogSubtitle");
    root->addWidget(title);
    root->addWidget(subtitle);

    auto *form = new QFormLayout();
    form->setSpacing(12);
    auto *statusLabel = new QLabel(status, &dialog);
    auto *typeCombo = new QComboBox(&dialog);
    typeCombo->addItems({"快充", "慢充"});
    const int typeIndex = typeCombo->findText(typeText);
    typeCombo->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
    auto *powerSpin = new QDoubleSpinBox(&dialog);
    powerSpin->setRange(1.0, 1000.0);
    powerSpin->setDecimals(1);
    powerSpin->setSuffix(" kW");
    powerSpin->setValue(powerKw > 0.0 ? powerKw : 60.0);
    form->addRow("当前状态", statusLabel);
    form->addRow("充电类型", typeCombo);
    form->addRow("额定功率", powerSpin);
    root->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("保存");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    buttons->button(QDialogButtonBox::Save)->setObjectName("primaryButton");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString message;
    const bool ok = service_->updateChargerAttributes(id,
                                                      typeCombo->currentText(),
                                                      powerSpin->value(),
                                                      &message);
    QMessageBox::information(this, ok ? "保存成功" : "保存失败", message);
    refresh();
    if (row >= 0 && row < table_->rowCount()) {
        table_->selectRow(row);
    }
}
