#pragma once

#include "persistence/SampleProjectSeeder.h"

#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>

#include <memory>

class QNetworkReply;
class QTemporaryFile;

namespace qtm {

/** Downloads, verifies, extracts, and atomically installs the optional sample project. */
class SampleProjectDownloader final : public QObject {
    Q_OBJECT

public:
    explicit SampleProjectDownloader(QObject* parent = nullptr);
    ~SampleProjectDownloader() override;

    static QString projectName();
    static QString projectDirectoryPath(const QString& destinationRoot);
    static QString projectFilePath(const QString& destinationRoot);

    bool isBusy() const;
    void start(const QString& destinationRoot, SampleProjectConflictPolicy conflictPolicy);
    void cancel();

signals:
    void progressChanged(qint64 bytesReceived, qint64 bytesTotal);
    void installing();
    void installed(const QString& projectPath, qtm::SampleProjectSeedStatus status);
    void failed(const QString& message, const QString& details);

private:
    void readReplyData();
    void finishReply();
    void finishInstall();
    void fail(const QString& message, const QString& details = {});

    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    std::unique_ptr<QTemporaryFile> m_packageFile;
    QFutureWatcher<Result<SampleProjectSeedResult>> m_installWatcher;
    QCryptographicHash m_digest{QCryptographicHash::Sha256};
    QString m_destinationRoot;
    QString m_writeFailure;
    SampleProjectConflictPolicy m_conflictPolicy{SampleProjectConflictPolicy::KeepExisting};
    qint64 m_receivedBytes{0};
    bool m_busy{false};
    bool m_cancelled{false};
};

} // namespace qtm
