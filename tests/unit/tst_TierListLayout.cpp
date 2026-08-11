#include "tier/TierListLayout.h"
#include "tier/TierListDelegate.h"
#include "tier/TierDragController.h"
#include "tier/TierListModel.h"
#include "tier/TierListView.h"
#include "tier/ProjectHistory.h"

#include <QtTest>

#include <QDragMoveEvent>
#include <QDropEvent>

#include <limits>
#include <memory>
#include <utility>

using namespace qtm;

class TierListLayoutTest final : public QObject {
    Q_OBJECT

private slots:
    void girlsProjectKeepsEveryImageInsideItsRow_data();
    void girlsProjectKeepsEveryImageInsideItsRow();
    void expandedWidthsReflowRows();
    void viewResizeRecomputesLayout();
    void noCropPreservesAspectAndSharedLineHeight();
    void missionControlBalancesCenteredFreePacking();
    void missionControlHoverKeepsNonLocalTilesFixed();
    void galleryMissionControlHoverPreservesLocalityAndGap();
    void resettingViewStateEndsMissionControlImmediately();
    void imageDropClearsTransientVisualsBeforeModelMutation();
    void historyUndoRefreshesClearedRowModelData();
};

namespace {
class DropTestView final : public TierListView {
public:
    using TierListView::TierListView;

