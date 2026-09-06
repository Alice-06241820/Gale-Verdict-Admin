#ifndef LOGINPAGE_H
#define LOGINPAGE_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

class LoginPage : public QWidget
{
    Q_OBJECT

public:
    explicit LoginPage(QWidget *parent = nullptr);

signals:
    void loginRequested(const QString &account, const QString &password);

public slots:
    void showError(const QString &message);

private:
    QLineEdit *accountEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QLabel *errorLabel_ = nullptr;
    QPushButton *loginButton_ = nullptr;
};

#endif // LOGINPAGE_H
