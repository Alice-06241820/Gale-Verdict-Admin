#ifndef STATIONSPAGE_H
#define STATIONSPAGE_H

#include <QWidget>

class AdminApiService;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

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

    AdminApiService *service_ = nullptr;
    QTableWidget *stationTable_ = nullptr;
    QTableWidget *detailTable_ = nullptr;
    QLineEdit *nameEdit_ = nullptr;
    QLineEdit *addressEdit_ = nullptr;
    QDoubleSpinBox *latSpin_ = nullptr;
    QDoubleSpinBox *lngSpin_ = nullptr;
    QSpinBox *chargerCountSpin_ = nullptr;
};

#endif // STATIONSPAGE_H
