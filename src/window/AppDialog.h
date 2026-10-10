#pragma once

#include <QDialog>

#include <vkui/window/VWindowAgent.h>

class QEvent;
class QLabel;
class QShowEvent;
class QVBoxLayout;
class QWidget;

namespace qtm {

/** Application-owned input dialog backed by VkUI native full-content windowing. */
class AppDialog : public QDialog {
public:
    enum class CloseButtonPlacement { Hidden, Platform };

    explicit AppDialog(const QString& title, QWidget* parent = nullptr);

    QVBoxLayout* contentLayout() const { return m_contentLayout; }
    QWidget* titleBar() const { return m_titleBar; }
    vkui::VWindowAgent* windowAgent() { return &m_windowAgent; }
    const vkui::VWindowAgent* windowAgent() const { return &m_windowAgent; }

    bool isResizable() const { return m_resizable; }
    void setResizable(bool resizable);
    CloseButtonPlacement closeButtonPlacement() const { return m_closeButtonPlacement; }
    void setCloseButtonPlacement(CloseButtonPlacement placement);

protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void buildUi();
    void refreshChrome();
    void refreshTitle();

    QWidget* m_titleBar{nullptr};
    QWidget* m_leadingButtonReserve{nullptr};
    QWidget* m_trailingButtonReserve{nullptr};
    QLabel* m_titleLabel{nullptr};
    QWidget* m_content{nullptr};
    QVBoxLayout* m_contentLayout{nullptr};
    bool m_resizable{false};
    CloseButtonPlacement m_closeButtonPlacement{CloseButtonPlacement::Hidden};
    // Declared last so native teardown precedes QDialog base destruction.
    vkui::VWindowAgent m_windowAgent;
};

} // namespace qtm
