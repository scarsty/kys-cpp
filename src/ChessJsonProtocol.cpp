#include "ChessJsonProtocol.h"

#include "ChessJsonCodec.h"
#include "ChessAsciiBoard.h"
#include "ChessSessionCheckpoint.h"
#include "ChessReplayJson.h"
#include "ChessReplayVerifier.h"
#include "ChessSaveFile.h"
#include "Utf8Path.h"

#include <array>
#include <cassert>
#include <cctype>
#include <filesystem>
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
constexpr std::string_view kAutoSaveSlot = "autosave";
constexpr std::string_view kAutoSaveLabel = "自動存檔";
constexpr std::string_view kNewGameSchema = R"({
    "type":"object",
    "properties":{
        "talent":{"type":"string","enum":["divine_arms","late_bloomer","gambler","backbone"],"description":"棋手天賦；省略時採用難度配置的預設值"},
        "difficulty":{"type":"string","enum":["easy","normal","hard"],"default":"normal"},
        "seed":{"type":"string","pattern":"^0x[0-9a-fA-F]{16}$","default":"0x0000000000000001","description":"0x 前綴加上固定 16 位十六進位數字，例如 0x000000000000BEEF"},
        "position_swap_enabled":{"type":"boolean","default":true},
        "detail":{"type":"string","enum":["compact","full"],"default":"compact"}
    },
    "additionalProperties":false
})";
constexpr std::string_view kObserveSchema = R"({
    "type":"object",
    "properties":{"detail":{"type":"string","enum":["compact","full"],"default":"compact"}},
    "additionalProperties":false
})";
constexpr std::string_view kLegalActionsSchema = R"({
    "type":"object",
    "properties":{"action_type":{"type":["string","null"],"default":null}},
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
constexpr std::string_view kActionsSchema = R"({
    "type":"object",
    "properties":{
        "actions":{"type":"array","items":{"type":"object"},"minItems":1,"maxItems":64},
        "detail":{"type":"string","enum":["summary","compact","full"],"default":"summary"}
    },
    "required":["actions"],"additionalProperties":false
})";
constexpr std::string_view kPlanSchema = R"({
    "type":"object","properties":{"role_ids":{"type":"array","items":{"type":"integer"},"default":[]}},"additionalProperties":false
})";
constexpr std::string_view kCatalogSchema = R"({
    "type":"object",
    "properties":{
        "role_ids":{"type":"array","items":{"type":"integer"},"default":[]},
        "item_ids":{"type":"array","items":{"type":"integer"},"default":[]},
        "combo_names":{"type":"array","items":{"type":"string"},"default":[]},
        "detail":{"type":"string","enum":["compact","full"],"default":"compact"}
    },"additionalProperties":false
})";
constexpr std::string_view kShopOddsSchema = R"({
    "type":"object","properties":{"level":{"type":["integer","null"],"default":null}},"additionalProperties":false
})";
constexpr std::string_view kChessInstanceSchema = R"({
    "type":"object","properties":{"chess_instance_id":{"type":"integer"}},"required":["chess_instance_id"],"additionalProperties":false
})";
constexpr std::string_view kRoleSchema = R"({
    "type":"object",
    "properties":{
        "role_id":{"type":"integer"},
        "detail":{"type":"string","enum":["compact","full"],"default":"compact"}
    },
    "required":["role_id"],
    "additionalProperties":false
})";
constexpr std::string_view kComboSchema = R"({
    "type":"object",
    "properties":{
        "combo_name":{"type":"string"},
        "detail":{"type":"string","enum":["summary","full"],"default":"summary"}
    },
    "required":["combo_name"],
    "additionalProperties":false
})";
constexpr std::string_view kEquipmentSchema = R"({
    "type":"object","properties":{"item_id":{"type":"integer"}},"required":["item_id"],"additionalProperties":false
})";
constexpr std::string_view kChallengeSchema = R"({
    "type":"object","properties":{"challenge_name":{"type":"string"}},"required":["challenge_name"],"additionalProperties":false
})";
constexpr std::string_view kPreparedBattleSchema = R"({
    "type":"object",
    "properties":{
        "detail":{"type":"string","enum":["summary","compact","full"],"default":"summary"},
        "include_board":{"type":"boolean","default":false}
    },
    "additionalProperties":false
})";
constexpr std::string_view kLastBattleSchema = R"({
    "type":"object",
    "properties":{
        "detail":{"type":"string","enum":["summary","compact","full"],"default":"summary"},
        "sections":{"type":"array","items":{"type":"string","enum":["survivors","unit_stats","death_order","initial_board","board","important_effects","effect_activations","key_events"]}},
        "unit_ids":{"type":"array","items":{"type":"integer"},"default":[],"description":"戰鬥記錄中的 unit_id"},
        "effect_types":{"type":"array","items":{"type":"string"},"default":[]},
        "metrics":{"type":"array","items":{"type":"string"},"description":"unit_stats 欄位名稱，例如 damage_dealt、damage_taken、kills、invulnerability_triggers；保留單位識別及指定欄位的零值"}
    },
    "additionalProperties":false
})";
constexpr std::string_view kBattleEventsSchema = R"({
    "type":"object",
    "properties":{
        "cursor":{"type":"integer","minimum":0,"default":0},
        "limit":{"type":"integer","minimum":1,"maximum":200,"default":50},
        "unit_ids":{"type":"array","items":{"type":"integer"},"default":[]},
        "effect_types":{"type":"array","items":{"type":"string"},"default":[]},
        "frame_range":{
            "type":["object","null"],
            "properties":{
                "start":{"type":["integer","null"],"minimum":0,"default":null},
                "end":{"type":["integer","null"],"minimum":0,"default":null}
            },
            "additionalProperties":false,
            "default":null
        },
        "detail":{"type":"string","enum":["compact","full"],"default":"compact"}
    },
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
constexpr std::string_view kResumeSchema = R"({
    "type":"object",
    "properties":{
        "detail":{"type":"string","enum":["compact","full"],"default":"compact"}
    },
    "additionalProperties":false
})";
constexpr std::string_view kSaveFileSchema = R"({
    "type":"object",
    "properties":{"slot":{"type":"string"},"path":{"type":"string"}},
    "required":["slot","path"],
    "additionalProperties":false
})";

