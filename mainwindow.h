#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "service/adminapiservice.h"

#include <QList>
#include <QMainWindow>

class DashboardPage;
class ChargersPage;
class LoginPage;
class QEvent;
class QPushButton;
class QStackedWidget;
class StationsPage;
class UsersPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    enum PageIndex {
        PageDashboard = 0,
        PageChargers,
        PageStations,
        PageUsers
    };

    QPushButton *createNavButton(const QString &text, PageIndex page);
    void showShell();
    void setPage(PageIndex page);
    void updateNavState(PageIndex page);
    // 侧边栏当前页面指示条：animate=false 时直接对齐（用于尺寸变化）。
    void updateSidebarIndicator(bool animate);
    bool eventFilter(QObject *watched, QEvent *event) override;

    AdminApiService api_;
    QWidget *loginContainer_ = nullptr;
    QWidget *shellContainer_ = nullptr;
    LoginPage *loginPage_ = nullptr;
    QWidget *sidebar_ = nullptr;
    QWidget *sidebarIndicator_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    DashboardPage *dashboardPage_ = nullptr;
    ChargersPage *chargersPage_ = nullptr;
    StationsPage *stationsPage_ = nullptr;
    UsersPage *usersPage_ = nullptr;
    QList<QPushButton *> navButtons_;
};

#endif // MAINWINDOW_H
