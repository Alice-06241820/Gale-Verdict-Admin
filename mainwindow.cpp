#include "mainwindow.h"

#include "pages/chargerspage.h"
#include "pages/dashboardpage.h"
#include "pages/loginpage.h"
#include "pages/stationspage.h"
#include "pages/userspage.h"
#include "ui/motion.h"

#include <QAbstractAnimation>
#include <QApplication>
#include <QEasingCurve>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Gale Verdict 运营管理端");
    resize(1260, 820);
    setMinimumSize(1040, 700);

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
    button->setMinimumHeight(42);
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

    sidebar_ = new QWidget(shellContainer_);
    sidebar_->setObjectName("sidebar");
    sidebar_->setFixedWidth(240);
    auto *sideLayout = new QVBoxLayout(sidebar_);
    sideLayout->setContentsMargins(20, 24, 20, 20);
    sideLayout->setSpacing(8);

    auto *brand = new QLabel("Gale Verdict", sidebar_);
    brand->setObjectName("brandTitle");
    auto *role = new QLabel("运营管理端", sidebar_);
    role->setObjectName("brandSubtitle");
    sideLayout->addWidget(brand);
    sideLayout->addWidget(role);
    sideLayout->addSpacing(20);
    sideLayout->addWidget(createNavButton("经营总览", PageDashboard));
    sideLayout->addWidget(createNavButton("充电桩管理", PageChargers));
    sideLayout->addWidget(createNavButton("充电站管理", PageStations));
    sideLayout->addWidget(createNavButton("用户管理", PageUsers));
    sideLayout->addStretch();

    auto *logoutButton = new QPushButton("退出登录", sidebar_);
    logoutButton->setObjectName("secondaryButton");
    sideLayout->addWidget(logoutButton);

    stack_ = new QStackedWidget(shellContainer_);
    stack_->setObjectName("contentStack");
    dashboardPage_ = new DashboardPage(&api_, stack_);
    chargersPage_ = new ChargersPage(&api_, stack_);
    stationsPage_ = new StationsPage(&api_, stack_);
    usersPage_ = new UsersPage(&api_, stack_);
    stack_->addWidget(static_cast<QWidget *>(dashboardPage_));
    stack_->addWidget(static_cast<QWidget *>(chargersPage_));
    stack_->addWidget(static_cast<QWidget *>(stationsPage_));
    stack_->addWidget(static_cast<QWidget *>(usersPage_));

    // 侧边栏：当前页面指示条（竖直滑动条，与客户端底部导航指示条同一套动效语言）。
    sidebarIndicator_ = new QWidget(sidebar_);
    sidebarIndicator_->setObjectName("sidebarIndicator");
    sidebarIndicator_->setAttribute(Qt::WA_StyledBackground, true);
    sidebarIndicator_->hide();
    sidebar_->installEventFilter(this);

    root->addWidget(sidebar_);
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

    QWidget *pageWidget = stack_->widget(page);
    stack_->setCurrentIndex(page);
    updateNavState(page);
    // 页面切换统一淡入；「经营总览」含 QChartView，跳过以免图表离屏渲染异常。
    if (pageWidget != nullptr
        && pageWidget != static_cast<QWidget *>(dashboardPage_)) {
        Motion::fadeIn(pageWidget, Motion::kNormalMs);
    }

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
    updateSidebarIndicator(true);
}

void MainWindow::updateSidebarIndicator(bool animate)
{
    if (!sidebarIndicator_ || !sidebar_) {
        return;
    }
    QPushButton *current = nullptr;
    for (QPushButton *button : navButtons_) {
        if (button->isChecked()) {
            current = button;
            break;
        }
    }
    if (!current || !sidebar_->isVisible()) {
        sidebarIndicator_->hide();
        return;
    }

    const QRect buttonRect = current->geometry();
    const QRect target(8, buttonRect.top() + 8, 3,
                       qMax(18, buttonRect.height() - 16));

    // 首次出现时直接对齐，避免从 (0,0) 滑入的突兀感。
    const bool firstAppearance = !sidebarIndicator_->isVisible();
    sidebarIndicator_->show();
    sidebarIndicator_->raise();
    if (!animate || firstAppearance) {
        sidebarIndicator_->setGeometry(target);
        return;
    }
    auto *animation =
        new QPropertyAnimation(sidebarIndicator_, "geometry", this);
    animation->setDuration(Motion::kNormalMs);
    animation->setStartValue(sidebarIndicator_->geometry());
    animation->setEndValue(target);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == sidebar_ && event->type() == QEvent::Resize) {
        updateSidebarIndicator(false);
    }
    return QMainWindow::eventFilter(watched, event);
}
