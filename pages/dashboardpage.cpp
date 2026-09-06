#include "pages/dashboardpage.h"

#include "service/adminapiservice.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QValueAxis>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QCursor>
#include <QDate>
#include <QDateTime>
#include <QFont>
#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMargins>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QPixmap>
#include <QTime>
#include <QToolTip>
#include <QVBoxLayout>
#include <QtMath>

DashboardPage::DashboardPage(AdminApiService *service, QWidget *parent)
    : QWidget(parent)
    , service_(service)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(18);

    auto *title = new QLabel("经营总览", this);
    title->setObjectName("pageTitle");
    auto *subtitle = new QLabel("营收、趋势和设备状态集中查看", this);
    subtitle->setObjectName("pageSubtitle");
    layout->addWidget(title);
    layout->addWidget(subtitle);

    auto *metricGrid = new QGridLayout();
    metricGrid->setSpacing(14);
    todayRevenue_ = createMetric("今日营收");
    monthRevenue_ = createMetric("本月营收");
    totalRevenue_ = createMetric("总营收");
    metricGrid->addWidget(todayRevenue_->parentWidget(), 0, 0);
    metricGrid->addWidget(monthRevenue_->parentWidget(), 0, 1);
    metricGrid->addWidget(totalRevenue_->parentWidget(), 0, 2);
    layout->addLayout(metricGrid);

    auto *analyticsRow = new QHBoxLayout();
    analyticsRow->setSpacing(16);

    auto *devicePanel = new QWidget(this);
    devicePanel->setObjectName("sectionPanel");
    devicePanel->setMinimumWidth(320);
    devicePanel->setMaximumWidth(420);
    auto *deviceLayout = new QVBoxLayout(devicePanel);
    deviceLayout->setContentsMargins(18, 16, 18, 18);
    deviceLayout->setSpacing(10);

    auto *deviceTitle = new QLabel("电桩状态分布", devicePanel);
    deviceTitle->setObjectName("sectionTitle");
    deviceChartView_ = new QChartView(devicePanel);
    deviceChartView_->setMinimumHeight(330);
    deviceChartView_->setRenderHint(QPainter::Antialiasing);
    deviceLayout->addWidget(deviceTitle);
    deviceLayout->addWidget(deviceChartView_, 1);
    analyticsRow->addWidget(devicePanel, 1);

    auto *chartPanel = new QWidget(this);
    chartPanel->setObjectName("sectionPanel");
    chartPanel->setMinimumWidth(520);
    auto *chartLayout = new QVBoxLayout(chartPanel);
    chartLayout->setContentsMargins(18, 16, 18, 18);
    chartLayout->setSpacing(10);

    auto *chartHeader = new QHBoxLayout();
    auto *chartTitle = new QLabel("营收趋势", chartPanel);
    chartTitle->setObjectName("sectionTitle");
    rangeBox_ = new QComboBox(chartPanel);
    rangeBox_->addItem("近 7 天", 7);
    rangeBox_->addItem("近 30 天", 30);
    chartHeader->addWidget(chartTitle);
    chartHeader->addStretch();
    chartHeader->addWidget(rangeBox_);
    chartLayout->addLayout(chartHeader);

    chartView_ = new QChartView(chartPanel);
    chartView_->setMinimumHeight(280);
    chartView_->setRenderHint(QPainter::Antialiasing);
    chartLayout->addWidget(chartView_);

    chartHint_ = new QLabel("提示：鼠标移到折线点上可查看当天营收，点击点可固定查看数值。", chartPanel);
    chartHint_->setObjectName("hintText");
    chartLayout->addWidget(chartHint_);
    analyticsRow->addWidget(chartPanel, 2);
    layout->addLayout(analyticsRow, 1);

    connect(rangeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        updateChart(rangeBox_->currentData().toInt());
    });

    refresh();
}

void DashboardPage::refresh()
{
    const RevenueSummary summary = service_->revenueSummary();
    todayRevenue_->setText(QString("%1 元").arg(summary.today, 0, 'f', 2));
    monthRevenue_->setText(QString("%1 元").arg(summary.month, 0, 'f', 2));
    totalRevenue_->setText(QString("%1 元").arg(summary.total, 0, 'f', 2));
    updateChart(rangeBox_->currentData().toInt());
    updateDeviceSummary();
}

QLabel *DashboardPage::createMetric(const QString &title)
{
    auto *panel = new QWidget(this);
    panel->setObjectName("metricCard");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(6);

    auto *label = new QLabel(title, panel);
    label->setObjectName("metricLabel");
    auto *value = new QLabel("0", panel);
    value->setObjectName("metricValue");
    layout->addWidget(label);
    layout->addWidget(value);
    return value;
}

