#include "ChessJsonProtocol.h"

#include "ChessJsonCodec.h"
#include "ChessReplayJson.h"
#include "ChessReplayVerifier.h"
#include "Utf8Path.h"

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <utility>

namespace KysChess
{
using namespace ProtocolDetail;

namespace
{

struct McpToolDefinition
{
    std::string_view name;
    std::string_view nativeMethod;
    std::string_view description;
    std::string_view inputSchema;
};

constexpr std::string_view kEmptySchema =
    R"({"type":"object","properties":{},"additionalProperties":false})";
constexpr std::string_view kNewGameSchema = R"({
    "type":"object",
    "properties":{
        "difficulty":{"type":"string","enum":["easy","normal","hard"],"default":"normal"},
        "seed":{"type":"string","pattern":"^0x[0-9a-fA-F]{16}$","default":"0x0000000000000001","description":"0x 前綴加上固定 16 位十六進位數字，例如 0x000000000000BEEF"},
        "position_swap_enabled":{"type":"boolean","default":true},
        "detail":{"type":"string","enum":["compact","full"],"default":"full"}
    },
    "additionalProperties":false
})";
constexpr std::string_view kObserveSchema = R"({
    "type":"object",
    "properties":{"detail":{"type":"string","enum":["compact","full"],"default":"compact"}},
    "additionalProperties":false
})";
constexpr std::string_view kActionSchema = R"({
    "type":"object",
    "properties":{
        "action":{"type":"object"},
        "detail":{"type":"string","enum":["summary","compact","full"],"default":"summary"}
    },
    "required":["action"],
    "additionalProperties":false
})";
constexpr std::string_view kShopSlotSchema = R"({
    "type":"object","properties":{"slot":{"type":"integer"}},"required":["slot"],"additionalProperties":false
})";
constexpr std::string_view kShopOddsSchema = R"({
    "type":"object","properties":{"level":{"type":["integer","null"],"default":null}},"additionalProperties":false
})";
constexpr std::string_view kChessInstanceSchema = R"({
    "type":"object","properties":{"chess_instance_id":{"type":"integer"}},"required":["chess_instance_id"],"additionalProperties":false
})";
constexpr std::string_view kRoleSchema = R"({
    "type":"object","properties":{"role_id":{"type":"integer"}},"required":["role_id"],"additionalProperties":false
})";
constexpr std::string_view kComboSchema = R"({
    "type":"object","properties":{"combo_name":{"type":"string"}},"required":["combo_name"],"additionalProperties":false
})";
constexpr std::string_view kEquipmentSchema = R"({
    "type":"object","properties":{"item_id":{"type":"integer"}},"required":["item_id"],"additionalProperties":false
})";
constexpr std::string_view kChallengeSchema = R"({
    "type":"object","properties":{"challenge_name":{"type":"string"}},"required":["challenge_name"],"additionalProperties":false
})";
constexpr std::string_view kPreparedBattleSchema = R"({
    "type":"object",
    "properties":{"detail":{"type":"string","enum":["summary","compact","full"],"default":"summary"}},
    "additionalProperties":false
})";
constexpr std::string_view kSlotSchema = R"({
    "type":"object","properties":{"slot":{"type":"string"}},"required":["slot"],"additionalProperties":false
})";
constexpr std::string_view kSaveSchema = R"({
    "type":"object",
    "properties":{"slot":{"type":"string"},"label":{"type":"string","default":""}},
    "required":["slot"],
    "additionalProperties":false
})";
constexpr std::string_view kSaveFileSchema = R"({
    "type":"object",
    "properties":{"slot":{"type":"string"},"path":{"type":"string"}},
    "required":["slot","path"],
    "additionalProperties":false
})";