    bool moveImageAt(const QString& imageId, const QPoint& viewportPosition) {
        std::unique_ptr<QMimeData> mimeData(TierDragController::createMimeData(imageId));
        QDragMoveEvent moveEvent(viewportPosition, Qt::MoveAction, mimeData.get(), Qt::LeftButton,
                                 Qt::NoModifier);
        dragMoveEvent(&moveEvent);
        if (!moveEvent.isAccepted()) {
            return false;
        }
        QDropEvent dropEvent(viewportPosition, Qt::MoveAction, mimeData.get(), Qt::LeftButton,
                             Qt::NoModifier);
        TierListView::dropEvent(&dropEvent);
        return dropEvent.isAccepted();
    }
};

TierProject girlsProject() {
    TierProject project;
    const QVector<int> imageCounts{6, 6, 7, 31, 12};
    const QStringList labels{QStringLiteral("S"), QStringLiteral("A"), QStringLiteral("B"),
                             QStringLiteral("C"), QStringLiteral("D")};
    for (int row = 0; row < imageCounts.size(); ++row) {
        TierRow tierRow;
        tierRow.id = QStringLiteral("row-%1").arg(row);
        tierRow.label = labels.at(row);
        tierRow.order = row;
        for (int image = 0; image < imageCounts.at(row); ++image) {
            tierRow.imageIds.append(QStringLiteral("image-%1-%2").arg(row).arg(image));
        }
        project.rows.append(std::move(tierRow));
    }
    return project;
}

bool rectsConflictWithGap(const QRectF& first, const QRectF& second, qreal gap) {
    const QRectF expandedFirst = first.adjusted(-gap / 2.0, -gap / 2.0, gap / 2.0, gap / 2.0);
    const QRectF expandedSecond = second.adjusted(-gap / 2.0, -gap / 2.0, gap / 2.0, gap / 2.0);
    const QRectF overlap = expandedFirst.intersected(expandedSecond);
    return overlap.width() > 0.01 && overlap.height() > 0.01;
}

bool canGrowRectWithGap(const QVector<QRectF>& rects, int index, const QSizeF& baseSize,
                        qreal scale, const QRectF& movementBounds, qreal gap) {
    const QRectF current = rects.at(index);
    const QSizeF targetSize = baseSize * scale;
    for (int horizontalAnchor = -1; horizontalAnchor <= 1; ++horizontalAnchor) {
        const qreal left =
            horizontalAnchor < 0
                ? current.left()
                : (horizontalAnchor > 0 ? current.right() - targetSize.width()
                                        : current.center().x() - targetSize.width() / 2.0);
        for (int verticalAnchor = -1; verticalAnchor <= 1; ++verticalAnchor) {
            const qreal top =
                verticalAnchor < 0
                    ? current.top()
                    : (verticalAnchor > 0 ? current.bottom() - targetSize.height()
                                          : current.center().y() - targetSize.height() / 2.0);
            const QRectF candidate(QPointF(left, top), targetSize);
            if (candidate.left() < movementBounds.left() - 0.25 ||
                candidate.top() < movementBounds.top() - 0.25 ||
                candidate.right() > movementBounds.right() + 0.25 ||
                candidate.bottom() > movementBounds.bottom() + 0.25) {
                continue;
            }

            bool conflict = false;
            for (int other = 0; other < rects.size(); ++other) {
                if (other != index && rectsConflictWithGap(candidate, rects.at(other), gap)) {
                    conflict = true;
                    break;
                }
            }
            if (!conflict) {
                return true;
            }
        }
    }
    return false;
}

qreal nearestAlignedGap(const QVector<QRectF>& rects, int index) {
    const QRectF current = rects.at(index);
    qreal nearest = std::numeric_limits<qreal>::max();
    for (int other = 0; other < rects.size(); ++other) {
        if (other == index) {
            continue;
        }
        const QRectF candidate = rects.at(other);
        const qreal verticalOverlap =
            qMin(current.bottom(), candidate.bottom()) - qMax(current.top(), candidate.top());
        if (verticalOverlap > 0.01) {
            nearest = qMin(nearest, qMax(qMax(candidate.left() - current.right(),
                                              current.left() - candidate.right()),
                                         0.0));
        }
        const qreal horizontalOverlap =
            qMin(current.right(), candidate.right()) - qMax(current.left(), candidate.left());
        if (horizontalOverlap > 0.01) {
            nearest = qMin(nearest, qMax(qMax(candidate.top() - current.bottom(),
                                              current.top() - candidate.bottom()),
                                         0.0));
        }
    }
    return nearest;
}

QRectF missionHoverTargetForTest(const QRectF& base, const QSizeF& viewportSize) {
    const qreal shortSide = qMax<qreal>(1.0, qMin(viewportSize.width(), viewportSize.height()));
    const qreal layoutMargin = qBound<qreal>(12.0, shortSide / 34.0, 24.0);
    const qreal safeMargin = layoutMargin + qBound<qreal>(6.0, shortSide / 96.0, 14.0);
    const QRectF safeBounds(
        QPointF(safeMargin, safeMargin),
        QSizeF(viewportSize.width() - safeMargin * 2.0, viewportSize.height() - safeMargin * 2.0));
    const qreal boardShortSide = qMin(safeBounds.width(), safeBounds.height());
    const qreal boardLongSide = qMax(safeBounds.width(), safeBounds.height());
    const qreal baseLongSide = qMax(base.width(), base.height());
    const qreal preferredScale =
        qBound<qreal>(1.58, boardShortSide / qMax<qreal>(1.0, baseLongSide * 3.15), 2.45);
    const qreal maximumLongSide =
        qMax(baseLongSide * 1.28, qMin(boardShortSide * 0.42, boardLongSide * 0.34));
    const qreal minimumLongSide = qMin(baseLongSide * 1.42, maximumLongSide);
    const qreal targetLongSide =
        qBound(minimumLongSide, baseLongSide * preferredScale, maximumLongSide);
    QSizeF targetSize = base.size() * (targetLongSide / qMax<qreal>(1.0, baseLongSide));
    targetSize *= qMin<qreal>(1.0, qMin(safeBounds.width() / targetSize.width(),
                                        safeBounds.height() / targetSize.height()));

    QRectF target(QPointF(), targetSize);
    target.moveCenter(base.center());
    target.moveLeft(qBound(safeBounds.left(), target.left(), safeBounds.right() - target.width()));
    target.moveTop(qBound(safeBounds.top(), target.top(), safeBounds.bottom() - target.height()));
    return target;
}

} // namespace

void TierListLayoutTest::girlsProjectKeepsEveryImageInsideItsRow_data() {
    QTest::addColumn<QSize>("viewportSize");
    QTest::newRow("normal-sidebar") << QSize(854, 650);
    QTest::newRow("collapsed-sidebar-threshold") << QSize(1090, 650);
    QTest::newRow("focus-mode") << QSize(1148, 780);
    QTest::newRow("minimum-window") << QSize(700, 550);
}

