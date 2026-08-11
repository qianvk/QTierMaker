#pragma once

#include "assets/AssetManager.h"
#include "assets/ThumbnailCache.h"
#include "tier/TierProject.h"

#include <QMetaObject>
#include <QPointer>
#include <QRect>
#include <QWidget>

namespace vkui {
class VkPopover;
}

namespace qtm {

class GalleryGridWidget;

/** Popup gallery for every imported image, with tight adaptive cells and shared image drag MIME. */
class ImageGalleryPopover final : public QWidget {
    Q_OBJECT

public:
    explicit ImageGalleryPopover(QWidget* parent = nullptr);

    void setData(const TierProject* project, const AssetManager* assetManager,
                 ThumbnailCache* thumbnailCache, const QString& selectedImageId);
    void setSelectedImageId(const QString& selectedImageId);
    void setOutsideDismissSuspended(bool suspended);
    void openFor(QWidget* anchor);
    void closeAnimated();
    void closeImmediately();
    void resetViewState();
    bool suspendForPreview();
    bool restoreAfterPreview();
    bool isOpen() const;
    QRect imageSourceRect(const QString& imageId) const;
    QSize sizeHint() const override;

signals:
    void closed();
    void importRequested();
    void imageFilesDropped(const QStringList& filePaths);
    void imageSelected(const QString& imageId);
    void imagePreviewRequested(const QString& imageId, const QRect& sourceRect);
    void imageEditRequested(const QString& imageId);
    void imageRemoveRequested(const QString& imageId);
    void dragActiveChanged(bool active);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    friend class GalleryGridWidget;
    enum class PreviewTrigger { DoubleClick, Space };

    QStringList imageIds() const;
    bool requestPreview(const QString& imageId, PreviewTrigger trigger);
    const TierImage* imageForId(const QString& imageId) const;
    QString resolvedPathForImage(const TierImage& image) const;
    QPixmap pixmapForImage(const QString& imageId, QSize requestedSize) const;
    QRect sourceRectForImage(const TierImage& image, const QPixmap& pixmap,
                             const QSize& targetSize) const;
    QRect cellRect(int index) const;
    int cellIndexAt(const QPoint& point) const;
    void recalculateGrid(const QRect& availableGeometry);
    void requestThumbnails();

    const TierProject* m_project{nullptr};
    const AssetManager* m_assetManager{nullptr};
    ThumbnailCache* m_thumbnailCache{nullptr};
    QString m_selectedImageId;
    GalleryGridWidget* m_grid{nullptr};
    vkui::VkPopover* m_popover{nullptr};
    QPointer<QWidget> m_anchor;
    QPointer<QWidget> m_hostWindow;
    QMetaObject::Connection m_thumbnailConnection;
    int m_tileExtent{72};
    int m_columns{1};
    int m_rows{1};
    QSize m_gridSize{72, 72};
    QVector<QRect> m_cellRects;
    bool m_outsideDismissSuspended{false};
    bool m_suspendedForPreview{false};
};

} // namespace qtm
