#include "tier/ProjectHistory.h"

#include <QtTest>

using namespace qtm;

class ProjectHistoryTest final : public QObject {
    Q_OBJECT

private slots:
    void ignoresRuntimeMetadataAndNoOpEdits();
    void restoresAtomicEditsAndTracksCleanState();
    void keepsImageMovesAndClearsAtomic();
    void detachesSnapshotsFromPointerBasedMutations();
};

void ProjectHistoryTest::ignoresRuntimeMetadataAndNoOpEdits() {
    ProjectHistory::State applied;
    ProjectHistory history([&applied](const ProjectHistory::State& state) { applied = state; });
    ProjectHistory::State before{TierProject::createUntitled(), QStringLiteral("selected")};
    ProjectHistory::State after = before;
    after.project.filePath = QStringLiteral("/tmp/project.qtm");
    after.project.updatedAt = after.project.updatedAt.addSecs(10);
    after.project.dirty = true;
    after.selectedImageId = QStringLiteral("another-selection");

    TierImage firstImage;
    firstImage.id = QStringLiteral("first");
    firstImage.assignedTierRowId = before.project.rows.constFirst().id;
    TierImage secondImage;
    secondImage.id = QStringLiteral("second");
    secondImage.assignedTierRowId = before.project.rows.constFirst().id;
    before.project.images = {firstImage, secondImage};
    before.project.rows.first().imageIds = {firstImage.id, secondImage.id};
    before.project.normalizeOrdering();
    after.project = before.project;
    after.project.filePath = QStringLiteral("/tmp/project.qtm");
    after.project.updatedAt = after.project.updatedAt.addSecs(10);
    after.project.dirty = true;
    after.project.rows.first().imageIds.removeAll(secondImage.id);
    after.project.rows.first().imageIds.insert(1, secondImage.id);
    after.project.normalizeOrdering();

    QVERIFY(!history.push(before, after, QStringLiteral("No-op")));
    QCOMPARE(history.count(), 0);
    QVERIFY(history.isClean());
}

void ProjectHistoryTest::restoresAtomicEditsAndTracksCleanState() {
    ProjectHistory::State before{TierProject::createUntitled(), QStringLiteral("before")};
    ProjectHistory::State after = before;
    after.project.name = QStringLiteral("Renamed");
    after.selectedImageId = QStringLiteral("after");
    ProjectHistory::State current = after;
    int applyCount = 0;
    ProjectHistory history([&](const ProjectHistory::State& state) {
        current = state;
        ++applyCount;
    });
    QSignalSpy stateSpy(&history, &ProjectHistory::stateChanged);

    QVERIFY(history.push(before, after, QStringLiteral("Rename project")));
    QCOMPARE(history.count(), 1);
    QCOMPARE(current.project.name, QStringLiteral("Renamed"));
    QCOMPARE(applyCount, 0);
    QVERIFY(!history.isClean());
    QVERIFY(!stateSpy.isEmpty());
    QCOMPARE(stateSpy.constLast().at(0).toBool(), false);

    history.undo();
    QCOMPARE(current.project.name, before.project.name);
    QCOMPARE(current.selectedImageId, QStringLiteral("before"));
    QCOMPARE(applyCount, 1);
    QVERIFY(history.isClean());
    QCOMPARE(stateSpy.constLast().at(0).toBool(), true);

    history.redo();
    QCOMPARE(current.project.name, QStringLiteral("Renamed"));
    QCOMPARE(current.selectedImageId, QStringLiteral("after"));
    QCOMPARE(applyCount, 2);
    history.setClean();
    QVERIFY(history.isClean());
}

void ProjectHistoryTest::keepsImageMovesAndClearsAtomic() {
    ProjectHistory::State current{TierProject::createUntitled(), QStringLiteral("image")};
    TierImage image;
    image.id = QStringLiteral("image");
    current.project.images.append(image);
    current.project.rows.first().imageIds.append(image.id);
    current.project.normalizeOrdering();

    ProjectHistory history([&current](const ProjectHistory::State& state) { current = state; });
    const QString sourceRowId = current.project.rows.at(0).id;
    const QString targetRowId = current.project.rows.at(1).id;

    const ProjectHistory::State beforeMove = current;
    for (TierRow& row : current.project.rows) {
        row.imageIds.removeAll(image.id);
    }
    current.project.rows[1].imageIds.append(image.id);
    current.project.normalizeOrdering();
    QVERIFY(history.push(beforeMove, current, QStringLiteral("Move image")));
    QCOMPARE(current.project.rows.at(0).imageIds.count(image.id), 0);
    QCOMPARE(current.project.rows.at(1).imageIds.count(image.id), 1);
    QCOMPARE(current.project.imageById(image.id)->assignedTierRowId.value(), targetRowId);

    history.undo();
    QCOMPARE(current.project.rows.at(0).imageIds.count(image.id), 1);
    QCOMPARE(current.project.rows.at(1).imageIds.count(image.id), 0);
    QCOMPARE(current.project.imageById(image.id)->assignedTierRowId.value(), sourceRowId);

    history.redo();
    const ProjectHistory::State beforeClear = current;
    current.project.rows[1].imageIds.clear();
    current.project.normalizeOrdering();
    QVERIFY(history.push(beforeClear, current, QStringLiteral("Clear tier row")));
    QVERIFY(current.project.rows.at(1).imageIds.isEmpty());
    QVERIFY(!current.project.imageById(image.id)->assignedTierRowId.has_value());

    history.undo();
    QCOMPARE(current.project.rows.at(1).imageIds, QStringList{image.id});
    QCOMPARE(current.project.imageById(image.id)->assignedTierRowId.value(), targetRowId);

    const QString originalLabel = current.project.rows.at(1).label;
    const QColor originalColor = current.project.rows.at(1).color;
    const ProjectHistory::State beforeTierEdit = current;
    current.project.rows[1].label = QStringLiteral("Renamed Tier");
    current.project.rows[1].color = QColor(QStringLiteral("#2468ac"));
    QVERIFY(history.push(beforeTierEdit, current, QStringLiteral("Edit tier row")));
    history.undo();
    QCOMPARE(current.project.rows.at(1).label, originalLabel);
    QCOMPARE(current.project.rows.at(1).color, originalColor);
    history.redo();
    QCOMPARE(current.project.rows.at(1).label, QStringLiteral("Renamed Tier"));
    QCOMPARE(current.project.rows.at(1).color, QColor(QStringLiteral("#2468ac")));

    const int commandCount = history.count();
    const ProjectHistory::State beforeNoOp = current;
    current.project.rows[1].imageIds.removeAll(image.id);
    current.project.rows[1].imageIds.append(image.id);
    current.project.normalizeOrdering();
    QVERIFY(!history.push(beforeNoOp, current, QStringLiteral("No-op move")));
    QCOMPARE(history.count(), commandCount);
}

