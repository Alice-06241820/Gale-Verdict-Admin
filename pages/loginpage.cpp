#include "pages/loginpage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

LoginPage::LoginPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("loginPage");

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(32, 32, 32, 32);
    outer->setAlignment(Qt::AlignCenter);

    auto *panel = new QWidget(this);
    panel->setObjectName("loginPanel");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(32, 30, 32, 30);
    layout->setSpacing(14);

    auto *title = new QLabel("Gale Verdict", panel);
    title->setObjectName("loginTitle");
    title->setAlignment(Qt::AlignCenter);
    auto *subtitle = new QLabel("运营管理端", panel);
    subtitle->setObjectName("loginSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);

    accountEdit_ = new QLineEdit(panel);
    accountEdit_->setPlaceholderText("管理员账号");
    accountEdit_->setText("admin");

    passwordEdit_ = new QLineEdit(panel);
    passwordEdit_->setPlaceholderText("密码");
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setText("123456");

    errorLabel_ = new QLabel(panel);
    errorLabel_->setObjectName("errorText");
    errorLabel_->setWordWrap(true);
    errorLabel_->hide();

    auto *fieldHint = new QLabel("账号由平台统一开通，管理端不提供自助注册入口。", panel);
    fieldHint->setObjectName("fieldHint");
    fieldHint->setWordWrap(true);

    loginButton_ = new QPushButton("登录", panel);
    loginButton_->setObjectName("primaryButton");
    loginButton_->setMinimumHeight(40);
    loginButton_->setDefault(true);

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addSpacing(4);
    layout->addWidget(accountEdit_);
    layout->addWidget(passwordEdit_);
    layout->addWidget(errorLabel_);
    layout->addWidget(fieldHint);
    layout->addSpacing(2);
    layout->addWidget(loginButton_);

    outer->addWidget(panel);

    connect(loginButton_, &QPushButton::clicked, this, [this]() {
        errorLabel_->hide();
        emit loginRequested(accountEdit_->text(), passwordEdit_->text());
    });
    connect(passwordEdit_, &QLineEdit::returnPressed, loginButton_, &QPushButton::click);
    connect(accountEdit_, &QLineEdit::returnPressed, loginButton_, &QPushButton::click);
}

void LoginPage::showError(const QString &message)
{
    errorLabel_->setText(message);
    errorLabel_->show();
}
