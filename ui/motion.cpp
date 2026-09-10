#include "ui/motion.h"

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QWidget>

namespace {

// 每个控件复用同一个透明度效果：避免动画过程中反复创建/销毁导致
// 悬空指针，也无需在动画结束后移除效果（只把它禁用即可）。
QGraphicsOpacityEffect *EnsureOpacityEffect(QWidget *widget)
{
    if (auto *existing =
            qobject_cast<QGraphicsOpacityEffect *>(widget->graphicsEffect())) {
        existing->setEnabled(true);
        return existing;
    }
    auto *effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);
    return effect;
}

// 动画结束后关闭效果，恢复控件直接渲染（不会删除效果对象）。
void RetireOpacityEffect(QWidget *widget, QGraphicsOpacityEffect *effect)
{
    if (widget->graphicsEffect() == effect) {
        effect->setEnabled(false);
    }
}

QEasingCurve EnterCurve()
{
    return QEasingCurve(QEasingCurve::OutCubic);
}

QEasingCurve ExitCurve()
{
    return QEasingCurve(QEasingCurve::InCubic);
}

} // namespace

namespace Motion {

int enterEasing()
{
    return QEasingCurve::OutCubic;
}

int exitEasing()
{
    return QEasingCurve::InCubic;
}

void fadeIn(QWidget *widget, int durationMs)
{
    if (!widget) {
        return;
    }
    QGraphicsOpacityEffect *effect = EnsureOpacityEffect(widget);
    effect->setOpacity(0.0);

    auto *animation = new QPropertyAnimation(effect, "opacity", widget);
    animation->setDuration(durationMs);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(EnterCurve());
    QObject::connect(animation, &QPropertyAnimation::finished, widget,
                     [widget, effect] { RetireOpacityEffect(widget, effect); });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void fadeOutAndClose(QWidget *widget, int durationMs)
{
    if (!widget) {
        return;
    }
    QGraphicsOpacityEffect *effect = EnsureOpacityEffect(widget);
    effect->setOpacity(1.0);

    auto *animation = new QPropertyAnimation(effect, "opacity", widget);
    animation->setDuration(durationMs);
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    animation->setEasingCurve(ExitCurve());
    QObject::connect(animation, &QPropertyAnimation::finished, widget,
                     [widget] { widget->close(); });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void slideInTo(QWidget *widget, const QPoint &target, int offsetPx,
               int durationMs)
{
    if (!widget) {
        return;
    }
    const QPoint start = target + QPoint(0, offsetPx);
    widget->move(start);

    QGraphicsOpacityEffect *effect = EnsureOpacityEffect(widget);
    effect->setOpacity(0.0);

    auto *group = new QParallelAnimationGroup(widget);
    auto *slide = new QPropertyAnimation(widget, "pos", group);
    slide->setDuration(durationMs);
    slide->setStartValue(start);
    slide->setEndValue(target);
    slide->setEasingCurve(EnterCurve());
    group->addAnimation(slide);

    auto *fade = new QPropertyAnimation(effect, "opacity", group);
    fade->setDuration(durationMs);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(EnterCurve());
    group->addAnimation(fade);

    QObject::connect(group, &QParallelAnimationGroup::finished, widget,
                     [widget, effect] { RetireOpacityEffect(widget, effect); });
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace Motion