constexpr std::array kMcpTools{
    McpToolDefinition{"new_game", "new", "建立可驗證的新棋局並更新自動存檔。", kNewGameSchema},
    McpToolDefinition{"resume_saved_game", "resume_game", "不先建立新棋局，直接由自動存檔建立並接續工作階段。", kResumeSchema},
    McpToolDefinition{"observe_game", "observe", "取得棋局；compact 只含決策所需狀態，full 含完整定義。", kObserveSchema},
    McpToolDefinition{"get_diagnostics", "get_diagnostics", "取得原生執行狀態與診斷資訊。", kEmptySchema},
    McpToolDefinition{"list_legal_actions", "legal_actions", "預設只列合法操作類型；指定 action_type 才回傳該操作的結構、範例及候選值。", kLegalActionsSchema},
    McpToolDefinition{"take_action", "act", "提交一個遊戲操作；summary 只回變更，compact 回精簡現況，full 含完整資料。", kActionSchema},
    McpToolDefinition{"take_actions", "act_batch", "依序提交明確選定的操作，遇拒絕即停止；每筆接受操作仍獨立寫入重播及自動存檔。", kActionsSchema},
    McpToolDefinition{"preview_actions", "preview_actions", "唯讀預覽 set_deployment、set_formation、equip、swap_positions、choose_map；回傳羈絆及開戰屬性，不改變棋局或亂數。操作仍需符合目前階段的規則。", kActionsSchema},
    McpToolDefinition{"inspect_plan", "inspect_plan", "一次彙整指定或全部持有角色的升星進度、候選池、板凳容量及扣除主線經驗後仍需購買的經驗。", kPlanSchema},
    McpToolDefinition{"inspect_catalog", "inspect_catalog", "批次取得靜態角色、裝備、羈絆定義及天賦說明；以 content_fingerprint 與 talent 作為快取鍵。", kCatalogSchema},
    McpToolDefinition{"inspect_shop_slot", "inspect_shop_slot", "分析單一商店欄位的價格、持有份數、合成結果、羈絆變化與抽取機率。", kShopSlotSchema},
    McpToolDefinition{"inspect_shop", "inspect_shop", "一次分析目前全部商店欄位及當前等級的費用機率。", kEmptySchema},
    McpToolDefinition{"get_shop_odds", "get_shop_odds", "取得指定或目前等級的商店費用機率與實際可用角色池。", kShopOddsSchema},
    McpToolDefinition{"inspect_chess_instance", "inspect_chess_instance", "檢視棋子實例的實際屬性、裝備、升星進度、出戰狀態與羈絆貢獻。", kChessInstanceSchema},
    McpToolDefinition{"inspect_bans", "inspect_bans", "檢視目前禁棋、剩餘容量、依費用分組的可選角色及生效時機。", kEmptySchema},
    McpToolDefinition{"inspect_role", "inspect_role", "檢視角色；compact 合併相同星級威力並省略空效果，full 回傳完整說明。", kRoleSchema},
    McpToolDefinition{"inspect_combo", "inspect_combo", "依繁體中文名稱檢視羈絆；summary 回傳目前進度、已啟用與下一門檻，full 回傳完整成員及解釋。", kComboSchema},
    McpToolDefinition{"inspect_equipment", "inspect_equipment", "檢視裝備；省略空欄位，只在存在時回傳羈絆與角色專屬效果。", kEquipmentSchema},
    McpToolDefinition{"inspect_challenge", "inspect_challenge", "依繁體中文名稱檢視遠征的權威敵人星級、裝備與獎勵。", kChallengeSchema},
    McpToolDefinition{"inspect_prepared_battle", "inspect_prepared_battle", "檢視部署；compact 增加裝備與羈絆，full 含完整資料。full 或 include_board 附上 ASCII 地圖。", kPreparedBattleSchema},
    McpToolDefinition{"inspect_last_battle", "inspect_last_battle", "檢視勝負、核心統計及死亡順序；sections 選取區段，unit_ids、metrics 及 effect_types 限定診斷範圍。", kLastBattleSchema},
    McpToolDefinition{"inspect_last_battle_events", "inspect_last_battle_events", "分頁檢視上一場戰鬥事件，可依單位、效果類型及幀範圍過濾。", kBattleEventsSchema},
    McpToolDefinition{"list_saves", "list_saves", "列出目前程序內可用存檔摘要。", kEmptySchema},
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

}

