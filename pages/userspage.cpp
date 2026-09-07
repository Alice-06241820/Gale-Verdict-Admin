#include "pages/userspage.h"

#include "model/adminmodels.h"
#include "service/adminapiservice.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

UsersPage::UsersPage(AdminApiService *service, QWidget *parent)
    : QWidget(parent)
    , service_(service)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(16);

    auto *title = new QLabel("用户管理", this);
    title->setObjectName("pageTitle");
    auto *subtitle = new QLabel("按手机号搜索用户，处理冻结与解冻操作", this);
    subtitle->setObjectName("pageSubtitle");
    layout->addWidget(title);
    layout->addWidget(subtitle);

    auto *panel = new QWidget(this);
    panel->setObjectName("sectionPanel");
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(18, 16, 18, 18);
    panelLayout->setSpacing(12);

    auto *panelTitle = new QLabel("用户列表", panel);
    panelTitle->setObjectName("sectionTitle");
    panelLayout->addWidget(panelTitle);

    auto *toolbar = new QHBoxLayout();
    searchEdit_ = new QLineEdit(panel);
    searchEdit_->setPlaceholderText("输入手机号关键字");
    auto *searchButton = new QPushButton("搜索", panel);
    auto *resetButton = new QPushButton("重置", panel);
    auto *freezeButton = new QPushButton("冻结", panel);
    auto *unfreezeButton = new QPushButton("解冻", panel);
    freezeButton->setObjectName("dangerButton");
    unfreezeButton->setObjectName("primaryButton");
    toolbar->addWidget(searchEdit_, 1);
    toolbar->addWidget(searchButton);
    toolbar->addWidget(resetButton);
    toolbar->addSpacing(12);
    toolbar->addWidget(freezeButton);
    toolbar->addWidget(unfreezeButton);
    panelLayout->addLayout(toolbar);

    messageLabel_ = new QLabel(panel);
    messageLabel_->setObjectName("hintText");
    panelLayout->addWidget(messageLabel_);

    table_ = new QTableWidget(panel);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({"用户ID", "手机号", "昵称", "余额", "注册时间", "状态"});
    auto *header = table_->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::Stretch);
    header->setSectionResizeMode(4, QHeaderView::Stretch);
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
    table_->setColumnWidth(0, 90);
    table_->setColumnWidth(1, 160);
    table_->setColumnWidth(2, 150);
    table_->setColumnWidth(3, 116);
    table_->setColumnWidth(4, 180);
    table_->setColumnWidth(5, 92);
    panelLayout->addWidget(table_, 1);
    layout->addWidget(panel, 1);

    connect(searchButton, &QPushButton::clicked, this, &UsersPage::refresh);
    connect(searchEdit_, &QLineEdit::returnPressed, this, &UsersPage::refresh);
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        searchEdit_->clear();
        refresh();
    });
    connect(freezeButton, &QPushButton::clicked, this, [this]() {
        setSelectedFrozen(true);
    });
    connect(unfreezeButton, &QPushButton::clicked, this, [this]() {
        setSelectedFrozen(false);
    });

    refresh();
}

void UsersPage::refresh()
{
    const QList<UserInfo> rows = service_->users(searchEdit_->text());
    table_->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        const UserInfo &user = rows[row];
        const QStringList values = {
            QString::number(user.id),
            user.phone,
            user.nickname,
            QString("%1 元").arg(user.balance, 0, 'f', 2),
            user.registeredAt.toString("yyyy-MM-dd HH:mm"),
            user.status,
        };
        for (int col = 0; col < values.size(); ++col) {
            auto *item = new QTableWidgetItem(values[col]);
            item->setTextAlignment(Qt::AlignCenter);
            item->setToolTip(values[col]);
            table_->setItem(row, col, item);
        }
    }
    messageLabel_->setText(QString("当前显示 %1 个用户").arg(rows.size()));
}

int UsersPage::selectedUserId() const
{
    const int row = table_->currentRow();
    if (row < 0 || !table_->item(row, 0)) {
        return -1;
    }
    return table_->item(row, 0)->text().toInt();
}

void UsersPage::setSelectedFrozen(bool frozen)
{
    const int userId = selectedUserId();
    if (userId < 0) {
        messageLabel_->setText("请先选择用户");
        return;
    }

    QString message;
    const bool ok = service_->setUserFrozen(userId, frozen, &message);
    QMessageBox::information(this, ok ? "操作成功" : "操作失败", message);
    refresh();
}
