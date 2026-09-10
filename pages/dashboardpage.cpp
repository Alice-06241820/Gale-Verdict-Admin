#include "pages/dashboardpage.h"

#include "service/adminapiservice.h"

#include <QtCharts/QAbstractSeries>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QValueAxis>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMargins>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPointF>
#include <QPixmap>
#include <QMouseEvent>
#include <QTime>
#include <QVBoxLayout>
#include <QEasingCurve>
#include <QPointer>
#include <QHash>
#include <QVariantAnimation>
#include <QVector>
#include <QtMath>
#include <cmath>
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef QT_CHARTS_NAMESPACE
using namespace QT_CHARTS_NAMESPACE;
#endif

namespace {
const QColor kTextColor("#1f1d1a");
const QColor kTitleColor("#171511");
const QColor kCardBackground("#fbfaf7");
const QColor kChartBackground("#f6f2ec");
const QColor kGridColor("#e2ddd6");
const QColor kMainLineColor("#4b4944");
const QColor kAccentRed("#d84b35");
const QColor kAccentRedDark("#a63325");
const QColor kGuideLineColor("#c85a46");
const QColor kPreviewGuideLineColor("#b3a89a");
const QColor kPreviewDotColor("#f0b85a");
const QColor kAccentYellow("#f7d84a");
const QColor kStripeBase("#d7d2cc");
const QColor kStripeLine("#fbfaf7");
const qreal kHoverScale = 1.06;
const qreal kGapPx = 6.0;
const qreal kSliceCornerPx = 9.5;
const qreal kHoverAnimationDurationMs = 240.0;

struct DonutSlice {
    QString label;
    int value = 0;
    QBrush brush;
};

qreal normalizeDegrees(qreal angle)
{
    angle = std::fmod(angle, 360.0);
    if (angle < 0.0) {
        angle += 360.0;
    }
    return angle;
}

qreal textWidth(const QFontMetrics &metrics, const QString &text)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    return metrics.horizontalAdvance(text);
#else
    return metrics.width(text);
#endif
}

bool prefersReducedMotion()
{
#ifdef Q_OS_WIN
    BOOL clientAreaAnimations = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &clientAreaAnimations, 0) && !clientAreaAnimations) {
        return true;
    }

    ANIMATIONINFO animationInfo;
    animationInfo.cbSize = sizeof(animationInfo);
    animationInfo.iMinAnimate = TRUE;
    if (SystemParametersInfoW(SPI_GETANIMATION, sizeof(animationInfo), &animationInfo, 0) && !animationInfo.iMinAnimate) {
        return true;
    }
#endif

    return qEnvironmentVariableIntValue("QT_REDUCED_MOTION") > 0
        || qEnvironmentVariableIntValue("QT_DISABLE_ANIMATIONS") > 0;
}

void stopVariantAnimation(QVariantAnimation **animation)
{
    if (!animation || !*animation) {
        return;
    }
    (*animation)->stop();
    (*animation)->deleteLater();
    *animation = nullptr;
}

QPointF pointOnCircle(const QPointF &center, qreal radius, qreal degrees)
{
    const qreal radians = qDegreesToRadians(degrees);
    return QPointF(center.x() + std::cos(radians) * radius,
                   center.y() - std::sin(radians) * radius);
}

qreal angleFromCenter(const QPointF &center, const QPointF &pos)
{
    const qreal dx = pos.x() - center.x();
    const qreal dy = pos.y() - center.y();
    return normalizeDegrees(qRadiansToDegrees(std::atan2(-dy, dx)));
}

QPainterPath buildDonutSlicePath(const QPointF &center,
                                 qreal outerRadius,
                                 qreal innerRadius,
                                 qreal startAngle,
                                 qreal span)
{
    QPainterPath path;
    if (span <= 0.0 || outerRadius <= 0.0 || innerRadius <= 0.0 || outerRadius <= innerRadius) {
        return path;
    }

    const QRectF outerRect(center.x() - outerRadius,
                           center.y() - outerRadius,
                           outerRadius * 2.0,
                           outerRadius * 2.0);
    const QRectF innerRect(center.x() - innerRadius,
                           center.y() - innerRadius,
                           innerRadius * 2.0,
                           innerRadius * 2.0);
    const qreal endAngle = startAngle - span;
    const qreal thickness = outerRadius - innerRadius;
    const qreal cornerPx = qMin(kSliceCornerPx, thickness * 0.2);
    const qreal maxCornerDegrees = span * 0.22;
    const qreal outerCornerDegrees = qMin(qRadiansToDegrees(cornerPx / outerRadius), maxCornerDegrees);
    const qreal innerCornerDegrees = qMin(qRadiansToDegrees(cornerPx / innerRadius), maxCornerDegrees);

    if (cornerPx <= 0.5 || outerCornerDegrees <= 0.1 || innerCornerDegrees <= 0.1) {
        path.moveTo(pointOnCircle(center, outerRadius, startAngle));
        path.arcTo(outerRect, startAngle, -span);
        path.lineTo(pointOnCircle(center, innerRadius, endAngle));
        path.arcTo(innerRect, endAngle, span);
        path.lineTo(pointOnCircle(center, outerRadius, startAngle));
        path.closeSubpath();
        return path;
    }

    const QPointF outerStartArc = pointOnCircle(center, outerRadius, startAngle - outerCornerDegrees);
    const QPointF outerEndRadial = pointOnCircle(center, outerRadius - cornerPx, endAngle);
    const QPointF innerEndRadial = pointOnCircle(center, innerRadius + cornerPx, endAngle);
    const QPointF innerEndArc = pointOnCircle(center, innerRadius, endAngle + innerCornerDegrees);
    const QPointF innerStartRadial = pointOnCircle(center, innerRadius + cornerPx, startAngle);
    const QPointF outerStartRadial = pointOnCircle(center, outerRadius - cornerPx, startAngle);

    path.moveTo(outerStartArc);
    path.arcTo(outerRect, startAngle - outerCornerDegrees, -(span - outerCornerDegrees * 2.0));
    path.quadTo(pointOnCircle(center, outerRadius, endAngle), outerEndRadial);
    path.lineTo(innerEndRadial);
    path.quadTo(pointOnCircle(center, innerRadius, endAngle), innerEndArc);
    path.arcTo(innerRect, endAngle + innerCornerDegrees, span - innerCornerDegrees * 2.0);
    path.quadTo(pointOnCircle(center, innerRadius, startAngle), innerStartRadial);
    path.lineTo(outerStartRadial);
    path.quadTo(pointOnCircle(center, outerRadius, startAngle), outerStartArc);
    path.closeSubpath();
    return path;
}

