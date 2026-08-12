#include "app/Application.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QSslSocket>
#include <QStyle>
#include <QTextStream>

#include <cstdlib>
#include <cstring>

namespace {

bool hasArgument(int argc, char* argv[], const char* expected) {
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], expected) == 0) {
            return true;
        }
    }
    return false;
}

int runDeploymentSmokeTest(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QStringList failures;

#ifdef Q_OS_MACOS
    if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
        failures.append(QStringLiteral("The Cocoa platform plugin did not load"));
    }
#endif

    const auto formats = QImageReader::supportedImageFormats();
    for (const QByteArray& format :
         {QByteArrayLiteral("png"), QByteArrayLiteral("bmp"), QByteArrayLiteral("gif"),
          QByteArrayLiteral("jpeg"), QByteArrayLiteral("webp")}) {
        if (!formats.contains(format)) {
            failures.append(
                QStringLiteral("Missing image format: %1").arg(QString::fromLatin1(format)));
        }
    }

#ifdef Q_OS_MACOS
    if (!QSslSocket::setActiveBackend(QStringLiteral("securetransport")) ||
        !QSslSocket::supportsSsl()) {
        failures.append(QStringLiteral("The SecureTransport TLS backend did not load"));
    }
    const QString helper =
        QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("../Helpers/QTierMakerMacUpdateHelper"));
    if (!QFileInfo::exists(helper)) {
        failures.append(QStringLiteral("The macOS update helper is missing"));
    }
#endif

    if (!QApplication::style()) {
        failures.append(QStringLiteral("The native application style did not load"));
    }

    QTextStream output(failures.isEmpty() ? stdout : stderr);
    if (failures.isEmpty()) {
        output << "QTierMaker deployment smoke test passed\n";
        return EXIT_SUCCESS;
    }
    for (const QString& failure : failures) {
        output << "QTierMaker deployment smoke test failed: " << failure << '\n';
    }
    return EXIT_FAILURE;
}

int printVersion(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("QTierMaker"));
    app.setApplicationVersion(QStringLiteral(QTM_APP_VERSION));
    QTextStream(stdout) << app.applicationName() << ' ' << app.applicationVersion() << '\n';
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char* argv[]) {
    // VkUI must keep the frameless host as the only native widget window. Creating a
    // transient popover must not promote title-bar siblings outside the agent's event pipeline.
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    if (hasArgument(argc, argv, "--deployment-smoke-test")) {
        return runDeploymentSmokeTest(argc, argv);
    }
    if (hasArgument(argc, argv, "--version")) {
        return printVersion(argc, argv);
    }
    qtm::Application app(argc, argv);
    return app.run();
}
