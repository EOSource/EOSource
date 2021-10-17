#ifndef CHARACTER_HPP_INCLUDED
#define CHARACTER_HPP_INCLUDED

#include "fwd/character.hpp"

#include <array>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <string>

#include "fwd/arena.hpp"
#include "fwd/guild.hpp"
#include "fwd/npc.hpp"
#include "fwd/map.hpp"
#include "fwd/packet.hpp"
#include "fwd/party.hpp"
#include "fwd/player.hpp"
#include "fwd/quest.hpp"
#include "fwd/world.hpp"

#include "command_source.hpp"
#include "eodata.hpp"
#include "guild.hpp"

struct Timestamp
{
	private:
		int ts;
		static const int max_ts = 8640000;

	public:
		Timestamp()
			: ts(-1)
		{ }

		Timestamp(int ts)
			: ts(ts % max_ts)
		{ }

		int GetHour() const        { return ts / 360000; }
		int GetMinute() const      { return ts / 6000 - GetHour() * 60; }
		int GetSecond() const      { return ts / 100 - GetHour() * 3600 - GetMinute() * 60; }
		int GetMillisecond() const { return ts * 10 - GetHour() * 3600000 - GetMinute() * 60000 - GetSecond() * 1000; }

		int operator-(Timestamp b) const
		{
			if (ts == -1) return b.ts;
			if (b.ts == -1) return ts;

			if (b.ts > ts)
				return ts - b.ts + max_ts;
			else
				return ts - b.ts;
		}
};

void character_cast_spell(void *character_void);

/**
 * Serialize a list of items in to a text format that can be restored with ItemUnserialize
 */
std::string ItemSerialize(const std::list<Character_Item> &list);

/**
 * Convert a string generated by ItemSerialze back to a list of items
 */
std::list<Character_Item> ItemUnserialize(const std::string& serialized);

/**
 * Serialize a paperdoll of 15 items in to a string that can be restored with DollUnserialize
 */
std::string DollSerialize(const std::array<int, 15> &list);

/**
 * Convert a string generated by DollSerialze back to a list of 15 items
 */
std::array<int, 15> DollUnserialize(const std::string& serialized);

/**
 * Serialize a list of spells in to a text format that can be restored with SpellUnserialize
 */
std::string SpellSerialize(const std::list<Character_Spell> &list);

/**
 * Convert a string generated by SpellSerialze back to a list of items
 */
std::list<Character_Spell> SpellUnserialize(const std::string& serialized);

/**
 * Serialize a list of achievements in to a text format that can be restored with AchievementsUnserialize
 */
std::string AchievementsSerialize(const std::list<Character_Achievements> &list);

/**
 * Convert a string generated by AchievementsSerialze back to a list of achievements
 */
std::list<Character_Achievements> AchievementsUnserialize(std::string serialized);

/**
 * One type of item in a Characters inventory
 */
struct Character_Item
{
	short id;
	int amount;

	Character_Item() = default;
	Character_Item(short id, int amount) : id(id), amount(amount) { }
};

/**
 * One spell that a Character knows
 */
struct Character_Spell
{
	short id;
	unsigned char level;

	Character_Spell() = default;
	Character_Spell(short id, unsigned char level) : id(id), level(level) { }
};

/**
 * Character quest states
 */
struct Character_QuestState
{
	short quest_id;
	std::string quest_state;
	std::string quest_progress;

	bool operator <(const Character_QuestState& rhs) const
	{
		return this->quest_id < rhs.quest_id;
	}
};

/**
 * Character achievements
 */
struct Character_Achievements
{
    std::string name;

    Character_Achievements() : name("") {}
};

class Character : public Command_Source
{
	public:
        enum NoInteractFlag
        {
            NoInteractCustom  = 0x01,
            NoInteractItems   = 0x02,
            NoInteractCombat  = 0x04,
            NoInteractDoors   = 0x08,
            NoInteractCharMod = 0x10
        };

        static constexpr int NoInteractAll = 0xFFFF;

        bool HasPet;

        NPC *pet;

		int login_time;
		int exp;
		int bankmax;
		int goldbank;
		int usage;
		int immune;