class RevenueChartView : public QChartView
{
public:
    using MouseHandler = std::function<void(const QPoint &)>;
    using LeaveHandler = std::function<void()>;

    explicit RevenueChartView(QWidget *parent = nullptr)
        : QChartView(parent)
    {
        setMouseTracking(true);
        viewport()->setMouseTracking(true);
    }

    void setMouseMoveHandler(MouseHandler handler)
    {
        mouseMoveHandler_ = std::move(handler);
    }

    void setMouseClickHandler(MouseHandler handler)
    {
        mouseClickHandler_ = std::move(handler);
    }

    void setLeaveHandler(LeaveHandler handler)
    {
        leaveHandler_ = std::move(handler);
    }

protected:
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (mouseMoveHandler_) {
            mouseMoveHandler_(event->pos());
        }
        QChartView::mouseMoveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && mouseClickHandler_) {
            mouseClickHandler_(event->pos());
        }
        QChartView::mousePressEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        if (leaveHandler_) {
            leaveHandler_();
        }
        QChartView::leaveEvent(event);
    }

private:
    MouseHandler mouseMoveHandler_;
    MouseHandler mouseClickHandler_;
    LeaveHandler leaveHandler_;
};

class DeviceDonutChart : public QWidget
{
public:
    explicit DeviceDonutChart(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMouseTracking(true);
        setAttribute(Qt::WA_Hover, true);
    }

    ~DeviceDonutChart() override
    {
        stopAllSliceAnimations();
        stopAnimation(&revealAnimation_);
    }

    void setSlices(const QList<DonutSlice> &slices)
    {
        stopAllSliceAnimations();
        stopAnimation(&revealAnimation_);
        slices_ = slices;
        total_ = 0;
        for (const auto &slice : slices_) {
            total_ += slice.value;
        }
        if (pinnedIndex_ >= slices_.size()) {
            pinnedIndex_ = -1;
        }
        hoveredIndex_ = -1;
        activeIndex_ = -1;
        fadingIndex_ = -1;
        sliceScales_ = QVector<qreal>(slices_.size(), 1.0);
        revealProgress_ = prefersReducedMotion() ? 1.0 : 0.0;
        syncActiveIndex(false);
        startRevealAnimation();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), Qt::transparent);

        const QRectF area = rect().adjusted(22, 10, -22, -50);
        const qreal size = qMin(area.width(), area.height());
        if (size <= 0.0 || total_ <= 0) {
            return;
        }

        const qreal outerRadius = size * 0.5;
        const qreal innerRadius = outerRadius * 0.42;
        const QPointF center = area.center();
        const qreal baseStart = 90.0;
        const qreal gapDegrees = 0.0;

        QFont titleFont = font();
        titleFont.setPointSize(qMax(9, titleFont.pointSize()));
        titleFont.setBold(true);

        QFont detailFont = font();
        detailFont.setPointSize(qMax(8, detailFont.pointSize() - 2));
        detailFont.setBold(false);

        drawSlices(&painter, center, outerRadius, innerRadius, baseStart, gapDegrees, false);
        drawSlices(&painter, center, outerRadius, innerRadius, baseStart, gapDegrees, true);
        drawSeparators(&painter, center, outerRadius, innerRadius, baseStart);

        drawDetailCard(&painter, center, outerRadius, baseStart, gapDegrees, titleFont, detailFont);
        drawLegend(&painter);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        updateHoveredIndex(event->pos());
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }

        const int index = indexAt(event->pos());
        if (index < 0) {
            if (pinnedIndex_ >= 0) {
                pinnedIndex_ = -1;
                syncActiveIndex(true);
            }
            update();
            return;
        }

        pinnedIndex_ = (pinnedIndex_ == index) ? -1 : index;
        syncActiveIndex(true);
        update();
    }

    void leaveEvent(QEvent *) override
    {
        setHoveredIndex(-1);
    }