void DashboardPage::updateChart(int days)
{
    auto *curve = new QSplineSeries();
    curve->setName("已完成订单营收");
    curve->setPen(QPen(QColor("#4b4944"), 2));

    auto *pointsSeries = new QScatterSeries();
    pointsSeries->setName("每日营收");
    pointsSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    pointsSeries->setMarkerSize(8.5);
    pointsSeries->setColor(QColor("#171511"));
    pointsSeries->setBorderColor(QColor("#171511"));

    auto *selectedVerticalLine = new QLineSeries();
    selectedVerticalLine->setName("选中点竖向指示线");
    QPen guidePen(QColor("#c85a46"), 1);
    guidePen.setStyle(Qt::DashLine);
    selectedVerticalLine->setPen(guidePen);

    auto *selectedHorizontalLine = new QLineSeries();
    selectedHorizontalLine->setName("选中点横向指示线");
    selectedHorizontalLine->setPen(guidePen);

    auto *selectedSeries = new QScatterSeries();
    selectedSeries->setName("选中点");
    selectedSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    selectedSeries->setMarkerSize(12);
    selectedSeries->setColor(QColor("#d84b35"));
    selectedSeries->setBorderColor(QColor("#a63325"));
    selectedSeries->setPointLabelsVisible(false);

    const QList<RevenuePoint> points = service_->revenueTrend(days);
    auto xValueForDate = [](const QDate &date) {
        return static_cast<qreal>(QDateTime(date, QTime(0, 0)).toMSecsSinceEpoch());
    };

    double maxValue = 0.0;
    for (int i = 0; i < points.size(); ++i) {
        const qreal x = xValueForDate(points[i].date);
        curve->append(x, points[i].amount);
        pointsSeries->append(x, points[i].amount);
        maxValue = qMax(maxValue, points[i].amount);
    }
    if (!points.isEmpty()) {
        if (selectedRevenueIndex_ < 0 || selectedRevenueIndex_ >= points.size()) {
            selectedRevenueIndex_ = qMin(4, points.size() - 1);
        }
        const double selectedX = xValueForDate(points[selectedRevenueIndex_].date);
        const double selectedY = points[selectedRevenueIndex_].amount;
        selectedVerticalLine->append(selectedX, 0.0);
        selectedVerticalLine->append(selectedX, selectedY);
        selectedHorizontalLine->append(xValueForDate(points.first().date), selectedY);
        selectedHorizontalLine->append(xValueForDate(points.last().date), selectedY);
        selectedSeries->append(selectedX, selectedY);
    }

    auto *chart = new QChart();
    chart->addSeries(selectedHorizontalLine);
    chart->addSeries(selectedVerticalLine);
    chart->addSeries(curve);
    chart->addSeries(pointsSeries);
    chart->addSeries(selectedSeries);
    chart->legend()->setVisible(false);
    chart->setBackgroundVisible(false);
    chart->setPlotAreaBackgroundVisible(true);
    chart->setPlotAreaBackgroundBrush(QBrush(QColor("#f6f2ec")));
    chart->setTitle("");
    chart->setMargins(QMargins(2, 8, 10, 4));

    auto *axisX = new QDateTimeAxis();
    if (points.isEmpty()) {
        const QDate today = QDate::currentDate();
        axisX->setRange(QDateTime(today, QTime(0, 0)), QDateTime(today.addDays(1), QTime(0, 0)));
    } else {
        axisX->setRange(QDateTime(points.first().date, QTime(0, 0)), QDateTime(points.last().date, QTime(0, 0)));
    }
    axisX->setFormat("MM-dd");
    axisX->setTickCount(points.size() <= 7 ? qMax(2, points.size()) : 8);
    axisX->setTitleText("");
    axisX->setGridLineVisible(false);

    auto *axisY = new QValueAxis();
    const int tickStep = 100;
    const int axisMax = qMax(tickStep, static_cast<int>(qCeil((maxValue + 60.0) / tickStep)) * tickStep);
    axisY->setRange(0, axisMax);
    axisY->setTickCount(axisMax / tickStep + 1);
    axisY->setLabelFormat("%.0f");
    axisY->setTitleText("");
    axisY->setGridLineColor(QColor("#e2ddd6"));

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    selectedHorizontalLine->attachAxis(axisX);
    selectedHorizontalLine->attachAxis(axisY);
    selectedVerticalLine->attachAxis(axisX);
    selectedVerticalLine->attachAxis(axisY);
    curve->attachAxis(axisX);
    curve->attachAxis(axisY);
    pointsSeries->attachAxis(axisX);
    pointsSeries->attachAxis(axisY);
    selectedSeries->attachAxis(axisX);
    selectedSeries->attachAxis(axisY);
    chartView_->setChart(chart);

    if (!points.isEmpty()) {
        auto *badgeBox = new QGraphicsPathItem(chart);
        badgeBox->setBrush(QBrush(QColor("#171511")));
        badgeBox->setPen(QPen(QColor("#171511")));
        badgeBox->setZValue(20);

        auto *badgeText = new QGraphicsSimpleTextItem(
            QString("%1 元").arg(points[selectedRevenueIndex_].amount, 0, 'f', 0), chart);
        badgeText->setBrush(QBrush(QColor("#fffaf4")));
        badgeText->setFont(QFont("Segoe UI", 10, QFont::DemiBold));
        badgeText->setZValue(21);

        auto updateBadge = [=]() {
            const QPointF pointPos = chart->mapToPosition(
                QPointF(xValueForDate(points[selectedRevenueIndex_].date), points[selectedRevenueIndex_].amount),
                selectedSeries);
            const QRectF textRect = badgeText->boundingRect();
            const double width = textRect.width() + 22.0;
            const double height = 24.0;
            const QRectF box(pointPos.x() - width / 2.0, pointPos.y() - 44.0, width, height);
            QPainterPath path;
            path.addRoundedRect(box, height / 2.0, height / 2.0);
            badgeBox->setPath(path);
            badgeText->setPos(box.x() + 11.0, box.y() + 3.0);
        };

        updateBadge();
        connect(chart, &QChart::plotAreaChanged, this, updateBadge);
    }

    auto indexForPoint = [points, xValueForDate](const QPointF &point) {
        int bestIndex = -1;
        qreal bestDistance = 1.0e30;
        for (int i = 0; i < points.size(); ++i) {
            const qreal distance = qAbs(point.x() - xValueForDate(points[i].date));
            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex = i;
            }
        }
        return bestIndex;
    };

    connect(pointsSeries, &QScatterSeries::hovered, this, [this, points, indexForPoint](const QPointF &point, bool state) {
        if (!state) {
            chartHint_->setText("提示：鼠标移到折线点上可查看当天营收，点击点可固定查看数值。");
            return;
        }

        const int index = indexForPoint(point);
        if (index < 0 || index >= points.size()) {
            return;
        }
        const QString text = QString("%1：%2 元")
                                 .arg(points[index].date.toString("yyyy-MM-dd"))
                                 .arg(points[index].amount, 0, 'f', 2);
        chartHint_->setText(text);
        QToolTip::showText(QCursor::pos(), text, chartView_);
    });

    connect(pointsSeries, &QScatterSeries::clicked, this, [this, points, indexForPoint](const QPointF &point) {
        const int index = indexForPoint(point);
        if (index < 0 || index >= points.size()) {
            return;
        }
        selectedRevenueIndex_ = index;
        updateChart(rangeBox_->currentData().toInt());
        chartHint_->setText(QString("已选中 %1，营收 %2 元")
                                .arg(points[index].date.toString("yyyy-MM-dd"))
                                .arg(points[index].amount, 0, 'f', 2));
    });

    connect(selectedSeries, &QScatterSeries::hovered, this, [this, points, indexForPoint](const QPointF &point, bool state) {
        if (!state) {
            chartHint_->setText("提示：鼠标移到折线点上可查看当天营收，点击点可固定查看数值。");
            return;
        }

        const int index = indexForPoint(point);
        if (index < 0 || index >= points.size()) {
            return;
        }
        const QString text = QString("选中点 %1：%2 元")
                                 .arg(points[index].date.toString("yyyy-MM-dd"))
                                 .arg(points[index].amount, 0, 'f', 2);
        chartHint_->setText(text);
        QToolTip::showText(QCursor::pos(), text, chartView_);
    });

    connect(selectedSeries, &QScatterSeries::clicked, this, [this, points, indexForPoint](const QPointF &point) {
        const int index = indexForPoint(point);
        if (index < 0 || index >= points.size()) {
            return;
        }
        selectedRevenueIndex_ = index;
        updateChart(rangeBox_->currentData().toInt());
        chartHint_->setText(QString("已选中 %1，营收 %2 元")
                                .arg(points[index].date.toString("yyyy-MM-dd"))
                                .arg(points[index].amount, 0, 'f', 2));
    });
}