void TierListLayoutTest::girlsProjectKeepsEveryImageInsideItsRow() {
    QFETCH(QSize, viewportSize);
    const QVector<int> imageCounts{6, 6, 7, 31, 12};
    constexpr int labelWidth = 82;

    const TierBoardLayoutMetrics layout =
        TierListLayout::fitBoard(imageCounts, viewportSize, labelWidth);
    QCOMPARE(layout.rowHeights.size(), imageCounts.size());
    QCOMPARE(layout.rowUnits.size(), imageCounts.size());

    int rowTop = 0;
    for (int row = 0; row < imageCounts.size(); ++row) {
        const QRect rowRect(0, rowTop, viewportSize.width(), layout.rowHeights.at(row));
        const TierRowGrid grid =
            TierListLayout::gridForRow(rowRect, layout.rowUnits.at(row), labelWidth);
        QVERIFY2(grid.requiredRows(imageCounts.at(row)) <= layout.rowUnits.at(row),
                 "The board allocated fewer lines than its delegate grid requires.");
        if (imageCounts.at(row) > 0) {
            const QRect lastTile = grid.tileRect(imageCounts.at(row) - 1);
            QVERIFY2(rowRect.contains(lastTile.topLeft()) && rowRect.contains(lastTile.bottomRight()),
                     "The last image falls outside the tier row and would be clipped.");
        }
        rowTop += rowRect.height();
    }
    QCOMPARE(rowTop, viewportSize.height());
}

void TierListLayoutTest::expandedWidthsReflowRows() {
    const QVector<int> imageCounts{6, 6, 7, 31, 12};
    const TierBoardLayoutMetrics normal =
        TierListLayout::fitBoard(imageCounts, QSize(854, 650), 82);
    const TierBoardLayoutMetrics collapsed =
        TierListLayout::fitBoard(imageCounts, QSize(1090, 650), 82);

    QVERIFY(normal.rowUnits != collapsed.rowUnits);
    QVERIFY(collapsed.rowUnits.at(3) <= normal.rowUnits.at(3));
}

void TierListLayoutTest::noCropPreservesAspectAndSharedLineHeight() {
    const QVector<QVector<QSize>> imageSizes{
        {QSize(1600, 900), QSize(900, 1200), QSize(1200, 800), QSize(1024, 1024)},
        {QSize(2400, 1000), QSize(800, 1200), QSize(1280, 720)},
    };
    constexpr int labelWidth = 82;
    const QSize viewport(900, 420);
    const TierBoardLayoutMetrics layout = TierListLayout::fitBoard(
        imageSizes, viewport, labelWidth, ImagePresentationMode::NoCrop);
    QCOMPARE(layout.rowHeights.size(), imageSizes.size());
    QCOMPARE(layout.rowUnits.size(), imageSizes.size());

    int top = 0;
    for (int row = 0; row < imageSizes.size(); ++row) {
        const QRect rowRect(0, top, viewport.width(), layout.rowHeights.at(row));
        const TierRowGrid grid =
            TierListLayout::gridForRow(rowRect, layout.rowUnits.at(row), labelWidth);
        const QVector<QRect> rects =
            grid.itemRects(imageSizes.at(row), ImagePresentationMode::NoCrop);
        QCOMPARE(rects.size(), imageSizes.at(row).size());
        QVERIFY(grid.requiredRows(imageSizes.at(row), ImagePresentationMode::NoCrop) <=
                layout.rowUnits.at(row));
        for (int index = 0; index < rects.size(); ++index) {
            const QRect rect = rects.at(index);
            QVERIFY(rowRect.contains(rect.topLeft()));
            QVERIFY(rowRect.contains(rect.bottomRight()));
            QCOMPARE(rect.height(), grid.lineHeight);
            const qreal expectedAspect =
                static_cast<qreal>(imageSizes.at(row).at(index).width()) /
                imageSizes.at(row).at(index).height();
            const qreal actualAspect = static_cast<qreal>(rect.width()) / rect.height();
            QVERIFY(qAbs(actualAspect - expectedAspect) <= 1.0 / grid.lineHeight + 0.01);
            for (int previous = 0; previous < index; ++previous) {
                QVERIFY(!rect.intersects(rects.at(previous)));
            }
        }
        top += rowRect.height();
    }
    QCOMPARE(top, viewport.height());
}

