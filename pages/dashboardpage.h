#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include "model/adminmodels.h"

#include <QWidget>

class AdminApiService;
class QLabel;
class QComboBox;
class QChartView;

class DashboardPage : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPage(AdminApiService *service, QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    QLabel *createMetric(const QString &title);
    void updateChart(int days);
    void updateDeviceSummary();

    AdminApiService *service_ = nullptr;
    QLabel *todayRevenue_ = nullptr;
    QLabel *monthRevenue_ = nullptr;
    QLabel *totalRevenue_ = nullptr;
    QComboBox *rangeBox_ = nullptr;
    QChartView *chartView_ = nullptr;
    QChartView *deviceChartView_ = nullptr;
    QLabel *chartHint_ = nullptr;
    int selectedRevenueIndex_ = -1;
};

#endif // DASHBOARDPAGE_H
