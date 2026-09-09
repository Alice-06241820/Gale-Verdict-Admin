#ifndef STATIONSPAGE_H
#define STATIONSPAGE_H

#include <QWidget>

class AdminApiService;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
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
    void addPointRow(const QString &type, double powerKw);

    AdminApiService *service_ = nullptr;
    QTableWidget *stationTable_ = nullptr;
    QTableWidget *detailTable_ = nullptr;
    QTableWidget *pointsEditTable_ = nullptr;
    QPushButton *addPointButton_ = nullptr;
    QPushButton *removePointButton_ = nullptr;
    QLineEdit *nameEdit_ = nullptr;
    QDoubleSpinBox *latSpin_ = nullptr;
    QDoubleSpinBox *lngSpin_ = nullptr;
};

#endif // STATIONSPAGE_H
