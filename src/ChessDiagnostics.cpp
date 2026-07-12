#include "ChessDiagnostics.h"

#include <algorithm>

namespace KysChess
{

bool ChessDiagnosticCollector::hasErrors() const
{
    return std::ranges::any_of(diagnostics_, [](const ChessDiagnostic& diagnostic) {
        return diagnostic.severity == ChessDiagnosticSeverity::Error;
    });
}

}
