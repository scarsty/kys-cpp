#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace KysChess
{

enum class ChessDiagnosticSeverity
{
    Info,
    Warning,
    Error,
};

struct ChessDiagnostic
{
    ChessDiagnosticSeverity severity = ChessDiagnosticSeverity::Info;
    std::string source;
    std::string message;
};

using ChessDiagnosticSink = std::function<void(const ChessDiagnostic&)>;
using ChessTextConverter = std::function<std::string(std::string_view)>;

inline void emitChessDiagnostic(
    const ChessDiagnosticSink& sink,
    ChessDiagnosticSeverity severity,
    std::string_view source,
    std::string_view message)
{
    if (sink)
    {
        sink({severity, std::string(source), std::string(message)});
    }
}

class ChessDiagnosticCollector
{
public:
    ChessDiagnosticSink sink()
    {
        return [this](const ChessDiagnostic& diagnostic) { diagnostics_.push_back(diagnostic); };
    }

    const std::vector<ChessDiagnostic>& diagnostics() const { return diagnostics_; }
    bool hasErrors() const;

private:
    std::vector<ChessDiagnostic> diagnostics_;
};

}
