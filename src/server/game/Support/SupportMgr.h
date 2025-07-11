/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SupportMgr_h__
#define SupportMgr_h__

#include "TicketPackets.h"
#include <map>

class ChatHandler;
class Field;
class Player;

enum class ReportType : int32
{
    Chat                    = 0,
    InWorld                 = 1,
    ClubFinderPosting       = 2,
    ClubFinderApplicant     = 3,
    GroupFinderPosting      = 4,
    GroupFinderApplicant    = 5,
    ClubMember              = 6,
    GroupMember             = 7,
    Friend                  = 8,
    Pet                     = 9,
    BattlePet               = 10,
    Calendar                = 11,
    Mail                    = 12,
    PvP                     = 13,
};

enum class ReportMajorCategory : int32
{
    InappropriateCommunication  = 0,
    GameplaySabotage            = 1,
    Cheating                    = 2,
    InappropriateName           = 3,
};

enum class ReportMinorCategory : int32
{
    TextChat                            = 0x00000001,
    Boosting                            = 0x00000002,
    Spam                                = 0x00000004,
    Afk                                 = 0x00000008,
    IntentionallyFeeding                = 0x00000010,
    BlockingProgress                    = 0x00000020,
    Hacking                             = 0x00000040,
    Botting                             = 0x00000080,
    Advertisement                       = 0x00000100,
    BTag                                = 0x00000200,
    GroupName                           = 0x00000400,
    CharacterName                       = 0x00000800,
    GuildName                           = 0x00001000,
    Description                         = 0x00002000,
    Name                                = 0x00004000,
    ChinaHarmfulMinors                  = 0x00008000,
    Disruption                          = 0x00010000,
    TerroristAndViolentExtremistContent = 0x00020000,
    ChildSexualExploitationAndAbuse     = 0x00040000,
};

// from blizzard lua
enum GMTicketSystemStatus
{
    GMTICKET_QUEUE_STATUS_DISABLED  = 0,
    GMTICKET_QUEUE_STATUS_ENABLED   = 1
};

enum SupportSpamType
{
    SUPPORT_SPAM_TYPE_MAIL     = 0,
    SUPPORT_SPAM_TYPE_CHAT     = 1,
    SUPPORT_SPAM_TYPE_CALENDAR = 2
};

using ChatLog = WorldPackets::Ticket::SupportTicketChatLog;

class TC_GAME_API Ticket
{
public:
    Ticket();
    Ticket(Player* player);
    Ticket(Ticket const&) = delete;
    Ticket(Ticket&&) = delete;
    Ticket& operator=(Ticket const&) = delete;
    Ticket& operator=(Ticket&&) = delete;
    virtual ~Ticket();

    bool IsClosed() const { return !_closedBy.IsEmpty(); }
    bool IsFromPlayer(ObjectGuid guid) const { return guid == _playerGuid; }
    bool IsAssigned() const { return !_assignedTo.IsEmpty(); }
    bool IsAssignedTo(ObjectGuid guid) const { return guid == _assignedTo; }
    bool IsAssignedNotTo(ObjectGuid guid) const { return IsAssigned() && !IsAssignedTo(guid); }

    uint32 GetId() const { return _id; }
    ObjectGuid GetPlayerGuid() const { return _playerGuid; }
    Player* GetPlayer() const;
    std::string GetPlayerName() const;
    Player* GetAssignedPlayer() const;
    ObjectGuid GetAssignedToGUID() const { return _assignedTo; }
    std::string GetAssignedToName() const;
    std::string const& GetComment() const { return _comment; }

    virtual void SetAssignedTo(ObjectGuid guid, bool /*isAdmin*/ = false) { _assignedTo = guid; }
    virtual void SetUnassigned() { _assignedTo.Clear(); }
    void SetClosedBy(ObjectGuid value) { _closedBy = value; }
    void SetComment(std::string const& comment) { _comment = comment; }
    void SetPosition(uint32 mapId, Position const& pos)
    {
        _mapId = mapId;
        _pos = pos;
    }
    void SetFacing(float facing) { _pos.SetOrientation(facing); }

    virtual void LoadFromDB(Field* fields) = 0;
    virtual void SaveToDB() const = 0;
    virtual void DeleteFromDB() = 0;

    void TeleportTo(Player* player) const;

    virtual std::string FormatViewMessageString(ChatHandler& handler, bool detailed = false) const = 0;
    virtual std::string FormatViewMessageString(ChatHandler& handler, const char* szClosedName, const char* szAssignedToName, const char* szUnassignedName, const char* szDeletedName) const;

protected:
    uint32 _id;
    ObjectGuid _playerGuid;
    uint16 _mapId;
    Position _pos;
    uint64 _createTime;
    ObjectGuid _closedBy; // 0 = Open, -1 = Console, playerGuid = player abandoned ticket, other = GM who closed it.
    ObjectGuid _assignedTo;
    std::string _comment;
};

class TC_GAME_API BugTicket : public Ticket
{
public:
    BugTicket();
    BugTicket(Player* player);
    ~BugTicket();

    std::string const& GetNote() const { return _note; }

    void SetNote(std::string const& note) { _note = note; }

    void LoadFromDB(Field* fields) override;
    void SaveToDB() const override;
    void DeleteFromDB() override;

    using Ticket::FormatViewMessageString;
    std::string FormatViewMessageString(ChatHandler& handler, bool detailed = false) const override;

private:
    std::string _note;
};

class TC_GAME_API ComplaintTicket : public Ticket
{
public:
    ComplaintTicket();
    ComplaintTicket(Player* player);
    ~ComplaintTicket();

