#pragma once

#include "persistence/Result.h"

#include <QString>

namespace tlm {

enum class SampleProjectSeedStatus { Copied, AlreadyExists };

struct SampleProjectSeedResult final {
    QString projectPath;
    SampleProjectSeedStatus status{SampleProjectSeedStatus::AlreadyExists};
};

/**
 * Copies an installed sample through a staging directory, then publishes it with a same-volume
 * rename. Existing user projects are never merged or overwritten.
 */
class SampleProjectSeeder final {
public:
    static Result<SampleProjectSeedResult> seed(const QString& sourceDirectory,
                                                const QString& destinationRoot,
                                                const QString& projectDirectoryName,
                                                const QString& projectFileName);
};

} // namespace tlm
