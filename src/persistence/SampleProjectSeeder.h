#pragma once

#include "persistence/Result.h"

#include <QString>

namespace qtm {

enum class SampleProjectConflictPolicy { KeepExisting, Replace, KeepBoth };
enum class SampleProjectSeedStatus { Copied, AlreadyExists, Replaced, KeptBoth };

struct SampleProjectSeedResult final {
    QString projectPath;
    SampleProjectSeedStatus status{SampleProjectSeedStatus::AlreadyExists};
};

/**
 * Copies a validated sample through a staging directory, then publishes it with a same-volume
 * rename. Replacement uses a rollback directory; projects are never merged file by file.
 */
class SampleProjectSeeder final {
public:
    static Result<SampleProjectSeedResult>
    seed(const QString& sourceDirectory, const QString& destinationRoot,
         const QString& projectDirectoryName, const QString& projectFileName,
         SampleProjectConflictPolicy conflictPolicy = SampleProjectConflictPolicy::KeepExisting);
};

} // namespace qtm
