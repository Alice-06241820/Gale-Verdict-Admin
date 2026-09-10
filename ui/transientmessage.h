#ifndef TRANSIENTMESSAGE_H
#define TRANSIENTMESSAGE_H

#include <QString>

class QWidget;

// 非模态临时消息：覆盖在当前页面底部，允许用户继续操作，并自动超时关闭。
// 与用户端 (Gale-Verdict-Frontend) 的实现保持一致。
namespace TransientMessage {

void information(QWidget *parent, const QString &title, const QString &message,
                 int timeoutMs = 2600);
void warning(QWidget *parent, const QString &title, const QString &message,
             int timeoutMs = 3200);

} // namespace TransientMessage

#endif // TRANSIENTMESSAGE_H
