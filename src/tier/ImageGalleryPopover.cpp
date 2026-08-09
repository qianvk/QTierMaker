#include "tier/ImageGalleryPopover.h"

#include "logging/Logger.h"
#include "theme/Theme.h"
#include "tier/TierDragController.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScreen>
#include <QShortcut>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <limits>

#include <vkui/core/VkIcon.h>
#include <vkui/core/VkTheme.h>
#include <vkui/core/VkThemeManager.h>
#include <vkui/widgets/overlays/VkPopover.h>

namespace qtm {

namespace {
constexpr int kMinimumTileExtent = 44;
constexpr int kPreferredTileExtent = 72;
constexpr int kMaximumTileExtent = 86;

int platformPopoverRadius() {
    return std::max(0, qRound(vkui::VkThemeManager::instance()
                                  ->theme()
                                  .metrics()
                                  .popoverCornerRadius));
}

QRect centeredCropSourceRect(const QPixmap& pixmap, const QSize& targetSize) {
    if (pixmap.isNull() || targetSize.isEmpty()) {
        return {};
    }

    const QSize sourceSize = pixmap.size();
    const qreal targetRatio = static_cast<qreal>(targetSize.width()) / qMax(1, targetSize.height());
    const qreal sourceRatio = static_cast<qreal>(sourceSize.width()) / qMax(1, sourceSize.height());
    if (sourceRatio > targetRatio) {
        const int cropWidth = qRound(sourceSize.height() * targetRatio);
        return QRect((sourceSize.width() - cropWidth) / 2, 0, cropWidth, sourceSize.height());
    }

    const int cropHeight = qRound(sourceSize.width() / targetRatio);
    return QRect(0, (sourceSize.height() - cropHeight) / 2, sourceSize.width(), cropHeight);
}

QPixmap dragPixmap(const QPixmap& pixmap, const QSize& logicalSize, qreal devicePixelRatio,
                   const QRect& sourceRect = {}) {
    if (pixmap.isNull() || logicalSize.isEmpty()) {
        return {};
    }

    QPixmap result(logicalSize * devicePixelRatio);
    result.setDevicePixelRatio(devicePixelRatio);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    painter.drawPixmap(QRect(QPoint(0, 0), logicalSize), pixmap,
                       sourceRect.isValid() ? sourceRect
                                            : centeredCropSourceRect(pixmap, logicalSize));
    return result;
}

QStringList filePathsFromMimeData(const QMimeData* mimeData) {
    QStringList paths;
    if (!mimeData || !mimeData->hasUrls()) {
        return paths;
    }
    for (const QUrl& url : mimeData->urls()) {
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        }
    }
    return paths;
}
} // namespace

class GalleryGridWidget final : public QWidget {
public:
    explicit GalleryGridWidget(ImageGalleryPopover* owner) : QWidget(owner), m_owner(owner) {
        setObjectName(QStringLiteral("ImageGalleryGrid"));
        setAcceptDrops(true);
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setCursor(Qt::ArrowCursor);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    }

