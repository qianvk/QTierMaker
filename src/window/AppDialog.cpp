#include "window/AppDialog.h"

#include <QFont>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPalette>
#include <QShowEvent>
#include <QVBoxLayout>

#include <vkui/window/VSystemButton.h>

namespace qtm {
namespace {
constexpr int kTitleBarHeight = 44;
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
constexpr int kMacCloseButtonReserve = 24;
constexpr QPoint kMacTrafficLightOrigin{18, 15};
#else
constexpr int kWindowsCloseButtonReserve = 56;
#endif
} // namespace

AppDialog::AppDialog(const QString& title, QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                          Qt::WindowCloseButtonHint),
      m_windowAgent(*this) {
    setObjectName(QStringLiteral("AppDialog"));
    setWindowTitle(title);
    setWindowModality(parent ? Qt::WindowModal : Qt::ApplicationModal);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_QuitOnClose, false);
    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);

    buildUi();
    m_windowAgent.setResizable(false);
    m_windowAgent.setSystemButtons(vkui::VSystemButton::Close);
    const bool titleBarAdded = m_windowAgent.addTitleBar(m_titleBar);
    Q_ASSERT(titleBarAdded);
    Q_UNUSED(titleBarAdded);
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    m_windowAgent.setTrafficLightOrigin(kMacTrafficLightOrigin);
#endif
    refreshChrome();
}

void AppDialog::setResizable(bool resizable) {
    if (m_resizable == resizable) {
        return;
    }
    m_resizable = resizable;
    m_windowAgent.setResizable(resizable);
}

void AppDialog::setCloseButtonPlacement(CloseButtonPlacement placement) {
    if (m_closeButtonPlacement == placement) {
        return;
    }
    m_closeButtonPlacement = placement;
    refreshChrome();
}

void AppDialog::changeEvent(QEvent* event) {
    QDialog::changeEvent(event);
    if (event && (event->type() == QEvent::WindowTitleChange ||
                  event->type() == QEvent::LanguageChange)) {
        refreshTitle();
    }
}

void AppDialog::showEvent(QShowEvent* event) {
    const bool explicitlySized = testAttribute(Qt::WA_Resized);
    if (layout()) {
        if (isResizable()) {
            layout()->setSizeConstraints(QLayout::SetDefaultConstraint,
                                         QLayout::SetDefaultConstraint);
        } else {
            layout()->setSizeConstraints(QLayout::SetMinimumSize,
                                         explicitlySized ? QLayout::SetMinimumSize
                                                         : QLayout::SetFixedSize);
        }
        layout()->activate();
    }
    QDialog::showEvent(event);
}

void AppDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName(QStringLiteral("AppDialogTitleBar"));
    m_titleBar->setFixedHeight(kTitleBarHeight);
    m_titleBar->setAutoFillBackground(false);
    m_titleBar->setAttribute(Qt::WA_StyledBackground, false);
    auto* titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(18, 0, 10, 0);
    titleLayout->setSpacing(0);

    m_leadingButtonReserve = new QWidget(m_titleBar);
    m_leadingButtonReserve->setFixedWidth(0);
    titleLayout->addWidget(m_leadingButtonReserve);

    m_titleLabel = new QLabel(windowTitle(), m_titleBar);
    m_titleLabel->setObjectName(QStringLiteral("AppDialogTitleLabel"));
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    titleLayout->addWidget(m_titleLabel, 1, Qt::AlignVCenter);

    m_trailingButtonReserve = new QWidget(m_titleBar);
    m_trailingButtonReserve->setFixedWidth(0);
    titleLayout->addWidget(m_trailingButtonReserve);
    root->addWidget(m_titleBar);

    m_content = new QWidget(this);
    m_content->setObjectName(QStringLiteral("AppDialogContent"));
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(18, 0, 18, 18);
    m_contentLayout->setSpacing(14);
    root->addWidget(m_content, 1);
}

void AppDialog::refreshChrome() {
    const bool platformButton = m_closeButtonPlacement == CloseButtonPlacement::Platform;
    m_windowAgent.setSystemButtonsVisible(platformButton);
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    m_leadingButtonReserve->setFixedWidth(platformButton ? kMacCloseButtonReserve : 0);
    m_trailingButtonReserve->setFixedWidth(0);
#else
    m_leadingButtonReserve->setFixedWidth(0);
    m_trailingButtonReserve->setFixedWidth(platformButton ? kWindowsCloseButtonReserve : 0);
#endif
}

void AppDialog::refreshTitle() {
    if (m_titleLabel) {
        m_titleLabel->setText(windowTitle());
    }
}

} // namespace qtm
