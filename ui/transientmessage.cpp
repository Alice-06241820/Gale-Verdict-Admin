#include "ui/transientmessage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

namespace {
QFrame *createToast(QWidget *parent, const QString &title,
                    const QString &message, bool warning)
{
    // 提示覆盖在当前页面底部，不创建会锁住主窗口的系统对话框。
    auto *toast = new QFrame(parent);
    toast->setObjectName(QStringLiteral("transientToast"));
    toast->setProperty("messageType", warning ? QStringLiteral("warning")
                                               : QStringLiteral("information"));
    toast->setAttribute(Qt::WA_DeleteOnClose);

    auto *layout = new QVBoxLayout(toast);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(3);

    auto *titleLabel = new QLabel(title, toast);
    titleLabel->setObjectName(QStringLiteral("toastTitle"));
    layout->addWidget(titleLabel);

    auto *messageLabel = new QLabel(message, toast);
    messageLabel->setObjectName(QStringLiteral("toastMessage"));
    messageLabel->setWordWrap(true);
    layout->addWidget(messageLabel);

    const int availableWidth = parent ? parent->width() - 32 : 320;
    toast->setFixedWidth(qBound(240, availableWidth, 340));
    toast->adjustSize();
    if (parent) {
        toast->move(qMax(16, (parent->width() - toast->width()) / 2),
                    qMax(16, parent->height() - toast->height() - 24));
    }
    toast->raise();
    toast->show();
    return toast;
}

void showNotice(QWidget *parent, const QString &title, const QString &message,
                bool warning, int timeoutMs)
{
    QFrame *toast = createToast(parent, title, message, warning);
    auto *buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    auto *okButton = new QPushButton(QStringLiteral("知道了"), toast);
    okButton->setProperty("toastButton", true);
    buttonRow->addWidget(okButton);
    toast->layout()->addItem(buttonRow);

    QObject::connect(okButton, &QPushButton::clicked, toast, &QFrame::close);
    QTimer::singleShot(timeoutMs, toast, &QFrame::close);
    toast->adjustSize();
    if (parent) {
        toast->move(qMax(16, (parent->width() - toast->width()) / 2),
                    qMax(16, parent->height() - toast->height() - 24));
    }
}
} // namespace

namespace TransientMessage {

void information(QWidget *parent, const QString &title, const QString &message,
                 int timeoutMs)
{
    showNotice(parent, title, message, false, timeoutMs);
}

void warning(QWidget *parent, const QString &title, const QString &message,
             int timeoutMs)
{
    showNotice(parent, title, message, true, timeoutMs);
}

} // namespace TransientMessage