    QRect imageSourceRect(const QString& imageId) const {
        if (!m_owner || imageId.isEmpty()) {
            return {};
        }
        const QStringList ids = m_owner->imageIds();
        const int index = static_cast<int>(ids.indexOf(imageId));
        if (index < 0) {
            return {};
        }
        const QRect localRect = m_owner->cellRect(index);
        const QPoint globalTopLeft = mapToGlobal(localRect.topLeft());
        QWidget* host = m_owner->m_hostWindow.data();
        return host ? QRect(host->mapFromGlobal(globalTopLeft), localRect.size())
                    : QRect(globalTopLeft, localRect.size());
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (!m_owner) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), platformPopoverRadius(),
                            platformPopoverRadius());
        painter.setClipPath(clip);

        painter.fillPath(clip, activeThemeTokens().popoverBackground);

        const QStringList ids = m_owner->imageIds();
        const int itemCount = static_cast<int>(ids.size()) + 1;
        for (int index = 0; index < itemCount; ++index) {
            const QRect cell = m_owner->cellRect(index);
            if (!cell.isValid() || !cell.intersects(rect())) {
                continue;
            }

            if (index < ids.size()) {
                paintImageCell(&painter, cell, ids.at(index));
            } else {
                paintImportCell(&painter, cell);
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton || !m_owner) {
            QWidget::mousePressEvent(event);
            return;
        }
        m_pressPosition = event->pos();
        m_pressedIndex = m_owner->cellIndexAt(event->pos());
        m_dragging = false;
        setFocus(Qt::MouseFocusReason);
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!m_owner || !(event->buttons() & Qt::LeftButton) || m_pressedIndex < 0) {
            QWidget::mouseMoveEvent(event);
            return;
        }
        if (m_dragging || (event->pos() - m_pressPosition).manhattanLength() <
                              QApplication::startDragDistance()) {
            return;
        }

        const QStringList ids = m_owner->imageIds();
        if (m_pressedIndex >= ids.size()) {
            return;
        }

        const QString imageId = ids.at(m_pressedIndex);
        const QRect sourceRect = m_owner->cellRect(m_pressedIndex);
        QPixmap pixmap = m_owner->pixmapForImage(imageId, sourceRect.size() * 2);
        const TierImage* image = m_owner->imageForId(imageId);
        auto* drag = new QDrag(this);
        drag->setMimeData(TierDragController::createMimeData(imageId));
        if (!pixmap.isNull()) {
            const QRect crop = image ? m_owner->sourceRectForImage(
                                           *image, pixmap, sourceRect.size())
                                     : QRect();
            drag->setPixmap(dragPixmap(pixmap, sourceRect.size(), devicePixelRatioF(), crop));
            drag->setHotSpot(event->pos() - sourceRect.topLeft());
        }

        m_dragging = true;
        QPointer<ImageGalleryPopover> guard(m_owner);
        emit m_owner->dragActiveChanged(true);
        Logger::info(QStringLiteral("tier.gallery.image.drag.start imageId=%1").arg(imageId));
        const Qt::DropAction result = drag->exec(Qt::MoveAction);
        if (guard) {
            emit m_owner->dragActiveChanged(false);
            Logger::info(QStringLiteral("tier.gallery.image.drag.finish imageId=%1 result=%2")
                             .arg(imageId)
                             .arg(static_cast<int>(result)));
        }
        m_dragging = false;
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (!m_owner || event->button() != Qt::LeftButton) {
            QWidget::mouseReleaseEvent(event);
            return;
        }

        const int releasedIndex = m_owner->cellIndexAt(event->pos());
        const QStringList ids = m_owner->imageIds();
        if (!m_dragging && releasedIndex == m_pressedIndex) {
            if (releasedIndex < 0) {
                emit m_owner->imageSelected({});
            } else if (releasedIndex >= ids.size()) {
                Logger::info(QStringLiteral("tier.gallery.import.request"));
                emit m_owner->importRequested();
            } else {
                const QString imageId = ids.at(releasedIndex);
                Logger::debug(QStringLiteral("tier.gallery.image.select imageId=%1").arg(imageId));
                emit m_owner->imageSelected(imageId);
            }
        }
        m_pressedIndex = -1;
        m_dragging = false;
        event->accept();
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (!m_owner || event->button() != Qt::LeftButton) {
            QWidget::mouseDoubleClickEvent(event);
            return;
        }
        const QStringList ids = m_owner->imageIds();
        const int index = m_owner->cellIndexAt(event->pos());
        if (index >= 0 && index < ids.size()) {
            m_owner->requestPreview(ids.at(index),
                                    ImageGalleryPopover::PreviewTrigger::DoubleClick);
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (m_owner && event->key() == Qt::Key_Space && !event->isAutoRepeat() &&
            m_owner->requestPreview(m_owner->m_selectedImageId,
                                    ImageGalleryPopover::PreviewTrigger::Space)) {
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent* event) override {
        if (!m_owner) {
            QWidget::contextMenuEvent(event);
            return;
        }

        const QStringList ids = m_owner->imageIds();
        const int index = m_owner->cellIndexAt(event->pos());
        if (index < 0 || index >= ids.size()) {
            QWidget::contextMenuEvent(event);
            return;
        }

        const QString imageId = ids.at(index);
        emit m_owner->imageSelected(imageId);
        QMenu menu(this);
        QAction* editAction = menu.addAction(vkui::icon(vkui::VkSymbol::Edit), tr("Edit"));
        QAction* removeAction =
            menu.addAction(vkui::icon(vkui::VkSymbol::Trash, vkui::VkIconRole::Destructive),
                           tr("Remove from Image Gallery"));
        QPointer<ImageGalleryPopover> guard(m_owner);
        m_owner->setOutsideDismissSuspended(true);
        QAction* chosen = menu.exec(event->globalPos());
        if (guard) {
            guard->setOutsideDismissSuspended(false);
        }
        if (!guard) {
            event->accept();
            return;
        }
        if (chosen == editAction) {
            Logger::info(QStringLiteral("tier.gallery.context.edit imageId=%1").arg(imageId));
            emit m_owner->imageEditRequested(imageId);
        } else if (chosen == removeAction) {
            Logger::info(QStringLiteral("tier.gallery.context.remove imageId=%1").arg(imageId));
            emit m_owner->imageRemoveRequested(imageId);
        }
        event->accept();
    }

    void dragEnterEvent(QDragEnterEvent* event) override {
        if (!filePathsFromMimeData(event->mimeData()).isEmpty()) {
            event->acceptProposedAction();
            return;
        }
        QWidget::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent* event) override {
        if (!filePathsFromMimeData(event->mimeData()).isEmpty()) {
            event->acceptProposedAction();
            return;
        }
        QWidget::dragMoveEvent(event);
    }

    void dropEvent(QDropEvent* event) override {
        if (!m_owner) {
            QWidget::dropEvent(event);
            return;
        }
        const QStringList paths = filePathsFromMimeData(event->mimeData());
        if (paths.isEmpty()) {
            QWidget::dropEvent(event);
            return;
        }
        Logger::info(QStringLiteral("tier.gallery.files.drop count=%1").arg(paths.size()));
        emit m_owner->imageFilesDropped(paths);
        event->acceptProposedAction();
    }

private:
    void paintImageCell(QPainter* painter, const QRect& cell, const QString& imageId) {
        const ThemeTokens& colors = activeThemeTokens();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->fillRect(cell, colors.elevatedBackground);
        const QPixmap pixmap = m_owner->pixmapForImage(imageId, cell.size() * 2);
        if (!pixmap.isNull()) {
            const TierImage* image = m_owner->imageForId(imageId);
            painter->drawPixmap(
                cell, pixmap,
                image ? m_owner->sourceRectForImage(*image, pixmap, cell.size())
                      : centeredCropSourceRect(pixmap, cell.size()));
        }
        if (imageId == m_owner->m_selectedImageId) {
            painter->setPen(QPen(colors.accent, 2));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(cell.adjusted(1, 1, -2, -2));
        }
        painter->restore();
    }

    void paintImportCell(QPainter* painter, const QRect& cell) {
        const ThemeTokens& colors = activeThemeTokens();
        painter->save();
        painter->fillRect(cell, colors.controlFillHovered);
        const QColor stroke = colors.symbolSecondary;
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(stroke, qMax(2, cell.width() / 28), Qt::SolidLine, Qt::RoundCap));
        const int arm = qMax(11, cell.width() / 5);
        const QPoint center = cell.center();
        painter->drawLine(QPoint(center.x() - arm, center.y()),
                          QPoint(center.x() + arm, center.y()));
        painter->drawLine(QPoint(center.x(), center.y() - arm),
                          QPoint(center.x(), center.y() + arm));
        painter->restore();
    }

    ImageGalleryPopover* m_owner{nullptr};
    QPoint m_pressPosition;
    int m_pressedIndex{-1};
    bool m_dragging{false};
};