		bool online;
		bool nowhere;
		bool hidden;
		bool whispers;
		bool trade_agree;
		bool trading;
		bool jukebox_open;

		unsigned int id;

		AdminLevel admin;

		std::string real_name;
		std::string title;
		std::string home;
		std::string fiance;
		std::string partner;
		std::string guild_rankname;

        int transfer_timer;
        int transfer_petid;

		std::string transfer_target;
		std::string transfer_cost;
		std::string transfer_petname;
		std::string transfer_requester;

		bool transfer_pending;

		unsigned char clas;
		unsigned char oldhairstyle, hairstyle, haircolor;
		unsigned char level;
		unsigned char x, y;

		int oldmap;
		int oldx;
		int oldy;

        int rebirth;
		int bounty;
		int member;
		int warn;
		int cooking;
		int cookid;
		int event;
		int muted_until;

        bool pettalk;
		bool petdrops;
        bool petheal;
        bool petattack;
        bool petspells;
        bool petinv_open;
        bool pettransfer;
        bool lockeraccess;

        bool blueteam;
        bool redteam;
        bool bluehost;
        bool redhost;

        bool pvp_blueteam;
        bool pvp_redteam;

        int boosttimer;
        int hidetimer;
        int freezetimer;

        int poisontimer;
        int poisonhp;
        int poisontp;
        int poisoneffect;

        int regeneratetimer;
        int regenerateid;

        bool fishing_exp;
        bool mining_exp;
        bool woodcutting_exp;
        bool cooking_exp;

        int fexp;
        int mexp;
        int wexp;
        int cexp;

        int lockerpin;

        unsigned char flevel;
        unsigned char mlevel;
        unsigned char wlevel;
        unsigned char clevel;

		Gender gender;
		Skin race;
		SitState sitting;
		int nointeract;

		Direction direction;

		short mapid;
		short hp, tp;
		short str, intl, wis, agi, con, cha;
		short adj_str, adj_intl, adj_wis, adj_agi, adj_con, adj_cha;
		short statpoints, skillpoints;
		short weight, maxweight;
		short karma;
		short maxsp;
		short maxhp, maxtp;
		short accuracy, evade, armor;
		short mindam, maxdam;

        int boosteffect;
		int booststr;
		int boostint;
		int boostwis;
		int boostagi;
		int boostcon;
		int boostcha;
		double boostexp;
		double boostdrop;

		bool bot;

		std::string faux_name;

		Arena *next_arena;
		Arena *arena;

		char arena_kills;

		Character *trade_partner;

		std::list<Character_Item> trade_inventory;

		Character *party_trust_send;
		Character *party_trust_recv;

		PartyRequestType party_send_type;

		NPC *npc;
		ENF::Type npc_type;
		Board *board;

		std::string guild_join;
		std::string guild_invite;

		Timestamp timestamp;

		std::deque<std::string> chat_log;

		enum SpellTarget
		{
			TargetInvalid,
			TargetSelf,
			TargetNPC,
			TargetPlayer,
			TargetGroup
		};

		bool spell_ready;
		unsigned short spell_id;
		TimeEvent *spell_event;
		SpellTarget spell_target;
		unsigned short spell_target_id;

		double last_walk;
		int attacks;

		WarpAnimation warp_anim;

		enum EquipLocation
		{
			Boots,
			Accessory,
			Gloves,
			Belt,
			Armor,
			Necklace,
			Hat,
			Shield,
			Weapon,
			Ring1,
			Ring2,
			Armlet1,
			Armlet2,
			Bracer1,
			Bracer2
		};

		std::list<Character_Item> inventory;
		std::list<Character_Item> bank;
		std::array<int, 15> paperdoll;
		std::array<int, 15> cosmetic_paperdoll;
		std::list<Character_Spell> spells;
		std::list<Character_Achievements> achievements;
		std::list<NPC *> unregister_npc;
		std::map<short, std::shared_ptr<Quest_Context>> quests;
		std::set<Character_QuestState> quests_inactive;
		std::string quest_string;

		Character(std::string name, World *);

