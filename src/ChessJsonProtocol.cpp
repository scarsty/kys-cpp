#include "ChessJsonProtocol.h"

#include "ChessJsonCodec.h"
#include "ChessReplayJson.h"
#include "ChessReplayVerifier.h"

#include <ranges>
#include <utility>

namespace KysChess
{
using namespace ProtocolDetail;

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
    if (request->method == "inspect_save")
    {
        const auto params = readJson<SlotParams>(request->params.str);
        const auto* checkpoint = params ? saves_.inspect(params->slot) : nullptr;
        if (!checkpoint)
        {
            return response(request->id, false, {}, "save_not_found", "存檔不存在");
        }
        const auto summaries = saves_.list(*session_);
        const auto summary = std::ranges::find(summaries, params->slot, &ChessSaveSlotSummary::slotId);
        return response(request->id, true, writeJson(InspectSaveDto{
            saveSlotDto(*summary),
            checkpoint->serializeJson(),
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
        const auto payload = params ? saves_.exportSave(params->slot) : std::nullopt;
        if (!payload)
        {
            return response(request->id, false, {}, "save_not_found", "存檔不存在");
        }
        return response(request->id, true, writeJson(ExportSaveDto{*payload}));
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
            params->payload,
            session_->content().gameVersion());
        if (error != ChessCheckpointError::None)
        {
            return response(request->id, false, {}, checkpointErrorId(error), "無法匯入存檔");
        }
        const auto* checkpoint = saves_.inspect(params->slot);
        return response(request->id, true, writeJson(SaveResultDto{params->slot, checkpoint->saveRevision}));
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

}