    ObjectGuid GetTargetCharacterGuid() const { return _targetCharacterGuid; }
    ReportType GetReportType() const { return _reportType; }
    ReportMajorCategory GetMajorCategory() const { return _majorCategory; }
    ReportMinorCategory GetMinorCategoryFlags() const { return _minorCategoryFlags; }
    std::string const& GetNote() const { return _note; }

    void SetTargetCharacterGuid(ObjectGuid targetCharacterGuid)
    {
        _targetCharacterGuid = targetCharacterGuid;
    }
    void SetReportType(ReportType reportType) { _reportType = reportType; }
    void SetMajorCategory(ReportMajorCategory majorCategory) { _majorCategory = majorCategory; }
    void SetMinorCategoryFlags(ReportMinorCategory minorCategoryFlags) { _minorCategoryFlags = minorCategoryFlags; }
    void SetChatLog(ChatLog const& log) { _chatLog = log; }
    void SetNote(std::string const& note) { _note = note; }

    void LoadFromDB(Field* fields) override;
    void LoadChatLineFromDB(Field* fields);
    void SaveToDB() const override;
    void DeleteFromDB() override;

    using Ticket::FormatViewMessageString;
    std::string FormatViewMessageString(ChatHandler& handler, bool detailed = false) const override;

private:
    ObjectGuid _targetCharacterGuid;
    ReportType _reportType;
    ReportMajorCategory _majorCategory;
    ReportMinorCategory _minorCategoryFlags;
    ChatLog _chatLog;
    std::string _note;
};

class TC_GAME_API SuggestionTicket : public Ticket
{
public:
    SuggestionTicket();
    SuggestionTicket(Player* player);
    ~SuggestionTicket();

    std::string const& GetNote() const { return _note; }
    void SetNote(std::string const& note) { _note = note; }

    void LoadFromDB(Field* fields) override;
    void SaveToDB() const override;
    void DeleteFromDB() override;

    using Ticket::FormatViewMessageString;
    std::string FormatViewMessageString(ChatHandler& handler, bool detailed = false) const override;

private:
    std::string _note;
};

typedef std::map<uint32, BugTicket*> BugTicketList;
typedef std::map<uint32, ComplaintTicket*> ComplaintTicketList;
typedef std::map<uint32, SuggestionTicket*> SuggestionTicketList;

class TC_GAME_API SupportMgr
{
private:
    SupportMgr();
    ~SupportMgr();

public:
    SupportMgr(SupportMgr const&) = delete;
    SupportMgr(SupportMgr&&) = delete;
    SupportMgr& operator=(SupportMgr const&) = delete;
    SupportMgr& operator=(SupportMgr&&) = delete;

    static SupportMgr* instance();

    template<typename T>
    T* GetTicket(uint32 ticketId);

    ComplaintTicketList GetComplaintsByPlayerGuid(ObjectGuid playerGuid) const;

    void Initialize();

    bool GetSupportSystemStatus() const { return _supportSystemStatus; }
    bool GetTicketSystemStatus() const { return _supportSystemStatus && _ticketSystemStatus; }
    bool GetBugSystemStatus() const { return _supportSystemStatus && _bugSystemStatus; }
    bool GetComplaintSystemStatus() const { return _supportSystemStatus && _complaintSystemStatus; }
    bool GetSuggestionSystemStatus() const { return _supportSystemStatus && _suggestionSystemStatus; }
    uint64 GetLastChange() const { return _lastChange; }
    template<typename T>
    uint32 GetOpenTicketCount() const;

    void SetSupportSystemStatus(bool status) { _supportSystemStatus = status; }
    void SetTicketSystemStatus(bool status) { _ticketSystemStatus = status; }
    void SetBugSystemStatus(bool status) { _bugSystemStatus = status; }
    void SetComplaintSystemStatus(bool status) { _complaintSystemStatus = status; }
    void SetSuggestionSystemStatus(bool status) { _suggestionSystemStatus = status; }

    void LoadBugTickets();
    void LoadComplaintTickets();
    void LoadSuggestionTickets();

    void AddTicket(BugTicket* ticket);
    void AddTicket(ComplaintTicket* ticket);
    void AddTicket(SuggestionTicket* ticket);

    template<typename T>
    void RemoveTicket(uint32 ticketId);

    template<typename T>
    void CloseTicket(uint32 ticketId, ObjectGuid closedBy);

    template<typename T>
    void ResetTickets();

    template<typename T>
    void ShowList(ChatHandler& handler) const;

    template<typename T>
    void ShowList(ChatHandler& handler, bool onlineOnly) const;

    template<typename T>
    void ShowClosedList(ChatHandler& handler) const;

    void UpdateLastChange();

    uint32 GenerateBugId() { return ++_lastBugId; }
    uint32 GenerateComplaintId() { return ++_lastComplaintId; }
    uint32 GenerateSuggestionId() { return ++_lastSuggestionId; }

private:
    bool _supportSystemStatus;
    bool _ticketSystemStatus;
    bool _bugSystemStatus;
    bool _complaintSystemStatus;
    bool _suggestionSystemStatus;
    BugTicketList _bugTicketList;
    ComplaintTicketList _complaintTicketList;
    SuggestionTicketList _suggestionTicketList;
    uint32 _lastBugId;
    uint32 _lastComplaintId;
    uint32 _lastSuggestionId;
    uint64 _lastChange;
    uint32 _openBugTicketCount;
    uint32 _openComplaintTicketCount;
    uint32 _openSuggestionTicketCount;
};

#define sSupportMgr SupportMgr::instance()

#endif // SupportMgr_h__