		bool CanInteractItems() const { return !(nointeract & NoInteractItems); }
        bool CanInteractCombat() const { return !(nointeract & NoInteractCombat); }
        bool CanInteractDoors() const { return !(nointeract & NoInteractDoors); }
        bool CanInteractCharMod() const { return !(nointeract & NoInteractCharMod); }

		void Login();

		static bool ValidName(std::string name);

		void Msg(Character *from, std::string message);
		void DialogMsg(std::string title, std::string message);
        void QuestMsg(std::string title, std::string message);
		void ServerMsg(std::string message);
		void StatusMsg(std::string message);
		void Attack(Direction direction);
		void Sit(SitState sit_type);
		void Stand();
		void Emote(enum Emote emote, bool echo = true);
		void Effect(int effect, bool echo = true);
		void CancelSpell();
		void SpellAct();
		void Warp(short map, unsigned char x, unsigned char y, WarpAnimation animation = WARP_ANIMATION_NONE);
		void Refresh();
		void ShowBoard(Board *board = 0);
		void CheckQuestRules();
		void CalculateStats(bool trigger_quests = true);
		void DropAll(Character *killer);
		void Hide();
		void Unhide();
		void Reset();
		void PetTransfer();
        void SummonPet(Character *owner, std::string petname);
        void SavePet();
        bool HatchPet(Character *hatcher, int npcid);
        bool PetNameTaken(Character *owner, std::string petname);
        bool AddPet(Character *owner, int npcid);
        bool RevivePets();

		bool Walk(Direction direction);
		bool AdminWalk(Direction direction);
		bool HasSpell(short spell);
		bool AddItem(short item, int amount);
		bool DelItem(short item, int amount);
		bool AddTradeItem(short item, int amount);
		bool DelTradeItem(short item);
		bool AddSpell(short spell);
		bool DelSpell(short spell);
		bool Unequip(short item, unsigned char subloc);
		bool Equip(short item, unsigned char subloc);
		bool InRange(unsigned char x, unsigned char y) const;
		bool InRange(const Character *) const;
		bool InRange(const NPC *) const;
		bool InRange(const Map_Item&) const;
		bool HasAchievement(std::string achievement);

		void ResetQuest(short id);
		void Mute(std::string from);
		void PlaySFX(unsigned char id);
		void PlayBard(unsigned char instrument, unsigned char note, bool echo = true);
		void FormulaVars(std::unordered_map<std::string, double> &vars, std::string prefix = "");

		void Dress(EquipLocation, unsigned short gfx_id);
		void Undress();
		void Undress(EquipLocation);
		void AddPaperdollData(PacketBuilder&, const char* format);

		void AddChatLog(std::string marker, std::string name, std::string msg);
		std::string GetChatLogDump();

		void Send(const PacketBuilder &);
		void Logout();
		void Save();
		void GiveRebirth(short rebirth);
		void GiveEXP(int exp);
		void GiveItem(short item, int amount);
		void RemoveItem(short item, int amount);
        void CalculateTNL();
		void PacketRecover();
		void UpdateStats();
		void SetClass(int id);
		void DeathRespawn();
		void UndoBuff();
		void ResetTransfer();
		void NewAchievement(std::string name);

		int HasItem(short item, bool include_trade = false);
		int HasLockerItem(short item);
		int CanHoldItem(short item, int max_amount);
		int Usage();

		short SpellLevel(short spell);
		short SpawnMap();

		unsigned char SpawnX();
		unsigned char SpawnY();

		std::string ReplaceStrings(Character *from, std::string message);

		std::list<Character_Item>::iterator DelItem(std::list<Character_Item>::iterator, int amount);
		std::string PaddedGuildTag();
		std::shared_ptr<Quest_Context> GetQuest(short id);

        AdminLevel SourceAccess() const;
        AdminLevel SourceDutyAccess() const;
		std::string SourceName() const;
		Character* SourceCharacter();
		World* SourceWorld();

		~Character();

		World *world;
		Player *player;

		std::shared_ptr<Guild> guild;
		std::shared_ptr<Guild_Create> guild_create;

		unsigned char guild_rank;

		Party *party;
		Map *map;

		const short &display_str, &display_intl, &display_wis, &display_agi, &display_con, &display_cha;
};

#endif