ImageGalleryPopover::ImageGalleryPopover(QWidget* parent)
    : QWidget(nullptr), m_grid(new GalleryGridWidget(this)), m_popover(new vkui::VkPopover(parent)),
      m_hostWindow(parent ? parent->window() : nullptr) {
    setObjectName(QStringLiteral("ImageGalleryPopover"));
    m_popover->setPreferredPlacement(vkui::VkPopoverPlacement::Below);
    m_popover->setContentWidget(this);
    connect(m_popover, &vkui::VkPopover::closed, this, [this]() {
        if (!m_suspendedForPreview) {
            emit closed();
        } else {
            Logger::debug(QStringLiteral("tier.gallery.popover.preview.suspended logicalClose=0"));
        }
    });
    auto* previewShortcut = new QShortcut(QKeySequence(Qt::Key_Space), m_popover);
    previewShortcut->setObjectName(QStringLiteral("ImageGalleryPreviewShortcut"));
    previewShortcut->setContext(Qt::WindowShortcut);
    connect(previewShortcut, &QShortcut::activated, this,
            [this]() { requestPreview(m_selectedImageId, PreviewTrigger::Space); });
}

void ImageGalleryPopover::setData(const TierProject* project, const AssetManager* assetManager,
                                  ThumbnailCache* thumbnailCache, const QString& selectedImageId) {
    m_project = project;
    m_assetManager = assetManager;
    if (m_thumbnailConnection) {
        disconnect(m_thumbnailConnection);
        m_thumbnailConnection = {};
    }
    m_thumbnailCache = thumbnailCache;
    m_selectedImageId = selectedImageId;
    if (m_thumbnailCache && m_grid) {
        m_thumbnailConnection = connect(m_thumbnailCache, &ThumbnailCache::thumbnailReady, m_grid,
                                        [this](const QString&) { m_grid->update(); });
    }
    requestThumbnails();
    if (isOpen()) {
        QScreen* screen = m_anchor && m_anchor->screen() ? m_anchor->screen() : nullptr;
        const QRect available = screen ? screen->availableGeometry().adjusted(10, 10, -10, -10)
                                       : QRect(QPoint(10, 10), QSize(820, 620));
        recalculateGrid(available);
        resize(sizeHint());
        if (m_grid) {
            m_grid->setGeometry(rect());
        }
    }
    if (m_grid) {
        m_grid->update();
    }
}

