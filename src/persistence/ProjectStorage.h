#pragma once

#include "persistence/Result.h"

#include <QString>

namespace qtm {

/** Performs destructive project-storage cleanup after a replacement is safely published. */
class ProjectStorage final {
public:
    static Result<void> remove(const QString& projectFilePath);
};

} // namespace qtm
