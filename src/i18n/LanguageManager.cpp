#include "i18n/LanguageManager.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QLibraryInfo>
#include <QLocale>
#include <QPushButton>
#include <QTimer>

#include <array>

namespace qtm {

namespace {
void refreshCancelLabel(QDialogButtonBox* buttons) {
    if (auto* cancel = buttons->button(QDialogButtonBox::Cancel)) {
        const QString text = QCoreApplication::translate("QPlatformTheme", "Cancel");
        if (cancel->text() != text) {
            cancel->setText(text);
        }
    }
}

// Qt obtains standard QDialogButtonBox labels from platform theme contexts. Keeping these source
// strings in the application catalog makes packaged builds independent of Qt's optional catalogs.
[[maybe_unused]] constexpr std::array kStandardDialogButtonTexts{
    QT_TRANSLATE_NOOP("QPlatformTheme", "OK"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Open"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Save"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Cancel"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Discard"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Don't Save"),
    QT_TRANSLATE_NOOP("QPlatformTheme", "Close"),
    QT_TRANSLATE_NOOP("QDialogButtonBox", "Don't Save"),
    QT_TRANSLATE_NOOP("QCocoaTheme", "Don't Save"),
    QT_TRANSLATE_NOOP("QWK::WindowsCaptionButton", "Minimize"),
    QT_TRANSLATE_NOOP("QWK::WindowsCaptionButton", "Maximize"),
    QT_TRANSLATE_NOOP("QWK::WindowsCaptionButton", "Close"),
    QT_TRANSLATE_NOOP("QWK::WindowsCaptionButton", "Restore Down"),
};
} // namespace

LanguageManager::LanguageManager(QApplication* app, QObject* parent) : QObject(parent), m_app(app) {
    if (m_app) {
        m_app->installEventFilter(this);
    }
}

bool LanguageManager::eventFilter(QObject* watched, QEvent* event) {
    if (event && (event->type() == QEvent::Show || event->type() == QEvent::Polish ||
                  event->type() == QEvent::LanguageChange || event->type() == QEvent::StyleChange)) {
        if (auto* buttons = qobject_cast<QDialogButtonBox*>(watched)) {
            if (event->type() == QEvent::Show) {
                refreshCancelLabel(buttons);
                return QObject::eventFilter(watched, event);
            }
            // Qt's own handler can refresh standard labels after an application event filter.
            // Reconcile afterwards, with the button box as context so destruction cancels it.
            QTimer::singleShot(0, buttons, [buttons]() { refreshCancelLabel(buttons); });
        }
    }
    return QObject::eventFilter(watched, event);
}

QStringList LanguageManager::availableLanguages() const {
    return {QStringLiteral("system"), QStringLiteral("en"), QStringLiteral("zh_CN")};
}

bool LanguageManager::setLanguage(const QString& languageCode) {
    if (!m_app) {
        return false;
    }
    const QString requestedLanguage = languageCode.isEmpty() ? QStringLiteral("system") : languageCode;
    const QString resolvedLanguage =
        requestedLanguage == QStringLiteral("system")
            ? (QLocale::system().name().startsWith(QStringLiteral("zh")) ? QStringLiteral("zh_CN")
                                                                          : QStringLiteral("en"))
            : requestedLanguage;

    m_app->removeTranslator(&m_translator);
    m_app->removeTranslator(&m_qtTranslator);
    bool loaded = true;
    if (resolvedLanguage == QStringLiteral("zh_CN")) {
        const bool qtLoaded = m_qtTranslator.load(QStringLiteral("qtbase_zh_CN"),
                                                  QLibraryInfo::path(QLibraryInfo::TranslationsPath));
        if (qtLoaded) {
            m_app->installTranslator(&m_qtTranslator);
        }
        loaded = m_translator.load(QStringLiteral(":/translations/QTierMaker_zh_CN.qm"));
        if (loaded) {
            m_app->installTranslator(&m_translator);
        }
    }
    m_currentLanguage = requestedLanguage;
    for (QWidget* widget : QApplication::allWidgets()) {
        if (auto* buttons = qobject_cast<QDialogButtonBox*>(widget)) {
            refreshCancelLabel(buttons);
        }
    }
    emit languageChanged(requestedLanguage);
    return loaded || resolvedLanguage == QStringLiteral("en");
}

} // namespace qtm