void ProjectHistoryTest::detachesSnapshotsFromPointerBasedMutations() {
    ProjectHistory::State current{TierProject::createUntitled(), QStringLiteral("image")};
    TierImage image;
    image.id = current.selectedImageId;
    image.displayName = QStringLiteral("Before");
    current.project.images.append(image);
    current.project.rows[0].imageIds.append(image.id);
    current.project.normalizeOrdering();

    ProjectHistory history([&current](const ProjectHistory::State& state) {
        current.project = state.project.detachedCopy();
        current.selectedImageId = state.selectedImageId;
    });
    const QString sourceRowId = current.project.rows.at(0).id;
    const QString targetRowId = current.project.rows.at(1).id;

    // Reproduce the former editor call order: pointers were acquired before the history snapshot,
    // then a container detach redirected the stale row pointer into snapshot storage.
    TierImage* movingImage = current.project.imageById(image.id);
    TierRow* targetRow = current.project.rowById(targetRowId);
    QVERIFY(movingImage);
    QVERIFY(targetRow);
    const ProjectHistory::State beforeMove{current.project, image.id};
    for (TierRow& row : current.project.rows) {
        row.imageIds.removeAll(image.id);
    }
    movingImage->assignedTierRowId = targetRowId;
    targetRow->imageIds.append(image.id);
    current.project.normalizeOrdering();

    QCOMPARE(beforeMove.project.rowById(sourceRowId)->imageIds, QStringList{image.id});
    QVERIFY(beforeMove.project.rowById(targetRowId)->imageIds.isEmpty());
    QVERIFY(current.project.rowById(sourceRowId)->imageIds.isEmpty());
    QCOMPARE(current.project.rowById(targetRowId)->imageIds, QStringList{image.id});
    QVERIFY(history.push(beforeMove, current, QStringLiteral("Pointer-based image move")));
    history.undo();
    QCOMPARE(current.project.rowById(sourceRowId)->imageIds.count(image.id), 1);
    QCOMPARE(current.project.rowById(targetRowId)->imageIds.count(image.id), 0);

    TierRow* sourceRow = current.project.rowById(sourceRowId);
    QVERIFY(sourceRow);
    const ProjectHistory::State beforeClear{current.project, image.id};
    sourceRow->imageIds.clear();
    current.project.normalizeOrdering();
    QVERIFY(history.push(beforeClear, current, QStringLiteral("Pointer-based row clear")));
    history.undo();
    QCOMPARE(current.project.rowById(sourceRowId)->imageIds, QStringList{image.id});

    TierImage* editedImage = current.project.imageById(image.id);
    QVERIFY(editedImage);
    const ProjectHistory::State beforeEdit{current.project, image.id};
    editedImage->displayName = QStringLiteral("After");
    QVERIFY(history.push(beforeEdit, current, QStringLiteral("Pointer-based image edit")));
    QVERIFY(!history.isClean());
    history.undo();
    QCOMPARE(current.project.imageById(image.id)->displayName, QStringLiteral("Before"));

    TierRow* editedRow = current.project.rowById(sourceRowId);
    QVERIFY(editedRow);
    const QString originalLabel = editedRow->label;
    const ProjectHistory::State beforeRowEdit{current.project, image.id};
    editedRow->label = QStringLiteral("Confirmed edit");
    QVERIFY(history.push(beforeRowEdit, current, QStringLiteral("Pointer-based tier edit")));
    QVERIFY(!history.isClean());
    history.undo();
    QCOMPARE(current.project.rowById(sourceRowId)->label, originalLabel);
}

QTEST_APPLESS_MAIN(ProjectHistoryTest)

#include "tst_ProjectHistory.moc"