void ImageGalleryPopover::setSelectedImageId(const QString& selectedImageId) {
    if (m_selectedImageId == selectedImageId) {
        return;
    }
    m_selectedImageId = selectedImageId;
    if (m_grid) {
        m_grid->update();
    }
}

void ImageGalleryPopover::setOutsideDismissSuspended(bool suspended) {
    if (m_outsideDismissSuspended == suspended) {
        return;
    }
    m_outsideDismissSuspended = suspended;
    if (m_popover) {
        const auto normalPolicy = vkui::VkPopoverClosePolicyFlag::OutsideClick |
                                  vkui::VkPopoverClosePolicyFlag::EscapeKey |
                                  vkui::VkPopoverClosePolicyFlag::AnchorDestroyed |
                                  vkui::VkPopoverClosePolicyFlag::WindowDeactivated;
        m_popover->setClosePolicy(suspended ? vkui::VkPopoverClosePolicyFlag::AnchorDestroyed
                                            : normalPolicy);
    }
    Logger::debug(QStringLiteral("tier.gallery.popover.dismiss.suspended value=%1").arg(suspended));
}

void ImageGalleryPopover::openFor(QWidget* anchor) {
    if (!anchor || !m_popover) {
        return;
    }
    m_suspendedForPreview = false;
    m_anchor = anchor;
    QScreen* screen = anchor->screen();
    const QRect available = screen ? screen->availableGeometry().adjusted(10, 10, -10, -10)
                                   : QRect(QPoint(10, 10), QSize(820, 620));
    recalculateGrid(available);
    resize(sizeHint());
    if (m_grid) {
        m_grid->setGeometry(rect());
    }
    m_popover->openFor(anchor);
    Logger::debug(QStringLiteral("tier.gallery.popover.place images=%1 columns=%2 rows=%3 tile=%4")
                      .arg(imageIds().size())
                      .arg(m_columns)
                      .arg(m_rows)
                      .arg(m_tileExtent));
}