private:
    void stopAnimation(QVariantAnimation **animation)
    {
        if (!animation || !*animation) {
            return;
        }
        (*animation)->stop();
        (*animation)->deleteLater();
        *animation = nullptr;
    }

    void drawSlices(QPainter *painter,
                    const QPointF &center,
                    qreal outerRadius,
                    qreal innerRadius,
                    qreal baseStart,
                    qreal gapDegrees,
                    bool drawHovered)
    {
        qreal current = baseStart + gapDegrees / 2.0;
        qreal consumed = 0.0;
        const qreal revealAngle = 360.0 * revealProgress_;
        for (int i = 0; i < slices_.size(); ++i) {
            const DonutSlice &slice = slices_[i];
            if (slice.value <= 0) {
                continue;
            }

            const qreal fullSpan = 360.0 * slice.value / total_;
            const qreal drawSpan = qMax<qreal>(0.0, fullSpan - gapDegrees);
            const qreal revealed = qBound<qreal>(0.0, revealAngle - consumed, drawSpan);
            if (revealed <= 0.0 || drawSpan <= 0.0 || (i == activeIndex_) != drawHovered) {
                consumed += fullSpan;
                current -= fullSpan;
                continue;
            }

            // 每块独立缩放：切换焦点时旧块缩回、新块放大可同时进行；
            // 高亮程度由缩放值反推，透明度随之平滑过渡。
            const qreal scale = sliceScales_.value(i, 1.0);
            const qreal highlight =
                qBound<qreal>(0.0, (scale - 1.0) / (kHoverScale - 1.0), 1.0);
            const qreal pieceOuter = outerRadius * scale;
            const qreal pieceInner = innerRadius * scale;
            const QPainterPath path = buildDonutSlicePath(center, pieceOuter, pieceInner, current, revealed);

            painter->save();
            painter->setOpacity(anyHighlight() ? 0.58 + 0.42 * highlight : 1.0);
            painter->setBrush(slice.brush);
            painter->setPen(Qt::NoPen);
            painter->drawPath(path);
            painter->restore();
            consumed += fullSpan;
            current -= fullSpan;
        }
    }

    void drawSeparators(QPainter *painter,
                        const QPointF &center,
                        qreal outerRadius,
                        qreal innerRadius,
                        qreal baseStart)
    {
        const qreal revealAngle = 360.0 * revealProgress_;
        if (revealAngle <= 0.0) {
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(kCardBackground, kGapPx, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
        const qreal separatorOuterRadius = outerRadius * sliceScales_.value(activeIndex_, 1.0);

        qreal current = baseStart;
        qreal consumed = 0.0;
        for (int i = 0; i < slices_.size(); ++i) {
            if (slices_[i].value <= 0) {
                continue;
            }

            const qreal fullSpan = 360.0 * slices_[i].value / total_;
            if (consumed <= revealAngle + 0.1) {
                const QPointF outer = pointOnCircle(center, separatorOuterRadius - 0.5, current);
                const QPointF inner = pointOnCircle(center, innerRadius + 0.5, current);
                painter->drawLine(inner, outer);
            }
            consumed += fullSpan;
            current -= fullSpan;
        }
        painter->restore();
    }

    void startRevealAnimation()
    {
        if (revealAnimation_) {
            revealAnimation_->stop();
            revealAnimation_->deleteLater();
            revealAnimation_ = nullptr;
        }
        if (total_ <= 0) {
            update();
            return;
        }

        if (prefersReducedMotion()) {
            revealProgress_ = 1.0;
            update();
            return;
        }

        revealAnimation_ = new QVariantAnimation(this);
        revealAnimation_->setDuration(1050);
        revealAnimation_->setStartValue(0.0);
        revealAnimation_->setEndValue(1.0);
        revealAnimation_->setEasingCurve(QEasingCurve::OutCubic);
        connect(revealAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            revealProgress_ = value.toReal();
            update();
        });
        connect(revealAnimation_, &QVariantAnimation::finished, this, [this]() {
            if (revealAnimation_) {
                revealAnimation_->deleteLater();
                revealAnimation_ = nullptr;
            }
        });
        revealAnimation_->start();
    }

    void drawDetailCard(QPainter *painter,
                        const QPointF &center,
                        qreal outerRadius,
                        qreal baseStart,
                        qreal gapDegrees,
                        const QFont &titleFont,
                        const QFont &detailFont)
    {
        const int index = activeDetailIndex();
        const bool hasSlice = index >= 0 && index < slices_.size() && slices_[index].value > 0;
        if (!hasSlice) {
            return;
        }

        const QColor textColor("#fffaf4");
        const QColor mutedTextColor("#ded7ce");
        const QColor cardColor("#171511");
        const QColor borderColor("#171511");

        const QString title = slices_[index].label;
        const QString valueLine = QStringLiteral("数量：%1 台").arg(slices_[index].value);
        const QString percentLine = QStringLiteral("占比：%1%")
                                        .arg(100.0 * slices_[index].value / total_, 0, 'f', 1);
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics detailMetrics(detailFont);
        const qreal maxCardWidth = qMin<qreal>(qMax<qreal>(86.0, width() - 28.0), 100.0);
        const qreal textMaxWidth = maxCardWidth - 16.0;
        const QRect titleBounds = titleMetrics.boundingRect(QRect(0, 0, static_cast<int>(textMaxWidth), 400), Qt::TextWordWrap, title);
        const qreal detailWidth = qMax(textWidth(detailMetrics, valueLine), textWidth(detailMetrics, percentLine));
        qreal cardWidth = qMax<qreal>(86.0, qMax<qreal>(titleBounds.width(), detailWidth) + 16.0);
        cardWidth = qMin(cardWidth, maxCardWidth);

        const QRect finalTitleBounds = titleMetrics.boundingRect(QRect(0, 0, static_cast<int>(cardWidth - 16.0), 400), Qt::TextWordWrap, title);
        const qreal lineHeight = detailMetrics.height();
        const qreal cardHeight = 14.0 + finalTitleBounds.height() + lineHeight * 2.0;

        const qreal midAngle = sliceMidAngle(index, baseStart, gapDegrees);
        const QPointF anchor = pointOnCircle(center, outerRadius + 1.0, midAngle);
        const qreal detailBottom = qMin<qreal>(height() - 24.0, legendTopY() - 8.0);
        const QRectF bounds(10.0, 8.0, width() - 20.0, qMax<qreal>(cardHeight, detailBottom - 8.0));
        QRectF box = bestDetailCardRect(center, outerRadius, anchor, cardWidth, cardHeight, bounds);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(borderColor, 1.0));
        painter->setBrush(cardColor);
        painter->drawRoundedRect(box, 7, 7);

        painter->setPen(textColor);
        painter->setFont(titleFont);
        const qreal padX = 7.0;
        qreal y = box.top() + 7.0;

        painter->setBrush(slices_[index].brush);
        painter->setPen(QPen(QColor("#fffaf4"), 1.0));
        painter->drawEllipse(QRectF(box.left() + padX, y + 5.0, 6.0, 6.0));
        painter->setPen(textColor);
        painter->drawText(QRectF(box.left() + padX + 10.0, y, box.width() - padX * 2.0 - 10.0, finalTitleBounds.height()),
                          Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                          title);
        y += finalTitleBounds.height() + 2.0;

        painter->setFont(detailFont);
        painter->setPen(mutedTextColor);
        painter->drawText(QRectF(box.left() + padX, y, box.width() - padX * 2.0, lineHeight),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          valueLine);
        y += lineHeight;
        painter->drawText(QRectF(box.left() + padX, y, box.width() - padX * 2.0, lineHeight),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          percentLine);
        painter->restore();
    }

    QRectF bestDetailCardRect(const QPointF &center,
                              qreal outerRadius,
                              const QPointF &anchor,
                              qreal cardWidth,
                              qreal cardHeight,
                              const QRectF &bounds) const
    {
        auto clamped = [bounds](QRectF rect) {
            if (rect.left() < bounds.left()) {
                rect.moveLeft(bounds.left());
            }
            if (rect.right() > bounds.right()) {
                rect.moveRight(bounds.right());
            }
            if (rect.top() < bounds.top()) {
                rect.moveTop(bounds.top());
            }
            if (rect.bottom() > bounds.bottom()) {
                rect.moveBottom(bounds.bottom());
            }
            return rect;
        };

        auto overlapsRing = [center, outerRadius](const QRectF &rect) {
            const qreal nearestX = qBound(rect.left(), center.x(), rect.right());
            const qreal nearestY = qBound(rect.top(), center.y(), rect.bottom());
            const qreal dx = nearestX - center.x();
            const qreal dy = nearestY - center.y();
            return std::hypot(dx, dy) < outerRadius + 1.5;
        };

        auto score = [anchor](const QRectF &rect) {
            const qreal dx = rect.center().x() - anchor.x();
            const qreal dy = rect.center().y() - anchor.y();
            return dx * dx + dy * dy;
        };

        QList<QRectF> candidates;
        candidates.append(QRectF(anchor.x() + 1.0, anchor.y() - cardHeight / 2.0, cardWidth, cardHeight));
        candidates.append(QRectF(anchor.x() - cardWidth - 1.0, anchor.y() - cardHeight / 2.0, cardWidth, cardHeight));
        candidates.append(QRectF(anchor.x() - cardWidth / 2.0, anchor.y() - cardHeight - 1.0, cardWidth, cardHeight));
        candidates.append(QRectF(anchor.x() - cardWidth / 2.0, anchor.y() + 1.0, cardWidth, cardHeight));
        candidates.append(QRectF(bounds.left(), bounds.top(), cardWidth, cardHeight));
        candidates.append(QRectF(bounds.right() - cardWidth, bounds.top(), cardWidth, cardHeight));
        candidates.append(QRectF(bounds.left(), bounds.bottom() - cardHeight, cardWidth, cardHeight));
        candidates.append(QRectF(bounds.right() - cardWidth, bounds.bottom() - cardHeight, cardWidth, cardHeight));

        QRectF best = clamped(candidates.first());
        qreal bestScore = 1.0e30;
        for (const QRectF &candidate : candidates) {
            const QRectF rect = clamped(candidate);
            if (overlapsRing(rect)) {
                continue;
            }
            const qreal candidateScore = score(rect);
            if (candidateScore < bestScore) {
                best = rect;
                bestScore = candidateScore;
            }
        }

        if (bestScore < 1.0e29) {
            return best;
        }

        qreal leastOverlapScore = -1.0e30;
        for (const QRectF &candidate : candidates) {
            const QRectF rect = clamped(candidate);
            const qreal dx = rect.center().x() - center.x();
            const qreal dy = rect.center().y() - center.y();
            const qreal candidateScore = std::hypot(dx, dy) - score(rect) * 0.0001;
            if (candidateScore > leastOverlapScore) {
                best = rect;
                leastOverlapScore = candidateScore;
            }
        }
        return best;
    }

    qreal sliceMidAngle(int index, qreal baseStart, qreal gapDegrees) const
    {
        qreal current = baseStart + gapDegrees / 2.0;
        for (int i = 0; i < slices_.size(); ++i) {
            if (slices_[i].value <= 0) {
                continue;
            }

            const qreal fullSpan = 360.0 * slices_[i].value / total_;
            const qreal drawSpan = qMax<qreal>(0.0, fullSpan - gapDegrees);
            if (i == index) {
                return current - drawSpan / 2.0;
            }
            current -= fullSpan;
        }
        return baseStart;
    }

    qreal legendTopY() const
    {
        QFont legendFont = font();
        legendFont.setPointSize(qMax(9, legendFont.pointSize() - 1));
        const QFontMetrics fm(legendFont);

        const qreal maxWidth = width() - 44.0;
        qreal x = 22.0;
        qreal y = height() - 16.0;
        qreal top = y - 11.0;
        bool firstInRow = true;
        for (const auto &slice : slices_) {
            if (slice.value <= 0) {
                continue;
            }

            const qreal textMaxWidth = 132.0;
            const QRect labelBounds = fm.boundingRect(QRect(0, 0, static_cast<int>(textMaxWidth), 120),
                                                      Qt::TextWordWrap,
                                                      slice.label);
            const qreal itemWidth = 10.0 + 6.0 + labelBounds.width() + 18.0;
            if (!firstInRow && x + itemWidth > maxWidth + 22.0) {
                x = 22.0;
                y -= qMax<qreal>(18.0, labelBounds.height() + 2.0);
                firstInRow = true;
            }

            top = qMin(top, y - 11.0);
            x += 14.0 + labelBounds.width() + 18.0;
            firstInRow = false;
        }

        return top;
    }

    void drawLegend(QPainter *painter)
    {
        QFont legendFont = font();
        legendFont.setPointSize(qMax(9, legendFont.pointSize() - 1));
        painter->setFont(legendFont);
        const QFontMetrics fm(legendFont);

        const qreal maxWidth = width() - 44.0;
        qreal x = 22.0;
        qreal y = height() - 16.0;
        bool firstInRow = true;
        for (const auto &slice : slices_) {
            if (slice.value <= 0) {
                continue;
            }
            const qreal textMaxWidth = 132.0;
            const QRect labelBounds = fm.boundingRect(QRect(0, 0, static_cast<int>(textMaxWidth), 120),
                                                      Qt::TextWordWrap,
                                                      slice.label);
            const qreal itemWidth = 10.0 + 6.0 + labelBounds.width() + 18.0;
            if (!firstInRow && x + itemWidth > maxWidth + 22.0) {
                x = 22.0;
                y -= qMax<qreal>(18.0, labelBounds.height() + 2.0);
                firstInRow = true;
            }

            painter->setPen(QPen(kTextColor, 1));
            painter->setBrush(slice.brush);
            painter->drawEllipse(QRectF(x, y - 7.0, 9.0, 9.0));
            x += 14.0;

            painter->setPen(kTextColor);
            painter->drawText(QRectF(x, y - 11.0, textMaxWidth, qMax<qreal>(18.0, labelBounds.height() + 2.0)),
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                              slice.label);
            x += labelBounds.width() + 18.0;
            firstInRow = false;
        }
    }

    void setHoveredIndex(int nextIndex)
    {
        if (nextIndex == hoveredIndex_) {
            return;
        }

        hoveredIndex_ = nextIndex;
        syncActiveIndex(true);
    }

    void updateHoveredIndex(const QPointF &pos)
    {
        const QRectF area = rect().adjusted(22, 10, -22, -50);
        const qreal size = qMin(area.width(), area.height());
        if (size <= 0.0 || total_ <= 0) {
            return;
        }

        const qreal outerRadius = size * 0.5;
        const qreal innerRadius = outerRadius * 0.42;
        const QPointF center = area.center();
        const qreal gapDegrees = 0.0;
        const qreal baseStart = 90.0;
        const qreal dx = pos.x() - center.x();
        const qreal dy = pos.y() - center.y();
        const qreal dist = std::hypot(dx, dy);
        if (dist < innerRadius || dist > outerRadius * 1.1) {
            setHoveredIndex(-1);
            return;
        }

        const qreal angle = angleFromCenter(center, pos);

        qreal current = baseStart + gapDegrees / 2.0;
        int nextIndex = -1;
        for (int i = 0; i < slices_.size(); ++i) {
            if (slices_[i].value <= 0) {
                continue;
            }
            const qreal fullSpan = 360.0 * slices_[i].value / total_;
            const qreal drawSpan = qMax<qreal>(0.0, fullSpan - gapDegrees);
            const qreal rel = normalizeDegrees(current - angle);
            if (rel >= 0.0 && rel < drawSpan) {
                nextIndex = i;
                break;
            }
            current -= fullSpan;
        }

        setHoveredIndex(nextIndex);
    }

    void syncActiveIndex(bool animate)
    {
        const int nextActiveIndex = hoveredIndex_ >= 0 ? hoveredIndex_ : pinnedIndex_;
        if (nextActiveIndex == activeIndex_) {
            return;
        }

        // 旧块缩回与新块放大并行进行：焦点切换不再"硬跳"，
        // 鼠标在相邻扇区之间滑动时高亮可以直接交接。
        const int previousIndex = activeIndex_;
        activeIndex_ = nextActiveIndex;

        if (previousIndex >= 0 && previousIndex != activeIndex_) {
            fadingIndex_ = previousIndex;
            animateSliceScale(previousIndex, 1.0, animate);
        }
        if (activeIndex_ >= 0) {
            if (fadingIndex_ == activeIndex_) {
                fadingIndex_ = -1;
            }
            animateSliceScale(activeIndex_, kHoverScale, animate);
        }
        update();
    }

    bool anyHighlight() const
    {
        return activeIndex_ >= 0 || fadingIndex_ >= 0;
    }

    void stopAllSliceAnimations()
    {
        for (auto it = sliceAnimations_.begin(); it != sliceAnimations_.end(); ++it) {
            if (it.value()) {
                it.value()->stop();
                it.value()->deleteLater();
            }
        }
        sliceAnimations_.clear();
    }

    // 单块缩放动画：target 为 1.0（缩回）或 kHoverScale（放大）。
    void animateSliceScale(int index, qreal target, bool animate)
    {
        if (index < 0 || index >= sliceScales_.size()) {
            return;
        }
        if (auto running = sliceAnimations_.find(index);
            running != sliceAnimations_.end()) {
            if (running.value()) {
                running.value()->stop();
                running.value()->deleteLater();
            }
            sliceAnimations_.erase(running);
        }

        const qreal start = sliceScales_.value(index, 1.0);
        if (!animate || prefersReducedMotion() || qAbs(start - target) < 0.001) {
            sliceScales_[index] = target;
            if (index == fadingIndex_) {
                fadingIndex_ = -1;
            }
            update();
            return;
        }

        auto *animation = new QVariantAnimation(this);
        sliceAnimations_.insert(index, animation);
        animation->setDuration(static_cast<int>(kHoverAnimationDurationMs));
        animation->setStartValue(start);
        animation->setEndValue(target);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(animation, &QVariantAnimation::valueChanged, this,
                [this, index](const QVariant &value) {
            if (index < sliceScales_.size()) {
                sliceScales_[index] = value.toReal();
                update();
            }
        });
        connect(animation, &QVariantAnimation::finished, this, [this, index]() {
            if (auto it = sliceAnimations_.find(index);
                it != sliceAnimations_.end()) {
                if (it.value()) {
                    it.value()->deleteLater();
                }
                sliceAnimations_.erase(it);
            }
            if (fadingIndex_ == index) {
                fadingIndex_ = -1;
                update();
            }
        });
        animation->start();
    }

    int activeDetailIndex() const
    {
        if (hoveredIndex_ >= 0 && hoveredIndex_ < slices_.size()) {
            return hoveredIndex_;
        }
        if (pinnedIndex_ >= 0 && pinnedIndex_ < slices_.size()) {
            return pinnedIndex_;
        }
        return -1;
    }

    int indexAt(const QPointF &pos) const
    {
        const QRectF area = rect().adjusted(22, 10, -22, -50);
        const qreal size = qMin(area.width(), area.height());
        if (size <= 0.0 || total_ <= 0) {
            return -1;
        }

        const qreal outerRadius = size * 0.5;
        const qreal innerRadius = outerRadius * 0.42;
        const QPointF center = area.center();
        const qreal gapDegrees = 0.0;
        const qreal baseStart = 90.0;
        const qreal dx = pos.x() - center.x();
        const qreal dy = pos.y() - center.y();
        const qreal dist = std::hypot(dx, dy);
        if (dist < innerRadius || dist > outerRadius * 1.12) {
            return -1;
        }

        const qreal angle = angleFromCenter(center, pos);

        qreal current = baseStart + gapDegrees / 2.0;
        for (int i = 0; i < slices_.size(); ++i) {
            if (slices_[i].value <= 0) {
                continue;
            }
            const qreal fullSpan = 360.0 * slices_[i].value / total_;
            const qreal drawSpan = qMax<qreal>(0.0, fullSpan - gapDegrees);
            const qreal rel = normalizeDegrees(current - angle);
            if (rel >= 0.0 && rel < drawSpan) {
                return i;
            }
            current -= fullSpan;
        }
        return -1;
    }

    QList<DonutSlice> slices_;
    int total_ = 0;
    int hoveredIndex_ = -1;
    int activeIndex_ = -1;
    int pinnedIndex_ = -1;
    QVector<qreal> sliceScales_;                       // 每块当前缩放（高亮程度）
    QHash<int, QVariantAnimation *> sliceAnimations_;  // 每块独立的缩放动画
    int fadingIndex_ = -1;                             // 正在缩回的块
    qreal revealProgress_ = 1.0;
    QVariantAnimation *revealAnimation_ = nullptr;
};
}