constexpr std::array kMcpTools{
    McpToolDefinition{"new_game", "new", "建立可驗證的新棋局；若要接續上次進度，建立同難度棋局後再載入存檔。", kNewGameSchema},
    McpToolDefinition{"observe_game", "observe", "取得棋局；compact 只含決策所需狀態，full 含完整定義。", kObserveSchema},
    McpToolDefinition{"get_diagnostics", "get_diagnostics", "取得原生執行狀態與診斷資訊。", kEmptySchema},
    McpToolDefinition{"list_legal_actions", "legal_actions", "列出目前合法操作、操作結構、範例及候選值。", kEmptySchema},
    McpToolDefinition{"take_action", "act", "提交一個遊戲操作；summary 只回變更，compact 回精簡現況，full 含完整資料。", kActionSchema},
    McpToolDefinition{"inspect_shop_slot", "inspect_shop_slot", "分析單一商店欄位的價格、持有份數、合成結果、羈絆變化與抽取機率。", kShopSlotSchema},
    McpToolDefinition{"inspect_shop", "inspect_shop", "一次分析目前全部商店欄位及當前等級的費用機率。", kEmptySchema},
    McpToolDefinition{"get_shop_odds", "get_shop_odds", "取得指定或目前等級的商店費用機率與實際可用角色池。", kShopOddsSchema},
    McpToolDefinition{"inspect_chess_instance", "inspect_chess_instance", "檢視棋子實例的實際屬性、裝備、升星進度、出戰狀態與羈絆貢獻。", kChessInstanceSchema},
    McpToolDefinition{"inspect_bans", "inspect_bans", "檢視目前禁棋、剩餘容量、依費用分組的可選角色及生效時機。", kEmptySchema},
    McpToolDefinition{"inspect_role", "inspect_role", "檢視一名角色的完整屬性、各星級武學威力、範圍與羈絆。", kRoleSchema},
    McpToolDefinition{"inspect_combo", "inspect_combo", "依繁體中文名稱檢視羈絆成員、目前進度、門檻及效果。", kComboSchema},
    McpToolDefinition{"inspect_equipment", "inspect_equipment", "檢視裝備的基礎屬性、特殊效果、計入羈絆及角色專屬加成。", kEquipmentSchema},
    McpToolDefinition{"inspect_challenge", "inspect_challenge", "依繁體中文名稱檢視遠征的權威敵人星級、裝備與獎勵。", kChallengeSchema},
    McpToolDefinition{"inspect_prepared_battle", "inspect_prepared_battle", "檢視已準備戰鬥；summary 只含關鍵部署，compact 增加裝備與羈絆，full 含完整資料。", kPreparedBattleSchema},
    McpToolDefinition{"inspect_last_battle", "inspect_last_battle", "完整檢視上一場戰鬥的開局棋盤、結構化效果軌跡與逐單位統計。", kEmptySchema},
    McpToolDefinition{"list_saves", "list_saves", "列出目前可用存檔及相容性摘要。", kEmptySchema},
    McpToolDefinition{"inspect_save", "inspect_save_summary", "唯讀檢視存檔摘要，不傳輸完整檢查點，也不改變棋局、亂數或重播。", kSlotSchema},
    McpToolDefinition{"save_game", "save_game", "在穩定決策邊界覆寫欄位；不消耗亂數，也不加入遊戲重播。", kSaveSchema},
    McpToolDefinition{"load_game", "load_game", "以指定存檔替換目前狀態、亂數與重播前綴。", kSlotSchema},
    McpToolDefinition{"export_save_file", "export_save_file", "由原生程序把自包含存檔直接寫入檔案，不讓檢查點內容經過 MCP。", kSaveFileSchema},
    McpToolDefinition{"import_save_file", "import_save_file", "由原生程序讀取並驗證自包含存檔；不會自動載入或替換目前時間線。", kSaveFileSchema},
    McpToolDefinition{"export_replay", "export_replay", "匯出目前選定時間線的權威 JSONL 重播。", kEmptySchema},
};

const McpToolDefinition* mcpTool(std::string_view name)
{
    const auto found = std::ranges::find(kMcpTools, name, &McpToolDefinition::name);
    return found == kMcpTools.end() ? nullptr : &*found;
}

