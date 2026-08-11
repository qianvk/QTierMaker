#include "tier/TierProject.h"

#include "persistence/ProjectFileLayout.h"

#include <QSet>
#include <QUuid>

#include <algorithm>

namespace qtm {

namespace {
QVector<TierRow> makeDefaultRows() {
    return {
        TierRow::makeDefault(QStringLiteral("S"), QColor(QStringLiteral("#ff7b7b")), 0),
        TierRow::makeDefault(QStringLiteral("A"), QColor(QStringLiteral("#ffc36b")), 1),
        TierRow::makeDefault(QStringLiteral("B"), QColor(QStringLiteral("#ffe17d")), 2),
        TierRow::makeDefault(QStringLiteral("C"), QColor(QStringLiteral("#8bdc8b")), 3),
        TierRow::makeDefault(QStringLiteral("D"), QColor(QStringLiteral("#82b7ff")), 4),
    };
}
} // namespace

TierProject TierProject::createUntitled() {
    TierProject project;
    project.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    project.name = QObject::tr("Untitled Tier List");
    project.createdAt = QDateTime::currentDateTimeUtc();
    project.updatedAt = project.createdAt;
    project.rows = makeDefaultRows();
    project.settings.insert(QStringLiteral("background"), QStringLiteral("default"));
    project.settings.insert(QStringLiteral("exportScale"), 2);
    project.canvas.insert(QStringLiteral("imagePresentationMode"), QStringLiteral("square"));
    project.dirty = false;
    return project;
}

TierRow* TierProject::rowById(const QString& rowId) {
    auto it = std::find_if(rows.begin(), rows.end(), [&](const TierRow& row) { return row.id == rowId; });
    return it == rows.end() ? nullptr : &(*it);
}

const TierRow* TierProject::rowById(const QString& rowId) const {
    auto it = std::find_if(rows.cbegin(), rows.cend(), [&](const TierRow& row) { return row.id == rowId; });
    return it == rows.cend() ? nullptr : &(*it);
}

TierImage* TierProject::imageById(const QString& imageId) {
    auto it = std::find_if(images.begin(), images.end(),
                           [&](const TierImage& image) { return image.id == imageId; });
    return it == images.end() ? nullptr : &(*it);
}

const TierImage* TierProject::imageById(const QString& imageId) const {
    auto it = std::find_if(images.cbegin(), images.cend(),
                           [&](const TierImage& image) { return image.id == imageId; });
    return it == images.cend() ? nullptr : &(*it);
}

QVector<TierImage*> TierProject::unassignedImages() {
    QVector<TierImage*> result;
    for (TierImage& image : images) {
        if (!image.assignedTierRowId.has_value()) {
            result.push_back(&image);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const TierImage* lhs, const TierImage* rhs) { return lhs->order < rhs->order; });
    return result;
}

QVector<const TierImage*> TierProject::unassignedImages() const {
    QVector<const TierImage*> result;
    for (const TierImage& image : images) {
        if (!image.assignedTierRowId.has_value()) {
            result.push_back(&image);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const TierImage* lhs, const TierImage* rhs) { return lhs->order < rhs->order; });
    return result;
}

QVector<const TierImage*> TierProject::imagesForRow(const QString& rowId) const {
    QVector<const TierImage*> result;
    const TierRow* row = rowById(rowId);
    if (!row) {
        return result;
    }
    for (const QString& imageId : row->imageIds) {
        if (const TierImage* image = imageById(imageId)) {
            result.push_back(image);
        }
    }
    return result;
}

ImagePresentationMode TierProject::imagePresentationMode() const {
    return canvas.value(QStringLiteral("imagePresentationMode")).toString() ==
                   QStringLiteral("noCrop")
               ? ImagePresentationMode::NoCrop
               : ImagePresentationMode::Square;
}

bool TierProject::setImagePresentationMode(ImagePresentationMode mode) {
    const QString value = mode == ImagePresentationMode::NoCrop ? QStringLiteral("noCrop")
                                                                : QStringLiteral("square");
    if (canvas.value(QStringLiteral("imagePresentationMode")).toString(
            QStringLiteral("square")) == value) {
        return false;
    }
    canvas.insert(QStringLiteral("imagePresentationMode"), value);
    return true;
}

int TierProject::customCropCount() const {
    return static_cast<int>(std::count_if(images.cbegin(), images.cend(),
                                          [](const TierImage& image) {
                                              return image.hasCropRect();
                                          }));
}

int TierProject::clearCustomCrops() {
    int cleared = 0;
    for (TierImage& image : images) {
        if (!image.hasCropRect()) {
            continue;
        }
        image.cropRect = {};
        ++cleared;
    }
    return cleared;
}

bool TierProject::hasSamePersistentContent(const TierProject& other) const {
    if (id != other.id || name != other.name || createdAt != other.createdAt ||
        thumbnailPath != other.thumbnailPath || cover != other.cover || canvas != other.canvas ||
        settings != other.settings || rows.size() != other.rows.size() ||
        images.size() != other.images.size()) {
        return false;
    }

    for (qsizetype index = 0; index < rows.size(); ++index) {
        const TierRow& left = rows.at(index);
        const TierRow& right = other.rows.at(index);
        if (left.id != right.id || left.label != right.label || left.color != right.color ||
            left.order != right.order || left.height != right.height ||
            left.imageIds != right.imageIds) {
            return false;
        }
    }

    for (qsizetype index = 0; index < images.size(); ++index) {
        const TierImage& left = images.at(index);
        const TierImage& right = other.images.at(index);
        if (left.id != right.id || left.sourcePath != right.sourcePath ||
            left.importedAssetPath != right.importedAssetPath ||
            left.originalFileName != right.originalFileName ||
            left.displayName != right.displayName || left.width != right.width ||
            left.height != right.height || left.thumbnailPath != right.thumbnailPath ||
            left.assignedTierRowId != right.assignedTierRowId || left.order != right.order ||
            left.cropRect != right.cropRect) {
            return false;
        }
    }
    return true;
}

TierProject TierProject::detachedCopy() const {
    TierProject copy = *this;

    // History snapshots must not share collection storage with the editable project. A mutable
    // pointer into a shared QVector can otherwise write into a snapshot or become invalid when a
    // later container operation detaches the live model.
    copy.rows.detach();
    for (TierRow& row : copy.rows) {
        row.imageIds.detach();
    }
    copy.images.detach();
    return copy;
}

void TierProject::resetDefaultRows() {
    rows = makeDefaultRows();
    for (TierImage& image : images) {
        image.assignedTierRowId.reset();
    }
    normalizeOrdering();
    touch();
}

void TierProject::normalizeOrdering() {
    std::sort(rows.begin(), rows.end(), [](const TierRow& lhs, const TierRow& rhs) {
        return lhs.order < rhs.order;
    });
    QSet<QString> assignedImageIds;
    for (int i = 0; i < rows.size(); ++i) {
        rows[i].order = i;
        QStringList valid;
        for (const QString& imageId : rows[i].imageIds) {
            TierImage* image = imageById(imageId);
            if (!image || assignedImageIds.contains(imageId)) {
                continue;
            }
            assignedImageIds.insert(imageId);
            image->assignedTierRowId = rows[i].id;
            image->order = static_cast<int>(valid.size());
            valid.push_back(imageId);
        }
        rows[i].imageIds = valid;
    }
    int unassignedOrder = 0;
    for (TierImage& image : images) {
        // Ordered row membership is authoritative because it carries both ownership and position.
        // Any assignment not represented there is stale and belongs back in the gallery.
        if (!assignedImageIds.contains(image.id)) {
            image.assignedTierRowId.reset();
            image.order = unassignedOrder++;
        }
    }
}

void TierProject::touch() {
    updatedAt = QDateTime::currentDateTimeUtc();
    dirty = true;
}

QString TierProject::suggestedFileName() const {
    return ProjectFileLayout::fileName(name);
}

} // namespace qtm
