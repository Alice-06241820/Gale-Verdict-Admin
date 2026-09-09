#include "pages/stationspage.h"

#include "model/adminmodels.h"
#include "service/adminapiservice.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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
    formLayout->setContentsMargins(16, 12, 16, 12);
    formLayout->setSpacing(8);

    auto *formTitle = new QLabel("新增充电站", formPanel);
    formTitle->setObjectName("sectionTitle");
    formLayout->addRow(formTitle);

    nameEdit_ = new QLineEdit(formPanel);
    nameEdit_->setPlaceholderText("例如：大学城快充站");
    latSpin_ = new QDoubleSpinBox(formPanel);
    latSpin_->setRange(-90.0, 90.0);
    latSpin_->setDecimals(6);
    latSpin_->setValue(39.980000);
    lngSpin_ = new QDoubleSpinBox(formPanel);
    lngSpin_->setRange(-180.0, 180.0);
    lngSpin_->setDecimals(6);
    lngSpin_->setValue(116.320000);
    auto *submitButton = new QPushButton("新增电站", formPanel);
    submitButton->setObjectName("primaryButton");

    pointsEditTable_ = new QTableWidget(formPanel);
    pointsEditTable_->setColumnCount(2);
    pointsEditTable_->setHorizontalHeaderLabels({"类型", "功率(kW)"});
    auto *pointsHeader = pointsEditTable_->horizontalHeader();
    pointsHeader->setSectionResizeMode(0, QHeaderView::Stretch);
    pointsHeader->setSectionResizeMode(1, QHeaderView::Interactive);
    pointsHeader->setStretchLastSection(false);
    pointsHeader->setMinimumSectionSize(72);
    pointsHeader->setDefaultAlignment(Qt::AlignCenter);
    pointsEditTable_->setColumnWidth(1, 100);
    pointsEditTable_->verticalHeader()->setVisible(false);
    pointsEditTable_->verticalHeader()->setDefaultSectionSize(30);
    pointsEditTable_->setFrameShape(QFrame::NoFrame);
    pointsEditTable_->setShowGrid(false);
    pointsEditTable_->setAlternatingRowColors(true);
    pointsEditTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    pointsEditTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    pointsEditTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    updatePointsTableHeight();

    addPointButton_ = new QPushButton("添加电桩", formPanel);
    removePointButton_ = new QPushButton("移除选中", formPanel);
    auto *pointButtonsLayout = new QHBoxLayout();
    pointButtonsLayout->setContentsMargins(0, 0, 0, 0);
    pointButtonsLayout->addWidget(addPointButton_);
    pointButtonsLayout->addWidget(removePointButton_);
    pointButtonsLayout->addStretch(1);

    auto *pointsBox = new QWidget(formPanel);
    auto *pointsLayout = new QVBoxLayout(pointsBox);
    pointsLayout->setContentsMargins(0, 0, 0, 0);
    pointsLayout->setSpacing(4);
    pointsLayout->addWidget(pointsEditTable_);
    pointsLayout->addLayout(pointButtonsLayout);

    addPointRow("DC", 60.0);
    connect(addPointButton_, &QPushButton::clicked, this, [this] {
        if (pointsEditTable_->rowCount() >= 50) {
            return;
        }
        addPointRow("DC", 60.0);
    });
    connect(removePointButton_, &QPushButton::clicked, this, [this] {
        if (pointsEditTable_->rowCount() <= 1) {
            return;
        }
        const int row = pointsEditTable_->currentRow();
        pointsEditTable_->removeRow(row >= 0 ? row
                                             : pointsEditTable_->rowCount() - 1);
        updatePointsTableHeight();
    });

    formLayout->addRow("站名", nameEdit_);
    formLayout->addRow("纬度", latSpin_);
    formLayout->addRow("经度", lngSpin_);
    formLayout->addRow("电桩明细", pointsBox);
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
    detailTable_->setColumnCount(6);
    detailTable_->setHorizontalHeaderLabels({"电桩编号", "类型", "功率(kW)", "状态", "累计时长(h)", "操作"});
    auto *detailHeader = detailTable_->horizontalHeader();
    detailHeader->setSectionResizeMode(QHeaderView::Interactive);
    detailHeader->setSectionResizeMode(0, QHeaderView::Stretch);
    detailHeader->setSectionResizeMode(4, QHeaderView::Stretch);
    detailHeader->setStretchLastSection(false);
    detailHeader->setMinimumSectionSize(72);
    detailHeader->setDefaultAlignment(Qt::AlignCenter);
    detailTable_->verticalHeader()->setVisible(false);
    detailTable_->setFrameShape(QFrame::NoFrame);
    detailTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    detailTable_->setSelectionMode(QAbstractItemView::SingleSelection);
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
    detailTable_->setColumnWidth(5, 86);
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
            stationTable_->setItem(row, col, centerItem(values[col]));
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
            QString(),
        };
        for (int col = 0; col < values.size(); ++col) {
            detailTable_->setItem(row, col, centerItem(values[col]));
        }

        auto *editButton = new QPushButton("编辑", detailTable_);
        editButton->setObjectName("tableEditButton");
        editButton->setToolTip(charger.status == "在用" ? "电桩正在使用中，后端当前禁止修改参数" : "编辑本行电桩参数");
        connect(editButton, &QPushButton::clicked, this, [this, editButton]() {
            int targetRow = -1;
            for (int i = 0; i < detailTable_->rowCount(); ++i) {
                if (detailTable_->cellWidget(i, 5) == editButton) {
                    targetRow = i;
                    break;
                }
            }
            openChargerEditor(targetRow);
        });
        detailTable_->setCellWidget(row, 5, editButton);
    }

    if (!chargers.isEmpty() && (detailTable_->currentRow() < 0 || detailTable_->currentRow() >= chargers.size())) {
        detailTable_->selectRow(0);
    }
}