std::string compactJson(std::string_view json)
{
    std::string result;
    result.reserve(json.size());
    bool inString{};
    bool escaped{};
    for (const char value : json)
    {
        if (inString)
        {
            result += value;
            if (escaped)
            {
                escaped = false;
            }
            else if (value == '\\')
            {
                escaped = true;
            }
            else if (value == '"')
            {
                inString = false;
            }
        }
        else if (value == '"')
        {
            inString = true;
            result += value;
        }
        else if (!std::isspace(static_cast<unsigned char>(value)))
        {
            result += value;
        }
    }
    return result;
}

McpCatalogDto mcpCatalogDto()
{
    McpCatalogDto result;
    for (const auto& tool : kMcpTools)
    {
        result.tools.push_back({
            std::string(tool.name),
            std::string(tool.nativeMethod),
            std::string(tool.description),
            glz::raw_json(compactJson(tool.inputSchema)),
        });
    }
    return result;
}

McpToolsResultDto mcpToolsResultDto()
{
    McpToolsResultDto result;
    for (const auto& tool : kMcpTools)
    {
        result.tools.push_back({
            std::string(tool.name),
            std::string(tool.description),
            glz::raw_json(compactJson(tool.inputSchema)),
        });
    }
    return result;
}

std::string mcpResult(const glz::raw_json& id, std::string result)
{
    McpResponseDto response;
    response.id = id;
    response.result = glz::raw_json(std::move(result));
    return writeJson(response);
}

std::string mcpError(const glz::raw_json& id, int code, std::string message)
{
    McpResponseDto response;
    response.id = id;
    response.error = McpJsonRpcErrorDto{code, std::move(message)};
    return writeJson(response);
}

std::string negotiatedMcpVersion(std::string_view requested)
{
    constexpr std::array supported{
        std::string_view("2024-11-05"),
        std::string_view("2025-03-26"),
        std::string_view("2025-06-18"),
        std::string_view("2025-11-25"),
    };
    return std::ranges::contains(supported, requested)
        ? std::string(requested)
        : std::string(supported.back());
}

std::optional<std::string> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(input), {});
}

bool writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::error_code error;
    const auto parent = path.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, error);
        if (error)
        {
            return false;
        }
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    return output.good();
}

}

ChessJsonProtocol::ChessJsonProtocol(ContentProvider contentProvider)
    : contentProvider_(std::move(contentProvider))
{
}

ChessJsonProtocol::ChessJsonProtocol(std::shared_ptr<const ChessGameContent> fixedContent)
    : fixedContent_(std::move(fixedContent))
{
}

std::shared_ptr<const ChessGameContent> ChessJsonProtocol::loadContent(Difficulty difficulty)
{
    if (fixedContent_)
    {
        return fixedContent_->difficulty() == difficulty ? fixedContent_ : nullptr;
    }
    return contentProvider_ ? contentProvider_(difficulty) : nullptr;
}