void ImageGalleryPopover::closeAnimated() {
    const bool wasSuspended = m_suspendedForPreview;
    m_suspendedForPreview = false;
    if (m_popover) {
        m_popover->closeAnimated();
    }
    if (wasSuspended && (!m_popover || !m_popover->isOpen())) {
        emit closed();
    }
}

void ImageGalleryPopover::closeImmediately() {
    const bool wasSuspended = m_suspendedForPreview;
    m_suspendedForPreview = false;
    if (m_popover) {
        m_popover->closeImmediately();
    }
    if (wasSuspended && (!m_popover || !m_popover->isOpen())) {
        emit closed();
    }
}

bool ImageGalleryPopover::suspendForPreview() {
    if (m_suspendedForPreview) {
        return true;
    }
    if (!m_popover || !m_popover->isOpen() || !m_anchor) {
        return false;
    }
    m_suspendedForPreview = true;
    m_popover->closeImmediately();
    Logger::debug(QStringLiteral("tier.gallery.popover.preview.suspend anchorValid=%1 imageId=%2")
                      .arg(m_anchor != nullptr)
                      .arg(m_selectedImageId));
    return true;
}

bool ImageGalleryPopover::restoreAfterPreview() {
    if (!m_suspendedForPreview) {
        return false;
    }
    if (!m_anchor || !m_anchor->isVisible()) {
        m_suspendedForPreview = false;
        emit closed();
        Logger::warn(
            QStringLiteral("tier.gallery.popover.preview.restore rejected anchorVisible=0"));
        return false;
    }
    const QPointer<QWidget> anchor = m_anchor;
    m_suspendedForPreview = false;
    openFor(anchor);
    const bool restored = isOpen();
    Logger::debug(QStringLiteral("tier.gallery.popover.preview.restore open=%1 imageId=%2")
                      .arg(restored)
                      .arg(m_selectedImageId));
    return restored;
}

bool ImageGalleryPopover::isOpen() const {
    return m_popover && m_popover->isOpen();
}

QRect ImageGalleryPopover::imageSourceRect(const QString& imageId) const {
    return m_grid ? m_grid->imageSourceRect(imageId) : QRect();
}

QSize ImageGalleryPopover::sizeHint() const {
    return m_gridSize.expandedTo(QSize(1, 1));
}

void ImageGalleryPopover::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_grid) {
        m_grid->setGeometry(rect());
    }
}

QStringList ImageGalleryPopover::imageIds() const {
    QStringList ids;
    if (!m_project) {
        return ids;
    }
    ids.reserve(m_project->images.size());
    for (const TierImage& image : m_project->images) {
        if (!image.id.isEmpty() && !image.assignedTierRowId.has_value()) {
            ids.append(image.id);
        }
    }
    return ids;
}

bool ImageGalleryPopover::requestPreview(const QString& imageId, PreviewTrigger trigger) {
    if (imageId.isEmpty() || !imageIds().contains(imageId)) {
        return false;
    }
    Logger::info(QStringLiteral("tier.gallery.preview.request source=%1 imageId=%2")
                     .arg(trigger == PreviewTrigger::Space ? QStringLiteral("space")
                                                           : QStringLiteral("double-click"),
                          imageId));
    emit imagePreviewRequested(imageId, imageSourceRect(imageId));
    return true;
}

const TierImage* ImageGalleryPopover::imageForId(const QString& imageId) const {
    return m_project ? m_project->imageById(imageId) : nullptr;
}

QString ImageGalleryPopover::resolvedPathForImage(const TierImage& image) const {
    return m_assetManager && m_project ? m_assetManager->resolvedImagePath(*m_project, image)
                                       : image.sourcePath;
}

