#pragma once

#include "persistence/Result.h"

#include <QString>

namespace qtm {

/** Extracts the regular-file subset of a POSIX TAR archive into a fresh directory. */
class TarArchive final {
public:
    static Result<void> extract(const QString& archivePath, const QString& destinationDirectory);
};

} // namespace qtm