void DashboardPage::updateDeviceSummary()
{
    const DeviceStatusSummary summary = service_->deviceStatusSummary();
    auto *series = new QPieSeries();
    series->setHoleSize(0.42);
    series->setPieSize(0.92);
    series->setPieStartAngle(90);
    series->setPieEndAngle(450);

    QPixmap stripePixmap(12, 12);
    stripePixmap.fill(QColor("#d7d2cc"));
    {
        QPainter stripePainter(&stripePixmap);
        stripePainter.setRenderHint(QPainter::Antialiasing);
        stripePainter.setPen(QPen(QColor("#fbfaf7"), 3));
        stripePainter.drawLine(-2, 12, 12, -2);
        stripePainter.drawLine(4, 14, 14, 4);
    }

    auto addSlice = [series](const QString &name, int count, const QBrush &brush) {
        if (count <= 0) {
            return;
        }
        auto *slice = series->append(name, count);
        slice->setLabel(name);
        slice->setLabelVisible(false);
        slice->setLabelColor(QColor("#1f1d1a"));
        slice->setBrush(brush);
        slice->setBorderColor(QColor("#fbfaf7"));
        slice->setBorderWidth(2);
    };

    addSlice("空闲", summary.idleCount, QBrush(QColor("#f7d84a")));
    addSlice("在用", summary.usingCount, QBrush(QColor("#d7d2cc")));
    addSlice("故障", summary.faultCount, QBrush(stripePixmap));

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->setBackgroundVisible(false);
    chart->setTitle("");
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setLabelColor(QColor("#1f1d1a"));
    chart->legend()->setMarkerShape(QLegend::MarkerShapeCircle);
    chart->setAnimationOptions(QChart::NoAnimation);
    deviceChartView_->setChart(chart);
}
