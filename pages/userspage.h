#ifndef USERSPAGE_H
#define USERSPAGE_H

#include <QWidget>

class AdminApiService;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

class UsersPage : public QWidget
{
    Q_OBJECT

public:
    explicit UsersPage(AdminApiService *service, QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    int selectedUserId() const;
    void setSelectedFrozen(bool frozen);

    AdminApiService *service_ = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *messageLabel_ = nullptr;
};

#endif // USERSPAGE_H
