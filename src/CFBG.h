/*
 * Copyright (С) since 2019 Andrei Guluaev (Winfidonarleyan/Kargatum) https://github.com/Winfidonarleyan
 * Copyright (С) since 2019+ AzerothCore <www.azerothcore.org>
 * Licence MIT https://opensource.org/MIT
 */

#ifndef _CFBG_H_
#define _CFBG_H_

#include "SharedDefines.h"
#include "Config.h"
#include "ConfigValueCache.h"
#include "DBCEnums.h"
#include "ObjectGuid.h"
#include <array>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class CFBGOutConfig
{
    _IsEnableSystem = 0,
    _IsEnableWGSystem,
    _IsEnableWGTeamLock,
    _IsEnableWGNativePriority,
    _IsEnableWGReapplyOnResurrect,
    _IsEnableWGSkipClasses,
    _IsEnableBalancedTeams,
    _IsEnableBalanceClassLowLevel,
    _balanceClassMinLevel,
    _balanceClassMaxLevel,
    _balanceClassLevelDiff,
    _IsEnableEvenTeams,
    _EvenTeamsMaxPlayersThreshold,
    _IsEnableAvgIlvl,
    _IsEnableBalanceTeamsOnEntry,
    _MaxPlayersCountInGroup,
    _IsEnableResetCooldowns,
    _randomizeRaces,
    NUM_CONFIGS
};

class CFBGOutConfigData : public ConfigValueCache<CFBGOutConfig>
{
public:
    CFBGOutConfigData() : ConfigValueCache(CFBGOutConfig::NUM_CONFIGS) { }

    void BuildConfigCache() override;
};

class Player;
class Battlefield;
class Battleground;
class BattlegroundQueue;
class Group;

struct GroupQueueInfo;
struct PvPDifficultyEntry;

enum FakeMorphs
{
    FAKE_M_FEL_ORC        = 21267,
    FAKE_F_ORC            = 20316,
    FAKE_M_DWARF          = 20317,
    FAKE_M_NIGHT_ELF      = 20318,
    FAKE_F_DRAENEI        = 20323,
    FAKE_M_BROKEN_DRAENEI = 21105,
    FAKE_M_TROLL          = 20321,
    FAKE_M_HUMAN          = 19723,
    FAKE_F_HUMAN          = 19724,
    FAKE_M_BLOOD_ELF      = 20578,
    FAKE_F_BLOOD_ELF      = 20579,
    FAKE_F_GNOME          = 20320,
    FAKE_M_GNOME          = 20580,
    FAKE_F_TAUREN         = 20584,
    FAKE_M_TAUREN         = 20585
};

constexpr auto FACTION_FROSTWOLF_CLAN = 729;
constexpr auto FACTION_STORMPIKE_GUARD = 730;

// Cfbg settings
constexpr auto SETTING_CFBG_RACE = 0;

struct FakePlayer
{
    // Fake
    uint8   FakeRace;
    uint32  FakeMorph;
    TeamId  FakeTeamID;

    // Real
    uint8   RealRace;
    uint32  RealMorph;
    uint32  RealNativeMorph;
    TeamId  RealTeamID;
};

struct RaceData
{
    uint8 charClass;
    std::vector<uint8> availableRacesA;
    std::vector<uint8> availableRacesH;
};

struct CFBGRaceInfo
{
    uint8 RaceId;
    std::string RaceName;
    uint8 TeamId;
};

struct CrossFactionGroupInfo
{
    explicit CrossFactionGroupInfo(GroupQueueInfo* groupInfo);

    uint32 AveragePlayersLevel{ 0 };
    uint32 AveragePlayersItemLevel{ 0 };
    bool IsHunterJoining{ false };
    uint32 SumAverageItemLevel{ 0 };
    uint32 SumPlayerLevel{ 0 };

    CrossFactionGroupInfo() = delete;
    CrossFactionGroupInfo(CrossFactionGroupInfo&&) = delete;
};

// Precomputed numeric aggregates fed to CFBG::ResolveBalancedTeam, the single
// shared team-decision cascade. Each caller (formation / reinforcement) builds
// one from its own data source (queue tallies or live BG counts).
struct TeamBalanceContext
{
    int32  countA{ 0 };               // head counts, candidate EXCLUDED
    int32  countH{ 0 };
    uint32 levelSumA{ 0 };            // candidate level ALREADY folded into its side
    uint32 levelSumH{ 0 };
    uint32 avgIlvlA{ 0 };            // per-team ilvl metric (bg avg OR queue sum)
    uint32 avgIlvlH{ 0 };
    uint32 evenCountA{ 0 };          // denominators for the EvenTeams avg-level step
    uint32 evenCountH{ 0 };
    std::optional<TeamId> hunterOverride{ std::nullopt }; // set only when the bg-gated hunter step fires
    TeamId fallback{ TEAM_NEUTRAL }; // provisional / candidate team
};

