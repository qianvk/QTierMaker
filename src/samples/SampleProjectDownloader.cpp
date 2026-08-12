#include "samples/SampleProjectDownloader.h"

#include "persistence/TarArchive.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QUrl>
#include <QtConcurrentRun>

namespace qtm {

namespace {
constexpr qint64 kMaximumPackageSize = 64LL * 1024LL * 1024LL;
constexpr auto kProjectName = "Anime Girls v5";
constexpr auto kProjectFileName = "Anime Girls v5.qtm";
constexpr auto kPackageUrl = "https://github.com/qianvk/QTierMaker/releases/download/samples-v1/"
                             "QTierMaker-Sample-Anime-Girls-v5.tar";
constexpr auto kPackageSha256 = "94b66bb3a44488867993c6f3ea5cf47c5eaa56bcfad7b06d63f918f9428ac094";

Result<SampleProjectSeedResult> installPackage(const QString& packagePath,
                                               const QString& destinationRoot,
                                               SampleProjectConflictPolicy conflictPolicy) {
    QTemporaryDir extracted;
    if (!extracted.isValid()) {
        return Result<SampleProjectSeedResult>::failure(
            QObject::tr("A temporary folder for the sample project could not be created."));
    }

    const auto extraction = TarArchive::extract(packagePath, extracted.path());
    if (!extraction) {
        return Result<SampleProjectSeedResult>::failure(extraction.error().message,
                                                        extraction.error().details);
    }

    const QString sourceDirectory =
        QDir(extracted.path()).filePath(QString::fromLatin1(kProjectName));
    return SampleProjectSeeder::seed(sourceDirectory, destinationRoot,
                                     QString::fromLatin1(kProjectName),
                                     QString::fromLatin1(kProjectFileName), conflictPolicy);
}
} // namespace

SampleProjectDownloader::SampleProjectDownloader(QObject* parent) : QObject(parent) {
    connect(&m_installWatcher, &QFutureWatcherBase::finished, this,
            &SampleProjectDownloader::finishInstall);
}

SampleProjectDownloader::~SampleProjectDownloader() {
    cancel();
}

QString SampleProjectDownloader::projectName() {
    return QString::fromLatin1(kProjectName);
}

QString SampleProjectDownloader::projectDirectoryPath(const QString& destinationRoot) {
    return QDir(destinationRoot).filePath(QString::fromLatin1(kProjectName));
}

QString SampleProjectDownloader::projectFilePath(const QString& destinationRoot) {
    return QDir(projectDirectoryPath(destinationRoot))
        .filePath(QString::fromLatin1(kProjectFileName));
}

bool SampleProjectDownloader::isBusy() const {
    return m_busy;
}

void SampleProjectDownloader::start(const QString& destinationRoot,
                                    SampleProjectConflictPolicy conflictPolicy) {
    if (m_busy) {
        return;
    }

    m_packageFile = std::make_unique<QTemporaryFile>(
        QDir(QDir::tempPath()).filePath(QStringLiteral("QTierMaker-sample-XXXXXX.tar")));
    if (!m_packageFile->open()) {
        const QString details = m_packageFile->errorString();
        m_packageFile.reset();
        fail(tr("The sample project download could not be started."), details);
        return;
    }

    m_busy = true;
    m_cancelled = false;
    m_destinationRoot = QDir::cleanPath(destinationRoot);
    m_conflictPolicy = conflictPolicy;
    m_receivedBytes = 0;
    m_writeFailure.clear();
    m_digest.reset();

    QNetworkRequest request(QUrl(QString::fromLatin1(kPackageUrl)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(60000);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("QTierMaker sample-project downloader"));

    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &SampleProjectDownloader::readReplyData);
    connect(m_reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) { emit progressChanged(received, total); });
    connect(m_reply, &QNetworkReply::finished, this, &SampleProjectDownloader::finishReply);
    emit progressChanged(0, 0);
}

void SampleProjectDownloader::cancel() {
    m_cancelled = true;
    if (m_reply) {
        m_reply->abort();
    }
}

void SampleProjectDownloader::readReplyData() {
    if (!m_reply || !m_packageFile) {
        return;
    }
    const QByteArray data = m_reply->readAll();
    if (data.isEmpty()) {
        return;
    }
    if (m_receivedBytes > kMaximumPackageSize - data.size()) {
        m_writeFailure = tr("The downloaded sample project is unexpectedly large.");
        m_reply->abort();
        return;
    }
    if (m_packageFile->write(data) != data.size()) {
        m_writeFailure = m_packageFile->errorString();
        m_reply->abort();
        return;
    }
    m_receivedBytes += data.size();
    m_digest.addData(data);
}

void SampleProjectDownloader::finishReply() {
    QNetworkReply* reply = m_reply.data();
    if (!reply) {
        return;
    }
    readReplyData();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString error;
    QString details;
    if (!m_writeFailure.isEmpty()) {
        error = tr("The sample project could not be downloaded.");
        details = m_writeFailure;
    } else if (reply->error() != QNetworkReply::NoError) {
        error = tr("The sample project could not be downloaded.");
        details = reply->errorString();
    } else if (status < 200 || status >= 300) {
        error = tr("The sample project could not be downloaded.");
        details = tr("The server returned HTTP status %1.").arg(status);
    }

    reply->deleteLater();
    m_reply = nullptr;

    if (m_cancelled) {
        m_packageFile.reset();
        m_busy = false;
        return;
    }
    if (!error.isEmpty()) {
        fail(error, details);
        return;
    }

    if (!m_packageFile->flush()) {
        fail(tr("The sample project could not be downloaded."), m_packageFile->errorString());
        return;
    }
    m_packageFile->close();
    const QByteArray actualDigest = m_digest.result().toHex();
    if (actualDigest != QByteArray(kPackageSha256)) {
        fail(tr("The downloaded sample project could not be verified."),
             tr("The SHA-256 checksum does not match the published sample."));
        return;
    }

    emit installing();

    const QString packagePath = m_packageFile->fileName();
    const QString destinationRoot = m_destinationRoot;
    const SampleProjectConflictPolicy conflictPolicy = m_conflictPolicy;
    m_installWatcher.setFuture(QtConcurrent::run([packagePath, destinationRoot, conflictPolicy]() {
        return installPackage(packagePath, destinationRoot, conflictPolicy);
    }));
}

void SampleProjectDownloader::finishInstall() {
    const Result<SampleProjectSeedResult> result = m_installWatcher.result();
    m_packageFile.reset();
    m_busy = false;
    if (!result) {
        emit failed(result.error().message, result.error().details);
        return;
    }
    emit installed(result.value().projectPath, result.value().status);
}

void SampleProjectDownloader::fail(const QString& message, const QString& details) {
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_packageFile.reset();
    m_busy = false;
    emit failed(message, details);
}

} // namespace qtm
