#pragma once

#include "ChessBattleAnalysis.h"
#include "ChessGameQueries.h"
#include "ChessGameSession.h"
#include "ChessJsonDtos.h"
#include "ChessPreparedBattleAnalysis.h"
#include "ChessSaveStore.h"
#include "ChessSessionCheckpoint.h"

#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>

namespace KysChess::ProtocolDetail
{

enum class ObservationDetail { Compact, Full };
enum class ActionResponseDetail { Summary, Compact, Full };
enum class PreparedBattleDetail { ObservationCompact, Summary, Compact, Full };

template<typename T>
std::optional<T> readJson(std::string_view json)
{
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    T value;
    if (const auto result = glz::read<options>(value, json); result)
    {
        return std::nullopt;
    }
    return value;
}

template<typename T>
std::string writeJson(const T& value)
{
    auto result = glz::write_json(value);
    return result ? std::move(result.value()) : std::string{};
}

std::optional<ObservationDetail> parseObservationDetail(std::string_view value);
std::optional<ActionResponseDetail> parseActionResponseDetail(std::string_view value);
std::optional<PreparedBattleDetail> parsePreparedBattleDetail(std::string_view value);
std::optional<Difficulty> parseDifficulty(std::string_view value);
std::optional<std::uint64_t> parseSeed(std::string_view value);
std::string response(
    const glz::raw_json& id,
    bool ok,
    std::string result,
    std::string errorCode = {},
    std::string errorMessage = {});

std::string phaseText(ChessSessionPhase phase);
std::string ruleErrorId(ChessRuleErrorCode error);
std::string checkpointErrorId(ChessCheckpointError error);

RoleDto roleDto(const ChessGameContent& content, int roleId);
std::optional<RoleDto> inspectRoleDto(
    const ChessGameSession& session,
    int roleId);
EquipmentInfoDto equipmentInfoDto(
    const ChessGameContent& content,
    int itemId,
    bool full = true);
ComboDto comboDto(const ChessComboMetadata& metadata, bool full = true);
ComboDto comboDto(
    const ChessGameContent& content,
    const ComboDef& definition,
    int physicalCount,
    int effectiveCount,
    int activeThresholdIndex,
    bool full = true,
    const std::vector<ResolvedChessComboContribution>* contributions = nullptr);
std::optional<ComboDto> inspectComboDto(
    const ChessGameSession& session,
    std::string_view comboName);
ChallengeDto challengeDto(
    const ChessGameContent& content,
    const BalanceConfig::ChallengeDef& challenge);
std::optional<EquipmentInfoDto> inspectEquipmentDto(
    const ChessGameSession& session,
    int itemId);
std::optional<ChallengeDto> inspectChallengeDto(
    const ChessGameSession& session,
    std::string_view challengeName);
PreparedBattleDto preparedBattleDto(
    const ChessGameContent& content,
    const PreparedChessBattle& battle,
    PreparedBattleDetail detail,
    const std::set<int>& obtainedNeigongIds,
    int maximumFrames);
std::optional<PreparedBattleDto> inspectPreparedBattleDto(
    const ChessGameSession& session,
    PreparedBattleDetail detail);
ObservationDto observationDto(
    const ChessGameplayObservation& observation,
    const ChessGameContent& content,
    ObservationDetail detail = ObservationDetail::Full,
    std::string roleMetadataScope = {},
    std::span<const ChessLegalActionDescriptor> legalActions = {});
LegalActionDto legalActionDto(
    const ChessGameSession& session,
    const ChessLegalActionDescriptor& descriptor);
std::optional<ShopOddsDto> inspectShopOddsDto(
    const ChessGameSession& session,
    std::optional<int> level);
std::optional<ShopSlotInspectionDto> inspectShopSlotDto(
    const ChessGameSession& session,
    int slotIndex);
ShopInspectionDto shopInspectionDto(const ChessGameSession& session);
std::optional<ChessInstanceInspectionDto> inspectChessInstanceDto(
    const ChessGameSession& session,
    int chessInstanceId);
BanInspectionDto banInspectionDto(const ChessGameSession& session);
SemanticEventDto semanticEventDto(
    const ChessGameContent& content,
    const ChessSemanticEvent& event);
SummaryActionResultDto summaryActionResultDto(
    const ChessGameSession& session,
    const ChessSessionState& before,
    const ChessActionResult& actionResult,
    ChessActionType actionType);
CompactRejectedActionResultDto compactRejectedActionResultDto(
    const ChessGameSession& session,
    const ChessActionResult& actionResult);
ActionResultDto actionResultDto(
    const ChessGameSession& session,
    const ChessActionResult& actionResult,
    ChessActionType actionType,
    ActionResponseDetail detail);
BattleResultDto battleResultDto(
    const ChessGameContent& content,
    const PreparedChessBattle& prepared,
    const HeadlessBattleResult& battle,
    ObservationDetail detail);
std::optional<BattleResultDto> inspectLastBattleDto(
    const ChessGameSession& session,
    ObservationDetail detail);
SaveSlotDto saveSlotDto(const ChessSaveSlotSummary& slot);
SessionObservationDto sessionObservationDto(
    const ChessGameSession& session,
    const ChessSaveStore& saves,
    ObservationDetail detail = ObservationDetail::Full);

}  // namespace KysChess::ProtocolDetail