class CFBG
{
public:
    using RandomSkinInfo = std::pair<uint8/*race*/, uint32/*morph*/>;
    using GroupsList = std::vector<GroupQueueInfo*>;

    static CFBG* instance();

    void LoadConfig(bool reload);

    inline bool IsEnableSystem() const { return _OutConfigData.GetConfigValue<bool>(CrossFactionBGOutConfig::_IsEnableSystem); }
    inline bool IsEnableWGSystem() const { return _OutConfigData.GetConfigValue<bool>(CrossFactionBGOutConfig::_IsEnableWGSystem); }
    inline bool IsEnableWGTeamLock() const { return _OutConfigData.GetConfigValue<bool>(CrossFactionBGOutConfig::_IsEnableWGTeamLock); }
    inline bool IsEnableWGNativePriority() const { return _OutConfigData.GetConfigValue<bool>(CrossFactionBGOutConfig::_IsEnableWGNativePriority); }
    inline bool IsEnableWGReapplyOnResurrect() const { return _OutConfigData.GetConfigValue<bool>(CrossFactionBGOutConfig::_IsEnableWGReapplyOnResurrect); }
    inline std::string IsEnableWGSkipClasses() const { return _OutConfigData.GetConfigValue<std::string>(CrossFactionBGOutConfig::_IsEnableWGSkipClasses); }
    inline bool IsEnableBalancedTeams() const { return _OutConfigData.GetConfigValue<bool>(CrossFactionBGOutConfig::_IsEnableBalancedTeams); }
    inline bool IsEnableBalanceClassLowLevel() const { return _OutConfigData.GetConfigValue<bool>(CrossFactionBGOutConfig::_IsEnableBalanceClassLowLevel); }
    inline uint32 GetBalanceClassMinLevel() const { return _OutConfigData.GetConfigValue<uint32>(CrossFactionBGOutConfig::_balanceClassMinLevel); }
    inline uint32 GetBalanceClassMaxLevel() const { return _OutConfigData.GetConfigValue<uint32>(CrossFactionBGOutConfig::_balanceClassMaxLevel); }
    inline uint32 GetBalanceClassLevelDiff() const { return _OutConfigData.GetConfigValue<uint32>(CrossFactionBGOutConfig::_balanceClassLevelDiff); }
    inline bool IsEnableEvenTeams() const { return _OutConfigData.GetConfigValue<bool>(CrossFactionBGOutConfig::_IsEnableEvenTeams); }
    inline uint32 EvenTeamsMaxPlayersThreshold() const { return _OutConfigData.GetConfigValue<uint32>(CrossFactionBGOutConfig::_EvenTeamsMaxPlayersThreshold); }
    inline bool IsEnableAvgIlvl() const { return _OutConfigData.GetConfigValue<bool>(CrossFactionBGOutConfig::_IsEnableAvgIlvl); }
    inline bool IsEnableBalanceTeamsOnEntry() const { return _OutConfigData.GetConfigValue<bool>(CrossFactionBGOutConfig::_IsEnableBalanceTeamsOnEntry); }
    inline uint32 GetMaxPlayersCountInGroup() const { return _OutConfigData.GetConfigValue<uint32>(CrossFactionBGOutConfig::_MaxPlayersCountInGroup); }
    inline bool IsEnableResetCooldowns() const { return _OutConfigData.GetConfigValue<bool>(CrossFactionBGOutConfig::_IsEnableResetCooldowns); }
    inline bool RandomizeRaces() const { return _OutConfigData.GetConfigValue<bool>(CrossFactionBGOutConfig::_randomizeRaces); }

    uint32 GetBGTeamAverageItemLevel(Battleground* bg, TeamId team);
    uint32 GetBGTeamSumPlayerLevel(Battleground* bg, TeamId team);

    TeamId ResolveBalancedTeam(TeamBalanceContext const& ctx);

    bool SendRealNameQuery(Player* player);
    bool IsPlayerFake(Player* player);
    FakePlayer const* GetFakePlayer(Player* player) const;

    // Per-war WG team lock, GUID-keyed so it survives leaving the war/zone and
    // relog. Cleared when the war ends.
    std::optional<TeamId> GetWGWarAssignment(ObjectGuid guid) const;
    void SetWGWarAssignment(ObjectGuid guid, TeamId team);
    void ClearWGWarAssignments();

    // Native-priority WG balancing: keep the minority native, flip only the
    // majority's surplus (latest accepters). Census captured on the first call
    // of a war, reset in ClearWGWarAssignments.
    TeamId ResolveWGWarTeam(Player* player, uint32 nativeAllianceInvited, uint32 nativeHordeInvited);