void TierListLayoutTest::viewResizeRecomputesLayout() {
    TierProject project = girlsProject();
    TierListModel model;
    TierListDelegate delegate;
    TierListView view;
    delegate.setContext(&project, nullptr, nullptr, {});
    view.setModel(&model);
    view.setItemDelegate(&delegate);
    model.setProject(&project);

    // Reflow only needs widget events; native exposure is nondeterministic on headless CI runners.
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(854, 650);
    view.show();
    QCoreApplication::processEvents();
    QTRY_VERIFY(view.viewport()->width() > 0 && view.viewport()->height() > 0);
    const QSize normalViewportSize = view.viewport()->size();
    const TierBoardLayoutMetrics normal =
        TierListLayout::fitBoard({6, 6, 7, 31, 12}, normalViewportSize,
                                 delegate.labelWidth());
    QTRY_COMPARE(model.rowUnitCountAt(3), normal.rowUnits.at(3));
    QTRY_COMPARE(model.rowUnitCountAt(4), normal.rowUnits.at(4));

    view.resize(1090, 650);
    QTRY_VERIFY(view.viewport()->width() > normalViewportSize.width());
    const TierBoardLayoutMetrics collapsed =
        TierListLayout::fitBoard({6, 6, 7, 31, 12}, view.viewport()->size(),
                                 delegate.labelWidth());
    QTRY_COMPARE(model.rowUnitCountAt(3), collapsed.rowUnits.at(3));
    QTRY_COMPARE(model.rowUnitCountAt(4), collapsed.rowUnits.at(4));
    QVERIFY(normal.rowUnits != collapsed.rowUnits);
}

void TierListLayoutTest::resettingViewStateEndsMissionControlImmediately() {
    TierListView view;
    view.setGalleryMissionControlActive(true);
    QVERIFY(view.isMissionControlActive());
    QVERIFY(view.isGalleryMissionLayerVisible());

    view.resetViewState();

    QVERIFY(!view.isMissionControlActive());
    QVERIFY(!view.isGalleryMissionLayerVisible());
    QCOMPARE(view.missionTransitionProgress(), 0.0);
}

void TierListLayoutTest::imageDropClearsTransientVisualsBeforeModelMutation() {
    TierProject project = TierProject::createUntitled();
    TierImage image;
    image.id = QStringLiteral("drop-image");
    image.width = 800;
    image.height = 600;
    project.images.append(image);
    project.rows[0].imageIds.append(image.id);
    project.normalizeOrdering();

    TierListModel model;
    TierListDelegate delegate;
    DropTestView view;
    delegate.setContext(&project, nullptr, nullptr, {});
    view.setModel(&model);
    view.setItemDelegate(&delegate);
    model.setProject(&project);
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(760, 500);
    view.show();
    QCoreApplication::processEvents();

    bool committed = false;
    bool transientVisualsAtCommit = true;
    connect(&view, &TierListView::imageDropped, &view,
            [&](const QString& imageId, const QString& rowId, int insertionIndex) {
                committed = true;
                transientVisualsAtCommit = view.isImageDragSource(imageId) ||
                                           !view.visualOffsetForImage(imageId).isNull();
                for (TierRow& row : project.rows) {
                    row.imageIds.removeAll(imageId);
                }
                TierRow* target = project.rowById(rowId);
                QVERIFY(target);
                target->imageIds.insert(
                    qBound(0, insertionIndex, static_cast<int>(target->imageIds.size())), imageId);
                project.normalizeOrdering();
                model.setProject(&project);
            });

    const QRect targetRow = view.visualRect(model.index(1, 0));
    QVERIFY(targetRow.isValid());
    QVERIFY(view.moveImageAt(image.id, targetRow.center()));
    QVERIFY(committed);
    QVERIFY(!transientVisualsAtCommit);
    QVERIFY(!view.isImageDragSource(image.id));
    QCOMPARE(project.rows.at(0).imageIds.count(image.id), 0);
    QCOMPARE(project.rows.at(1).imageIds.count(image.id), 1);
}

void TierListLayoutTest::historyUndoRefreshesClearedRowModelData() {
    ProjectHistory::State current{TierProject::createUntitled(), QStringLiteral("clear-image")};
    TierImage image;
    image.id = current.selectedImageId;
    current.project.images.append(image);
    current.project.rows[0].imageIds.append(image.id);
    current.project.normalizeOrdering();

    TierListModel model;
    model.setProject(&current.project);
    ProjectHistory history([&](const ProjectHistory::State& state) { current = state; });
    connect(&history, &ProjectHistory::stateChanged, &model,
            [&](bool, int) { model.setProject(&current.project); });

    TierRow* row = current.project.rowById(current.project.rows.at(0).id);
    QVERIFY(row);
    const ProjectHistory::State beforeClear{current.project, current.selectedImageId};
    row->imageIds.clear();
    current.project.normalizeOrdering();
    current.project.touch();
    QVERIFY(history.push(beforeClear, current, QStringLiteral("Clear tier row")));
    QVERIFY(model.index(0, 0).data(TierListModel::ImageIdsRole).toStringList().isEmpty());

    history.undo();
    QCOMPARE(model.index(0, 0).data(TierListModel::ImageIdsRole).toStringList(),
             QStringList{image.id});
    QCOMPARE(model.index(0, 0).data(TierListModel::LabelRole).toString(),
             beforeClear.project.rows.at(0).label);
}

