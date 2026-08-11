#include "update/AppUpdater.h"

#include <QtTest>

using namespace qtm;

class AppUpdaterTest final : public QObject {
    Q_OBJECT

private slots:
    void comparesSemanticVersions_data();
    void comparesSemanticVersions();
    void parsesPlatformManifest();
    void selectsLocalizedChangelog();
    void parsesGitHubReleaseFeed();
    void requiresSecurePackageMetadata();
    void usesRateLimitIndependentDefaultFeed();
};

void AppUpdaterTest::comparesSemanticVersions_data() {
    QTest::addColumn<QString>("left");
    QTest::addColumn<QString>("right");
    QTest::addColumn<int>("expectedSign");

    QTest::newRow("equal") << QStringLiteral("1.2.3") << QStringLiteral("v1.2.3") << 0;
    QTest::newRow("patch") << QStringLiteral("1.2.3") << QStringLiteral("1.2.4") << -1;
    QTest::newRow("stable-after-beta")
        << QStringLiteral("1.0.0") << QStringLiteral("1.0.0-beta.9") << 1;
    QTest::newRow("numeric-prerelease")
        << QStringLiteral("1.0.0-beta.10") << QStringLiteral("1.0.0-beta.2") << 1;
    QTest::newRow("shorter-prerelease")
        << QStringLiteral("1.0.0-beta") << QStringLiteral("1.0.0-beta.1") << -1;
    QTest::newRow("build-metadata")
        << QStringLiteral("1.0.0+build.1") << QStringLiteral("1.0.0+build.2") << 0;
}

void AppUpdaterTest::selectsLocalizedChangelog() {
    const QByteArray manifest = R"json(
{
  "channel": "beta",
  "localizations": {
    "en": { "changelog": "English release notes" },
    "zh_CN": { "changelog": "Chinese release notes" }
  },
  "updates": {
    "default": {
      "latest-version": "0.2.0-beta.2"
    }
  }
}
)json";

    QString error;
    const UpdateCheckResult chinese = AppUpdater::parseUpdatePayload(
        manifest, QStringLiteral("0.2.0-beta.1"), &error, QStringLiteral("zh_CN"));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(chinese.changelog, QStringLiteral("Chinese release notes"));

    const UpdateCheckResult english = AppUpdater::parseUpdatePayload(
        manifest, QStringLiteral("0.2.0-beta.1"), &error, QStringLiteral("en"));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(english.changelog, QStringLiteral("English release notes"));
}

void AppUpdaterTest::comparesSemanticVersions() {
    QFETCH(QString, left);
    QFETCH(QString, right);
    QFETCH(int, expectedSign);
    const int comparison = AppUpdater::compareVersions(left, right);
    QCOMPARE(comparison == 0 ? 0 : (comparison < 0 ? -1 : 1), expectedSign);
}

