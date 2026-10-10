#pragma once

#include <QString>

namespace qtm {

enum class ExportFormat { Png, Jpeg };

/** Options for exporting the board exactly as it is currently laid out. */
class ExportOptions {
public:
    ExportFormat format{ExportFormat::Png};
    int scale{2};
    int jpegQuality{94};

    static ExportFormat formatFromSuffix(const QString& suffix);
    static QString suffixForFormat(ExportFormat format);
};

} // namespace qtm
