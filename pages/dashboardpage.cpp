#include "pages/dashboardpage.h"

#include "service/adminapiservice.h"

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
#include <QCursor>
#include <QDate>
#include <QDateTime>
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
#include <QToolTip>
#include <QVBoxLayout>
#include <QEasingCurve>
#include <QVariantAnimation>
#include <QtMath>
#include <cmath>

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
const QColor kAccentYellow("#f7d84a");
const QColor kStripeBase("#d7d2cc");
const QColor kStripeLine("#fbfaf7");
const qreal kHoverScale = 1.06;
const qreal kGapPx = 5.0;
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

QPointF pointOnCircle(const QPointF &center, qreal radius, qreal degrees)
{
    const qreal radians = qDegreesToRadians(degrees);
    return QPointF(center.x() + std::cos(radians) * radius,
                   center.y() - std::sin(radians) * radius);
}

QPainterPath buildDonutSliceArcPath(const QPointF &center,
                                    qreal radius,
                                    qreal startAngle,
                                    qreal span)
{
    QPainterPath path;
    if (span <= 0.0 || radius <= 0.0) {
        return path;
    }

    const QRectF arcRect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
    path.moveTo(pointOnCircle(center, radius, startAngle));
    path.arcTo(arcRect, startAngle, -span);
    return path;
}

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
        stopAnimation(&hoverAnimation_);
        stopAnimation(&revealAnimation_);
    }

    void setSlices(const QList<DonutSlice> &slices)
    {
        stopAnimation(&hoverAnimation_);
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
        hoverScale_ = 1.0;
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
        const qreal gapDegrees = sliceGapDegrees((outerRadius + innerRadius) / 2.0);

        QFont titleFont = font();
        titleFont.setPointSize(qMax(9, titleFont.pointSize()));
        titleFont.setBold(true);

        QFont detailFont = font();
        detailFont.setPointSize(qMax(8, detailFont.pointSize() - 2));
        detailFont.setBold(false);

        drawSlices(&painter, center, outerRadius, innerRadius, baseStart, gapDegrees, false);
        drawSlices(&painter, center, outerRadius, innerRadius, baseStart, gapDegrees, true);

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

            const bool hovered = (i == activeIndex_);
            const qreal scale = hovered ? hoverScale_ : 1.0;
            const qreal pieceOuter = outerRadius * scale;
            const qreal pieceInner = innerRadius * scale;
            const qreal pieceThickness = qMax<qreal>(1.0, pieceOuter - pieceInner);
            const qreal pieceRadius = (pieceOuter + pieceInner) / 2.0;
            const QPainterPath path = buildDonutSliceArcPath(center, pieceRadius, current, revealed);

            painter->save();
            painter->setOpacity(activeIndex_ >= 0 && i != activeIndex_ ? 0.58 : 1.0);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(slice.brush, pieceThickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(path);
            painter->restore();
            consumed += fullSpan;
            current -= fullSpan;
        }
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
        const QString detailLine = QStringLiteral("%1 台 · %2%")
                                       .arg(slices_[index].value)
                                       .arg(100.0 * slices_[index].value / total_, 0, 'f', 1);
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics detailMetrics(detailFont);
        const qreal maxCardWidth = qMin<qreal>(qMax<qreal>(74.0, width() - 28.0), 92.0);
        const qreal textMaxWidth = maxCardWidth - 16.0;
        const QRect titleBounds = titleMetrics.boundingRect(QRect(0, 0, static_cast<int>(textMaxWidth), 400), Qt::TextWordWrap, title);
        const qreal detailWidth = textWidth(detailMetrics, detailLine);
        qreal cardWidth = qMax<qreal>(74.0, qMax<qreal>(titleBounds.width(), detailWidth) + 16.0);
        cardWidth = qMin(cardWidth, maxCardWidth);

        const QRect finalTitleBounds = titleMetrics.boundingRect(QRect(0, 0, static_cast<int>(cardWidth - 16.0), 400), Qt::TextWordWrap, title);
        const qreal lineHeight = detailMetrics.height();
        const qreal cardHeight = 13.0 + finalTitleBounds.height() + lineHeight;

        const qreal midAngle = sliceMidAngle(index, baseStart, gapDegrees);
        const QPointF anchor = pointOnCircle(center, outerRadius + 3.5, midAngle);
        const QRectF bounds = rect().adjusted(10, 8, -10, -24);
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
                          detailLine);
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
            return std::hypot(dx, dy) < outerRadius + 2.0;
        };

        auto score = [anchor](const QRectF &rect) {
            const qreal dx = rect.center().x() - anchor.x();
            const qreal dy = rect.center().y() - anchor.y();
            return dx * dx + dy * dy;
        };

        QList<QRectF> candidates;
        candidates.append(QRectF(anchor.x() + 3.5, anchor.y() - cardHeight / 2.0, cardWidth, cardHeight));
        candidates.append(QRectF(anchor.x() - cardWidth - 3.5, anchor.y() - cardHeight / 2.0, cardWidth, cardHeight));
        candidates.append(QRectF(anchor.x() - cardWidth / 2.0, anchor.y() - cardHeight - 3.5, cardWidth, cardHeight));
        candidates.append(QRectF(anchor.x() - cardWidth / 2.0, anchor.y() + 3.5, cardWidth, cardHeight));
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
        const qreal gapDegrees = sliceGapDegrees((outerRadius + innerRadius) / 2.0);
        const qreal baseStart = 90.0;
        const qreal dx = pos.x() - center.x();
        const qreal dy = pos.y() - center.y();
        const qreal dist = std::hypot(dx, dy);
        if (dist < innerRadius || dist > outerRadius * 1.1) {
            setHoveredIndex(-1);
            return;
        }

        qreal angle = qRadiansToDegrees(std::atan2(dy, dx));
        angle += 90.0;
        angle = normalizeDegrees(angle);

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
        if (nextActiveIndex == activeIndex_ && hoveredIndex_ >= 0) {
            if (animate && !prefersReducedMotion()) {
                const qreal targetScale = nextActiveIndex >= 0 ? kHoverScale : 1.0;
                if (qAbs(hoverScale_ - targetScale) > 0.001) {
                    stopAnimation(&hoverAnimation_);
                    hoverAnimation_ = new QVariantAnimation(this);
                    hoverAnimation_->setDuration(static_cast<int>(kHoverAnimationDurationMs));
                    hoverAnimation_->setStartValue(hoverScale_);
                    hoverAnimation_->setEndValue(targetScale);
                    hoverAnimation_->setEasingCurve(QEasingCurve::OutCubic);
                    connect(hoverAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
                        hoverScale_ = value.toReal();
                        update();
                    });
                    connect(hoverAnimation_, &QVariantAnimation::finished, this, [this]() {
                        if (hoverAnimation_) {
                            hoverAnimation_->deleteLater();
                            hoverAnimation_ = nullptr;
                        }
                    });
                    hoverAnimation_->start();
                    return;
                }
            }
        }

        if (activeIndex_ != nextActiveIndex) {
            activeIndex_ = nextActiveIndex;
            if (hoveredIndex_ < 0 && activeIndex_ < 0) {
                hoverScale_ = 1.0;
            }
        }

        stopAnimation(&hoverAnimation_);
        if (prefersReducedMotion() || !animate) {
            hoverScale_ = activeIndex_ >= 0 ? kHoverScale : 1.0;
            update();
            return;
        }

        const qreal targetScale = activeIndex_ >= 0 ? kHoverScale : 1.0;
        hoverAnimation_ = new QVariantAnimation(this);
        hoverAnimation_->setDuration(static_cast<int>(kHoverAnimationDurationMs));
        hoverAnimation_->setStartValue(hoverScale_);
        hoverAnimation_->setEndValue(targetScale);
        hoverAnimation_->setEasingCurve(QEasingCurve::OutCubic);
        connect(hoverAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            hoverScale_ = value.toReal();
            update();
        });
        connect(hoverAnimation_, &QVariantAnimation::finished, this, [this]() {
            if (hoverAnimation_) {
                hoverAnimation_->deleteLater();
                hoverAnimation_ = nullptr;
            }
        });
        hoverAnimation_->start();
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
        const qreal gapDegrees = sliceGapDegrees((outerRadius + innerRadius) / 2.0);
        const qreal baseStart = 90.0;
        const qreal dx = pos.x() - center.x();
        const qreal dy = pos.y() - center.y();
        const qreal dist = std::hypot(dx, dy);
        if (dist < innerRadius || dist > outerRadius * 1.12) {
            return -1;
        }

        qreal angle = qRadiansToDegrees(std::atan2(dy, dx));
        angle += 90.0;
        angle = normalizeDegrees(angle);

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

    qreal sliceGapDegrees(qreal midRadius) const
    {
        if (total_ <= 0) {
            return 0.0;
        }

        qreal minSpan = 360.0;
        int positiveCount = 0;
        for (const auto &slice : slices_) {
            if (slice.value <= 0) {
                continue;
            }
            ++positiveCount;
            minSpan = qMin(minSpan, 360.0 * slice.value / total_);
        }
        if (positiveCount <= 0) {
            return 0.0;
        }

        const qreal targetGapDegrees = qRadiansToDegrees(kGapPx / qMax<qreal>(1.0, midRadius));
        return qMax<qreal>(0.0, qMin(targetGapDegrees, minSpan * 0.22));
    }

    QList<DonutSlice> slices_;
    int total_ = 0;
    int hoveredIndex_ = -1;
    int activeIndex_ = -1;
    int pinnedIndex_ = -1;
    qreal hoverScale_ = 1.0;
    qreal revealProgress_ = 1.0;
    QVariantAnimation *hoverAnimation_ = nullptr;
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

    chartView_ = new QChartView(chartPanel);
    chartView_->setMinimumHeight(280);
    chartView_->setRenderHint(QPainter::Antialiasing);
    chartLayout->addWidget(chartView_);

    chartHint_ = new QLabel("提示：鼠标移到折线点上可查看当天营收，点击点可固定查看数值。", chartPanel);
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
    selectedVerticalLine->setName("选中点竖向指示线");
    QPen guidePen(kGuideLineColor, 1);
    guidePen.setStyle(Qt::DashLine);
    selectedVerticalLine->setPen(guidePen);

    auto *selectedHorizontalLine = new QLineSeries();
    selectedHorizontalLine->setName("选中点横向指示线");
    selectedHorizontalLine->setPen(guidePen);

    auto *selectedSeries = new QScatterSeries();
    selectedSeries->setName("选中点");
    selectedSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    selectedSeries->setMarkerSize(12);
    selectedSeries->setColor(kAccentRed);
    selectedSeries->setBorderColor(kAccentRedDark);
    selectedSeries->setPointLabelsVisible(false);

    const QList<RevenuePoint> points = service_->revenueTrend(days);
    auto xValueForDate = [](const QDate &date) {
        return static_cast<qreal>(QDateTime(date, QTime(0, 0)).toMSecsSinceEpoch());
    };

    double maxValue = 0.0;
    for (int i = 0; i < points.size(); ++i) {
        const qreal x = xValueForDate(points[i].date);
        curve->append(x, points[i].amount);
        pointsSeries->append(x, points[i].amount);
        maxValue = qMax(maxValue, points[i].amount);
    }
    if (!points.isEmpty()) {
        if (selectedRevenueIndex_ < 0 || selectedRevenueIndex_ >= points.size()) {
            selectedRevenueIndex_ = qMin(4, points.size() - 1);
        }
        const double selectedX = xValueForDate(points[selectedRevenueIndex_].date);
        const double selectedY = points[selectedRevenueIndex_].amount;
        selectedVerticalLine->append(selectedX, 0.0);
        selectedVerticalLine->append(selectedX, selectedY);
        selectedHorizontalLine->append(xValueForDate(points.first().date), selectedY);
        selectedHorizontalLine->append(xValueForDate(points.last().date), selectedY);
        selectedSeries->append(selectedX, selectedY);
    }

    auto *chart = new QChart();
    chart->addSeries(selectedHorizontalLine);
    chart->addSeries(selectedVerticalLine);
    chart->addSeries(curve);
    chart->addSeries(pointsSeries);
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
    const int tickStep = 100;
    const int axisMax = qMax(tickStep, static_cast<int>(qCeil((maxValue + 60.0) / tickStep)) * tickStep);
    axisY->setRange(0, axisMax);
    axisY->setTickCount(axisMax / tickStep + 1);
    axisY->setLabelFormat("%.0f");
    axisY->setTitleText("");
    axisY->setGridLineColor(kGridColor);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    selectedHorizontalLine->attachAxis(axisX);
    selectedHorizontalLine->attachAxis(axisY);
    selectedVerticalLine->attachAxis(axisX);
    selectedVerticalLine->attachAxis(axisY);
    curve->attachAxis(axisX);
    curve->attachAxis(axisY);
    pointsSeries->attachAxis(axisX);
    pointsSeries->attachAxis(axisY);
    selectedSeries->attachAxis(axisX);
    selectedSeries->attachAxis(axisY);
    chartView_->setChart(chart);

    if (!points.isEmpty()) {
        auto *badgeBox = new QGraphicsPathItem(chart);
        badgeBox->setBrush(QBrush(kTitleColor));
        badgeBox->setPen(QPen(kTitleColor));
        badgeBox->setZValue(20);

        auto *badgeText = new QGraphicsSimpleTextItem(
            QString("%1 元").arg(points[selectedRevenueIndex_].amount, 0, 'f', 0), chart);
        badgeText->setBrush(QBrush(QColor("#fffaf4")));
        badgeText->setFont(QFont("Segoe UI", 10, QFont::DemiBold));
        badgeText->setZValue(21);

        auto updateBadge = [=]() {
            const QPointF pointPos = chart->mapToPosition(
                QPointF(xValueForDate(points[selectedRevenueIndex_].date), points[selectedRevenueIndex_].amount),
                selectedSeries);
            const QRectF textRect = badgeText->boundingRect();
            const double width = textRect.width() + 22.0;
            const double height = 24.0;
            const QRectF box(pointPos.x() - width / 2.0, pointPos.y() - 44.0, width, height);
            QPainterPath path;
            path.addRoundedRect(box, height / 2.0, height / 2.0);
            badgeBox->setPath(path);
            badgeText->setPos(box.x() + 11.0, box.y() + 3.0);
        };

        updateBadge();
        connect(chart, &QChart::plotAreaChanged, this, updateBadge);
    }

    auto indexForPoint = [points, xValueForDate](const QPointF &point) {
        int bestIndex = -1;
        qreal bestDistance = 1.0e30;
        for (int i = 0; i < points.size(); ++i) {
            const qreal distance = qAbs(point.x() - xValueForDate(points[i].date));
            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex = i;
            }
        }
        return bestIndex;
    };

    connect(pointsSeries, &QScatterSeries::hovered, this, [this, points, indexForPoint](const QPointF &point, bool state) {
        if (!state) {
            chartHint_->setText("提示：鼠标移到折线点上可查看当天营收，点击点可固定查看数值。");
            return;
        }

        const int index = indexForPoint(point);
        if (index < 0 || index >= points.size()) {
            return;
        }
        const QString text = QString("%1：%2 元")
                                 .arg(points[index].date.toString("yyyy-MM-dd"))
                                 .arg(points[index].amount, 0, 'f', 2);
        chartHint_->setText(text);
        QToolTip::showText(QCursor::pos(), text, chartView_);
    });

    connect(pointsSeries, &QScatterSeries::clicked, this, [this, points, indexForPoint](const QPointF &point) {
        const int index = indexForPoint(point);
        if (index < 0 || index >= points.size()) {
            return;
        }
        selectedRevenueIndex_ = index;
        updateChart(rangeBox_->currentData().toInt());
        chartHint_->setText(QString("已选中 %1，营收 %2 元")
                                .arg(points[index].date.toString("yyyy-MM-dd"))
                                .arg(points[index].amount, 0, 'f', 2));
    });

    connect(selectedSeries, &QScatterSeries::hovered, this, [this, points, indexForPoint](const QPointF &point, bool state) {
        if (!state) {
            chartHint_->setText("提示：鼠标移到折线点上可查看当天营收，点击点可固定查看数值。");
            return;
        }

        const int index = indexForPoint(point);
        if (index < 0 || index >= points.size()) {
            return;
        }
        const QString text = QString("选中点 %1：%2 元")
                                 .arg(points[index].date.toString("yyyy-MM-dd"))
                                 .arg(points[index].amount, 0, 'f', 2);
        chartHint_->setText(text);
        QToolTip::showText(QCursor::pos(), text, chartView_);
    });

    connect(selectedSeries, &QScatterSeries::clicked, this, [this, points, indexForPoint](const QPointF &point) {
        const int index = indexForPoint(point);
        if (index < 0 || index >= points.size()) {
            return;
        }
        selectedRevenueIndex_ = index;
        updateChart(rangeBox_->currentData().toInt());
        chartHint_->setText(QString("已选中 %1，营收 %2 元")
                                .arg(points[index].date.toString("yyyy-MM-dd"))
                                .arg(points[index].amount, 0, 'f', 2));
    });
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
