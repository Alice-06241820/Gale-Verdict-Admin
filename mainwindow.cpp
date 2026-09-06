#include "mainwindow.h"

#include "pages/chargerspage.h"
#include "pages/dashboardpage.h"
#include "pages/loginpage.h"
#include "pages/stationspage.h"
#include "pages/userspage.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Gale Verdict 运营管理端");
    resize(1180, 760);
    setMinimumSize(980, 640);

    loginPage_ = new LoginPage(this);
    loginContainer_ = loginPage_;
    setCentralWidget(loginContainer_);

    connect(loginPage_, &LoginPage::loginRequested, this, [this](const QString &account, const QString &password) {
        QString error;
        if (api_.login(account, password, &error)) {
            showShell();
        } else {
            loginPage_->showError(error);
        }
    });
}

QPushButton *MainWindow::createNavButton(const QString &text, PageIndex page)
{
    auto *button = new QPushButton(text, this);
    button->setObjectName("navButton");
    button->setCheckable(true);
    button->setMinimumHeight(38);
    navButtons_.append(button);
    connect(button, &QPushButton::clicked, this, [this, page]() {
        setPage(page);
    });
    return button;
}

void MainWindow::showShell()
{
    shellContainer_ = new QWidget(this);
    shellContainer_->setObjectName("shell");

    auto *root = new QHBoxLayout(shellContainer_);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *sidebar = new QWidget(shellContainer_);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(220);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(18, 22, 18, 18);
    sideLayout->setSpacing(10);

    auto *brand = new QLabel("Gale Verdict", sidebar);
    brand->setObjectName("brandTitle");
    auto *role = new QLabel("运营管理端", sidebar);
    role->setObjectName("brandSubtitle");
    sideLayout->addWidget(brand);
    sideLayout->addWidget(role);
    sideLayout->addSpacing(20);
    sideLayout->addWidget(createNavButton("经营总览", PageDashboard));
    sideLayout->addWidget(createNavButton("充电桩管理", PageChargers));
    sideLayout->addWidget(createNavButton("充电站管理", PageStations));
    sideLayout->addWidget(createNavButton("用户管理", PageUsers));
    sideLayout->addStretch();

    auto *logoutButton = new QPushButton("退出登录", sidebar);
    logoutButton->setObjectName("secondaryButton");
    sideLayout->addWidget(logoutButton);

    stack_ = new QStackedWidget(shellContainer_);
    stack_->setObjectName("contentStack");
    dashboardPage_ = new DashboardPage(&api_, stack_);
    chargersPage_ = new ChargersPage(&api_, stack_);
    stationsPage_ = new StationsPage(&api_, stack_);
    usersPage_ = new UsersPage(&api_, stack_);
    stack_->addWidget(dashboardPage_);
    stack_->addWidget(chargersPage_);
    stack_->addWidget(stationsPage_);
    stack_->addWidget(usersPage_);

    root->addWidget(sidebar);
    root->addWidget(stack_, 1);

    setCentralWidget(shellContainer_);
    setPage(PageDashboard);

    connect(logoutButton, &QPushButton::clicked, this, [this]() {
        navButtons_.clear();
        loginPage_ = new LoginPage(this);
        connect(loginPage_, &LoginPage::loginRequested, this, [this](const QString &account, const QString &password) {
            QString error;
            if (api_.login(account, password, &error)) {
                showShell();
            } else {
                loginPage_->showError(error);
            }
        });
        setCentralWidget(loginPage_);
    });
}

void MainWindow::setPage(PageIndex page)
{
    if (!stack_) {
        return;
    }

    stack_->setCurrentIndex(page);
    updateNavState(page);

    switch (page) {
    case PageDashboard:
        dashboardPage_->refresh();
        break;
    case PageChargers:
        chargersPage_->refresh();
        break;
    case PageStations:
        stationsPage_->refresh();
        break;
    case PageUsers:
        usersPage_->refresh();
        break;
    }
}

void MainWindow::updateNavState(PageIndex page)
{
    for (int i = 0; i < navButtons_.size(); ++i) {
        navButtons_[i]->setChecked(i == page);
    }
}
