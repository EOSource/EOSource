#ifndef WORLD_HPP_INCLUDED
#define WORLD_HPP_INCLUDED

#include "fwd/world.hpp"

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "config.hpp"
#include "database.hpp"
#include "map.hpp"
#include "timer.hpp"
#include "util/secure_string.hpp"
#include "i18n.hpp"

#include "fwd/character.hpp"
#include "fwd/command_source.hpp"
#include "fwd/eodata.hpp"
#include "fwd/eoserver.hpp"
#include "fwd/guild.hpp"
#include "fwd/map.hpp"
#include "fwd/party.hpp"
#include "fwd/player.hpp"
#include "fwd/quest.hpp"
#include "fwd/socket.hpp"
#include "fwd/commands.hpp"

struct Board_Post
{
	short id;
	std::string author;
	int author_admin;
	std::string subject;
	std::string body;
	double time;
};

struct Board
{
	int id;
	short last_id;
	std::list<Board_Post *> posts;

	Board(int id_) : id(id_), last_id(0) { }
};

struct Home
{
	std::string id;
	std::string name;
	short map;
	unsigned char x;
	unsigned char y;
	int level;
	int innkeeper_vend;

	Home() : map(1), x(0), y(0), level(-1), innkeeper_vend(0) { }
};

struct Fish_Drop
{
    short item;
    double chance;
    unsigned char levelreq;
    int exp;
};

struct Wood_Drop
{
    short item;
    double chance;
    unsigned char levelreq;
    int exp;
};

struct Mine_Drop
{
    short item;
    double chance;
    unsigned char levelreq;
    int exp;
};

/**
 * Object which holds and manages all maps and characters on the server, as well as timed events
 * Only one of these should exist per server
 */
class World
{
	protected:
		int last_character_id;
		void UpdateConfig();

	public:
		Timer timer;

		EOServer *server;
		Database db;

		GuildManager *guildmanager;

		EIF *eif;
		ENF *enf;
		ESF *esf;
		ECF *ecf;

		Config config;
		Config admin_config;
		Config drops_config;
		Config shops_config;
		Config arenas_config;
		Config formulas_config;
		Config home_config;
		Config skills_config;
		Config npcs_config;

		Config eosbot_config;
		Config achievements_config;
        Config message_config;
        Config event_config;
        Config devilgate_config;
        Config pets_config;
        Config harvesting_config;
        Config chatlogs_config;
        Config fishing_config;
        Config mining_config;
        Config woodcutting_config;
        Config cooking_config;
        Config effects_config;
        Config timedeffects_config;
        Config members_config;
        Config warps_config;
        Config giftbox1_config;
        Config giftbox2_config;
        Config shaving_config;
        Config tilemessage_config;
        Config partymap_config;
        Config ctf_config;
        Config buffspells_config;
        Config cursefilter_config;
        Config buffitems_config;
        Config commands_config;
        Config equipment_config;
        Config spells_config;
        Config poison_config;
        Config pvp_config;
        Config shrines_config;

        I18N i18n;

		std::vector<Character *> characters;
		std::vector<Party *> parties;
		std::vector<Map *> maps;
		std::vector<Home *> homes;
		std::vector<Command *> commands;
		std::vector<Fish_Drop *> fish_drops;
		std::vector<Mine_Drop *> mine_drops;
		std::vector<Wood_Drop *> wood_drops;

		std::map<short, std::shared_ptr<Quest>> quests;

		std::list<std::string> namelist;
		std::vector<std::string> wlist;

		std::array<Board *, 10> boards;
		std::array<int, 254> exp_table;
		std::vector<int> instrument_ids;

		int admin_count;
		int WaveNPCs;
        int wave;
        int counter;

		World(std::array<std::string, 6> dbinfo, const Config &eoserv_config, const Config &admin_config);

		void BeginDB();
		void CommitDB();

		void UpdateAdminCount(int admin_count);
		void IncAdminCount() { UpdateAdminCount(this->admin_count + 1); }
		void DecAdminCount() { UpdateAdminCount(this->admin_count - 1); }

		void AdminCommands(std::string command, const std::vector<std::string>& arguments, Command_Source* from = 0);
		void PlayerCommands(std::string command, const std::vector<std::string>& arguments, Command_Source* from = 0);

		void LoadHome();
		void LoadFish();
		void LoadMine();
		void LoadWood();

        int FindMap(std::string mapname);
		int GenerateCharacterID();
		int GeneratePlayerID();

		bool SpawnDevilNPC(int id);
		bool CheckBan(std::string username);
        bool DevilGateEnabled;

		void Login(Character *);
		void Logout(Character *);

		void Msg(Command_Source *from, std::string message, bool echo = true);
		void AdminMsg(Command_Source *from, std::string message, int minlevel = ADMIN_GUARDIAN, bool echo = true);
		void AnnounceMsg(Command_Source *from, std::string message, bool echo = true);

		void ServerMsg(std::string message);
		void StatusMsg(std::string message);
		void AdminReport(Character *from, std::string reportee, std::string message);
		void AdminRequest(Character *from, std::string message);

		void Rehash();
		void ReloadPub(bool quiet = false);
		void ReloadQuests();

		void Restart();
		void PartyMaps();
		void DevilTimer();
		void CTFTimer();
		void CTFDelay();
		void PVPTimer();
		void DevilGateEnd();

		bool global;
		bool devil;
        bool ctf;
        bool pvp;
        bool pvpenabled;
        bool ctfenabled;

        int pvp_bluekillcount;
        int pvp_redkillcount;

        int pvp_bluemembers;
        int pvp_redmembers;

        int bluecounter;
        int redcounter;

        int bluemembers;
        int redmembers;

        bool atbluebase;
        bool atredbase;

        int charactercounter;

		double last_chat;

        void ResetCTF();
		void ResetPVP();

        void Mute(Command_Source *from, Character *victim, bool announce = true);
		void Kick(Command_Source *from, Character *victim, bool announce = true);
		void Wall(Command_Source *from, Character *victim, bool announce = true);
		void Jail(Command_Source *from, Character *victim, bool announce = true);
		void Unjail(Command_Source *from, Character *victim);
		void Ban(Command_Source *from, Character *victim, int duration, bool announce = true);
		void Unban(Command_Source* from, std::string name, bool announce = true);

		int CheckBan(const std::string *username, const IPAddress *address, const int *hdid);

		Character *GetCharacter(std::string name);
		Character *GetCharacterReal(std::string real_name);
		Character *GetCharacterPID(unsigned int id);
		Character *GetCharacterCID(unsigned int id);

		Map *GetMap(short id);
		Home *GetHome(Character *);
		Home *GetHome(std::string);

		bool CharacterExists(std::string name);
		Character *CreateCharacter(Player *, std::string name, Gender, int hairstyle, int haircolor, Skin);
		void DeleteCharacter(std::string name);

		Player *Login(const std::string& username, util::secure_string&& password);
		Player *Login(std::string username);
		LoginReply LoginCheck(const std::string& username, util::secure_string&& password);

		bool CreatePlayer(const std::string& username, util::secure_string&& password,
			const std::string& fullname,const std::string& location, const std::string& email,
			const std::string& computer, const std::string& hdid, const std::string& ip);

		bool PlayerExists(std::string username);
		bool PlayerOnline(std::string username);

		bool PKExcept(const Map *map);
		bool PKExcept(int mapid);

		bool IsInstrument(int graphic_id);

		bool LoadWlist();
        bool CheckWlist(std::string checkstr, std::string checktag = "");

		~World();
};

#endif
