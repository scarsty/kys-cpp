#pragma once

#include <string>
#include <string_view>

enum class ExternalJsonTransferStatus
{
    Completed,
    Cancelled,
    Error,
};

struct ExternalJsonTransferResult
{
    ExternalJsonTransferStatus status = ExternalJsonTransferStatus::Cancelled;
    std::string text;
    std::string error;

    bool completed() const { return status == ExternalJsonTransferStatus::Completed; }
};

class ExternalJsonFileTransfer
{
public:
    static ExternalJsonTransferResult importJson(std::string_view title);
    static ExternalJsonTransferResult exportJson(
        std::string_view title,
        std::string_view suggestedFilename,
        std::string_view text);
};