QPixmap ImageGalleryPopover::pixmapForImage(const QString& imageId, QSize requestedSize) const {
    if (const TierImage* image = imageForId(imageId)) {
        requestedSize = requestedSize.expandedTo(QSize(m_tileExtent * 2, m_tileExtent * 2));
        if (m_thumbnailCache) {
            if (!m_thumbnailCache->hasThumbnail(imageId, requestedSize)) {
                m_thumbnailCache->requestThumbnail(imageId, resolvedPathForImage(*image),
                                                   requestedSize);
            }
            const QPixmap cached = m_thumbnailCache->thumbnail(imageId, requestedSize);
            if (!cached.isNull()) {
                return cached;
            }
        }
    }
    return vkui::icon(vkui::VkSymbol::Eye, vkui::VkIconRole::Secondary)
        .pixmap(requestedSize.isEmpty() ? QSize(64, 64) : requestedSize);
}

QRect ImageGalleryPopover::sourceRectForImage(const TierImage& image, const QPixmap& pixmap,
                                              const QSize& targetSize) const {
    if (m_project && m_project->imagePresentationMode() == ImagePresentationMode::NoCrop) {
        return QRect(QPoint(0, 0), pixmap.size());
    }
    return image.thumbnailSourceRect(pixmap.size(), targetSize);
}

QRect ImageGalleryPopover::cellRect(int index) const {
    if (index < 0 || index >= m_cellRects.size()) {
        return {};
    }
    return m_cellRects.at(index);
}

int ImageGalleryPopover::cellIndexAt(const QPoint& point) const {
    if (!m_grid || !m_grid->rect().contains(point)) {
        return -1;
    }
    for (int index = 0; index < m_cellRects.size(); ++index) {
        if (m_cellRects.at(index).contains(point)) {
            return index;
        }
    }
    return -1;
}