void TierListLayoutTest::missionControlBalancesCenteredFreePacking() {
    const QVector<QSizeF> sourceSizes{
        {1000.0, 1500.0}, {1500.0, 2000.0}, {1600.0, 2000.0}, {1800.0, 2000.0},
        {2400.0, 2400.0}, {3600.0, 2000.0}, {3000.0, 2250.0}, {2400.0, 2000.0},
        {3000.0, 1800.0}, {2400.0, 1200.0},
    };
    const QRectF bounds(20.0, 30.0, 1200.0, 700.0);
    constexpr qreal gap = 12.0;
    constexpr qreal epsilon = 0.01;

    const MissionControlLayoutMetrics layout =
        TierListLayout::fitMissionControl(sourceSizes, bounds, gap);
    QCOMPARE(layout.itemRects.size(), sourceSizes.size());
    QVERIFY(layout.scale > 0.0);
    QVERIFY(qMin(layout.horizontalOccupancy, layout.verticalOccupancy) > 0.55);
    QVERIFY(qAbs(layout.horizontalOccupancy - layout.verticalOccupancy) < 0.30);
    const qreal packedDensity =
        layout.imageAreaOccupancy /
        (layout.horizontalOccupancy * layout.verticalOccupancy);
    QVERIFY(packedDensity > 0.50);

    QRectF groupRect;
    bool hasDifferentHeights = false;
    for (int index = 0; index < layout.itemRects.size(); ++index) {
        const QRectF rect = layout.itemRects.at(index);
        QVERIFY(rect.left() >= bounds.left() - epsilon);
        QVERIFY(rect.top() >= bounds.top() - epsilon);
        QVERIFY(rect.right() <= bounds.right() + epsilon);
        QVERIFY(rect.bottom() <= bounds.bottom() + epsilon);
        QVERIFY(qAbs(rect.width() / rect.height() -
                     sourceSizes.at(index).width() / sourceSizes.at(index).height()) <
                epsilon);
        QVERIFY(qAbs(rect.width() / sourceSizes.at(index).width() - layout.scale) < epsilon);
        QVERIFY(qAbs(rect.height() / sourceSizes.at(index).height() - layout.scale) < epsilon);
        if (index > 0 &&
            qAbs(rect.height() - layout.itemRects.constFirst().height()) > epsilon) {
            hasDifferentHeights = true;
        }
        groupRect = groupRect.isValid() ? groupRect.united(rect) : rect;
    }
    QVERIFY(hasDifferentHeights);
    QVERIFY(qAbs(groupRect.center().x() - bounds.center().x()) < epsilon);
    QVERIFY(qAbs(groupRect.center().y() - bounds.center().y()) < epsilon);

    for (int first = 0; first < layout.itemRects.size(); ++first) {
        for (int second = first + 1; second < layout.itemRects.size(); ++second) {
            const QRectF left = layout.itemRects.at(first);
            const QRectF right = layout.itemRects.at(second);
            const bool separated = left.right() + gap <= right.left() + epsilon ||
                                   right.right() + gap <= left.left() + epsilon ||
                                   left.bottom() + gap <= right.top() + epsilon ||
                                   right.bottom() + gap <= left.top() + epsilon;
            QVERIFY2(separated, "Mission Control images overlap or violate the fixed gap.");
        }
    }
}