    bool ShouldForgetInListPlayers(Player* player);
    bool IsPlayingNative(Player* player);

    void ValidatePlayerForBG(Battleground* bg, Player* player);
    // Forces race/faction/m_team/fake-store into agreement with the player's
    // assigned BG team (GetBgTeamId()); idempotent and self-correcting.
    void EnforceBGTeamConsistency(Player* player);
    void SetFakeRaceAndMorph(Player* player);
    void SetFakeRaceAndMorphForBF(Player* player, TeamId assignedTeam);
    void SetFactionForRace(Player* player, uint8 Race, TeamId teamId);
    void ClearFakePlayer(Player* player);
    void ReapplyFakePlayer(Player* player);
    void DoForgetPlayersInList(Player* player);
    void FitPlayerInTeam(Player* player, bool action, Battleground* bg);
    void DoForgetPlayersInBG(Player* player, Battleground* bg);
    void SetForgetBGPlayers(Player* player, bool value);
    bool ShouldForgetBGPlayers(Player* player);
    void SetForgetInListPlayers(Player* player, bool value);
    void UpdateForget(Player* player);
    void SendMessageQueue(BattlegroundQueue* bgQueue, Battleground* bg, PvPDifficultyEntry const* bracketEntry, Player* leader);

    bool FillPlayersToCFBG(BattlegroundQueue* bgqueue, Battleground* bg, BattlegroundBracketId bracket_id);
    bool CheckCrossFactionMatch(BattlegroundQueue* bgqueue, BattlegroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers);

    void LoadWGSkipClasses(std::string skipClasses);
    bool IsWGSkipClass(uint8 playerClass) const { return _wgSkipClasses.count(playerClass) > 0; }

    bool IsRaceValidForFaction(uint8 teamId, uint8 race);
    TeamId getTeamWithLowerClass(Battleground* bg, Classes c);
    uint8 getBalanceClassMinLevel(const Battleground* bg) const;

    inline auto GetRaceData() { return &_raceData; }
    inline auto GetRaceInfo() { return &_raceInfo; }

private:
    bool isClassJoining(uint8 _class, Player* player, uint32 minLevel);

    // Live, bg-gated hunter-class override for the reinforcement path. Returns a
    // team only when EvenTeams + class-low-level balancing apply to the candidate.
    std::optional<TeamId> ResolveHunterOverride(Battleground* bg, CrossFactionGroupInfo const& cfGroupInfo);

    // Arrival-time head-count correction for solo entrants.
    void BalanceTeamsOnEntry(Battleground* bg, Player* player);

    RandomSkinInfo GetRandomRaceMorph(TeamId team, uint8 playerClass, uint8 gender);

    uint32 GetMorphFromRace(uint8 race, uint8 gender);

    // Projected head counts per team: in-BG players plus invited-not-yet-entered
    // ones. bg == nullptr (formation) yields zeros.
    std::array<uint32, 2> GetProjectedBaseCounts(Battleground* bg, BattlegroundQueue* queue, BattlegroundBracketId bracketId) const;

    // Unified stage->repair->invite selection (formation + reinforcement): fills
    // m_SelectionPools keeping projected sizes within allowedDiff, premades atomic.
    void SelectBalancedGroups(BattlegroundQueue* queue, BattlegroundBracketId bracketId, Battleground* bg, uint32 maxPerTeam, uint32 allowedDiff);

    std::unordered_map<Player*, FakePlayer> _fakePlayerStore;
    std::unordered_map<ObjectGuid, TeamId> _wgWarAssignmentStore;

    // Per-war native census for ResolveWGWarTeam, reset in ClearWGWarAssignments.
    bool   _wgCensusValid        = false;
    TeamId _wgMajorityTeam       = TEAM_ALLIANCE;
    uint32 _wgMajorityFairShare  = 0;
    uint32 _wgMajorityNativeKept = 0;
    std::unordered_map<Player*, ObjectGuid> _fakeNamePlayersStore;
    std::unordered_map<Player*, bool> _forgetBGPlayersStore;
    std::unordered_map<Player*, bool> _forgetInListPlayersStore;

    std::array<RaceData, 12> _raceData{};
    std::array<CFBGRaceInfo, 9> _raceInfo{};

    // For config
    CFBGOutConfigData _OutConfigData;
    std::unordered_set<uint8> _wgSkipClasses;

    CFBG();
    ~CFBG() = default;

    CFBG(CFBG const&) = delete;
    CFBG(CFBG&&) = delete;
    CFBG& operator=(CFBG const&) = delete;
    CFBG& operator=(CFBG&&) = delete;
};

#define sCFBG CFBG::instance()

#endif // _KARGATUM_CFBG_H_