void ImageGalleryPopover::recalculateGrid(const QRect& availableGeometry) {
    const QStringList ids = imageIds();
    const int count = qMax(1, static_cast<int>(ids.size()) + 1);
    const int maxWidth = qMax(kMinimumTileExtent, qMin(760, availableGeometry.width()));
    const int maxHeight = qMax(kMinimumTileExtent, availableGeometry.height());

    m_cellRects.clear();
    m_cellRects.reserve(count);

    if (m_project && m_project->imagePresentationMode() == ImagePresentationMode::NoCrop) {
        struct Candidate {
            QVector<QRect> rects;
            QSize size;
            int tileHeight{kMinimumTileExtent};
            int rows{1};
            int maximumItemsPerRow{1};
            qreal score{-std::numeric_limits<qreal>::max()};
        };

        Candidate best;
        for (int tileHeight = kMinimumTileExtent; tileHeight <= kMaximumTileExtent;
             tileHeight += 2) {
            QVector<QSize> itemSizes;
            itemSizes.reserve(count);
            int widestItem = tileHeight;
            for (const QString& imageId : ids) {
                const TierImage* image = imageForId(imageId);
                const QSize source = image && image->size().isValid() ? image->size() : QSize(1, 1);
                const qreal aspect = static_cast<qreal>(source.width()) / qMax(1, source.height());
                int itemWidth = qMax(1, qRound(tileHeight * aspect));
                int itemHeight = tileHeight;
                if (itemWidth > maxWidth) {
                    itemWidth = maxWidth;
                    itemHeight = qMax(1, qRound(itemWidth / qMax<qreal>(0.001, aspect)));
                }
                itemSizes.append(QSize(itemWidth, itemHeight));
                widestItem = qMax(widestItem, itemWidth);
            }
            itemSizes.append(QSize(tileHeight, tileHeight));

            for (int shelfWidth = widestItem; shelfWidth <= maxWidth; shelfWidth += 12) {
                QVector<QRect> rects;
                rects.reserve(count);
                int x = 0;
                int row = 0;
                int usedWidth = 0;
                int itemsInRow = 0;
                int maximumItemsPerRow = 0;
                qreal imageArea = 0.0;
                for (const QSize& itemSize : std::as_const(itemSizes)) {
                    if (x > 0 && x + itemSize.width() > shelfWidth) {
                        maximumItemsPerRow = qMax(maximumItemsPerRow, itemsInRow);
                        ++row;
                        x = 0;
                        itemsInRow = 0;
                    }
                    const int y = row * tileHeight + (tileHeight - itemSize.height()) / 2;
                    rects.append(QRect(x, y, itemSize.width(), itemSize.height()));
                    x += itemSize.width();
                    usedWidth = qMax(usedWidth, x);
                    imageArea += static_cast<qreal>(itemSize.width()) * itemSize.height();
                    ++itemsInRow;
                }
                maximumItemsPerRow = qMax(maximumItemsPerRow, itemsInRow);
                const int rows = row + 1;
                const int usedHeight = rows * tileHeight;
                if (usedHeight > maxHeight) {
                    continue;
                }

                const qreal density = imageArea / qMax<qreal>(1.0, usedWidth * usedHeight);
                const qreal aspect = static_cast<qreal>(usedWidth) / qMax(1, usedHeight);
                const qreal targetAspect = count <= 6 ? 1.2 : 1.55;
                const qreal score = tileHeight * 80.0 + density * 34.0 -
                                    std::abs(aspect - targetAspect) * 18.0 - rows * 1.5;
                if (score > best.score) {
                    best = Candidate{std::move(rects), QSize(usedWidth, usedHeight), tileHeight,
                                     rows, maximumItemsPerRow, score};
                }
            }
        }

        if (!best.rects.isEmpty()) {
            m_cellRects = std::move(best.rects);
            m_gridSize = best.size;
            m_tileExtent = best.tileHeight;
            m_rows = best.rows;
            m_columns = best.maximumItemsPerRow;
            requestThumbnails();
            return;
        }
    }

    const int maxColumns = qMax(1, qMin(count, maxWidth / kMinimumTileExtent));

    int bestColumns = 1;
    int bestRows = count;
    int bestTile = kMinimumTileExtent;
    qreal bestScore = -std::numeric_limits<qreal>::max();
    for (int columns = 1; columns <= maxColumns; ++columns) {
        const int rows = (count + columns - 1) / columns;
        int tile = qMin(maxWidth / columns, maxHeight / rows);
        tile = qMin(tile, kMaximumTileExtent);
        if (tile < kMinimumTileExtent) {
            continue;
        }
        const int width = columns * tile;
        const int height = rows * tile;
        const qreal aspect = static_cast<qreal>(width) / qMax(1, height);
        const qreal targetAspect = count <= 6 ? 1.2 : 1.55;
        const qreal preferredPenalty = std::abs(tile - kPreferredTileExtent) * 0.55;
        const qreal aspectPenalty = std::abs(aspect - targetAspect) * 24.0;
        const qreal rowPenalty = rows * 1.5;
        const qreal score = tile * 80.0 - preferredPenalty - aspectPenalty - rowPenalty;
        if (score > bestScore) {
            bestScore = score;
            bestColumns = columns;
            bestRows = rows;
            bestTile = tile;
        }
    }

    m_columns = qMax(1, bestColumns);
    m_rows = qMax(1, bestRows);
    m_tileExtent = qBound(kMinimumTileExtent, bestTile, kMaximumTileExtent);
    for (int index = 0; index < count; ++index) {
        const int row = index / m_columns;
        const int column = index % m_columns;
        m_cellRects.append(QRect(column * m_tileExtent, row * m_tileExtent, m_tileExtent,
                                 m_tileExtent));
    }
    m_gridSize = QSize(m_columns * m_tileExtent, m_rows * m_tileExtent);
    requestThumbnails();
}

void ImageGalleryPopover::requestThumbnails() {
    if (!m_thumbnailCache || !m_project) {
        return;
    }
    const QStringList ids = imageIds();
    for (int index = 0; index < ids.size(); ++index) {
        const TierImage* image = imageForId(ids.at(index));
        if (!image) {
            continue;
        }
        const QSize cellSize = cellRect(index).size();
        const QSize requestSize =
            cellSize.isValid() ? cellSize * 2 : QSize(m_tileExtent * 2, m_tileExtent * 2);
        m_thumbnailCache->requestThumbnail(image->id, resolvedPathForImage(*image), requestSize);
    }
}

} // namespace qtm