DashboardPage::DashboardPage(AdminApiService *service, QWidget *parent)
    : QWidget(parent)
    , service_(service)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(18);

    auto *title = new QLabel("经营总览", this);
    title->setObjectName("pageTitle");
    auto *subtitle = new QLabel("营收、趋势和设备状态集中查看", this);
    subtitle->setObjectName("pageSubtitle");
    layout->addWidget(title);
    layout->addWidget(subtitle);

    auto *metricGrid = new QGridLayout();
    metricGrid->setSpacing(14);
    todayRevenue_ = createMetric("今日营收");
    monthRevenue_ = createMetric("本月营收");
    totalRevenue_ = createMetric("总营收");
    metricGrid->addWidget(todayRevenue_->parentWidget(), 0, 0);
    metricGrid->addWidget(monthRevenue_->parentWidget(), 0, 1);
    metricGrid->addWidget(totalRevenue_->parentWidget(), 0, 2);
    layout->addLayout(metricGrid);

    auto *analyticsRow = new QHBoxLayout();
    analyticsRow->setSpacing(16);

    auto *devicePanel = new QWidget(this);
    devicePanel->setObjectName("sectionPanel");
    devicePanel->setMinimumWidth(320);
    devicePanel->setMaximumWidth(420);
    auto *deviceLayout = new QVBoxLayout(devicePanel);
    deviceLayout->setContentsMargins(18, 16, 18, 18);
    deviceLayout->setSpacing(10);

    auto *deviceTitle = new QLabel("电桩状态分布", devicePanel);
    deviceTitle->setObjectName("sectionTitle");
    auto *deviceHint = new QLabel("提示：悬停或点击扇区查看完整故障详情。", devicePanel);
    deviceHint->setObjectName("hintText");
    deviceHint->setWordWrap(true);
    deviceChartView_ = new DeviceDonutChart(devicePanel);
    deviceChartView_->setMinimumHeight(330);
    deviceLayout->addWidget(deviceTitle);
    deviceLayout->addWidget(deviceHint);
    deviceLayout->addWidget(deviceChartView_, 1);
    analyticsRow->addWidget(devicePanel, 1);

    auto *chartPanel = new QWidget(this);
    chartPanel->setObjectName("sectionPanel");
    chartPanel->setMinimumWidth(520);
    auto *chartLayout = new QVBoxLayout(chartPanel);
    chartLayout->setContentsMargins(18, 16, 18, 18);
    chartLayout->setSpacing(10);

    auto *chartHeader = new QHBoxLayout();
    auto *chartTitle = new QLabel("营收趋势", chartPanel);
    chartTitle->setObjectName("sectionTitle");
    rangeBox_ = new QComboBox(chartPanel);
    rangeBox_->addItem("近 7 天", 7);
    rangeBox_->addItem("近 30 天", 30);
    chartHeader->addWidget(chartTitle);
    chartHeader->addStretch();
    chartHeader->addWidget(rangeBox_);
    chartLayout->addLayout(chartHeader);

    chartView_ = new RevenueChartView(chartPanel);
    chartView_->setMinimumHeight(280);
    chartView_->setRenderHint(QPainter::Antialiasing);
    chartLayout->addWidget(chartView_);

    chartHint_ = new QLabel("提示：鼠标在图中移动可预览当天营收，点击可固定查看详细数值。", chartPanel);
    chartHint_->setObjectName("hintText");
    chartLayout->addWidget(chartHint_);
    analyticsRow->addWidget(chartPanel, 2);
    layout->addLayout(analyticsRow, 1);

    connect(rangeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        updateChart(rangeBox_->currentData().toInt());
    });

    refresh();
}