void TierListLayoutTest::missionControlHoverKeepsNonLocalTilesFixed() {
    const QVector<QRectF> baseRects{
        {100.0, 200.0, 100.0, 100.0}, {210.0, 200.0, 100.0, 100.0}, {320.0, 200.0, 100.0, 100.0},
        {430.0, 200.0, 100.0, 100.0}, {540.0, 200.0, 100.0, 100.0}, {650.0, 200.0, 100.0, 100.0},
    };
    const QRectF hoverTarget(10.0, 140.0, 280.0, 220.0);
    const QRectF movementBounds(0.0, 0.0, 800.0, 500.0);
    constexpr qreal gap = 10.0;

    for (qreal progress : {0.2, 0.5, 1.0}) {
        const MissionControlHoverLayoutMetrics layout = TierListLayout::applyMissionControlHover(
            baseRects, 0, hoverTarget, movementBounds, progress, gap);

        QCOMPARE(layout.itemRects.size(), baseRects.size());
        QCOMPARE(layout.localNeighborCount, 1);
        QVERIFY(qAbs(layout.appliedProgress - progress) < 0.001);
        QVERIFY(!layout.constrained);
        const QRectF expectedHover = QRectF(
            baseRects.at(0).topLeft() +
                (hoverTarget.topLeft() - baseRects.at(0).topLeft()) * progress,
            baseRects.at(0).size() + (hoverTarget.size() - baseRects.at(0).size()) * progress);
        QCOMPARE(layout.itemRects.at(0), expectedHover);
        QVERIFY(layout.itemRects.at(0).width() > baseRects.at(0).width());
        QVERIFY(layout.itemRects.at(0).height() > baseRects.at(0).height());
        QVERIFY(layout.itemRects.at(1) != baseRects.at(1));
        for (int index = 2; index < baseRects.size(); ++index) {
            QCOMPARE(layout.itemRects.at(index), baseRects.at(index));
        }
        for (int first = 0; first < layout.itemRects.size(); ++first) {
            for (int second = first + 1; second < layout.itemRects.size(); ++second) {
                QVERIFY(!rectsConflictWithGap(layout.itemRects.at(first),
                                              layout.itemRects.at(second), gap));
            }
        }
    }
}