void StationsPage::addPointRow(const QString &type, double powerKw)
{
    const int row = pointsEditTable_->rowCount();
    pointsEditTable_->insertRow(row);

    auto *typeBox = new QComboBox(pointsEditTable_);
    typeBox->addItem("直流快充", "DC");
    typeBox->addItem("交流慢充", "AC");
    const int typeIndex = typeBox->findData(type);
    typeBox->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
    pointsEditTable_->setCellWidget(row, 0, typeBox);

    auto *powerSpin = new QDoubleSpinBox(pointsEditTable_);
    powerSpin->setRange(0.1, 2000.0);
    powerSpin->setDecimals(1);
    powerSpin->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    powerSpin->setValue(powerKw);
    pointsEditTable_->setCellWidget(row, 1, powerSpin);

    updatePointsTableHeight();
}

void StationsPage::updatePointsTableHeight()
{
    constexpr int kMaxVisibleRows = 5;
    const int rows = pointsEditTable_->rowCount();
    const int visibleRows = qMin(rows, kMaxVisibleRows);
    const int headerHeight =
        pointsEditTable_->horizontalHeader()->sizeHint().height();
    const int rowHeight =
        pointsEditTable_->verticalHeader()->defaultSectionSize();
    pointsEditTable_->setFixedHeight(headerHeight + visibleRows * rowHeight + 4);
}

void StationsPage::submitStation()
{
    QList<PointInput> points;
    const int rows = pointsEditTable_->rowCount();
    for (int row = 0; row < rows; ++row) {
        PointInput input;
        if (auto *typeBox = qobject_cast<QComboBox *>(
                pointsEditTable_->cellWidget(row, 0))) {
            input.type = typeBox->currentData().toString();
        }
        if (auto *powerSpin = qobject_cast<QDoubleSpinBox *>(
                pointsEditTable_->cellWidget(row, 1))) {
            input.powerKw = powerSpin->value();
        }
        points.append(input);
    }

    QString message;
    const bool ok = service_->addStation(nameEdit_->text(),
                                         latSpin_->value(),
                                         lngSpin_->value(),
                                         points,
                                         &message);
    QMessageBox::information(this, ok ? "新增成功" : "新增失败", message);
    if (ok) {
        nameEdit_->clear();
        pointsEditTable_->setRowCount(0);
        addPointRow("DC", 60.0);
        refresh();
    }
}

void StationsPage::openChargerEditor(int row)
{
    const int stationRow = stationTable_->currentRow();
    if (row < 0 || row >= detailTable_->rowCount() || !detailTable_->item(row, 0)) {
        QMessageBox::warning(this, "保存失败", "请先选择需要修改的电桩");
        return;
    }

    const QString chargerId = detailTable_->item(row, 0)->text();
    const QString stationName = stationRow >= 0 && stationTable_->item(stationRow, 1)
                                    ? stationTable_->item(stationRow, 1)->text()
                                    : QString();
    const QString typeText = detailTable_->item(row, 1) ? detailTable_->item(row, 1)->text() : QString();
    const double powerKw = detailTable_->item(row, 2) ? detailTable_->item(row, 2)->text().toDouble() : 60.0;
    const QString status = detailTable_->item(row, 3) ? detailTable_->item(row, 3)->text() : QString();

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
    auto *subtitle = new QLabel(QString("电桩 %1 · %2").arg(chargerId, stationName), &dialog);
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
    const bool ok = service_->updateChargerAttributes(chargerId,
                                                      typeCombo->currentText(),
                                                      powerSpin->value(),
                                                      &message);
    QMessageBox::information(this, ok ? "保存成功" : "保存失败", message);
    if (ok) {
        refresh();
        if (stationRow >= 0 && stationRow < stationTable_->rowCount()) {
            stationTable_->selectRow(stationRow);
            fillDetailTable(stationRow);
        }
        if (row >= 0 && row < detailTable_->rowCount()) {
            detailTable_->selectRow(row);
        }
    }
}