std::string ChessJsonProtocol::handleLine(std::string_view requestJson)
{
    const auto request = readJson<RequestDto>(requestJson);
    if (!request)
    {
        return response(glz::raw_json("null"), false, {}, "malformed_request", "要求不是有效 JSON");
    }

    if (request->method == "mcp_tools")
    {
        return response(request->id, true, writeJson(mcpCatalogDto()));
    }
    if (request->method == "get_diagnostics")
    {
        return response(request->id, true, writeJson(NativeDiagnosticsDto{
            "native",
            session_ != nullptr,
        }));
    }

    if (request->method == "new")
    {
        const auto params = readJson<NewParams>(request->params.str);
        const auto difficulty = params ? parseDifficulty(params->difficulty) : std::nullopt;
        const auto seed = params ? parseSeed(params->seed) : std::nullopt;
        const auto detail = params ? parseObservationDetail(params->detail) : std::nullopt;
        if (!params || !difficulty || !seed || !detail)
        {
            return response(request->id, false, {}, "invalid_params", "難度或種子格式無效");
        }
        auto content = loadContent(*difficulty);
        if (!content)
        {
            return response(request->id, false, {}, "content_load_failed", "無法載入遊戲內容");
        }
        ChessSessionOptions options;
        if (params->position_swap_enabled)
        {
            options.positionSwapEnabled = *params->position_swap_enabled;
        }
        session_ = std::make_unique<ChessGameSession>(std::move(content), *seed, options);
        return response(request->id, true, writeJson(sessionObservationDto(*session_, saves_, *detail)));
    }

    if (request->method == "verify_replay")
    {
        const auto params = readJson<VerifyParams>(request->params.str);
        ChessReplayJsonError parseError;
        const auto replay = params ? parseChessReplayJsonl(params->replay_jsonl, parseError) : std::nullopt;
        if (!replay)
        {
            return response(request->id, false, {}, "malformed_replay", parseError.message);
        }
        const auto difficulty = parseDifficulty(replay->header.difficulty);
        if (!difficulty)
        {
            return response(request->id, true, writeJson(ReplayVerificationDto{
                false,
                static_cast<int>(ChessReplayMismatch::Header),
                0,
                "重播難度識別碼無效",
            }));
        }
        auto verificationContent = fixedContent_ ? fixedContent_ : loadContent(*difficulty);
        if (!verificationContent)
        {
            return response(request->id, false, {}, "content_load_failed", "無法載入驗證規則內容");
        }
        const auto verification = ChessReplayVerifier::verify(std::move(verificationContent), *replay);
        return response(request->id, true, writeJson(ReplayVerificationDto{
            verification.valid,
            static_cast<int>(verification.mismatch),
            verification.sequence,
            verification.message,
        }));
    }

    if (!session_)
    {
        return response(request->id, false, {}, "no_session", "請先建立遊戲工作階段");
    }
    if (request->method == "observe")
    {
        const auto params = readJson<ObserveParams>(request->params.str);
        const auto detail = params ? parseObservationDetail(params->detail) : std::nullopt;
        if (!params || !detail)
        {
            return response(request->id, false, {}, "invalid_params", "detail 必須是 compact 或 full");
        }
        return response(
            request->id,
            true,
            writeJson(sessionObservationDto(*session_, saves_, *detail)));
    }
    if (request->method == "legal_actions")
    {
        std::vector<LegalActionDto> legal;
        for (const auto& descriptor : session_->legalActions())
        {
            legal.push_back(legalActionDto(*session_, descriptor));
        }
        return response(request->id, true, writeJson(legal));
    }
    if (request->method == "inspect_shop_slot")
    {
        const auto params = readJson<ShopSlotParams>(request->params.str);
        const auto inspection = params
            ? inspectShopSlotDto(*session_, params->slot)
            : std::nullopt;
        if (!inspection)
        {
            return response(request->id, false, {}, "invalid_shop_slot", "商店欄位不存在");
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "inspect_shop")
    {
        return response(request->id, true, writeJson(shopInspectionDto(*session_)));
    }
    if (request->method == "get_shop_odds")
    {
        const auto params = readJson<ShopOddsParams>(request->params.str);
        const auto inspection = params
            ? inspectShopOddsDto(*session_, params->level)
            : std::nullopt;
        if (!inspection)
        {
            return response(request->id, false, {}, "invalid_level", "等級超出商店機率表範圍");
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "inspect_chess_instance")
    {
        const auto params = readJson<ChessInstanceParams>(request->params.str);
        const auto inspection = params
            ? inspectChessInstanceDto(*session_, params->chess_instance_id)
            : std::nullopt;
        if (!inspection)
        {
            return response(request->id, false, {}, "unknown_chess_instance", "棋子實例不存在");
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "inspect_bans")
    {
        return response(request->id, true, writeJson(banInspectionDto(*session_)));
    }
    if (request->method == "inspect_role")
    {
        const auto params = readJson<RoleParams>(request->params.str);
        const auto inspection = params
            ? inspectRoleDto(*session_, params->role_id)
            : std::nullopt;
        if (!inspection)
        {
            return response(request->id, false, {}, "invalid_role", "角色不存在");
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "inspect_combo")
    {
        const auto params = readJson<ComboParams>(request->params.str);
        const auto inspection = params
            ? inspectComboDto(*session_, params->combo_name)
            : std::nullopt;
        if (!inspection)
        {
            return response(request->id, false, {}, "invalid_combo", "羈絆不存在");
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "inspect_equipment")
    {
        const auto params = readJson<EquipmentParams>(request->params.str);
        const auto inspection = params
            ? inspectEquipmentDto(*session_, params->item_id)
            : std::nullopt;
        if (!inspection)
        {
            return response(request->id, false, {}, "invalid_equipment", "裝備不存在");
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "inspect_challenge")
    {
        const auto params = readJson<ChallengeParams>(request->params.str);
        const auto inspection = params
            ? inspectChallengeDto(*session_, params->challenge_name)
            : std::nullopt;
        if (!inspection)
        {
            return response(request->id, false, {}, "invalid_challenge", "遠征挑戰不存在");
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "inspect_prepared_battle")
    {
        const auto params = readJson<InspectPreparedBattleParams>(request->params.str);
        const auto detail = params ? parsePreparedBattleDetail(params->detail) : std::nullopt;
        if (!params || !detail)
        {
            return response(
                request->id,
                false,
                {},
                "invalid_params",
                "detail 必須是 summary、compact 或 full");
        }
        const auto inspection = inspectPreparedBattleDto(*session_, *detail);
        if (!inspection)
        {
            return response(request->id, false, {}, "no_prepared_battle", "目前沒有已準備的戰鬥");
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "inspect_last_battle")
    {
        const auto inspection =
            inspectLastBattleDto(*session_, ObservationDetail::Full);
        if (!inspection)
        {
            return response(request->id, false, {}, "no_last_battle", "目前沒有已完成的戰鬥");
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "act")
    {
        const auto params = readJson<ActParams>(request->params.str);
        std::string actionError;
        const auto action = params
            ? parseChessActionJson(params->action.str, actionError)
            : std::nullopt;
        const auto requestedDetail = params
            ? parseActionResponseDetail(params->detail)
            : std::nullopt;
        if (params && !requestedDetail)
        {
            return response(
                request->id,
                false,
                {},
                "invalid_params",
                "detail 必須是 summary、compact 或 full");
        }
        if (!action)
        {
            return response(
                request->id,
                false,
                {},
                "invalid_action",
                params ? actionError : "缺少 action JSON 物件");
        }
        const auto before = session_->state();
        const auto result = session_->submitAndDrain(*action);
        if (*requestedDetail == ActionResponseDetail::Summary)
        {
            return response(request->id, true, writeJson(summaryActionResultDto(
                *session_,
                before,
                result,
                action->type)));
        }
        if (!result.accepted && *requestedDetail == ActionResponseDetail::Compact)
        {
            return response(
                request->id,
                true,
                writeJson(compactRejectedActionResultDto(*session_, result)));
        }
        return response(
            request->id,
            true,
            writeJson(actionResultDto(
                *session_,
                result,
                action->type,
                *requestedDetail)));
    }
    if (request->method == "list_saves")
    {
        std::vector<SaveSlotDto> slots;
        for (const auto& slot : saves_.list(*session_))
        {
            slots.push_back(saveSlotDto(slot));
        }
        return response(request->id, true, writeJson(slots));
    }
    if (request->method == "save_game")
    {
        const auto params = readJson<SaveParams>(request->params.str);
        if (!params || params->slot.empty())
        {
            return response(request->id, false, {}, "invalid_params", "缺少存檔欄位");
        }
        const auto error = saves_.save(params->slot, *session_, params->label);
        if (error != ChessCheckpointError::None)
        {
            return response(request->id, false, {}, checkpointErrorId(error), "無法建立存檔");
        }
        const auto* checkpoint = saves_.inspect(params->slot);
        return response(request->id, true, writeJson(SaveResultDto{params->slot, checkpoint->saveRevision}));
    }
    if (request->method == "inspect_save" || request->method == "inspect_save_summary")
    {
        const auto params = readJson<SlotParams>(request->params.str);
        const auto* checkpoint = params ? saves_.inspect(params->slot) : nullptr;
        if (!checkpoint)
        {
            return response(request->id, false, {}, "save_not_found", "存檔不存在");
        }
        const auto summaries = saves_.list(*session_);
        const auto summary = std::ranges::find(summaries, params->slot, &ChessSaveSlotSummary::slotId);
        if (request->method == "inspect_save_summary")
        {
            return response(request->id, true, writeJson(saveSlotDto(*summary)));
        }
        return response(request->id, true, writeJson(InspectSaveDto{
            saveSlotDto(*summary),
            checkpoint->toData(),
        }));
    }
    if (request->method == "load_game")
    {
        const auto params = readJson<SlotParams>(request->params.str);
        if (!params)
        {
            return response(request->id, false, {}, "invalid_params", "缺少存檔欄位");
        }
        ChessTimelineReplacement replacement;
        const auto error = saves_.load(params->slot, *session_, replacement);
        if (error != ChessCheckpointError::None)
        {
            return response(request->id, false, {}, checkpointErrorId(error), "無法載入存檔");
        }
        return response(request->id, true, writeJson(TimelineReplacementDto{
            replacement.loadedSlot,
            replacement.previousSequence,
            replacement.restoredSequence,
            replacement.discardedActiveActions,
            chessSha256Hex(replacement.currentStateHash),
        }));
    }
    if (request->method == "export_save")
    {
        const auto params = readJson<SlotParams>(request->params.str);
        const auto* checkpoint = params ? saves_.inspect(params->slot) : nullptr;
        if (!checkpoint)
        {
            return response(request->id, false, {}, "save_not_found", "存檔不存在");
        }
        return response(request->id, true, writeJson(ExportSaveDto{checkpoint->toData()}));
    }
    if (request->method == "import_save")
    {
        const auto params = readJson<ImportSaveParams>(request->params.str);
        if (!params)
        {
            return response(request->id, false, {}, "invalid_params", "匯入資料無效");
        }
        const auto error = saves_.importSave(
            params->slot,
            params->checkpoint,
            session_->content().gameVersion());
        if (error != ChessCheckpointError::None)
        {
            return response(request->id, false, {}, checkpointErrorId(error), "無法匯入存檔");
        }
        const auto* checkpoint = saves_.inspect(params->slot);
        return response(request->id, true, writeJson(SaveResultDto{params->slot, checkpoint->saveRevision}));
    }
    if (request->method == "export_save_file")
    {
        const auto params = readJson<SaveFileParams>(request->params.str);
        const auto* checkpoint = params ? saves_.inspect(params->slot) : nullptr;
        if (!params || params->path.empty())
        {
            return response(request->id, false, {}, "invalid_params", "缺少存檔欄位或輸出路徑");
        }
        if (!checkpoint)
        {
            return response(request->id, false, {}, "save_not_found", "存檔不存在");
        }
        const auto path = std::filesystem::u8path(params->path);
        if (!writeFile(path, checkpoint->serializeJson()))
        {
            return response(request->id, false, {}, "save_file_write_failed", "無法寫出存檔檔案");
        }
        return response(request->id, true, writeJson(SaveFileResultDto{
            params->slot,
            pathToUtf8(std::filesystem::absolute(path)),
            chessSha256Hex(checkpoint->snapshotHash),
            checkpoint->saveRevision,
        }));
    }
    if (request->method == "import_save_file")
    {
        const auto params = readJson<SaveFileParams>(request->params.str);
        if (!params || params->path.empty())
        {
            return response(request->id, false, {}, "invalid_params", "缺少存檔欄位或輸入路徑");
        }
        const auto path = std::filesystem::u8path(params->path);
        const auto payload = readFile(path);
        if (!payload)
        {
            return response(request->id, false, {}, "save_file_read_failed", "無法讀取存檔檔案");
        }
        ChessCheckpointError parseError;
        const auto parsed = parseChessSavePayload(*payload, parseError);
        if (!parsed)
        {
            return response(request->id, false, {}, checkpointErrorId(parseError), "無法匯入存檔檔案");
        }
        const auto error = saves_.importSave(
            params->slot,
            parsed->checkpoint.toData(),
            session_->content().gameVersion());
        if (error != ChessCheckpointError::None)
        {
            return response(request->id, false, {}, checkpointErrorId(error), "無法匯入存檔檔案");
        }
        const auto* checkpoint = saves_.inspect(params->slot);
        return response(request->id, true, writeJson(SaveFileResultDto{
            params->slot,
            pathToUtf8(std::filesystem::absolute(path)),
            chessSha256Hex(checkpoint->snapshotHash),
            checkpoint->saveRevision,
        }));
    }
    if (request->method == "export_replay")
    {
        const auto replay = session_->exportReplay();
        if (!replay)
        {
            return response(request->id, false, {}, "unstable_boundary", "自動流程尚未完成，不能匯出重播");
        }
        return response(request->id, true, writeJson(ReplayExportDto{serializeChessReplayJsonl(*replay)}));
    }
    return response(request->id, false, {}, "unknown_method", "未知的通訊協定方法");
}

std::string ChessJsonProtocol::handleMcpLine(std::string_view requestJson)
{
    const auto request = readJson<McpRequestDto>(requestJson);
    if (!request)
    {
        return mcpError(glz::raw_json("null"), -32700, "Parse error");
    }
    if (request->jsonrpc != "2.0" || request->method.empty())
    {
        return request->id
            ? mcpError(*request->id, -32600, "Invalid Request")
            : std::string{};
    }
    if (request->method.starts_with("notifications/"))
    {
        return {};
    }
    if (!request->id)
    {
        return {};
    }
    if (request->method == "initialize")
    {
        const auto params = readJson<McpInitializeParams>(request->params.str);
        if (!params)
        {
            return mcpError(*request->id, -32602, "Invalid initialize parameters");
        }
        return mcpResult(*request->id, writeJson(McpInitializeResultDto{
            negotiatedMcpVersion(params->protocolVersion),
            glz::raw_json(R"({"tools":{"listChanged":false}})"),
            {"KYS 自走棋", "dev"},
            "以工具操作可驗證的 KYS 自走棋工作階段。",
        }));
    }
    if (request->method == "ping")
    {
        return mcpResult(*request->id, "{}");
    }
    if (request->method == "tools/list")
    {
        return mcpResult(*request->id, writeJson(mcpToolsResultDto()));
    }
    if (request->method == "tools/call")
    {
        const auto params = readJson<McpCallToolParams>(request->params.str);
        const auto* tool = params ? mcpTool(params->name) : nullptr;
        if (!params || !tool)
        {
            return mcpError(*request->id, -32602, "Unknown tool or invalid arguments");
        }
        RequestDto nativeRequest;
        nativeRequest.id = *request->id;
        nativeRequest.method = std::string(tool->nativeMethod);
        nativeRequest.params = params->arguments;
        const auto nativeRequestJson = writeJson(nativeRequest);
        const auto nativeResponse = handleLine(nativeRequestJson);
        return mcpResult(*request->id, writeJson(McpCallToolResultDto{
            {{"text", nativeResponse}},
            glz::raw_json(nativeResponse),
            false,
        }));
    }
    return mcpError(*request->id, -32601, "Method not found");
}

}
