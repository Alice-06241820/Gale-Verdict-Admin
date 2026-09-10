#ifndef MOTION_H
#define MOTION_H

#include <QPoint>

class QWidget;

// 客户端统一动效：时长、缓动与位移量集中在这里定义，
// 保证各页面的动画节奏与配色保持一致（颜色取自 style/app.qss）。
namespace Motion {

// 时长（毫秒）：小反馈/退出 140，常规进入 220，大面积/长距离 320。
constexpr int kFastMs = 140;
constexpr int kNormalMs = 220;
constexpr int kSlowMs = 320;

// 位移量（像素）：提示浮层进场时自下方抬起的距离。
constexpr int kToastOffsetPx = 12;

// 入场缓动（先快后慢）：QEasingCurve::OutCubic。
int enterEasing();
// 退场缓动（先慢后快）：QEasingCurve::InCubic。
int exitEasing();

// 淡入：透明度 0 -> 1；结束后关闭图形效果，恢复直接渲染。
void fadeIn(QWidget *widget, int durationMs = kNormalMs);

// 淡出并关闭（用于临时提示浮层）。
void fadeOutAndClose(QWidget *widget, int durationMs = kFastMs);

// 自下方滑入并淡入：目标位置为 target，起始位置为 target 下移 offsetPx。
void slideInTo(QWidget *widget, const QPoint &target,
               int offsetPx = kToastOffsetPx, int durationMs = kNormalMs);

} // namespace Motion

#endif // MOTION_H