void DashboardPage::refresh()
{
    const RevenueSummary summary = service_->revenueSummary();
    todayRevenue_->setText(QString("%1 元").arg(summary.today, 0, 'f', 2));
    monthRevenue_->setText(QString("%1 元").arg(summary.month, 0, 'f', 2));
    totalRevenue_->setText(QString("%1 元").arg(summary.total, 0, 'f', 2));
    updateChart(rangeBox_->currentData().toInt());
    updateDeviceSummary();
}

QLabel *DashboardPage::createMetric(const QString &title)
{
    auto *panel = new QWidget(this);
    panel->setObjectName("metricCard");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(6);

    auto *label = new QLabel(title, panel);
    label->setObjectName("metricLabel");
    auto *value = new QLabel("0", panel);
    value->setObjectName("metricValue");
    layout->addWidget(label);
    layout->addWidget(value);
    return value;
}

void DashboardPage::updateChart(int days)
{
    // 重建图表前先停掉上一轮的绘制动画（旧 chart/series 会被 setChart 释放）。
    stopVariantAnimation(&trendRevealAnimation_);

    const QString defaultHint = "提示：鼠标在图中移动可预览当天营收，点击可固定查看详细数值。";

    auto *curve = new QSplineSeries();
    curve->setName("已完成订单营收");
    curve->setPen(QPen(kMainLineColor, 2));

    auto *pointsSeries = new QScatterSeries();
    pointsSeries->setName("每日营收");
    pointsSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    pointsSeries->setMarkerSize(8.5);
    pointsSeries->setColor(kTextColor);
    pointsSeries->setBorderColor(kTextColor);

    auto *selectedVerticalLine = new QLineSeries();
    selectedVerticalLine->setName("固定点竖向指示线");
    QPen guidePen(kGuideLineColor, 1.6);
    guidePen.setStyle(Qt::DashLine);
    selectedVerticalLine->setPen(guidePen);

    auto *selectedHorizontalLine = new QLineSeries();
    selectedHorizontalLine->setName("固定点横向指示线");
    selectedHorizontalLine->setPen(guidePen);

    auto *previewVerticalLine = new QLineSeries();
    previewVerticalLine->setName("预览点竖向指示线");
    QPen previewGuidePen(kPreviewGuideLineColor, 1.0);
    previewGuidePen.setStyle(Qt::DashLine);
    previewVerticalLine->setPen(previewGuidePen);

    auto *previewHorizontalLine = new QLineSeries();
    previewHorizontalLine->setName("预览点横向指示线");
    previewHorizontalLine->setPen(previewGuidePen);

    auto *previewSeries = new QScatterSeries();
    previewSeries->setName("预览点");
    previewSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    previewSeries->setMarkerSize(10);
    previewSeries->setColor(kPreviewDotColor);
    previewSeries->setBorderColor(kPreviewGuideLineColor);
    previewSeries->setPointLabelsVisible(false);

    auto *selectedSeries = new QScatterSeries();
    selectedSeries->setName("固定点");
    selectedSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    selectedSeries->setMarkerSize(13);
    selectedSeries->setColor(kAccentRed);
    selectedSeries->setBorderColor(kAccentRedDark);
    selectedSeries->setPointLabelsVisible(false);

    const QList<RevenuePoint> points = service_->revenueTrend(days);
    auto xValueForDate = [](const QDate &date) {
        return static_cast<qreal>(QDateTime(date, QTime(0, 0)).toMSecsSinceEpoch());
    };

    // 曲线/散点的数据先收集，稍后由 reveal 动画按进度逐段填充（从左往右绘制）。
    QVector<QPointF> curvePoints;
    curvePoints.reserve(points.size());
    double maxValue = 0.0;
    for (int i = 0; i < points.size(); ++i) {
        const qreal x = xValueForDate(points[i].date);
        curvePoints.append(QPointF(x, points[i].amount));
        maxValue = qMax(maxValue, points[i].amount);
    }

    const int tickStep = 100;
    const int axisMax = qMax(tickStep, static_cast<int>(qCeil((maxValue + 60.0) / tickStep)) * tickStep);

    auto setCrosshair = [points, xValueForDate, axisMax](int index,
                                                         QLineSeries *verticalLine,
                                                         QLineSeries *horizontalLine,
                                                         QScatterSeries *markerSeries) {
        verticalLine->clear();
        horizontalLine->clear();
        markerSeries->clear();
        const bool validIndex = index >= 0 && index < points.size();
        verticalLine->setVisible(validIndex);
        horizontalLine->setVisible(validIndex);
        markerSeries->setVisible(validIndex);
        if (!validIndex) {
            return;
        }

        const qreal selectedX = xValueForDate(points[index].date);
        const qreal selectedY = points[index].amount;
        verticalLine->append(selectedX, 0.0);
        verticalLine->append(selectedX, axisMax);
        horizontalLine->append(xValueForDate(points.first().date), selectedY);
        horizontalLine->append(xValueForDate(points.last().date), selectedY);
        markerSeries->append(selectedX, selectedY);
    };

    previewVerticalLine->setVisible(false);
    previewHorizontalLine->setVisible(false);
    previewSeries->setVisible(false);

    if (!points.isEmpty()) {
        if (selectedRevenueIndex_ >= points.size()) {
            selectedRevenueIndex_ = -1;
        }
        setCrosshair(selectedRevenueIndex_, selectedVerticalLine, selectedHorizontalLine, selectedSeries);
    }

    auto *chart = new QChart();
    chart->addSeries(curve);
    chart->addSeries(pointsSeries);
    chart->addSeries(previewHorizontalLine);
    chart->addSeries(previewVerticalLine);
    chart->addSeries(previewSeries);
    chart->addSeries(selectedHorizontalLine);
    chart->addSeries(selectedVerticalLine);
    chart->addSeries(selectedSeries);
    chart->legend()->setVisible(false);
    chart->setBackgroundVisible(false);
    chart->setPlotAreaBackgroundVisible(true);
    chart->setPlotAreaBackgroundBrush(QBrush(kChartBackground));
    chart->setTitle("");
    chart->setMargins(QMargins(2, 8, 10, 4));

    auto *axisX = new QDateTimeAxis();
    if (points.isEmpty()) {
        const QDate today = QDate::currentDate();
        axisX->setRange(QDateTime(today, QTime(0, 0)), QDateTime(today.addDays(1), QTime(0, 0)));
    } else {
        axisX->setRange(QDateTime(points.first().date, QTime(0, 0)), QDateTime(points.last().date, QTime(0, 0)));
    }
    axisX->setFormat("MM-dd");
    axisX->setTickCount(points.size() <= 7 ? qMax(2, points.size()) : 8);
    axisX->setTitleText("");
    axisX->setGridLineVisible(false);

    auto *axisY = new QValueAxis();
    axisY->setRange(0, axisMax);
    axisY->setTickCount(axisMax / tickStep + 1);
    axisY->setLabelFormat("%.0f");
    axisY->setTitleText("");
    axisY->setGridLineColor(kGridColor);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    curve->attachAxis(axisX);
    curve->attachAxis(axisY);
    pointsSeries->attachAxis(axisX);
    pointsSeries->attachAxis(axisY);
    previewHorizontalLine->attachAxis(axisX);
    previewHorizontalLine->attachAxis(axisY);
    previewVerticalLine->attachAxis(axisX);
    previewVerticalLine->attachAxis(axisY);
    previewSeries->attachAxis(axisX);
    previewSeries->attachAxis(axisY);
    selectedHorizontalLine->attachAxis(axisX);
    selectedHorizontalLine->attachAxis(axisY);
    selectedVerticalLine->attachAxis(axisX);
    selectedVerticalLine->attachAxis(axisY);
    selectedSeries->attachAxis(axisX);
    selectedSeries->attachAxis(axisY);
    chartView_->setChart(chart);

    // 趋势曲线：从左往右绘制。节奏与环状图 reveal 一致（1050ms / OutCubic），
    // 末端按 x 插值以保证平滑生长而非逐点跳动。
    if (!curvePoints.isEmpty()) {
        const qreal xStart = curvePoints.first().x();
        const qreal xEnd = curvePoints.last().x();
        QPointer<QSplineSeries> curveGuard(curve);
        QPointer<QScatterSeries> pointsGuard(pointsSeries);
        if (prefersReducedMotion() || xEnd <= xStart) {
            for (const QPointF &point : curvePoints) {
                if (curveGuard) curveGuard->append(point);
                if (pointsGuard) pointsGuard->append(point);
            }
        } else {
            if (curveGuard) curveGuard->append(curvePoints.first());
            if (pointsGuard) pointsGuard->append(curvePoints.first());

            auto *reveal = new QVariantAnimation(this);
            trendRevealAnimation_ = reveal;
            reveal->setDuration(1050);
            reveal->setStartValue(0.0);
            reveal->setEndValue(1.0);
            reveal->setEasingCurve(QEasingCurve::OutCubic);
            connect(reveal, &QVariantAnimation::valueChanged, this,
                    [curveGuard, pointsGuard, curvePoints, xStart, xEnd](
                        const QVariant &value) {
                if (!curveGuard || !pointsGuard) {
                    return;
                }
                const qreal limit = xStart + (xEnd - xStart) * value.toReal();
                QVector<QPointF> visible;
                for (const QPointF &point : curvePoints) {
                    if (point.x() <= limit) {
                        visible.append(point);
                    }
                }
                if (visible.isEmpty()) {
                    visible.append(curvePoints.first());
                }
                if (visible.size() < curvePoints.size()) {
                    const QPointF last = visible.last();
                    const QPointF next = curvePoints.at(visible.size());
                    const qreal span = next.x() - last.x();
                    if (span > 0.0) {
                        const qreal ratio = (limit - last.x()) / span;
                        visible.append(QPointF(
                            limit, last.y() + (next.y() - last.y()) * ratio));
                    }
                }
                curveGuard->clear();
                pointsGuard->clear();
                for (const QPointF &point : visible) {
                    curveGuard->append(point);
                    pointsGuard->append(point);
                }
            });
            connect(reveal, &QVariantAnimation::finished, this, [this]() {
                if (trendRevealAnimation_) {
                    trendRevealAnimation_->deleteLater();
                    trendRevealAnimation_ = nullptr;
                }
            });
            reveal->start();
        }
    }

    auto updateBadge = [chart, points, xValueForDate](QGraphicsPathItem *badgeBox,
                                                      QGraphicsSimpleTextItem *badgeText,
                                                      QAbstractSeries *series,
                                                      int index,
                                                      const QString &text,
                                                      qreal yOffset) {
        const bool validIndex = index >= 0 && index < points.size();
        badgeBox->setVisible(validIndex);
        badgeText->setVisible(validIndex);
        if (!validIndex) {
            return;
        }

        badgeText->setText(text);
        const QPointF pointPos = chart->mapToPosition(
            QPointF(xValueForDate(points[index].date), points[index].amount),
            series);
        const QRectF textRect = badgeText->boundingRect();
        const double width = textRect.width() + 22.0;
        const double height = 24.0;
        const QRectF plotArea = chart->plotArea();
        const double boxX = qBound(plotArea.left() + 4.0,
                                   pointPos.x() - width / 2.0,
                                   plotArea.right() - width - 4.0);
        double boxY = pointPos.y() - yOffset;
        if (boxY < plotArea.top() + 4.0) {
            boxY = pointPos.y() + 14.0;
        }
        const QRectF box(boxX, boxY, width, height);
        QPainterPath path;
        path.addRoundedRect(box, height / 2.0, height / 2.0);
        badgeBox->setPath(path);
        badgeText->setPos(box.x() + 11.0, box.y() + 3.0);
    };

    auto briefText = [points](int index) {
        return QString("%1：%2 元")
            .arg(points[index].date.toString("MM-dd"))
            .arg(points[index].amount, 0, 'f', 0);
    };

    auto detailText = [points](int index) {
        return QString("%1，营收 %2 元")
            .arg(points[index].date.toString("yyyy-MM-dd"))
            .arg(points[index].amount, 0, 'f', 2);
    };

    auto *fixedBadgeBox = new QGraphicsPathItem(chart);
    fixedBadgeBox->setBrush(QBrush(kTitleColor));
    fixedBadgeBox->setPen(QPen(kTitleColor));
    fixedBadgeBox->setZValue(24);

    auto *fixedBadgeText = new QGraphicsSimpleTextItem(chart);
    fixedBadgeText->setBrush(QBrush(QColor("#fffaf4")));
    fixedBadgeText->setFont(QFont("Segoe UI", 10, QFont::DemiBold));
    fixedBadgeText->setZValue(25);

    auto *previewBadgeBox = new QGraphicsPathItem(chart);
    previewBadgeBox->setBrush(QBrush(QColor("#fffaf4")));
    previewBadgeBox->setPen(QPen(kPreviewGuideLineColor));
    previewBadgeBox->setZValue(20);

    auto *previewBadgeText = new QGraphicsSimpleTextItem(chart);
    previewBadgeText->setBrush(QBrush(kTextColor));
    previewBadgeText->setFont(QFont("Segoe UI", 9, QFont::DemiBold));
    previewBadgeText->setZValue(21);

    auto updateFixedBadge = [=]() {
        if (selectedRevenueIndex_ < 0 || selectedRevenueIndex_ >= points.size()) {
            updateBadge(fixedBadgeBox, fixedBadgeText, selectedSeries, -1, QString(), 44.0);
            return;
        }
        updateBadge(fixedBadgeBox,
                    fixedBadgeText,
                    selectedSeries,
                    selectedRevenueIndex_,
                    briefText(selectedRevenueIndex_),
                    44.0);
    };

    auto updatePreviewBadge = [=](int index) {
        updateBadge(previewBadgeBox,
                    previewBadgeText,
                    previewSeries,
                    index,
                    index >= 0 && index < points.size() ? briefText(index) : QString(),
                    36.0);
    };

    updateFixedBadge();
    updatePreviewBadge(-1);
    connect(chart, &QChart::plotAreaChanged, this, updateFixedBadge);

    auto indexForPoint = [points, xValueForDate](qreal xValue) {
        int bestIndex = -1;
        qreal bestDistance = 1.0e30;
        for (int i = 0; i < points.size(); ++i) {
            const qreal distance = qAbs(xValue - xValueForDate(points[i].date));
            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex = i;
            }
        }
        return bestIndex;
    };

    auto indexForMousePosition = [=](const QPoint &position) {
        if (points.isEmpty()) {
            return -1;
        }
        const QPointF scenePoint = chartView_->mapToScene(position);
        const QPointF chartPoint = chart->mapFromScene(scenePoint);
        if (!chart->plotArea().adjusted(-4.0, -4.0, 4.0, 4.0).contains(chartPoint)) {
            return -1;
        }
        const QPointF value = chart->mapToValue(chartPoint, curve);
        return indexForPoint(value.x());
    };

    auto showFixedHint = [=]() {
        if (selectedRevenueIndex_ >= 0 && selectedRevenueIndex_ < points.size()) {
            chartHint_->setText(QString("已固定 %1").arg(detailText(selectedRevenueIndex_)));
        } else {
            chartHint_->setText(defaultHint);
        }
    };

    auto *revenueChartView = dynamic_cast<RevenueChartView *>(chartView_);
    if (!revenueChartView) {
        chartHint_->setText(defaultHint);
        return;
    }

    revenueChartView->setMouseMoveHandler([=](const QPoint &position) {
        const int index = indexForMousePosition(position);
        setCrosshair(index, previewVerticalLine, previewHorizontalLine, previewSeries);
        updatePreviewBadge(index);
        if (index < 0) {
            showFixedHint();
            return;
        }
        chartHint_->setText(QString("预览 %1，点击可固定。").arg(briefText(index)));
    });

    revenueChartView->setMouseClickHandler([=](const QPoint &position) {
        const int index = indexForMousePosition(position);
        if (index < 0) {
            return;
        }
        selectedRevenueIndex_ = index;
        setCrosshair(selectedRevenueIndex_, selectedVerticalLine, selectedHorizontalLine, selectedSeries);
        updateFixedBadge();
        chartHint_->setText(QString("已固定 %1").arg(detailText(selectedRevenueIndex_)));
    });

    revenueChartView->setLeaveHandler([=]() {
        setCrosshair(-1, previewVerticalLine, previewHorizontalLine, previewSeries);
        updatePreviewBadge(-1);
        showFixedHint();
    });

    showFixedHint();
}

void DashboardPage::updateDeviceSummary()
{
    const DeviceStatusSummary summary = service_->deviceStatusSummary();
    QPixmap stripePixmap(12, 12);
    stripePixmap.fill(kStripeBase);
    {
        QPainter stripePainter(&stripePixmap);
        stripePainter.setRenderHint(QPainter::Antialiasing);
        stripePainter.setPen(QPen(kStripeLine, 3));
        stripePainter.drawLine(-2, 12, 12, -2);
        stripePainter.drawLine(4, 14, 14, 4);
    }

    QList<DonutSlice> slices;
    slices.append({"空闲", summary.idleCount, QBrush(kAccentYellow)});
    slices.append({"在用", summary.usingCount, QBrush(QColor("#d7d2cc"))});
    slices.append({"故障", summary.faultCount, QBrush(stripePixmap)});

    if (!deviceChartView_) {
        return;
    }
    auto *chart = static_cast<DeviceDonutChart *>(deviceChartView_);
    chart->setSlices(slices);
}
