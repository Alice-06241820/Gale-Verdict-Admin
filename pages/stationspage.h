#ifndef STATIONSPAGE_H
#define STATIONSPAGE_H

#include "model/adminmodels.h"

#include <QWidget>

class AdminApiService;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QMenu;
class QPushButton;
class QTableWidget;
class QToolButton;

class StationsPage : public QWidget
{
    Q_OBJECT

public:
    explicit StationsPage(AdminApiService *service, QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    void fillStationTable();
    void fillDetailTable(int stationRow);
    void submitStation();
    void openChargerEditor(int row);

    // 新增充电站的桩明细：始终编辑“当前桩”，经展开菜单切换/增删。
    void syncEditorToInput();
    void applyInputToEditor(int index);
    void selectPointInput(int index);
    void addPointInput();
    void removePointInput(int index);
    void rebuildPointMenu(QMenu *menu);

    AdminApiService *service_ = nullptr;
    QTableWidget *stationTable_ = nullptr;
    QTableWidget *detailTable_ = nullptr;
    QLineEdit *nameEdit_ = nullptr;
    QDoubleSpinBox *latSpin_ = nullptr;
    QDoubleSpinBox *lngSpin_ = nullptr;

    QComboBox *pointTypeCombo_ = nullptr;
    QDoubleSpinBox *pointPowerSpin_ = nullptr;
    QToolButton *pointExpandButton_ = nullptr;
    QList<PointInput> pointInputs_;
    int currentPointIndex_ = 0;
};

#endif // STATIONSPAGE_H