void AppUpdaterTest::parsesPlatformManifest() {
    const QByteArray manifest = R"json(
{
  "schema-version": 2,
  "channel": "beta",
  "updates": {
    "windows": {
      "latest-version": "0.2.0-beta.2",
      "runtime-version": "qt-6.10.1-r1",
      "minimum-supported-version": "0.2.0",
      "download-url": "https://github.com/qianvk/QTierMaker/releases/download/v0.2.0-beta.2/QTierMaker-0.2.0-beta.2-Windows-AMD64.exe",
      "release-url": "https://github.com/qianvk/QTierMaker/releases/tag/v0.2.0-beta.2",
      "file-name": "QTierMaker-0.2.0-beta.2-Windows-AMD64.exe",
      "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "size": 1234,
      "changelog": "Beta fixes",
      "update": {
        "download-url": "https://github.com/qianvk/QTierMaker/releases/download/v0.2.0-beta.2/QTierMaker-0.2.0-beta.2-WinUpdate-AMD64.exe",
        "file-name": "QTierMaker-0.2.0-beta.2-WinUpdate-AMD64.exe",
        "sha256": "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
        "size": 567
      }
    },
    "macos": {
      "latest-version": "0.2.0-beta.2",
      "minimum-supported-version": "0.2.0",
      "download-url": "https://github.com/qianvk/QTierMaker/releases/download/v0.2.0-beta.2/QTierMaker-0.2.0-beta.2-Darwin-arm64.dmg",
      "release-url": "https://github.com/qianvk/QTierMaker/releases/tag/v0.2.0-beta.2",
      "file-name": "QTierMaker-0.2.0-beta.2-Darwin-arm64.dmg",
      "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "size": 1234,
      "update": {
        "download-url": "https://github.com/qianvk/QTierMaker/releases/download/v0.2.0-beta.2/QTierMaker-0.2.0-beta.2-macOS-arm64-Update.zip",
        "file-name": "QTierMaker-0.2.0-beta.2-macOS-arm64-Update.zip",
        "sha256": "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
        "size": 789
      }
    },
    "linux": {
      "latest-version": "0.2.0-beta.2",
      "minimum-supported-version": "0.2.0",
      "download-url": "https://github.com/qianvk/QTierMaker/releases/download/v0.2.0-beta.2/QTierMaker-0.2.0-beta.2-Linux-x86_64.AppImage",
      "release-url": "https://github.com/qianvk/QTierMaker/releases/tag/v0.2.0-beta.2",
      "file-name": "QTierMaker-0.2.0-beta.2-Linux-x86_64.AppImage",
      "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "size": 1234
    }
  }
}
)json";

    QString error;
    const UpdateCheckResult result =
        AppUpdater::parseUpdatePayload(manifest, QStringLiteral("0.2.0-beta.1"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(result.updateAvailable);
    QVERIFY(result.mandatory);
    QCOMPARE(result.latestVersion, QStringLiteral("0.2.0-beta.2"));
    QCOMPARE(result.channel, QStringLiteral("beta"));
    QCOMPARE(result.sha256.size(), 64);
    QVERIFY(result.downloadUrl.isValid());
#if defined(Q_OS_WIN)
    QCOMPARE(AppUpdater::runtimeVersion(), QStringLiteral("qt-6.10.1-r1"));
    QVERIFY(result.updatePackage);
    QCOMPARE(result.fileName, QStringLiteral("QTierMaker-0.2.0-beta.2-WinUpdate-AMD64.exe"));
    QCOMPARE(result.packageSize, 567);

    QByteArray incompatibleManifest = manifest;
    incompatibleManifest.replace("qt-6.10.1-r1", "qt-6.10.1-r2");
    const UpdateCheckResult fallbackResult =
        AppUpdater::parseUpdatePayload(incompatibleManifest,
                                       QStringLiteral("0.2.0-beta.1"),
                                       &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!fallbackResult.updatePackage);
    QCOMPARE(fallbackResult.fileName,
             QStringLiteral("QTierMaker-0.2.0-beta.2-Windows-AMD64.exe"));
    QCOMPARE(fallbackResult.packageSize, 1234);
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    QVERIFY(result.updatePackage);
    QCOMPARE(result.fileName,
             QStringLiteral("QTierMaker-0.2.0-beta.2-macOS-arm64-Update.zip"));
    QCOMPARE(result.packageSize, 789);
    QCOMPARE(result.installerFileName,
             QStringLiteral("QTierMaker-0.2.0-beta.2-Darwin-arm64.dmg"));
    QCOMPARE(result.installerPackageSize, 1234);
#else
    QVERIFY(!result.updatePackage);
    QCOMPARE(result.packageSize, 1234);
#endif
}

void AppUpdaterTest::parsesGitHubReleaseFeed() {
#if defined(Q_OS_WIN)
    constexpr auto packageName = "QTierMaker-0.2.0-beta.3-Windows-AMD64.exe";
    const QString updateAsset = QStringLiteral(R"json(,
      {
        "name": "QTierMaker-0.2.0-beta.3-WinUpdate-AMD64.exe",
        "browser_download_url": "https://github.com/qianvk/QTierMaker/releases/download/v0.2.0-beta.3/QTierMaker-0.2.0-beta.3-WinUpdate-AMD64.exe",
        "size": 678,
        "digest": "sha256:dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"
      })json");
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    constexpr auto packageName = "QTierMaker-0.2.0-beta.3-Darwin-universal.dmg";
    const QString updateAsset = QStringLiteral(R"json(,
      {
        "name": "QTierMaker-0.2.0-beta.3-macOS-arm64-Update.zip",
        "browser_download_url": "https://github.com/qianvk/QTierMaker/releases/download/v0.2.0-beta.3/QTierMaker-0.2.0-beta.3-macOS-arm64-Update.zip",
        "size": 679,
        "digest": "sha256:eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
      })json");
#else
    constexpr auto packageName = "QTierMaker-0.2.0-beta.3-Linux-x86_64.AppImage";
    const QString updateAsset;
#endif
    const QByteArray releaseTemplate = R"json(
[
  {
    "tag_name": "v0.2.0-beta.4",
    "html_url": "https://github.com/qianvk/QTierMaker/releases/tag/v0.2.0-beta.4",
    "draft": true,
    "prerelease": true,
    "assets": []
  },
  {
    "tag_name": "v0.2.0-beta.3",
    "html_url": "https://github.com/qianvk/QTierMaker/releases/tag/v0.2.0-beta.3",
    "body": "Release notes",
    "draft": false,
    "prerelease": true,
    "assets": [
      {
        "name": "%1",
        "browser_download_url": "https://github.com/qianvk/QTierMaker/releases/download/v0.2.0-beta.3/%1",
        "size": 4321,
        "digest": "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      },
      {
        "name": "updates.json",
        "browser_download_url": "https://github.com/qianvk/QTierMaker/releases/download/v0.2.0-beta.3/updates.json",
        "size": 1024
      }%2
    ]
  },
  {
    "tag_name": "v0.2.0-beta.2",
    "html_url": "https://github.com/qianvk/QTierMaker/releases/tag/v0.2.0-beta.2",
    "draft": false,
    "prerelease": true,
    "assets": []
  }
]
)json";
    const QByteArray payload =
        QString::fromUtf8(releaseTemplate)
            .arg(QString::fromLatin1(packageName), updateAsset)
            .toUtf8();

    QString error;
    const UpdateCheckResult result =
        AppUpdater::parseUpdatePayload(payload, QStringLiteral("0.2.0-beta.1"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(result.updateAvailable);
    QCOMPARE(result.latestVersion, QStringLiteral("0.2.0-beta.3"));
    QCOMPARE(result.channel, QStringLiteral("beta"));
    QCOMPARE(result.metadataUrl,
             QUrl(QStringLiteral("https://github.com/qianvk/QTierMaker/releases/download/"
                                 "v0.2.0-beta.3/updates.json")));
#if !defined(Q_OS_MACOS) && !defined(Q_OS_MAC)
    QCOMPARE(result.fileName, QString::fromLatin1(packageName));
    QCOMPARE(result.packageSize, 4321);
    QCOMPARE(result.sha256,
             QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
#endif
#if defined(Q_OS_WIN)
    QCOMPARE(result.updateFileName,
             QStringLiteral("QTierMaker-0.2.0-beta.3-WinUpdate-AMD64.exe"));
    QCOMPARE(result.updatePackageSize, 678);
    QCOMPARE(result.updateSha256,
             QStringLiteral("dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"));
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    QVERIFY(result.updatePackage);
    QCOMPARE(result.fileName,
             QStringLiteral("QTierMaker-0.2.0-beta.3-macOS-arm64-Update.zip"));
    QCOMPARE(result.packageSize, 679);
    QCOMPARE(result.sha256,
             QStringLiteral("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"));
    QCOMPARE(result.installerFileName, QString::fromLatin1(packageName));
    QCOMPARE(result.installerPackageSize, 4321);
    QCOMPARE(result.updateFileName,
             QStringLiteral("QTierMaker-0.2.0-beta.3-macOS-arm64-Update.zip"));
    QCOMPARE(result.updatePackageSize, 679);
    QCOMPARE(result.updateSha256,
             QStringLiteral("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"));
#endif
}

void AppUpdaterTest::requiresSecurePackageMetadata() {
    const QByteArray manifest = R"json(
{
  "updates": {
    "default": {
      "latest-version": "9.0.0",
      "download-url": "http://example.com/QTierMaker.exe",
      "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    }
  }
}
)json";

    QString error;
    const UpdateCheckResult result =
        AppUpdater::parseUpdatePayload(manifest, QStringLiteral("0.2.0"), &error);
    QVERIFY(!error.isEmpty());
    QVERIFY(!result.updateAvailable);
}

void AppUpdaterTest::usesRateLimitIndependentDefaultFeed() {
    const QUrl url = AppUpdater::defaultUpdateDefinitionUrl();
    QCOMPARE(url.scheme(), QStringLiteral("https"));
    QCOMPARE(url.host(), QStringLiteral("github.com"));
    QVERIFY(url.path().endsWith(QStringLiteral("/releases/latest/download/updates.json")));
    QVERIFY(url.query().isEmpty());
}

QTEST_MAIN(AppUpdaterTest)

#include "tst_AppUpdater.moc"