void TierListLayoutTest::galleryMissionControlHoverPreservesLocalityAndGap() {
    // Anime Girls v5 reproduces the regression because its gallery mixes extreme aspect ratios.
    const QVector<QSizeF> sourceSizes{
        {1900.0, 1069.0}, {2480.0, 3508.0}, {3223.0, 4021.0}, {3557.0, 1949.0}, {1920.0, 1080.0},
        {2500.0, 4000.0}, {3680.0, 2162.0}, {2901.0, 4304.0}, {3330.0, 2449.0}, {4294.0, 5368.0},
        {2500.0, 3700.0}, {2400.0, 1440.0}, {2366.0, 3600.0}, {4006.0, 6076.0}, {3277.0, 4096.0},
        {1372.0, 2048.0}, {6344.0, 3480.0}, {1638.0, 2048.0}, {1333.0, 2000.0}, {3541.0, 2508.0},
        {2480.0, 3508.0}, {2550.0, 1650.0}, {1593.0, 2048.0}, {3840.0, 2156.0}, {1404.0, 2000.0},
        {2894.0, 4093.0}, {1200.0, 1697.0}, {1254.0, 1771.0}, {3200.0, 5200.0}, {3024.0, 4032.0},
        {2305.0, 4096.0}, {5787.0, 5787.0}, {2948.0, 5262.0}, {1640.0, 2360.0}, {2048.0, 1260.0},
        {8640.0, 4860.0},
    };
    constexpr qreal gap = 8.3;
    const QRectF layoutBounds(21.0, 21.0, 1106.0, 668.0);
    const QRectF movementBounds(0.0, 0.0, 1148.0, 710.0);
    const MissionControlLayoutMetrics baseLayout =
        TierListLayout::fitMissionControl(sourceSizes, layoutBounds, gap);
    QCOMPARE(baseLayout.itemRects.size(), sourceSizes.size());

    constexpr int hoverIndex = 35;
    const QRectF hoverTarget(359.0, 233.0, 427.0, 240.0);
    for (qreal progress : {0.2, 0.5, 1.0}) {
        const MissionControlHoverLayoutMetrics hoverLayout =
            TierListLayout::applyMissionControlHover(baseLayout.itemRects, hoverIndex, hoverTarget,
                                                     movementBounds, progress, gap);
        QVERIFY2(qAbs(hoverLayout.appliedProgress - progress) < 0.001,
                 qPrintable(QStringLiteral("requested=%1 applied=%2 constrained=%3")
                                .arg(progress, 0, 'f', 3)
                                .arg(hoverLayout.appliedProgress, 0, 'f', 3)
                                .arg(hoverLayout.constrained)));
        QVERIFY2(!hoverLayout.constrained,
                 qPrintable(QStringLiteral("Hover projection was unresolved at progress %1.")
                                .arg(progress, 0, 'f', 3)));
        const QSizeF expectedHoverSize =
            baseLayout.itemRects.at(hoverIndex).size() +
            (hoverTarget.size() - baseLayout.itemRects.at(hoverIndex).size()) * progress;
        const QRectF expectedHover(
            baseLayout.itemRects.at(hoverIndex).topLeft() +
                (hoverTarget.topLeft() - baseLayout.itemRects.at(hoverIndex).topLeft()) * progress,
            expectedHoverSize);
        QCOMPARE(hoverLayout.itemRects.at(hoverIndex), expectedHover);
        QVERIFY(hoverLayout.localNeighborCount > 0);
        QCOMPARE(hoverLayout.affectedNeighborCount, hoverLayout.localNeighborCount);
        QVERIFY(hoverLayout.changedNeighborCount <= hoverLayout.affectedNeighborCount);

        for (int index = 0; index < baseLayout.itemRects.size(); ++index) {
            const QRectF displayedRect = hoverLayout.itemRects.at(index);
            if (index != hoverIndex) {
                if (!rectsConflictWithGap(baseLayout.itemRects.at(index), expectedHover, gap)) {
                    QCOMPARE(displayedRect, baseLayout.itemRects.at(index));
                }
                const QSizeF baseSize = baseLayout.itemRects.at(index).size();
                const qreal widthScale = displayedRect.width() / baseSize.width();
                const qreal heightScale = displayedRect.height() / baseSize.height();
                QVERIFY(widthScale <= 1.001);
                QVERIFY(widthScale >= 0.579);
                QVERIFY(qAbs(widthScale - heightScale) < 0.001);
            }
            QVERIFY(displayedRect.left() >= movementBounds.left() - 0.25);
            QVERIFY(displayedRect.top() >= movementBounds.top() - 0.25);
            QVERIFY(displayedRect.right() <= movementBounds.right() + 0.25);
            QVERIFY(displayedRect.bottom() <= movementBounds.bottom() + 0.25);
        }
        for (int first = 0; first < hoverLayout.itemRects.size(); ++first) {
            for (int second = first + 1; second < hoverLayout.itemRects.size(); ++second) {
                QVERIFY2(!rectsConflictWithGap(hoverLayout.itemRects.at(first),
                                               hoverLayout.itemRects.at(second), gap),
                         "Hover layout folded images or changed the fixed gap.");
            }
        }
    }

    qreal worstDisplacementRatio = 0.0;
    int worstDisplacementHover = -1;
    int worstDisplacementImage = -1;
    int worstRankShift = 0;
    int worstRankHover = -1;
    qreal minimumRankCorrelation = 1.0;
    int minimumRankCorrelationHover = -1;
    for (int candidateIndex = 0; candidateIndex < baseLayout.itemRects.size(); ++candidateIndex) {
        const QRectF candidateTarget = missionHoverTargetForTest(
            baseLayout.itemRects.at(candidateIndex), movementBounds.size());
        const MissionControlHoverLayoutMetrics candidateLayout =
            TierListLayout::applyMissionControlHover(baseLayout.itemRects, candidateIndex,
                                                     candidateTarget, movementBounds, 1.0, gap);
        QVERIFY2(!candidateLayout.constrained,
                 qPrintable(QStringLiteral("Full hover target was unresolved for image %1.")
                                .arg(candidateIndex)));
        QCOMPARE(candidateLayout.itemRects.at(candidateIndex), candidateTarget);
        QVERIFY(candidateLayout.changedNeighborCount <= candidateLayout.affectedNeighborCount);
        QVERIFY(candidateLayout.affectedNeighborCount < baseLayout.itemRects.size() / 2);
        QVector<QPair<qreal, int>> baseDistances;
        QVector<QPair<qreal, int>> displayedDistances;
        for (int first = 0; first < candidateLayout.itemRects.size(); ++first) {
            if (first != candidateIndex) {
                const QSizeF baseSize = baseLayout.itemRects.at(first).size();
                const QSizeF displayedSize = candidateLayout.itemRects.at(first).size();
                const qreal widthScale = displayedSize.width() / baseSize.width();
                const qreal heightScale = displayedSize.height() / baseSize.height();
                QVERIFY(widthScale <= 1.001);
                QVERIFY(widthScale >= 0.579);
                QVERIFY(qAbs(widthScale - heightScale) < 0.001);
                if (widthScale < 0.999) {
                    const qreal largerScale = qMin<qreal>(1.0, widthScale + 0.01);
                    QVERIFY2(!canGrowRectWithGap(candidateLayout.itemRects, first, baseSize,
                                                largerScale, movementBounds, gap),
                             "A hover neighbor remained smaller than its feasible local maximum.");
                    QVERIFY2(nearestAlignedGap(candidateLayout.itemRects, first) <= gap + 0.25,
                             "A reduced hover neighbor no longer preserves the configured gap.");
                }
                const QPointF displacement = candidateLayout.itemRects.at(first).center() -
                                             baseLayout.itemRects.at(first).center();
                const qreal displacementRatio = std::hypot(displacement.x(), displacement.y()) /
                                                std::hypot(baseSize.width(), baseSize.height());
                if (displacementRatio > worstDisplacementRatio) {
                    worstDisplacementRatio = displacementRatio;
                    worstDisplacementHover = candidateIndex;
                    worstDisplacementImage = first;
                }
                const QPointF baseRadial = baseLayout.itemRects.at(first).center() -
                                           baseLayout.itemRects.at(candidateIndex).center();
                const QPointF displayedRadial =
                    candidateLayout.itemRects.at(first).center() -
                    candidateLayout.itemRects.at(candidateIndex).center();
                baseDistances.append({std::hypot(baseRadial.x(), baseRadial.y()), first});
                displayedDistances.append(
                    {std::hypot(displayedRadial.x(), displayedRadial.y()), first});
            }
            const QRectF rect = candidateLayout.itemRects.at(first);
            QVERIFY(rect.left() >= movementBounds.left() - 0.25);
            QVERIFY(rect.top() >= movementBounds.top() - 0.25);
            QVERIFY(rect.right() <= movementBounds.right() + 0.25);
            QVERIFY(rect.bottom() <= movementBounds.bottom() + 0.25);
            for (int second = first + 1; second < candidateLayout.itemRects.size(); ++second) {
                QVERIFY2(!rectsConflictWithGap(candidateLayout.itemRects.at(first),
                                               candidateLayout.itemRects.at(second), gap),
                         "An exhaustive gallery hover changed the fixed gap.");
            }
        }
        std::sort(baseDistances.begin(), baseDistances.end());
        std::sort(displayedDistances.begin(), displayedDistances.end());
        QHash<int, int> displayedRank;
        for (int rank = 0; rank < displayedDistances.size(); ++rank) {
            displayedRank.insert(displayedDistances.at(rank).second, rank);
        }
        qreal squaredRankDelta = 0.0;
        for (int rank = 0; rank < baseDistances.size(); ++rank) {
            const int rankShift = qAbs(displayedRank.value(baseDistances.at(rank).second) - rank);
            squaredRankDelta += rankShift * rankShift;
            if (rankShift > worstRankShift) {
                worstRankShift = rankShift;
                worstRankHover = candidateIndex;
            }
        }
        const qreal itemCount = baseDistances.size();
        const qreal rankCorrelation =
            1.0 - (6.0 * squaredRankDelta) / (itemCount * (itemCount * itemCount - 1.0));
        if (rankCorrelation < minimumRankCorrelation) {
            minimumRankCorrelation = rankCorrelation;
            minimumRankCorrelationHover = candidateIndex;
        }
    }
    constexpr qreal kMaximumDisplacementInOwnDiagonals = 1.0;
    QVERIFY2(worstDisplacementRatio <= kMaximumDisplacementInOwnDiagonals,
             qPrintable(QStringLiteral("A local image moved %1 diagonals (hover %2, image %3).")
                            .arg(worstDisplacementRatio, 0, 'f', 3)
                            .arg(worstDisplacementHover)
                            .arg(worstDisplacementImage)));
    const int maximumRankShift = qMax(2, static_cast<int>(baseLayout.itemRects.size() - 1) / 6);
    QVERIFY2(worstRankShift <= maximumRankShift,
             qPrintable(QStringLiteral("Radial rank shifted by %1 places for hover %2.")
                            .arg(worstRankShift)
                            .arg(worstRankHover)));
    constexpr qreal kMinimumRadialRankCorrelation = 0.95;
    QVERIFY2(minimumRankCorrelation >= kMinimumRadialRankCorrelation,
             qPrintable(QStringLiteral("Radial rank correlation fell to %1 for hover %2.")
                            .arg(minimumRankCorrelation, 0, 'f', 3)
                            .arg(minimumRankCorrelationHover)));
}

QTEST_MAIN(TierListLayoutTest)

#include "tst_TierListLayout.moc"
