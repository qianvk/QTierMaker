#include "tier/ImageEditDialog.h"

#include "tier/CropEditorWidget.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace qtm {

ImageEditDialog::ImageEditDialog(const TierImage& image, const QPixmap& pixmap, QWidget* parent,
                                 QSizeF aspectRatio, bool cropEnabled)
    : AppDialog(QObject::tr("Edit Image"), parent), m_nameEdit(new QLineEdit(this)) {
    setWindowTitle(tr("Edit Image"));
    setMinimumWidth(440);
    setObjectName(QStringLiteral("ImageEditDialog"));

    auto* layout = contentLayout();
    layout->setSpacing(14);

    auto* title = new QLabel(cropEnabled ? tr("Edit thumbnail crop") : tr("Image details"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);
    if (cropEnabled) {
        m_cropEditor = new CropEditorWidget(
            pixmap, image.hasCropRect() ? image.cropRect : QRectF(), aspectRatio, this);
        layout->addWidget(m_cropEditor, 1);
    } else {
        auto* preview = new QLabel(this);
        preview->setObjectName(QStringLiteral("ImageEditNoCropPreview"));
        preview->setAlignment(Qt::AlignCenter);
        preview->setMinimumHeight(180);
        preview->setPixmap(pixmap.scaled(QSize(420, 260), Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
        layout->addWidget(preview, 1);
        auto* note = new QLabel(tr("No Crop preserves the original image ratio."), this);
        note->setWordWrap(true);
        layout->addWidget(note);
    }

    m_nameEdit->setText(image.displayName.isEmpty() ? image.originalFileName : image.displayName);
    m_nameEdit->setCursorPosition(m_nameEdit->text().size());
    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(tr("Name"), m_nameEdit);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString ImageEditDialog::displayName() const {
    return m_nameEdit ? m_nameEdit->text().trimmed() : QString();
}

QRectF ImageEditDialog::cropRect() const {
    return m_cropEditor ? m_cropEditor->cropRect() : QRectF();
}

} // namespace qtm
