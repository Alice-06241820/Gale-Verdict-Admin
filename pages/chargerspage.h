#ifndef CHARGERSPAGE_H
#define CHARGERSPAGE_H

#include <QWidget>

class AdminApiService;
class QLabel;
class QPushButton;
class QTableWidget;

class ChargersPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChargersPage(AdminApiService *service, QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    QString selectedChargerId() const;

    AdminApiService *service_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *messageLabel_ = nullptr;
    QPushButton *restartButton_ = nullptr;
};

#endif // CHARGERSPAGE_H
