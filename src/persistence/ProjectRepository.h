#pragma once

#include "persistence/ProjectSerializer.h"

#include <QObject>
#include <QString>

namespace qtm {

/** Opens and saves current `.qtmproject` and legacy `.tlmproject` files. */
class ProjectRepository : public QObject {
    Q_OBJECT

public:
    explicit ProjectRepository(QObject* parent = nullptr);

    Result<TierProject> openProject(const QString& filePath) const;
    Result<void> saveProject(TierProject& project, const QString& filePath) const;

private:
    ProjectSerializer m_serializer;
};

} // namespace qtm
