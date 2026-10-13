#pragma once

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <functional>
#include <iterator>

namespace Zqxxxt {
// A shared list for fixed, squad and stage annotations. Folder actions are
// deliberately owned by each caller; these are geometry types only.
inline void addAnnotationTypeActions(QMenu &menu, bool plot,
    const std::function<void(const QString &)> &create)
{
        struct Entry { const char *label; const char *kind; const char *icon; };
        static const Entry markEntries[]{
            {"点标记", "point", "point.png"},
            {"线标记", "line", "line.png"},
            {"多边形标记", "polygon", "polygon.png"},
            {"模型标记", "model", "model.png"},
            {"广告牌标记", "billboard", "billboard.png"},
            {"遮盖标签", "overlay", "cover.png"},
            {"弧形标记", "arc", "arc.png"},
            {"椭圆标记", "ellipse", "ellipse.png"},
            {"矩形标记", "rectangle", "rectangle.png"},
            {"扇形标记", "sector", "sector.png"},
            {"圆形标记", "circle", "circle.png"},
            {"文本标记", "text", "text.png"},
            {"倾斜摄影", "tilt-model", "oblique_model.png"},
            {"粒子标记", "particle", "particle.png"},
            {"标牌标记", "label", "label.png"}
        };
        static const Entry plotEntries[]{
            {"直箭头", "plot-straight-arrow", "StraightArrow.png"},
            {"多点直箭头", "plot-straight-multi-arrow", "StraightMoreArrow.png"},
            {"燕尾斜箭头", "plot-dovetail-diagonal-arrow", "DovetailDiagonalArrow.png"},
            {"多点燕尾斜箭头", "plot-dovetail-diagonal-multi-arrow", "DovetailDiagonalMoreArrow.png"},
            {"燕尾直箭头", "plot-dovetail-straight-arrow", "DoveTailStraightArrow.png"},
            {"多点燕尾直箭头", "plot-dovetail-straight-multi-arrow", "DoveTailStraightMoreArrow.png"},
            {"斜箭头", "plot-diagonal-arrow", "DiagonalArrow.png"},
            {"多点斜箭头", "plot-diagonal-multi-arrow", "DiagonalMoreArrow.png"},
            {"双箭头", "plot-double-arrow", "DoubleArrow.png"},
            {"闭合曲线", "plot-closed-curve", "CloseCurve.png"},
            {"圆角矩形", "plot-rounded-rect", "RoundedRect.png"},
            {"直线箭头", "plot-straight-line-arrow", "StraightLineArrow.png"},
            {"贝塞尔曲线箭头", "plot-bezier-arrow", "BezierCurveArrow.png"},
            {"折线箭头", "plot-polyline-arrow", "PolyLineArrow.png"},
            {"平行搜索", "plot-parallel-search", "ParallelSearch.png"},
            {"扇形搜索", "plot-sector-search", "SectorSearch.png"},
            {"基数曲线箭头", "plot-cardinal-arrow", "CardinalCurveArrow.png"}
        };

    const Entry *entries = plot ? plotEntries : markEntries;
    const int count = plot ? int(std::size(plotEntries)) : int(std::size(markEntries));
    for (int index = 0; index < count; ++index) {
        const QString kind = QString::fromLatin1(entries[index].kind);
        auto *action = menu.addAction(QIcon(QStringLiteral(":/deployment/icons/")
            + QString::fromLatin1(entries[index].icon)), QString::fromUtf8(entries[index].label));
        action->setData(kind);
        QObject::connect(action, &QAction::triggered, &menu, [create, kind] { create(kind); });
    }
}
} // namespace Zqxxxt
