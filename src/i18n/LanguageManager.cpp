#include "i18n/LanguageManager.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>

#include <array>

namespace qtm {

namespace {
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

LanguageManager::LanguageManager(QApplication* app, QObject* parent) : QObject(parent), m_app(app) {}

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
    emit languageChanged(requestedLanguage);
    return loaded || resolvedLanguage == QStringLiteral("en");
}

} // namespace qtm