ChessJsonProtocol::ChessJsonProtocol(
    ContentProvider contentProvider,
    ChessJsonProtocolOptions options)
    : contentProvider_(std::move(contentProvider)),
      autosaveFile_(std::move(options.autosaveFile))
{
    loadAutosave();
}

ChessJsonProtocol::ChessJsonProtocol(
    std::shared_ptr<const ChessGameContent> fixedContent,
    ChessJsonProtocolOptions options)
    : fixedContent_(std::move(fixedContent)),
      autosaveFile_(std::move(options.autosaveFile))
{
    loadAutosave();
}

std::shared_ptr<const ChessGameContent> ChessJsonProtocol::loadContent(Difficulty difficulty)
{
    if (fixedContent_)
    {
        return fixedContent_->difficulty() == difficulty ? fixedContent_ : nullptr;
    }
    return contentProvider_ ? contentProvider_(difficulty) : nullptr;
}

void ChessJsonProtocol::loadAutosave()
{
    if (autosaveFile_.empty())
    {
        return;
    }
    std::error_code filesystemError;
    const bool exists = std::filesystem::exists(autosaveFile_, filesystemError);
    if (filesystemError)
    {
        lastAutosaveError_ = "無法檢查自動存檔：" + pathToUtf8(autosaveFile_);
        return;
    }
    if (!exists)
    {
        return;
    }
    auto checkpoint = readChessCheckpointFile(autosaveFile_);
    if (!checkpoint)
    {
        lastAutosaveError_ = "無法讀取自動存檔：" + checkpoint.error();
        return;
    }
    saves_.restoreSave(
        std::string(kAutoSaveSlot),
        std::move(*checkpoint));
}

std::expected<void, std::string> ChessJsonProtocol::persistAutosave()
{
    if (autosaveFile_.empty())
    {
        return {};
    }
    auto written = writeChessCheckpointFile(
        autosaveFile_,
        *saves_.inspect(std::string(kAutoSaveSlot)));
    if (!written)
    {
        lastAutosaveError_ = written.error();
        return written;
    }
    lastAutosaveError_.clear();
    return {};
}

void ChessJsonProtocol::updateAutosave()
{
    if (autosaveFile_.empty())
    {
        return;
    }
    const auto error = saves_.save(
        std::string(kAutoSaveSlot),
        *session_,
        std::string(kAutoSaveLabel));
    if (error != ChessCheckpointError::None)
    {
        lastAutosaveError_ = chessCheckpointErrorDescription(error);
        return;
    }
    (void)persistAutosave();
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
            pathToUtf8(autosaveFile_),
            lastAutosaveError_.empty()
                ? std::nullopt
                : std::optional(lastAutosaveError_),
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
        if (params->talent)
        {
            options.talent = parseChessTalent(*params->talent);
            if (!options.talent || !content->balance().allowsTalent(*options.talent))
                return response(request->id, false, {}, "invalid_params", "未知天賦或此難度不可選用");
        }
        if (params->position_swap_enabled)
        {
            options.positionSwapEnabled = *params->position_swap_enabled;
        }
        session_ = std::make_unique<ChessGameSession>(std::move(content), *seed, options);
        updateAutosave();
        return response(request->id, true, writeJson(sessionObservationDto(*session_, saves_, *detail)));
    }

    if (request->method == "resume_game")
    {
        const auto params = readJson<ResumeParams>(request->params.str);
        const auto* checkpoint = saves_.inspect(std::string(kAutoSaveSlot));
        const auto detail = params ? parseObservationDetail(params->detail) : std::nullopt;
        if (!params || !detail)
        {
            return response(request->id, false, {}, "invalid_params", "detail 無效");
        }
        if (!checkpoint)
        {
            return response(request->id, false, {}, "save_not_found", "存檔不存在");
        }
        const auto difficulty = parseDifficulty(checkpoint->replay.header.difficulty);
        if (!difficulty)
        {
            return response(request->id, false, {}, "save_snapshot_unrepresentable", "存檔難度無效");
        }
        auto content = loadContent(*difficulty);
        if (!content)
        {
            return response(request->id, false, {}, "content_load_failed", "無法載入遊戲內容");
        }
        auto resumed = std::make_unique<ChessGameSession>(
            std::move(content),
            checkpoint->replay.header.rootSeed,
            checkpoint->replay.header.options);
        ChessTimelineReplacement replacement;
        const auto error = saves_.load(
            std::string(kAutoSaveSlot),
            *resumed,
            replacement);
        if (error != ChessCheckpointError::None)
        {
            return response(request->id, false, {}, checkpointErrorId(error), "無法接續存檔");
        }
        session_ = std::move(resumed);
        updateAutosave();
        return response(
            request->id,
            true,
            writeJson(sessionObservationDto(*session_, saves_, *detail)));
    }

    if (request->method == "list_saves")
    {
        return response(
            request->id,
            true,
            writeJson(saveSlotDtos(saves_.list())));
    }
    if (request->method == "inspect_save" || request->method == "inspect_save_summary")
    {
        const auto params = readJson<SlotParams>(request->params.str);
        const auto* checkpoint = params ? saves_.inspect(params->slot) : nullptr;
        if (!checkpoint)
        {
            return response(request->id, false, {}, "save_not_found", "存檔不存在");
        }
        const auto summaries = saves_.list();
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
        const auto written = writeChessCheckpointFile(path, *checkpoint);
        if (!written)
        {
            return response(
                request->id,
                false,
                {},
                "save_file_write_failed",
                "無法寫出存檔檔案：" + written.error());
        }
        return response(request->id, true, writeJson(SaveFileResultDto{
            params->slot,
            pathToUtf8(std::filesystem::absolute(path)),
            chessSha256Hex(checkpoint->snapshotHash),
            checkpoint->saveRevision,
        }));
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
        const auto params = readJson<LegalActionsParams>(request->params.str);
        if (!params)
        {
            return response(request->id, false, {}, "invalid_params", "合法操作查詢參數無效");
        }
        const auto legalActions = session_->legalActions();
        if (!params->action_type)
        {
            std::vector<std::string> legalTypes;
            for (const auto& descriptor : legalActions)
            {
                legalTypes.push_back(chessActionTypeId(descriptor.type));
            }
            return response(request->id, true, writeJson(legalTypes));
        }
        const auto actionType = chessActionTypeFromId(*params->action_type);
        if (!actionType)
        {
            return response(request->id, false, {}, "invalid_action_type", "未知的操作類型");
        }
        const auto descriptor = std::ranges::find(
            legalActions,
            *actionType,
            &ChessLegalActionDescriptor::type);
        if (descriptor == legalActions.end())
        {
            return response(request->id, false, {}, "action_not_legal", "目前不能執行指定操作");
        }
        return response(request->id, true, writeJson(legalActionDto(*session_, *descriptor)));
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
    if (request->method == "inspect_plan")
    {
        const auto params = readJson<PlanParams>(request->params.str);
        if (!params || std::ranges::any_of(params->role_ids, [&](int id) {
            const auto* role = session_->content().role(id);
            return !role || role->Cost < 1 || role->Cost > 5;
        }))
        {
            return response(request->id, false, {}, "invalid_role", "升星規劃只接受一至五費角色");
        }
        return response(request->id, true, writeJson(planDto(*session_, params->role_ids)));
    }
    if (request->method == "inspect_catalog")
    {
        const auto params = readJson<CatalogParams>(request->params.str);
        const auto detail = params ? parseCatalogDetail(params->detail) : std::nullopt;
        const auto catalog = detail ? inspectCatalogDto(*session_, *params, *detail) : std::nullopt;
        if (!catalog)
        {
            return response(request->id, false, {}, "invalid_catalog_query", "角色、裝備、羈絆或 detail 無效");
        }
        return response(request->id, true, writeJson(*catalog));
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
        const auto detail = params ? parseCatalogDetail(params->detail) : std::nullopt;
        const auto inspection = detail
            ? inspectRoleDto(*session_, params->role_id, *detail)
            : std::nullopt;
        if (!params || !detail)
        {
            return response(request->id, false, {}, "invalid_params", "detail 必須是 compact 或 full");
        }
        if (!inspection)
        {
            return response(request->id, false, {}, "invalid_role", "角色不存在");
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "inspect_combo")
    {
        const auto params = readJson<ComboParams>(request->params.str);
        const auto detail = params
            ? parseComboInspectionDetail(params->detail)
            : std::nullopt;
        const auto inspection = detail
            ? inspectComboDto(*session_, params->combo_name, *detail)
            : std::nullopt;
        if (!params || !detail)
        {
            return response(request->id, false, {}, "invalid_params", "detail 必須是 summary 或 full");
        }
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
        const auto& prepared = session_->state().preparedBattle;
        if (prepared && prepared->chosenMapId < 0 && !prepared->mapCandidates.empty())
        {
            return response(request->id, false, {}, "map_selection_required", "請先使用 choose_map 選擇戰場，再檢視部署");
        }
        auto inspection = inspectPreparedBattleDto(*session_, *detail);
        if (!inspection)
        {
            return response(request->id, false, {}, "no_prepared_battle", "目前沒有已準備的戰鬥");
        }
        if (params->include_board)
        {
            inspection->board = ChessAsciiBoard::render(*session_->state().preparedBattle, session_->content());
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "inspect_last_battle")
    {
        const auto params = readJson<InspectLastBattleParams>(request->params.str);
        const auto detail = params
            ? parseBattleReportDetail(params->detail)
            : std::nullopt;
        if (!params || !detail || !validBattleReportParams(*params))
        {
            return response(
                request->id,
                false,
                {},
                "invalid_params",
                "detail 必須是 summary、compact 或 full，sections 與 metrics 必須使用有效的區段及統計欄位名稱");
        }
        const auto inspection = inspectLastBattleDto(*session_, *detail, *params);
        if (!inspection)
        {
            return response(request->id, false, {}, "no_last_battle", "目前沒有已完成的戰鬥");
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "inspect_last_battle_events")
    {
        const auto params = readJson<BattleEventsParams>(request->params.str);
        const auto detail = params
            ? parseBattleEventDetail(params->detail)
            : std::nullopt;
        const bool invalidRange = params
            && params->frame_range
            && params->frame_range->start
            && params->frame_range->end
            && *params->frame_range->start > *params->frame_range->end;
        if (!params
            || !detail
            || params->cursor < 0
            || params->limit < 1
            || params->limit > 200
            || invalidRange)
        {
            return response(request->id, false, {}, "invalid_params", "戰鬥事件分頁或過濾參數無效");
        }
        const auto inspection = inspectLastBattleEventsDto(*session_, *params, *detail);
        if (!inspection)
        {
            return response(request->id, false, {}, "no_last_battle", "目前沒有已完成的戰鬥");
        }
        return response(request->id, true, writeJson(*inspection));
    }
    if (request->method == "act_batch" || request->method == "preview_actions")
    {
        const auto params = readJson<ActionsParams>(request->params.str);
        const auto detail = params ? parseActionResponseDetail(params->detail) : std::nullopt;
        if (!params || !detail || params->actions.empty() || params->actions.size() > 64)
        {
            return response(request->id, false, {}, "invalid_params", "批次需包含一至六十四筆操作及有效 detail");
        }
        const bool preview = request->method == "preview_actions";
        std::vector<ChessAction> actions;
        for (const auto& json : params->actions)
        {
            std::string error;
            const auto action = parseChessActionJson(json.str, error);
            if (!action)
            {
                return response(request->id, false, {}, "invalid_action",
                    "操作 " + std::to_string(actions.size()) + "：" + error);
            }
            if (preview && action->type != ChessActionType::SetDeployment
                && action->type != ChessActionType::SetFormation
                && action->type != ChessActionType::Equip
                && action->type != ChessActionType::SwapPositions
                && action->type != ChessActionType::ChooseMap)
            {
                return response(request->id, false, {}, "unsupported_preview_action", "預覽只接受部署、站位及裝備操作");
            }
            actions.push_back(*action);
        }
        std::unique_ptr<ChessGameSession> scratch;
        if (preview)
        {
            const auto checkpoint = ChessSessionCheckpoint::capture(*session_, 0);
            scratch = std::make_unique<ChessGameSession>(session_->sharedContent(),
                checkpoint.replay.header.rootSeed, checkpoint.replay.header.options);
            const auto restored = checkpoint.restore(*scratch);
            assert(restored == ChessCheckpointError::None);
        }
        auto& target = preview ? *scratch : *session_;
        const auto before = target.state();
        BatchActionResultDto batch{};
        batch.requested_count = static_cast<int>(actions.size());
        batch.preview = preview;
        ChessActionResult combined{};
        combined.accepted = true;
        combined.description = preview ? "預覽完成" : "批次完成";
        auto lastType = actions.front().type;
        bool battleOccurred{};
        for (int index = 0; index < static_cast<int>(actions.size()); ++index)
        {
            const auto& action = actions[index];
            const auto result = target.submitAndDrain(action);
            batch.actions.push_back({index, chessActionTypeId(action.type), result.accepted,
                result.accepted ? std::nullopt : std::optional(ruleErrorId(result.error)), result.description});
            if (!result.accepted)
            {
                batch.failed_index = index;
                combined.accepted = false;
                combined.error = result.error;
                combined.description = result.description;
                break;
            }
            ++batch.accepted_count;
            lastType = action.type;
            battleOccurred = battleOccurred || action.type == ChessActionType::StartBattle;
            combined.events.insert(combined.events.end(), result.events.begin(), result.events.end());
            if (!preview) updateAutosave();
        }
        batch.changes = summaryActionResultDto(target, before, combined,
            battleOccurred ? ChessActionType::StartBattle : lastType);
        if (*detail != ActionResponseDetail::Summary)
        {
            batch.next_observation = observationDto(target.observe(), target.content(),
                *detail == ActionResponseDetail::Full ? ObservationDetail::Full : ObservationDetail::Compact,
                {}, target.legalActions());
        }
        if (preview)
        {
            batch.live_state_hash = chessSha256Hex(session_->observe().stateHash);
            batch.projected_units = previewRosterStatsDto(target, *session_);
            batch.projected_stats_context = target.state().preparedBattle
                ? "使用目前已準備的戰場；尚未選圖時只列星級與勝場基礎值"
                : "使用無敵方的開戰預覽；對手及地形條件效果須於實際戰場再檢視";
        }
        return response(request->id, true, writeJson(batch));
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
        if (result.accepted)
        {
            updateAutosave();
        }
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
    if (request->method == "save_game")
    {
        const auto params = readJson<SaveParams>(request->params.str);
        if (!params || params->slot.empty())
        {
            return response(request->id, false, {}, "invalid_params", "缺少存檔欄位");
        }
        const auto error = saves_.save(
            params->slot,
            *session_,
            params->label);
        if (error != ChessCheckpointError::None)
        {
            return response(request->id, false, {}, checkpointErrorId(error), "無法建立存檔");
        }
        if (params->slot == kAutoSaveSlot)
        {
            const auto persisted = persistAutosave();
            if (!persisted)
            {
                return response(
                    request->id,
                    false,
                    {},
                    "save_file_write_failed",
                    "無法寫出自動存檔：" + persisted.error());
            }
        }
        const auto* checkpoint = saves_.inspect(params->slot);
        return response(request->id, true, writeJson(SaveResultDto{params->slot, checkpoint->saveRevision}));
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
        updateAutosave();
        return response(request->id, true, writeJson(TimelineReplacementDto{
            replacement.loadedSlot,
            replacement.previousSequence,
            replacement.restoredSequence,
            replacement.discardedActiveActions,
            chessSha256Hex(replacement.currentStateHash),
        }));
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
    if (request->method == "import_save_file")
    {
        const auto params = readJson<SaveFileParams>(request->params.str);
        if (!params || params->path.empty())
        {
            return response(request->id, false, {}, "invalid_params", "缺少存檔欄位或輸入路徑");
        }
        const auto path = std::filesystem::u8path(params->path);
        const auto importedCheckpoint = readChessCheckpointFile(path);
        if (!importedCheckpoint)
        {
            return response(
                request->id,
                false,
                {},
                "save_file_read_failed",
                "無法匯入存檔檔案：" + importedCheckpoint.error());
        }
        const auto error = saves_.importSave(
            params->slot,
            importedCheckpoint->toData(),
            session_->content().gameVersion());
        if (error != ChessCheckpointError::None)
        {
            return response(request->id, false, {}, checkpointErrorId(error), "無法匯入存檔檔案");
        }
        const auto* storedCheckpoint = saves_.inspect(params->slot);
        return response(request->id, true, writeJson(SaveFileResultDto{
            params->slot,
            pathToUtf8(std::filesystem::absolute(path)),
            chessSha256Hex(storedCheckpoint->snapshotHash),
            storedCheckpoint->saveRevision,
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
        const auto parsed = readJson<ResponseDto>(nativeResponse);
        assert(parsed);
        return mcpResult(*request->id, writeJson(McpCallToolResultDto{
            {},
            glz::raw_json(nativeResponse),
            !parsed->ok,
        }));
    }
    return mcpError(*request->id, -32601, "Method not found");
}

}
