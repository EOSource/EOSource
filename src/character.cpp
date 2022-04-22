#include "character.hpp"

#include <algorithm>
#include <ctime>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <unordered_map>

#include "util.hpp"
#include "util/rpn.hpp"

#include "arena.hpp"
#include "console.hpp"
#include "database.hpp"
#include "eoclient.hpp"
#include "eodata.hpp"
#include "eoplus.hpp"
#include "map.hpp"
#include "npc.hpp"
#include "packet.hpp"
#include "party.hpp"
#include "player.hpp"
#include "quest.hpp"
#include "world.hpp"

void Character::DeathRespawn()
{
    this->hp = int(this->maxhp * static_cast<double>(this->world->config["DeathRecover"]));

    if (this->world->config["Deadly"])
        this->DropAll(this);

    this->PacketRecover();

    if (this->poisontimer > 0)
        this->poisontimer = 0;

    int diemap = this->mapid;

    if (this->mapid == int(this->world->pvp_config["PVPMap"]) && this->world->pvp)
    {
        int rx = util::to_int(util::explode(',', this->world->pvp_config["RedTeam.Location"])[0]);
        int ry = util::to_int(util::explode(',', this->world->pvp_config["RedTeam.Location"])[1]);

        int bx = util::to_int(util::explode(',', this->world->pvp_config["BlueTeam.Location"])[0]);
        int by = util::to_int(util::explode(',', this->world->pvp_config["BlueTeam.Location"])[1]);

        if (this->pvp_redteam)
            this->Warp(int(this->world->pvp_config["PVPMap"]), rx, ry, WARP_ANIMATION_NONE);
        else if (this->pvp_blueteam)
            this->Warp(int(this->world->pvp_config["PVPMap"]), bx, by, WARP_ANIMATION_NONE);

        UTIL_FOREACH(this->map->characters, character)
        {
            if (character->InRange(this))
                character->Refresh();
        }
    }
    else
    {
        this->Warp(this->SpawnMap(), this->SpawnX(), this->SpawnY());

        if (diemap == this->SpawnMap())
        {
            UTIL_FOREACH(this->map->characters, character)
            {
                if (character->InRange(this))
                    character->Refresh();
            }
        }
    }
}

void character_cast_spell(void *character_void)
{
	Character *character(static_cast<Character *>(character_void));

	if (!character->spell_event)
		return;

	character->spell_event = 0;

	const ESF_Data& spell = character->world->esf->Get(character->spell_id);

	if (spell.id == 0)
	{
	    return;
	}

	if (character->spell_target != Character::TargetInvalid)
	{
	    character->SpellAct();
	}
	else
	{
	    character->spell_ready = true;
	}
}

std::string ItemSerialize(const std::list<Character_Item> &list)
{
	std::string serialized;

	UTIL_CIFOREACH(list, item)
	{
		serialized.append(util::to_string(item->id));
		serialized.append(",");
		serialized.append(util::to_string(item->amount));
		serialized.append(";");
	}

	return serialized;
}

std::list<Character_Item> ItemUnserialize(const std::string& serialized)
{
	std::list<Character_Item> list;

	std::vector<std::string> parts = util::explode(';', serialized);

	UTIL_FOREACH(parts, part)
	{
		std::size_t pp = part.find_first_of(',', 0);

		if (pp == std::string::npos)
		{
			continue;
		}

		Character_Item newitem;
		newitem.id = util::to_int(part.substr(0, pp));
		newitem.amount = util::to_int(part.substr(pp + 1));

		list.emplace_back(std::move(newitem));
	}

	return list;
}

std::string DollSerialize(const std::array<int, 15> &list)
{
	std::string serialized;

	UTIL_FOREACH (list, item)
	{
		serialized.append(util::to_string(item));
		serialized.append(",");
	}

	return serialized;
}

std::array<int, 15> DollUnserialize(const std::string& serialized)
{
	std::array<int, 15> list{{}};
	std::size_t i = 0;

	std::vector<std::string> parts = util::explode(',', serialized);

	UTIL_FOREACH(parts, part)
	{
		list[i++] = util::to_int(part);

		if (i == list.size())
			break;
	}

	return list;
}

std::string SpellSerialize(const std::list<Character_Spell> &list)
{
	std::string serialized;

	UTIL_FOREACH (list, spell)
	{
		serialized.append(util::to_string(spell.id));
		serialized.append(",");
		serialized.append(util::to_string(spell.level));
		serialized.append(";");
	}

	return serialized;
}

std::list<Character_Spell> SpellUnserialize(const std::string& serialized)
{
	std::list<Character_Spell> list;

	std::vector<std::string> parts = util::explode(';', serialized);

	UTIL_FOREACH(parts, part)
	{
		std::size_t pp = 0;
		pp = part.find_first_of(',', 0);

		if (pp == std::string::npos)
			continue;

		Character_Spell newspell;
		newspell.id = util::to_int(part.substr(0, pp));
		newspell.level = util::to_int(part.substr(pp+1));

		list.emplace_back(std::move(newspell));
	}

	return list;
}

std::string AchievementsSerialize(const std::list<Character_Achievements> &list)
{
	std::string serialized;

	UTIL_FOREACH(list, achievement)
	{
		serialized.append(achievement.name);
		serialized.append(";");
	}

	return serialized;
}

std::list<Character_Achievements> AchievementsUnserialize(std::string serialized)
{
    std::list<Character_Achievements> list;

    std::size_t p = 0;
    std::size_t lastp = std::numeric_limits<std::size_t>::max();

    if (!serialized.empty() && *(serialized.end()-1) != ';')
	{
		serialized.push_back(';');
	}

    while ((p = serialized.find_first_of(';', p+1)) != std::string::npos)
    {

        Character_Achievements newachievement;
		newachievement.name = serialized.substr(lastp+1, p-lastp-1);

        list.push_back(newachievement);

        lastp = p;
    }

    return list;
}

std::string QuestSerialize(const std::map<short, std::shared_ptr<Quest_Context>>& list, const std::set<Character_QuestState>& list_inactive)
{
	std::string serialized;

	UTIL_FOREACH(list, quest)
	{
		if (!quest.second)
			continue;

		serialized.append(util::to_string(quest.second->GetQuest()->ID()));
		serialized.append(",");
		serialized.append(quest.second->StateName());
		serialized.append(",");
		serialized.append(quest.second->SerializeProgress());
		serialized.append(";");
	}

	UTIL_FOREACH(list_inactive, state)
	{
		if (list.find(state.quest_id) != list.end())
		{
            #ifdef DEBUG
			Console::Dbg("Discarding inactive quest save as the quest was restarted: %i", state.quest_id);
            #endif // DEBUG

			continue;
		}

		serialized.append(util::to_string(state.quest_id));
		serialized.append(",");
		serialized.append(state.quest_state);
		serialized.append(",");
		serialized.append(state.quest_progress);
		serialized.append(";");
	}

	return serialized;
}

void QuestUnserialize(std::string serialized, Character* character)
{
	bool conversion_warned = false;

	std::vector<std::string> parts = util::explode(';', serialized);

	UTIL_FOREACH(parts, part)
	{
		std::size_t pp1 = part.find_first_of(',');

		if (pp1 == std::string::npos)
			continue;

		std::size_t pp2 = part.find_first_of(',', pp1 + 1);

		Character_QuestState state;
		state.quest_id = util::to_int(part.substr(0, pp1));

		bool conversion_needed = false;

		if (pp2 != std::string::npos)
		{
			state.quest_state = part.substr(pp1 + 1, pp2 - pp1 - 1);
			state.quest_progress = part.substr(pp2 + 1);

			if (!state.quest_progress.empty() && state.quest_progress[0] != '{')
			{
				conversion_needed = true;

				Console::Wrn("State progress counter reset for quest: %i", state.quest_id);
				state.quest_progress = "{}";
			}
			else if (state.quest_progress.empty())
			{
				conversion_needed = true;
			}
		}
		else
		{
			pp2 = part.find_first_of(';', pp1 + 1);

			if (pp2 == std::string::npos)
				pp2 = part.length() + 1;

			state.quest_state = part.substr(pp1 + 1, pp2 - pp1 - 1);
			state.quest_progress = "{}";

			conversion_needed = true;
		}

		if (conversion_needed)
		{
			if (!conversion_warned)
			{
				Console::Wrn("Converting quests from old format...");
				conversion_warned = true;
			}
		}

		auto quest_it = character->world->quests.find(state.quest_id);

		if (quest_it == character->world->quests.end())
		{
			Console::Wrn("Quest not found: %i. Marking as inactive.", state.quest_id);

			if (!character->quests_inactive.insert(std::move(state)).second)
				Console::Wrn("Duplicate inactive quest record dropped for quest: %i", state.quest_id);

			continue;
		}

		Quest* quest = quest_it->second.get();
		auto quest_context(std::make_shared<Quest_Context>(character, quest));

		try
		{
			quest_context->SetState(state.quest_state, false);
			quest_context->UnserializeProgress(UTIL_CRANGE(state.quest_progress));
		}
		catch (EOPlus::Runtime_Error& ex)
		{
			Console::Wrn(ex.what());
			Console::Wrn("Could not resume quest: %i. Marking as inactive.", state.quest_id);

			if (!character->quests_inactive.insert(std::move(state)).second)
				Console::Wrn("Duplicate inactive quest record dropped for quest: %i", state.quest_id);

			continue;
		}

		auto result = character->quests.insert(std::make_pair(state.quest_id, std::move(quest_context)));

		if (!result.second)
		{
			Console::Wrn("Duplicate quest record dropped for quest: %i", state.quest_id);
			continue;
		}
	}
}

std::vector<std::string> BotListUnserialize(std::string serialized)
{
	std::vector<std::string> bots = util::explode(',', serialized);
	std::transform(UTIL_CRANGE(bots), bots.begin(), [](std::string s) { return util::lowercase(util::trim(s)); });
	return bots;
}

template <typename T> static T GetRow(std::unordered_map<std::string, util::variant> &row, const char *col)
{
	return row[col];
}

Character::Character(std::string name, World *world) : muted_until(0), bot(false), cosmetic_paperdoll{{}}, world(world),
display_str(this->world->config["UseAdjustedStats"] ? adj_str : str),
display_intl(this->world->config["UseAdjustedStats"] ? adj_intl : intl),
display_wis(this->world->config["UseAdjustedStats"] ? adj_wis : wis),
display_agi(this->world->config["UseAdjustedStats"] ? adj_agi : agi),
display_con(this->world->config["UseAdjustedStats"] ? adj_con : con),
display_cha(this->world->config["UseAdjustedStats"] ? adj_cha : cha)
{
	{
	    if (std::string(this->world->eosbot_config["bot.name"]) != "EOSbot")
	    {
	        std::vector<std::string> bot_characters = BotListUnserialize(this->world->eosbot_config["bot.name"]);

	        auto bot_it = std::find(UTIL_CRANGE(bot_characters), util::lowercase(name));
	        this->bot = bot_it != bot_characters.end();
	    }
	    else
	    {
	        std::vector<std::string> bot_characters = BotListUnserialize("EOSbot");

	        auto bot_it = std::find(UTIL_CRANGE(bot_characters), util::lowercase(name));
	        this->bot = bot_it != bot_characters.end();
	    }
	}

	Database_Result res = this->world->db.Query("SELECT `name`, `title`, `guild_rankname`, `home`, `fiance`, `partner`, `admin`, `class`, `rebirth`, `immune`, `bounty`, `member`, `gender`, `race`, `oldhairstyle`, `hairstyle`, `haircolor`,"
	"`map`, `x`, `y`, `oldmap`, `oldx`, `oldy`, `direction`, `level`, `exp`, `flevel`, `fexp`, `mlevel`, `mexp`, `wlevel`, `wexp`, `clevel`, `cexp`, `lockerpin`, `hp`, `tp`, `str`, `int`, `wis`, `agi`, `con`, `cha`, `statpoints`, `skillpoints`, "
	"`karma`, `sitting`, `bankmax`, `goldbank`, `usage`, `warn`, `inventory`, `bank`, `paperdoll`, `spells`, `guild`, `guild_rank`, `quest`, `achievements`, `vars`, `nointeract` FROM `characters` "
	"WHERE `name` = '$'", name.c_str());

	std::unordered_map<std::string, util::variant> row = res.front();

	this->login_time = std::time(0);

	this->online = false;
	this->nowhere = false;

    this->pettalk = true;
	this->petdrops = true;
    this->petheal = true;
    this->petattack = true;
    this->petspells = true;
    this->pettransfer = false;

    this->lockeraccess = false;

    this->blueteam = false;
    this->redteam = false;

    this->pvp_blueteam = false;
    this->pvp_redteam = false;

    this->bluehost = false;
    this->redhost = false;

	this->id = this->world->GenerateCharacterID();

	this->admin = static_cast<AdminLevel>(GetRow<int>(row, "admin"));
	this->real_name = GetRow<std::string>(row, "name");
	this->title = GetRow<std::string>(row, "title");
	this->home = GetRow<std::string>(row, "home");
	this->fiance = GetRow<std::string>(row, "fiance");
	this->partner = GetRow<std::string>(row, "partner");
	this->guild_rankname = GetRow<std::string>(row, "guild_rankname");

	this->clas = GetRow<int>(row, "class");
	this->rebirth = GetRow<int>(row, "rebirth");
	this->immune = GetRow<int>(row, "immune");
	this->bounty = GetRow<int>(row, "bounty");
	this->member = GetRow<int>(row, "member");
	this->gender = static_cast<Gender>(GetRow<int>(row, "gender"));
	this->race = static_cast<Skin>(GetRow<int>(row, "race"));
	this->oldhairstyle = GetRow<int>(row, "oldhairstyle");
	this->hairstyle = GetRow<int>(row, "hairstyle");
	this->haircolor = GetRow<int>(row, "haircolor");

	this->x = GetRow<int>(row, "x");
	this->y = GetRow<int>(row, "y");
	this->direction = static_cast<Direction>(GetRow<int>(row, "direction"));

	this->oldmap = GetRow<int>(row, "oldmap");
	this->oldx = GetRow<int>(row, "oldx");
	this->oldy = GetRow<int>(row, "oldy");

	this->level = GetRow<int>(row, "level");
	this->exp = GetRow<int>(row, "exp");

	this->flevel = GetRow<int>(row, "flevel");
	this->fexp = GetRow<int>(row, "fexp");

	this->mlevel = GetRow<int>(row, "mlevel");
	this->mexp = GetRow<int>(row, "mexp");

	this->wlevel = GetRow<int>(row, "wlevel");
	this->wexp = GetRow<int>(row, "wexp");

	this->clevel = GetRow<int>(row, "clevel");
	this->cexp = GetRow<int>(row, "cexp");

	this->fishing_exp = false;
	this->mining_exp = false;
	this->woodcutting_exp = false;
	this->cooking_exp = false;

	this->lockerpin = GetRow<int>(row, "lockerpin");

	this->hp = GetRow<int>(row, "hp");
	this->tp = GetRow<int>(row, "tp");

	this->str = GetRow<int>(row, "str");
	this->intl = GetRow<int>(row, "int");
	this->wis = GetRow<int>(row, "wis");
	this->agi = GetRow<int>(row, "agi");
	this->con = GetRow<int>(row, "con");
	this->cha = GetRow<int>(row, "cha");
	this->statpoints = GetRow<int>(row, "statpoints");
	this->skillpoints = GetRow<int>(row, "skillpoints");
	this->karma = GetRow<int>(row, "karma");

	this->weight = 0;
	this->maxweight = 0;

	this->cooking = 0;
	this->cookid = 0;
	this->event = 0;

	this->boosttimer = 0;
	this->hidetimer = 0;
	this->freezetimer = 0;

	this->poisontimer = 0;
	this->poisonhp = 0;
	this->poisontp = 0;

    this->regeneratetimer = 0;
    this->regenerateid = 0;

	this->maxhp = 0;
	this->maxtp = 0;
	this->maxsp = 0;

	this->mindam = 0;
	this->maxdam = 0;

	this->accuracy = 0;
	this->evade = 0;
	this->armor = 0;

	this->trading = false;
	this->trade_partner = 0;
	this->trade_agree = false;

	this->party_trust_send = 0;
	this->party_trust_recv = 0;

	this->npc = 0;
	this->npc_type = ENF::NPC;
	this->board = 0;
	this->jukebox_open = false;

	this->spell_ready = false;
	this->spell_id = 0;
	this->spell_event = 0;
	this->spell_target = TargetInvalid;

	this->next_arena = 0;
	this->arena = 0;

	this->warp_anim = WARP_ANIMATION_INVALID;

	this->sitting = static_cast<SitState>(GetRow<int>(row, "sitting"));
	this->hidden = false;
	this->whispers = true;

	this->bankmax = GetRow<int>(row, "bankmax");
	this->goldbank = GetRow<int>(row, "goldbank");
	this->usage = GetRow<int>(row, "usage");
	this->warn = GetRow<int>(row, "warn");

	this->inventory = ItemUnserialize(row["inventory"]);
	this->bank = ItemUnserialize(row["bank"]);
	this->paperdoll = DollUnserialize(row["paperdoll"]);
	this->spells = SpellUnserialize(row["spells"]);
	this->achievements = AchievementsUnserialize(row["achievements"]);

	this->transfer_timer = 0;
	this->transfer_petid = 0;

	this->transfer_target = "";
	this->transfer_cost = "";
	this->transfer_petname = "";
	this->transfer_requester = "";

	this->transfer_pending = false;

    this->pet = 0;
	this->player = 0;

    this->boosteffect = 0;
	this->booststr = 0;
	this->boostint = 0;
	this->boostwis = 0;
	this->boostagi = 0;
	this->boostcon = 0;
	this->boostcha = 0;
	this->boostexp = 0;
	this->boostdrop = 0;

	this->HasPet = false;

	std::string guild_tag = util::trim(static_cast<std::string>(row["guild"]));

	if (!guild_tag.empty())
	{
		this->guild = this->world->guildmanager->GetGuild(guild_tag);
		this->guild_rank = static_cast<int>(row["guild_rank"]);
	}
	else
	{
		this->guild_rank = 0;
	}

	this->party = 0;
	this->map = this->world->GetMap(GetRow<int>(row, "map"));
	this->mapid = this->map->id;

	this->last_walk = 0.0;
	this->attacks = 0;

	this->quest_string = GetRow<std::string>(row, "quest");

	this->nointeract = GetRow<int>(row, "nointeract");

    if (this->admin >= static_cast<int>(world->config["NoInteractDefaultAdmin"]) && !(this->nointeract & NoInteractCustom))
    {
        this->nointeract = static_cast<int>(world->config["NoInteractDefault"]);
    }
}

void Character::Login()
{
	this->CalculateStats(false);

	QuestUnserialize(this->quest_string, this);
	this->quest_string.clear();

	if (!this->GetQuest(0))
	{
		auto it = this->world->quests.find(0);

		if (it != this->world->quests.end())
		{
			Quest* quest = it->second.get();

			if (!quest->Disabled())
			{
				auto context = std::make_shared<Quest_Context>(this, quest);
                this->quests[it->first] = context;
				context->SetState("begin");
			}
		}
	}

	this->online = true;

	this->CalculateStats();
}

bool Character::ValidName(std::string name)
{
	if (name.length() < 4)
	{
		return false;
	}

	if (name.length() > 12)
	{
		return false;
	}

	for (std::size_t i = 0; i < name.length(); ++i)
	{
		if (name[i] < 'a' || name[i] > 'z')
		{
			return false;
		}
	}

	if (name == "server")
		return false;

	return true;
}

void Character::Msg(Character *from, std::string message)
{
	message = util::text_cap(message, static_cast<int>(this->world->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from->SourceName()) + "  "));

	from->AddChatLog("!", "to " + this->SourceName(), message);
    this->AddChatLog("!", "from " + from->SourceName(), message);

    PacketBuilder builder(PACKET_TALK, PACKET_TELL, 2 + from->SourceName().length() + message.length());
    builder.AddBreakString(from->SourceName());
    builder.AddBreakString(message);
    this->player->Send(builder);
}

void Character::DialogMsg(std::string title, std::string message)
{
    if (message != "0")
    {
        PacketBuilder builder;
        builder.SetID(PACKET_MESSAGE, PACKET_ACCEPT);
        builder.AddString(title);
        builder.AddByte(255);
        builder.AddString(message);
        this->Send(builder);
    }
}

void Character::QuestMsg(std::string title, std::string message)
{
    if (message != "0")
    {
        PacketBuilder builder;
        builder.SetID(PACKET_QUEST, PACKET_DIALOG);
        builder.AddChar(1);
        builder.AddShort(1);
        builder.AddByte(255);
        builder.AddShort(1);
        builder.AddString(title);
        builder.AddByte(255);
        builder.AddShort(1);
        builder.AddString(message);
        builder.AddByte(255);
        this->Send(builder);
    }
}

void Character::ServerMsg(std::string message)
{
	message = util::text_cap(message, static_cast<int>(this->world->config["ChatMaxWidth"]) - util::text_width("Server  "));

	if (message != "0")
	{
        PacketBuilder builder(PACKET_TALK, PACKET_SERVER, message.length());
        builder.AddString(message);
        this->player->Send(builder);
	}
}

void Character::StatusMsg(std::string message)
{
	message = util::text_cap(message, static_cast<int>(this->world->config["ChatMaxWidth"]));

	if (message != "0")
	{
        PacketBuilder builder(PACKET_MESSAGE, PACKET_OPEN, message.length());
        builder.AddString(message);
        this->player->Send(builder);
	}
}

bool Character::Walk(Direction direction)
{
	return this->map->Walk(this, direction);
}

bool Character::AdminWalk(Direction direction)
{
	return this->map->Walk(this, direction, true);
}

void Character::Attack(Direction direction)
{
	this->map->Attack(this, direction);
}

void Character::Sit(SitState sit_type)
{
	this->map->Sit(this, sit_type);
}

void Character::Stand()
{
	this->map->Stand(this);
}

void Character::Emote(enum Emote emote, bool echo)
{
	this->map->Emote(this, emote, echo);
}

void Character::Effect(int effect, bool echo)
{
	PacketBuilder builder(PACKET_EFFECT, PACKET_PLAYER, 5);
	builder.AddShort(this->player->id);
	builder.AddThree(effect);

	UTIL_FOREACH(this->map->characters, character)
	{
		if (!echo && (character == this || !this->InRange(character)))
		{
			continue;
		}

		character->Send(builder);
	}
}

int Character::HasItem(short item, bool include_trade)
{
	UTIL_IFOREACH(this->inventory, it)
	{
		if (it->id == item)
		{
			if (this->trading && !include_trade)
			{
				UTIL_FOREACH(this->trade_inventory, trade_item)
				{
					if (trade_item.id == item)
					{
						return std::max(it->amount - trade_item.amount, 0);
					}
				}

				return it->amount;
			}
			else
			{
				return it->amount;
			}
		}
	}

	return 0;
}

int Character::HasLockerItem(short item)
{
    UTIL_FOREACH(this->bank, it)
    {
        if (it.id == item)
        {
            return it.amount;
        }
    }
    return 0;
}

bool Character::HasSpell(short spell)
{
	return (std::find_if(UTIL_RANGE(this->spells), [&](Character_Spell cs) { return cs.id == spell; }) != this->spells.end());
}

short Character::SpellLevel(short spell)
{
	auto it = std::find_if(UTIL_RANGE(this->spells), [&](Character_Spell cs) { return cs.id == spell; });

	if (it != this->spells.end())
	{
	    return it->level;
	}
	else
	{
	    return 0;
	}
}

bool Character::HasAchievement(std::string achievement)
{
    UTIL_FOREACH(this->achievements, achieve)
    {
        if (achieve.name == achievement)
        {
            return true;
        }
    }

    return false;
}

bool Character::AddItem(short item, int amount)
{
	if (amount <= 0)
	{
		return false;
	}

	if (item <= 0 || static_cast<std::size_t>(item) >= this->world->eif->data.size())
	{
		return false;
	}

	UTIL_IFOREACH(this->inventory, it)
	{
		if (it->id == item)
		{
			if (it->amount + amount < 0)
			{
				return false;
			}

			it->amount += amount;

			it->amount = std::min<int>(it->amount, this->world->config["MaxItem"]);

			this->CalculateStats();

			return true;
		}
	}

	Character_Item newitem;

	newitem.id = item;
	newitem.amount = amount;

	this->inventory.push_back(newitem);
	this->CalculateStats();

	return true;
}

bool Character::DelItem(short item, int amount)
{
	if (amount <= 0)
	{
		return false;
	}

	UTIL_IFOREACH(this->inventory, it)
	{
		if (it->id == item)
		{
			if (it->amount < 0 || it->amount - amount <= 0)
			{
				this->inventory.erase(it);
			}
			else
			{
				it->amount -= amount;
			}

			this->CalculateStats();

			return true;
		}
	}

	return false;
}

std::list<Character_Item>::iterator Character::DelItem(std::list<Character_Item>::iterator it, int amount)
{
	if (amount <= 0)
	{
		return ++it;
	}

	if (it->amount < 0 || it->amount - amount <= 0)
	{
		it = this->inventory.erase(it);
	}
	else
	{
		it->amount -= amount;
		++it;
	}

	this->CalculateStats();

	return it;
}

int Character::CanHoldItem(short itemid, int max_amount)
{
	int amount = max_amount;

	if (int(this->world->config["EnforceWeight"]) >= 2 && SourceDutyAccess() < static_cast<int>(world->config["unlimitedweight"]))
	{
		const EIF_Data &item = this->world->eif->Get(itemid);

		if (this->weight > this->maxweight)
			amount = 0;
		else if (!item || item.weight == 0)
			amount = max_amount;
		else
			amount = std::min((this->maxweight - this->weight) / item.weight, max_amount);
	}

	return std::min<int>(amount, this->world->config["MaxItem"]);
}

bool Character::AddTradeItem(short item, int amount)
{
	bool trade_add_quantity = bool(this->world->config["TradeAddQuantity"]);

	if (amount <= 0 || amount > int(this->world->config["MaxTrade"]))
	{
		return false;
	}

	if (item <= 0 || static_cast<std::size_t>(item) >= this->world->eif->data.size())
	{
		return false;
	}

	int hasitem = this->HasItem(item, !trade_add_quantity);

	amount = std::min(amount, hasitem);

	if (trade_add_quantity)
	{
		int tradeitem = 0;

		UTIL_FOREACH(this->trade_inventory, trade_item)
		{
			if (trade_item.id == item)
			{
				tradeitem = trade_item.amount;
			}
		}

		if (tradeitem + amount < 0 || tradeitem + amount > int(this->world->config["MaxTrade"]))
		{
			return false;
		}

	}

	UTIL_FOREACH_REF(this->trade_inventory, character_item)
 	{
 		if (character_item.id == item)
 		{
			if (trade_add_quantity)
				character_item.amount += amount;
			else
				character_item.amount = amount;

 			return true;
 		}
 	}

	Character_Item newitem;
	newitem.id = item;
	newitem.amount = amount;

	this->trade_inventory.push_back(newitem);
	this->CheckQuestRules();

	return true;
}

bool Character::DelTradeItem(short item)
{
	for (std::list<Character_Item>::iterator it = this->trade_inventory.begin(); it != this->trade_inventory.end(); ++it)
	{
		if (it->id == item)
		{
			this->trade_inventory.erase(it);
			this->CheckQuestRules();

			return true;
		}
	}

	return false;
}

bool Character::AddSpell(short spell)
{
	if (spell <= 0 || std::size_t(spell) >= this->world->esf->data.size())
		return false;

	if (this->HasSpell(spell))
	{
	    return false;
	}

	this->spells.push_back(Character_Spell(spell, 0));

	this->CheckQuestRules();

	return true;
}

bool Character::DelSpell(short spell)
{
	auto remove_it = std::remove_if(UTIL_RANGE(this->spells), [&](Character_Spell cs) { return cs.id == spell; });
	bool removed = (remove_it != this->spells.end());
	this->spells.erase(remove_it, this->spells.end());

	this->CheckQuestRules();

	return removed;
}

void Character::CancelSpell()
{
	this->spell_target = TargetInvalid;

	delete this->spell_event;
	this->spell_event = 0;

	if (this->spell_ready)
	{
		this->spell_ready = false;
	}
}

void Character::SpellAct()
{
	const ESF_Data &spell = world->esf->Get(this->spell_id);

	if (spell.id == 0 || spell.type == ESF::Bard)
	{
		this->CancelSpell();

		return;
	}

	Character *victim;
	NPC *npc_victim;

	SpellTarget spell_target = this->spell_target;

	short spell_id = this->spell_id;
	unsigned short spell_target_id = this->spell_target_id;

	this->CancelSpell();

	switch (spell_target)
	{
		case TargetSelf:
        if (spell.target_restrict != ESF::Friendly || spell.target != ESF::Self)
            return;

        this->map->SpellSelf(this, spell_id);

        break;

		case TargetNPC:
        if (spell.target_restrict == ESF::Friendly || spell.target != ESF::Normal)
            return;

        npc_victim = this->map->GetNPCIndex(spell_target_id);

        if (npc_victim)
        {
            this->map->SpellAttack(this, npc_victim, spell_id);
        }

        break;

		case TargetPlayer:
        if (spell.target_restrict == ESF::NPCOnly || spell.target != ESF::Normal)
            return;

        victim = this->map->GetCharacterPID(spell_target_id);

        if (spell.target_restrict != ESF::Friendly && victim == this)
            return;

        if (victim)
            this->map->SpellAttackPK(this, victim, spell_id);

        break;

		case TargetGroup:
        if (spell.target_restrict != ESF::Friendly || spell.target != ESF::Group)
            return;

        this->map->SpellGroup(this, spell_id);

        break;

		default:
        return;
	}

	UTIL_FOREACH(this->quests, q)
	{
		if (!q.second || q.second->GetQuest()->Disabled())
			continue;

		q.second->UsedSpell(spell_id);
	}
}

void Character::NewAchievement(std::string name)
{
    Character_Achievements newachievement;
    newachievement.name = name;
    this->achievements.push_back(newachievement);
}

bool Character::Unequip(short item, unsigned char subloc)
{
	if (item == 0)
		return false;

	for (std::size_t i = 0; i < this->paperdoll.size(); ++i)
	{
		if (this->paperdoll[i] == item)
		{
			if (((i == Character::Ring2 || i == Character::Armlet2 || i == Character::Bracer2) ? 1 : 0) == subloc)
			{
				this->paperdoll[i] = 0;

				this->AddItem(item, 1);
				this->CalculateStats();

				return true;
			}
		}
	}

	return false;
}

static bool character_equip_oneslot(Character *character, short item, unsigned char subloc, Character::EquipLocation slot)
{
	(void)subloc;

	if (character->paperdoll[slot] != 0)
	{
		return false;
	}

	character->paperdoll[slot] = item;
	character->DelItem(item, 1);

	character->CalculateStats();
	return true;
}

static bool character_equip_twoslot(Character *character, short item, unsigned char subloc, Character::EquipLocation slot1, Character::EquipLocation slot2)
{
	if (subloc == 0)
	{
		if (character->paperdoll[slot1] != 0)
		{
			return false;
		}

		character->paperdoll[slot1] = item;
		character->DelItem(item, 1);
	}
	else
	{
		if (character->paperdoll[slot2] != 0)
		{
			return false;
		}

		character->paperdoll[slot2] = item;
		character->DelItem(item, 1);
	}

	character->CalculateStats();
	return true;
}

bool Character::Equip(short item, unsigned char subloc)
{
	if (!this->HasItem(item))
		return false;

	const EIF_Data &eif = this->world->eif->Get(item);
	const ECF_Data &ecf = this->world->ecf->Get(this->clas);

	if (eif.type == EIF::Armor && eif.gender != this->gender)
		return false;

	if (eif.type == EIF::Weapon && eif.subtype == EIF::TwoHanded)
	{
		if (this->paperdoll[Shield])
		{
			const EIF_Data& shield_eif = this->world->eif->Get(this->paperdoll[Shield]);

			if (eif.dual_wield_dollgraphic || (shield_eif.subtype != EIF::Arrows && shield_eif.subtype != EIF::Wings))
			{
				this->StatusMsg(this->world->i18n.Format("TwoHandedFail1"));
				return false;
			}
		}
	}

	if (eif.type == EIF::Shield)
	{
		if (this->paperdoll[Weapon])
		{
			const EIF_Data& weapon_eif = this->world->eif->Get(this->paperdoll[Weapon]);

			if (weapon_eif.subtype == EIF::TwoHanded && (weapon_eif.dual_wield_dollgraphic || (eif.subtype != EIF::Arrows && eif.subtype != EIF::Wings)))
			{
				this->StatusMsg(this->world->i18n.Format("TwoHandedFail2"));
				return false;
			}
		}
	}

	if (this->display_str < eif.strreq)
        this->StatusMsg("This item requires " + util::to_string(eif.strreq) + " STR");

	if ((this->level < eif.levelreq && this->rebirth == 0)
    || this->display_str < eif.strreq || this->display_intl < eif.intreq
    || this->display_wis < eif.wisreq || this->display_agi < eif.agireq
    || this->display_con < eif.conreq || this->display_cha < eif.chareq)
	{
		return false;
	}

	for (ECF_Data &classreq = this->world->ecf->Get(this->clas); classreq.base != 0; classreq = this->world->ecf->Get(classreq.base))
    {
          if (this->clas != eif.classreq && ecf.base != eif.classreq && this->world->ecf->Get(ecf.base).base != eif.classreq && eif.classreq != 0)
              return false;
    }

	switch (eif.type)
	{
		case EIF::Weapon: return character_equip_oneslot(this, item, subloc, Weapon);
		case EIF::Shield: return character_equip_oneslot(this, item, subloc, Shield);
		case EIF::Hat: return character_equip_oneslot(this, item, subloc, Hat);
		case EIF::Boots: return character_equip_oneslot(this, item, subloc, Boots);
		case EIF::Gloves: return character_equip_oneslot(this, item, subloc, Gloves);
		case EIF::Accessory: return character_equip_oneslot(this, item, subloc, Accessory);
		case EIF::Belt: return character_equip_oneslot(this, item, subloc, Belt);
		case EIF::Armor: return character_equip_oneslot(this, item, subloc, Armor);
		case EIF::Necklace: return character_equip_oneslot(this, item, subloc, Necklace);
		case EIF::Ring: return character_equip_twoslot(this, item, subloc, Ring1, Ring2);
		case EIF::Armlet: return character_equip_twoslot(this, item, subloc, Armlet1, Armlet2);
		case EIF::Bracer: return character_equip_twoslot(this, item, subloc, Bracer1, Bracer2);
		default: return false;
	}
}

bool Character::InRange(unsigned char x, unsigned char y) const
{
	return util::path_length(this->x, this->y, x, y) <= static_cast<int>(this->world->config.at("SeeDistance"));
}

bool Character::InRange(const Character *other) const
{
	if (this->nowhere || other->nowhere)
	{
		return false;
	}

	return this->InRange(other->x, other->y);
}

bool Character::InRange(const NPC *other) const
{
	if (this->nowhere)
	{
		return false;
	}

	return this->InRange(other->x, other->y);
}

bool Character::InRange(const Map_Item &other) const
{
	if (this->nowhere)
	{
		return false;
	}

	return this->InRange(other.x, other.y);
}

void Character::Warp(short map, unsigned char x, unsigned char y, WarpAnimation animation)
{
    if (map <= 0 || map > int(this->world->maps.size()) || !this->world->GetMap(map)->exists)
        return;

    if ((map == int(this->world->partymap_config[util::to_string(map) + ".map"]) || this->mapid == int(this->world->partymap_config[util::to_string(this->mapid) + ".map"])) && map != int(this->world->partymap_config[util::to_string(this->mapid) + ".kickmap"]))
    {
        int partyreq = int(this->world->partymap_config[util::to_string(map) + ".partyreq"]);
        int kickMap = int(this->world->partymap_config[util::to_string(map) + ".kickmap"]);
        int kickX = int(this->world->partymap_config[util::to_string(map) + ".kickx"]);
        int kickY = int(this->world->partymap_config[util::to_string(map) + ".kicky"]);

        if (this->party)
        {
            if (this->party->members.size() < int64_t(partyreq))
            {
                map = kickMap;
                x = kickX;
                y = kickY;

                this->ServerMsg("You cannot enter the map because your party requires " + util::to_string(partyreq) + " players.");
            }
        }
        else
        {
            map = kickMap;
            x = kickX;
            y = kickY;

            this->ServerMsg("You cannot enter the map because your party requires " + util::to_string(partyreq) + " members.");
        }
    }

    PacketBuilder builder(PACKET_WARP, PACKET_REQUEST);

    if (this->mapid == map && !this->nowhere)
    {
        builder.ReserveMore(5);
        builder.AddChar(WARP_LOCAL);
        builder.AddShort(map);
        builder.AddChar(x);
        builder.AddChar(y);
    }
    else
    {
        builder.ReserveMore(14);
        builder.AddChar(WARP_SWITCH);
        builder.AddShort(map);

        if (this->world->config["GlobalPK"] && !this->world->PKExcept(map))
        {
                builder.AddByte(0xFF);
                builder.AddByte(0x01);
        }
        else
        {
                builder.AddByte(this->world->GetMap(map)->rid[0]);
                builder.AddByte(this->world->GetMap(map)->rid[1]);
        }

        builder.AddByte(this->world->GetMap(map)->rid[2]);
        builder.AddByte(this->world->GetMap(map)->rid[3]);
        builder.AddThree(this->world->GetMap(map)->filesize);
        builder.AddChar(0);
        builder.AddChar(0);
    }

    if (this->HasPet)
        this->PetTransfer();

    if (this->map && this->map->exists)
        this->map->Leave(this, animation);

    if (!this->HasAchievement(std::string(this->world->achievements_config["EnterMap.Achievement." + util::to_string(map)])))
    {
        if (std::string(this->world->achievements_config["EnterMap.Achievement." + util::to_string(map)]) != "0")
        {
            this->GiveEXP(std::min(std::max(int(this->world->achievements_config["EnterMap.Reward." + util::to_string(map)]), 0), int(this->world->config["MaxExp"])));

            if (std::string(this->world->achievements_config["EnterMap.Title." + util::to_string(map)]) != "0")
                this->title = std::string(this->world->achievements_config["EnterMap.Title." + util::to_string(map)]);

            if (std::string(this->world->achievements_config["EnterMap.Description." + util::to_string(map)]) != "0")
                this->ServerMsg(std::string(this->world->achievements_config["EnterMap.Description." + util::to_string(map)]));

            if (std::string(this->world->achievements_config["EnterMap.Announcement"]) == "yes")
                this->world->ServerMsg(util::ucfirst(this->SourceName()) + " earned the achievement '" + std::string(this->world->achievements_config["EnterMap.Achievement." + util::to_string(map)]) + "'.");

            this->NewAchievement(std::string(this->world->achievements_config["EnterMap.Achievement." + util::to_string(map)]));
        }
    }

    this->map = this->world->GetMap(map);
    this->mapid = this->map->id;
    this->x = x;
    this->y = y;
    this->sitting = SIT_STAND;

    this->npc = 0;
    this->npc_type = ENF::NPC;
    this->board = 0;
    this->jukebox_open = false;
    this->guild_join = "";
    this->guild_invite = "";

    if (this->trading)
    {
        PacketBuilder builder(PACKET_TRADE, PACKET_CLOSE, 2);
        builder.AddShort(this->player->id);
        this->trade_partner->Send(builder);

        this->trading = false;
        this->trade_inventory.clear();
        this->trade_agree = false;

        this->trade_partner->trading = false;
        this->trade_partner->trade_inventory.clear();
        this->trade_agree = false;

        this->CheckQuestRules();
		this->trade_partner->CheckQuestRules();

        this->trade_partner->trade_partner = 0;
        this->trade_partner = 0;
    }

    this->warp_anim = animation;
    this->nowhere = false;
    this->map->Enter(this, animation);

    this->player->Send(builder);

    if (this->arena)
    {
        --this->arena->occupants;
        this->arena = 0;
    }

    if (this->next_arena)
    {
        this->arena = this->next_arena;
        ++this->arena->occupants;
        this->next_arena = 0;
    }

    if (this->pettransfer)
        this->PetTransfer();
}

void Character::Refresh()
{
	std::vector<Character *> updatecharacters;
	std::vector<NPC *> updatenpcs;
	std::vector<std::shared_ptr<Map_Item>> updateitems;

	UTIL_FOREACH(this->map->characters, character)
	{
		if (this->InRange(character))
		{
			if (!character->hidden)
            {
                updatecharacters.push_back(character);
            }
            else if ((character->hidden && this->player->character->admin >= static_cast<int>(this->world->admin_config["seehide"])) || this->player->character == character)
            {
                updatecharacters.push_back(character);
            }
		}
	}

	UTIL_FOREACH(this->map->npcs, npc)
	{
		if (this->InRange(npc) && npc->alive)
		{
			updatenpcs.push_back(npc);
		}
	}

	UTIL_FOREACH(this->map->items, item)
	{
		if (this->InRange(*item))
		{
			updateitems.push_back(item);
		}
	}

	PacketBuilder builder(PACKET_REFRESH, PACKET_REPLY, 3 + updatecharacters.size() * 60 + updatenpcs.size() * 6 + updateitems.size() * 9);

	if (this->mapid == static_cast<int>(this->world->eosbot_config["bot.map"]) && static_cast<std::string>(this->world->eosbot_config["bot.enabled"]) != "no")
    {
        builder.AddChar(updatecharacters.size() + 1);
    }
    else
    {
        builder.AddChar(updatecharacters.size());
    }

	builder.AddByte(255);

	UTIL_FOREACH(updatecharacters, character)
	{
		builder.AddBreakString(character->SourceName());
		builder.AddShort(character->player->id);
		builder.AddShort(character->mapid);
		builder.AddShort(character->x);
		builder.AddShort(character->y);
		builder.AddChar(character->direction);
		builder.AddChar(6);
		builder.AddString(character->PaddedGuildTag());
		builder.AddChar(character->level);
		builder.AddChar(character->gender);
		builder.AddChar(character->hairstyle);
		builder.AddChar(character->haircolor);
		builder.AddChar(character->race);
		builder.AddShort(character->maxhp);
		builder.AddShort(character->hp);
		builder.AddShort(character->maxtp);
		builder.AddShort(character->tp);

		// equipment

		character->AddPaperdollData(builder, "B000A0HSW");

		builder.AddChar(character->sitting);
		builder.AddChar(character->hidden);
		builder.AddByte(255);
	}

	if (this->mapid == static_cast<int>(this->world->eosbot_config["bot.map"]) && static_cast<std::string>(this->world->eosbot_config["bot.enabled"]) != "no")
    {
        if (static_cast<std::string>(this->world->eosbot_config["bot.name"]) == "0")
        {
            builder.AddBreakString("EOSbot");
        }
        else
        {
            builder.AddBreakString(static_cast<std::string>(this->world->eosbot_config["bot.name"]));
        }

        builder.AddShort(0);
        builder.AddShort(static_cast<int>(this->world->eosbot_config["bot.map"]));
        builder.AddShort(static_cast<int>(this->world->eosbot_config["bot.locx"]));
        builder.AddShort(static_cast<int>(this->world->eosbot_config["bot.locy"]));
        builder.AddChar(static_cast<int>(this->world->eosbot_config["bot.direction"]));
        builder.AddChar(6);
        builder.AddString(static_cast<std::string>(this->world->eosbot_config["bot.guildtag"]));
        builder.AddChar(static_cast<int>(this->world->eosbot_config["bot.level"]));
        builder.AddChar(static_cast<int>(this->world->eosbot_config["bot.gender"]));
        builder.AddChar(static_cast<int>(this->world->eosbot_config["bot.hairstyle"]));
        builder.AddChar(static_cast<int>(this->world->eosbot_config["bot.haircolor"]));
        builder.AddChar(static_cast<int>(this->world->eosbot_config["bot.race"]));
        builder.AddShort(0);
        builder.AddShort(0);
        builder.AddShort(0);
        builder.AddShort(0);

        // equipment

        builder.AddShort(static_cast<int>(this->world->eosbot_config["bot.boots"]));
        builder.AddShort(0);
        builder.AddShort(0);
        builder.AddShort(0);
        builder.AddShort(static_cast<int>(this->world->eosbot_config["bot.armor"]));
        builder.AddShort(0);
        builder.AddShort(static_cast<int>(this->world->eosbot_config["bot.hat"]));
        builder.AddShort(static_cast<int>(this->world->eosbot_config["bot.shield"]));
        builder.AddShort(static_cast<int>(this->world->eosbot_config["bot.weapon"]));
        builder.AddChar(static_cast<int>(this->world->eosbot_config["bot.sitting"]));
        builder.AddChar(static_cast<int>(this->world->eosbot_config["bot.hidden"]));
        builder.AddByte(255);
    }

	UTIL_FOREACH(updatenpcs, npc)
	{
		builder.AddChar(npc->index);
		builder.AddShort(npc->Data().id);
		builder.AddChar(npc->x);
		builder.AddChar(npc->y);
		builder.AddChar(npc->direction);
	}

	builder.AddByte(255);

	UTIL_FOREACH(updateitems, item)
	{
		builder.AddShort(item->uid);
		builder.AddShort(item->id);
		builder.AddChar(item->x);
		builder.AddChar(item->y);
		builder.AddThree(item->amount);
	}

	this->player->Send(builder);
}

void Character::ShowBoard(Board *board)
{
	if (!board)
	{
		board = this->board;
	}

	const int date_res = (this->world->config["BoardDatePosts"]) ? 17 : 0;

	PacketBuilder builder(PACKET_BOARD, PACKET_OPEN, 2 + board->posts.size() * (17 + int(this->world->config["BoardMaxSubjectLength"]) + date_res));
	builder.AddChar(board->id + 1);
	builder.AddChar(board->posts.size());

	int post_count = 0;
	int recent_post_count = 0;

	UTIL_FOREACH(board->posts, post)
	{
		if (post->author == this->player->character->SourceName())
		{
			++post_count;

			if (post->time + static_cast<int>(this->world->config["BoardRecentPostTime"]) > Timer::GetTime())
			{
				++recent_post_count;
			}
		}
	}

	int posts_remaining = std::min(static_cast<int>(this->world->config["BoardMaxUserPosts"]) - post_count, static_cast<int>(this->world->config["BoardMaxUserRecentPosts"]) - recent_post_count);

	UTIL_FOREACH(board->posts, post)
	{
		builder.AddShort(post->id);
		builder.AddByte(255);

		std::string author_extra;

		if (posts_remaining > 0)
		{
			author_extra = " ";
		}

		builder.AddBreakString(post->author + author_extra);

		std::string subject_extra;

		if (this->world->config["BoardDatePosts"])
		{
			subject_extra = " (" + util::timeago(post->time, Timer::GetTime()) + ")";
		}

		builder.AddBreakString(post->subject + subject_extra);
	}

	this->player->Send(builder);
}

std::string Character::PaddedGuildTag()
{
	std::string tag;

	if (this->world->config["ShowLevel"])
	{
		tag = util::to_string(this->level);
		if (tag.length() < 3)
		{
			tag.insert(tag.begin(), 'L');
		}
	}
	else
	{
		tag = this->guild ? this->guild->tag : "";
	}

	for (std::size_t i = tag.length(); i < 3; ++i)
	{
		tag.push_back(' ');
	}

	return tag;
}

int Character::Usage()
{
	return this->usage + (std::time(0) - this->login_time) / 60;
}

short Character::SpawnMap()
{
	return this->world->GetHome(this)->map;
}

unsigned char Character::SpawnX()
{
	return this->world->GetHome(this)->x;
}

unsigned char Character::SpawnY()
{
	return this->world->GetHome(this)->y;
}

void Character::CheckQuestRules()
{
	UTIL_FOREACH(this->quests, q)
	{
		if (!q.second || q.second->GetQuest()->Disabled())
			continue;

		q.second->CheckRules();
	}
}

void Character::CalculateStats(bool trigger_quests)
{
	const ECF_Data& ecf = world->ecf->Get(this->clas);

	this->adj_str = this->str + ecf.str;
	this->adj_intl = this->intl + ecf.intl;
	this->adj_wis = this->wis + ecf.wis;
	this->adj_agi = this->agi + ecf.agi;
	this->adj_con = this->con + ecf.con;
	this->adj_cha = this->cha + ecf.cha;

	this->maxweight = 70;

	this->weight = 0;
	this->maxhp = 0;
	this->maxtp = 0;
	this->mindam = 0;
	this->maxdam = 0;
	this->accuracy = 0;
	this->evade = 0;
	this->armor = 0;
	this->maxsp = 0;

	UTIL_FOREACH(this->inventory, item)
	{
		this->weight += this->world->eif->Get(item.id).weight * item.amount;

		if (this->weight >= 250)
		{
			break;
		}
	}

	if (boosttimer > 0)
    {
        this->adj_str += this->booststr;
        this->adj_intl += this->boostint;
        this->adj_wis += this->boostwis;
        this->adj_agi += this->boostagi;
        this->adj_con += this->boostcon;
        this->adj_cha += this->boostcha;
    }

	UTIL_FOREACH(this->paperdoll, i)
	{
		if (i)
		{
			const EIF_Data& item = this->world->eif->Get(i);

			this->weight += item.weight;
			this->maxhp += item.hp;
			this->maxtp += item.tp;
			this->mindam += item.mindam;
			this->maxdam += item.maxdam;
			this->accuracy += item.accuracy;
			this->evade += item.evade;
			this->armor += item.armor;

			this->adj_str += item.str;
			this->adj_intl += item.intl;
			this->adj_wis += item.wis;
			this->adj_agi += item.agi;
			this->adj_con += item.con;
			this->adj_cha += item.cha;
		}
	}

	if (this->weight < 0 || this->weight > 250)
	{
		this->weight = 250;
	}

	std::unordered_map<std::string, double> formula_vars;

	this->FormulaVars(formula_vars);

	this->maxhp += rpn_eval(rpn_parse(this->world->formulas_config["hp"]), formula_vars);
	this->maxtp += rpn_eval(rpn_parse(this->world->formulas_config["tp"]), formula_vars);
	this->maxsp += rpn_eval(rpn_parse(this->world->formulas_config["sp"]), formula_vars);

	this->maxweight = rpn_eval(rpn_parse(this->world->formulas_config["weight"]), formula_vars);

	if (this->hp > this->maxhp || this->tp > this->maxtp)
	{
		this->hp = std::min(this->hp, this->maxhp);
		this->tp = std::min(this->tp, this->maxtp);

		PacketBuilder builder(PACKET_RECOVER, PACKET_PLAYER, 6);
		builder.AddShort(this->hp);
		builder.AddShort(this->tp);
		builder.AddShort(0);
		this->Send(builder);
	}

	if (this->SourceDutyAccess() >= static_cast<int>(world->admin_config["unlimitedweight"]))
	{
	    this->weight = 0;
		this->maxweight = 251;
	}
	else
    {
        if (this->maxweight < 70 || this->maxweight > 250)
        {
            this->maxweight = 250;
        }
    }

	if (this->world->config["UseClassFormulas"])
	{
		auto damage = rpn_eval(rpn_parse(this->world->formulas_config["class." + util::to_string(ecf.type) + ".damage"]), formula_vars);

		this->mindam += damage;
		this->maxdam += damage;

		this->armor += rpn_eval(rpn_parse(this->world->formulas_config["class." + util::to_string(ecf.type) + ".defence"]), formula_vars);
		this->accuracy += rpn_eval(rpn_parse(this->world->formulas_config["class." + util::to_string(ecf.type) + ".accuracy"]), formula_vars);
		this->evade += rpn_eval(rpn_parse(this->world->formulas_config["class." + util::to_string(ecf.type) + ".evade"]), formula_vars);
	}
	else
	{
		this->mindam += this->adj_str / 2;
		this->maxdam += this->adj_str / 2;
		this->accuracy += this->adj_agi / 2;
		this->evade += this->adj_agi / 2;
		this->armor += this->adj_con / 2;
	}

    this->mindam += int(this->world->config["BaseMinDamage"]);
    this->maxdam += int(this->world->config["BaseMaxDamage"]);

    if (this->paperdoll[Character::Shield] == int(this->world->equipment_config["BagID"]))
        this->maxweight += int(this->world->equipment_config["BagWeight"]);

	if (trigger_quests)
	{
	    this->CheckQuestRules();
	}

	if (this->party)
	{
		this->party->UpdateHP(this);
	}
}

void Character::DropAll(Character *killer)
{
    if (!CanInteractItems()) return;

	std::list<Character_Item>::iterator it = this->inventory.begin();

	while (it != this->inventory.end())
	{
		if (this->world->eif->Get(it->id).special == EIF::Lore)
		{
			++it;
			continue;
		}

		std::shared_ptr<Map_Item> map_item = this->player->character->map->AddItem(it->id, it->amount, this->x, this->y, 0);

		if (map_item)
		{
			if (killer)
			{
				map_item->owner = killer->player->id;
				map_item->unprotecttime = Timer::GetTime() + static_cast<double>(this->world->config["ProtectPKDrop"]);
			}
			else
			{
				map_item->owner = this->player->id;
				map_item->unprotecttime = Timer::GetTime() + static_cast<double>(this->world->config["ProtectDeathDrop"]);
			}

			PacketBuilder builder(PACKET_ITEM, PACKET_DROP, 15);
			builder.AddShort(it->id);
			builder.AddThree(it->amount);
			builder.AddInt(0);
			builder.AddShort(map_item->uid);
			builder.AddChar(this->x);
			builder.AddChar(this->y);
			builder.AddChar(this->weight);
			builder.AddChar(this->maxweight);
			this->player->Send(builder);
		}

		it = this->inventory.erase(it);
	}

	this->CalculateStats();

    for (std::size_t i = 0; i < this->paperdoll.size(); ++i)
    {
        if (this->paperdoll[i] != 0)
        {
            int itemid = this->paperdoll[i];
            int subloc = ((i == Character::Ring2 || i == Character::Armlet2 || i == Character::Bracer2) ? 1 : 0);

            if (itemid == 0 || this->world->eif->Get(itemid).special == EIF::Lore || this->world->eif->Get(itemid).special == EIF::Cursed)
            {
                ++i;
                continue;
            }

            std::shared_ptr<Map_Item> map_item = this->player->character->map->AddItem(itemid, 1, this->x, this->y, 0);

            if (map_item)
            {
                    if (killer)
                    {
                        map_item->owner = killer->player->id;
                        map_item->unprotecttime = Timer::GetTime() + static_cast<double>(this->world->config["ProtectPKDrop"]);
                    }
                    else
                    {
                        map_item->owner = this->player->id;
                        map_item->unprotecttime = Timer::GetTime() + static_cast<double>(this->world->config["ProtectDeathDrop"]);
                    }

                if (this->Unequip(itemid, subloc))
                {
                    PacketBuilder builder(PACKET_PAPERDOLL, PACKET_REMOVE, 43);
                    builder.AddShort(this->player->id);
                    builder.AddChar(SLOT_CLOTHES);
                    builder.AddChar(0);
                    this->AddPaperdollData(builder, "BAHWS");
                    builder.AddShort(itemid);
                    builder.AddChar(subloc);
                    builder.AddShort(this->maxhp);
                    builder.AddShort(this->maxtp);
                    builder.AddShort(this->display_str);
                    builder.AddShort(this->display_intl);
                    builder.AddShort(this->display_wis);
                    builder.AddShort(this->display_agi);
                    builder.AddShort(this->display_con);
                    builder.AddShort(this->display_cha);
                    builder.AddShort(this->mindam);
                    builder.AddShort(this->maxdam);
                    builder.AddShort(this->accuracy);
                    builder.AddShort(this->evade);
                    builder.AddShort(this->armor);
                    this->Send(builder);
                }

                PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 14);
                builder.AddShort(this->player->id);
                builder.AddChar(SLOT_CLOTHES);
                builder.AddChar(0);
                builder.AddShort(0);
                builder.AddShort(0);
                builder.AddShort(0);
                builder.AddShort(0);
                builder.AddShort(0);

                UTIL_FOREACH(this->map->characters, updatecharacter)
                {
                    if (updatecharacter == this || !this->InRange(updatecharacter))
                    {
                        continue;
                    }

                    updatecharacter->Send(builder);
                }

                this->player->character->DelItem(itemid, 1);

                PacketBuilder reply(PACKET_ITEM, PACKET_REPLY, 3);
                reply.AddChar(EIF::Teleport);
                reply.AddShort(itemid);
                reply.AddInt(this->HasItem(itemid));
                reply.AddChar(this->weight);
                reply.AddChar(this->maxweight);
                this->player->Send(reply);
            }
        }
    }

    this->player->character->StatusMsg("You have dropped all your items!");
}

void Character::Hide()
{
    this->hidden = true;

    PacketBuilder builder(PACKET_ADMININTERACT, PACKET_REMOVE);
    builder.AddShort(this->player->id);

    UTIL_FOREACH(this->map->characters, character)
    {
        if (character->admin != 0)
            character->Send(builder);

        if (character->admin == 0)
        {
            PacketBuilder builder(PACKET_AVATAR, PACKET_REMOVE);
            builder.AddShort(this->player->id);
            character->Send(builder);
        }
    }
}

void Character::Unhide()
{
    this->hidden = false;

    PacketBuilder builder(PACKET_ADMININTERACT, PACKET_AGREE);
    builder.AddShort(this->player->id);

    UTIL_FOREACH(this->map->characters, character)
    {
        if (character->admin != 0)
            character->Send(builder);

        if (character->admin == 0)
        {
            PacketBuilder builder(PACKET_PLAYERS, PACKET_AGREE);
            builder.AddByte(255);
            builder.AddBreakString(this->player->character->SourceName());
            builder.AddShort(this->player->id);
            builder.AddShort(this->player->character->mapid);
            builder.AddShort(this->player->character->x);
            builder.AddShort(this->player->character->y);
            builder.AddChar(this->player->character->direction);
            builder.AddChar(6);
            builder.AddString(this->player->character->PaddedGuildTag());
            builder.AddChar(this->player->character->level);
            builder.AddChar(this->player->character->gender);
            builder.AddChar(this->player->character->hairstyle);
            builder.AddChar(this->player->character->haircolor);
            builder.AddChar(this->player->character->race);
            builder.AddShort(this->player->character->maxhp);
            builder.AddShort(this->player->character->hp);
            builder.AddShort(this->player->character->maxtp);
            builder.AddShort(this->player->character->tp);
            builder.AddShort(this->world->eif->Get(this->player->character->paperdoll[Character::Boots]).dollgraphic);
            builder.AddShort(0);
            builder.AddShort(0);
            builder.AddShort(0);
            builder.AddShort(this->world->eif->Get(this->player->character->paperdoll[Character::Armor]).dollgraphic);
            builder.AddShort(0);
            builder.AddShort(this->world->eif->Get(this->player->character->paperdoll[Character::Hat]).dollgraphic);
            builder.AddShort(this->world->eif->Get(this->player->character->paperdoll[Character::Shield]).dollgraphic);
            builder.AddShort(this->world->eif->Get(this->player->character->paperdoll[Character::Weapon]).dollgraphic);
            builder.AddChar(this->player->character->sitting);
            builder.AddChar(this->player->character->hidden);
            builder.AddChar(WARP_ANIMATION_ADMIN);
            builder.AddByte(255);
            builder.AddChar(1);
            character->Send(builder);
        }
    }
}

void Character::Reset()
{
    if (this->boosttimer <= 0)
    {
        this->statpoints += this->str + this->intl + this->wis + this->agi + this->con + this->cha;

        this->str = 0;
        this->intl = 0;
        this->wis = 0;
        this->agi = 0;
        this->con = 0;
        this->cha = 0;

        UTIL_FOREACH(this->spells, spell)
        {
            this->skillpoints += spell.level;
        }

        this->spells.clear();
        this->CancelSpell();

        PacketBuilder reply;
        reply.SetID(PACKET_STATSKILL, PACKET_JUNK);
        reply.AddShort(this->statpoints);
        reply.AddShort(this->skillpoints);
        reply.AddShort(this->hp);
        reply.AddShort(this->maxhp);
        reply.AddShort(this->tp);
        reply.AddShort(this->maxtp);
        reply.AddShort(this->maxsp);
        reply.AddShort(this->str);
        reply.AddShort(this->intl);
        reply.AddShort(this->wis);
        reply.AddShort(this->agi);
        reply.AddShort(this->con);
        reply.AddShort(this->cha);
        reply.AddShort(this->mindam);
        reply.AddShort(this->maxdam);
        reply.AddShort(this->accuracy);
        reply.AddShort(this->evade);
        reply.AddShort(this->armor);
        this->Send(reply);

        this->CalculateStats();
        this->UpdateStats();
    }
    else
    {
        this->ServerMsg("You cannot reset while being boosted.");
    }
}

std::shared_ptr<Quest_Context> Character::GetQuest(short id)
{
	auto it = this->quests.find(id);

	if (it == this->quests.end())
		return std::shared_ptr<Quest_Context>();

	return it->second;
}

void Character::ResetQuest(short id)
{
	this->quests[id].reset();
}

void Character::Mute(std::string by)
{
    this->muted_until = time(0) + int(this->world->config["MuteLength"]);

    PacketBuilder builder;
    builder.SetID(PACKET_TALK, PACKET_SPEC);
    builder.AddString(by);
    this->player->client->Send(builder);
}

void Character::PlaySFX(unsigned char id)
{
    PacketBuilder builder(PACKET_MUSIC, PACKET_PLAYER, 1);
	builder.AddChar(id);
	this->Send(builder);
}

void Character::PlayBard(unsigned char instrument, unsigned char note, bool echo)
{
	PacketBuilder builder(PACKET_JUKEBOX, PACKET_MSG, 5);

	builder.AddShort(this->player->id);
	builder.AddChar(this->direction);
	builder.AddChar(instrument);
	builder.AddChar(note);

	UTIL_FOREACH(this->map->characters, character)
	{
		if (!echo && (character == this || !this->InRange(character)))
		{
			continue;
		}

		character->Send(builder);
	}
}

#define v(x) vars[prefix + #x] = x;
#define vv(x, n) vars[prefix + n] = x;

void Character::FormulaVars(std::unordered_map<std::string, double> &vars, std::string prefix)
{
	v(level) v(exp) v(hp) v(maxhp) v(tp) v(maxtp) v(maxsp)
	v(weight) v(maxweight) v(karma) v(mindam) v(maxdam)
	vv(adj_str, "str") vv(adj_intl, "int") vv(adj_wis, "wis") vv(adj_agi, "agi") vv(adj_con, "con") vv(adj_cha, "cha")
	vv(str, "base_str") vv(intl, "base_int") vv(wis, "base_wis") vv(agi, "base_agi") vv(con, "base_con") vv(cha, "base_cha")
	v(display_str) vv(display_intl, "display_intl") v(display_wis) v(display_agi) v(display_con) v(display_cha)
	v(accuracy) v(evade) v(armor) v(admin) v(bot) v(usage)
	vv(clas, "class") v(gender) v(race) v(hairstyle) v(haircolor)
	v(mapid) v(x) v(y) v(direction) v(sitting) v(hidden) v(whispers) v(goldbank)
	v(statpoints) v(skillpoints)
}

#undef vv
#undef v

void Character::Dress(EquipLocation loc, unsigned short gfx_id)
{
	if (gfx_id == 0)
		gfx_id = 65535;

	this->cosmetic_paperdoll[loc] = gfx_id;

	PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 14);
	builder.AddShort(this->player->id);
	builder.AddChar(SLOT_CLOTHES);
	builder.AddChar(0); // sound
	this->AddPaperdollData(builder, "BAHWS");

	UTIL_FOREACH(this->map->characters, updatecharacter)
	{
		if (!this->InRange(updatecharacter))
			continue;

		updatecharacter->Send(builder);
	}
}

void Character::Undress()
{
	for (std::size_t i = 0; i < cosmetic_paperdoll.size(); ++i)
		this->cosmetic_paperdoll[i] = 0;

	PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 14);
	builder.AddShort(this->player->id);
	builder.AddChar(SLOT_CLOTHES);
	builder.AddChar(0); // sound
	this->AddPaperdollData(builder, "BAHWS");

	UTIL_FOREACH(this->map->characters, updatecharacter)
	{
		if (!this->InRange(updatecharacter))
			continue;

		updatecharacter->Send(builder);
	}
}

void Character::Undress(EquipLocation loc)
{
	this->cosmetic_paperdoll[loc] = 0;

	PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 14);
	builder.AddShort(this->player->id);
	builder.AddChar(SLOT_CLOTHES);
	builder.AddChar(0); // sound
	this->AddPaperdollData(builder, "BAHWS");

	UTIL_FOREACH(this->map->characters, updatecharacter)
	{
		if (!this->InRange(updatecharacter))
			continue;

		updatecharacter->Send(builder);
	}
}

void Character::AddPaperdollData(PacketBuilder& builder, const char* format)
{
	const EIF_Data& wep = this->world->eif->Get(this->paperdoll[Character::Weapon]);

	unsigned short boots = this->world->eif->Get(this->paperdoll[Character::Boots]).dollgraphic;
	unsigned short armor = this->world->eif->Get(this->paperdoll[Character::Armor]).dollgraphic;
	unsigned short hat = this->world->eif->Get(this->paperdoll[Character::Hat]).dollgraphic;
	unsigned short weapon = wep.dollgraphic;
	unsigned short shield = this->world->eif->Get(this->paperdoll[Character::Shield]).dollgraphic;

	if (wep.subtype == EIF::TwoHanded && wep.dual_wield_dollgraphic)
		shield = wep.dual_wield_dollgraphic;

	if (this->cosmetic_paperdoll[Character::Boots])  boots = this->cosmetic_paperdoll[Character::Boots];
	if (this->cosmetic_paperdoll[Character::Armor])  armor = this->cosmetic_paperdoll[Character::Armor];
	if (this->cosmetic_paperdoll[Character::Hat])    hat = this->cosmetic_paperdoll[Character::Hat];
	if (this->cosmetic_paperdoll[Character::Weapon]) weapon = this->cosmetic_paperdoll[Character::Weapon];
	if (this->cosmetic_paperdoll[Character::Shield]) shield = this->cosmetic_paperdoll[Character::Shield];

	if (boots == 65535)  boots = 0;
	if (armor == 65535)  armor = 0;
	if (hat == 65535)    hat = 0;
	if (weapon == 65535) weapon = 0;
	if (shield == 65535) shield = 0;

	for (const char* p = format; *p != '\0'; ++p)
	{
		switch (*p)
		{
			case 'B': builder.AddShort(boots); break;
			case 'A': builder.AddShort(armor); break;
			case 'H': builder.AddShort(hat); break;
			case 'W': builder.AddShort(weapon); break;
			case 'S': builder.AddShort(shield); break;
			case '0': builder.AddShort(0); break;
		}
	}
}

void Character::PetTransfer()
{
    if (this->HasPet)
        this->SavePet();

    if (this->HasPet && !this->pettransfer)
    {
        UTIL_FOREACH(this->map->characters, character)
        {
            if (this->InRange(character))
                this->pet->RemoveFromView(character);
        }

        pet->map->npcs.erase(std::remove(pet->map->npcs.begin(), pet->map->npcs.end(), pet),this->map->npcs.end());

        this->HasPet = false;
        this->pettransfer = true;
    }
    else if (this->pettransfer)
    {
        if (pettransfer && !this->nowhere)
        {
            this->SummonPet(this, this->pet->name);
            this->pettransfer = false;
        }
    }
}

void Character::SavePet()
{
    if (HasPet)
    {
        this->world->db.Query("UPDATE `pets` SET `name` = '$', `owner_name` = '$', `npcid` = #, "
        "`rebirth` = #, `level` = #, `exp` = #, `hp` = #, `tp` = #, "
        "`inventory` = '$' WHERE `name` = '$' AND `owner_name` = '$'",

        this->pet->name.c_str(), this->SourceName().c_str(), this->pet->id, this->pet->rebirth, this->pet->level, this->pet->exp, this->pet->hp, this->pet->tp,
        ItemSerialize(this->pet->inventory).c_str(), this->pet->name.c_str(), this->SourceName().c_str());
    }
    else
    {
        Console::Wrn(util::ucfirst(std::string(this->SourceName().c_str())) + " attempted a pet save without a pet spawned.");
    }
}

bool Character::PetNameTaken(Character *owner, std::string petname)
{
    Database_Result res = this->world->db.Query("SELECT `name` FROM `pets` WHERE `name` = '$' AND `owner_name` = '$'", petname.c_str(), owner->SourceName().c_str());

    return res.empty() ? true : false;
}

bool Character::HatchPet(Character *hatcher, int npcid)
{
    ENF_Data& npc = hatcher->world->enf->Get(npcid);

    Database_Result res = this->world->db.Query("SELECT `npcid` FROM `pets` WHERE `owner_name` = '$' AND `npcid` = #", hatcher->SourceName().c_str(), npcid);

    if (res.empty())
    {
        std::string petname = util::lowercase(npc.name);

        for (int i = 2; !hatcher->PetNameTaken(hatcher, petname); i++)
            petname = util::lowercase(npc.name) + "-" + util::to_string(i);

        this->world->db.Query("INSERT INTO `pets` (`name`, `owner_name`, `npcid`, `rebirth`, `level`, `exp`, `hp`, `tp`)"
        "VALUES ('$', '$', #, #, #, #, #, #)", util::lowercase(petname).c_str(), hatcher->SourceName().c_str(), npcid, 0, 0, 0, npc.hp, 0);

        this->ServerMsg(util::ucfirst(petname) + " has successfully hatched.");

        return true;
    }

    return false;
}

bool Character::RevivePets()
{
    Database_Result res = this->world->db.Query("SELECT `npcid`, `hp` FROM `pets` WHERE `owner_name` = '$'", this->SourceName().c_str());

    if (!res.empty())
    {
        bool success = false;

        UTIL_FOREACH_REF(res, pet)
        {
            if (int(pet["hp"]) <= 0)
            {
                this->world->db.Query("UPDATE `pets` SET `hp` = '#' WHERE `npcid` = '#' AND `owner_name` = '$'",
                this->world->enf->Get(int(pet["npcid"])).hp, int(pet["npcid"]), this->SourceName().c_str());

                success = true;
            }
        }

        if (success == true)
            this->StatusMsg("All dead pets have been revived.");
        else if (success == false)
            this->StatusMsg("No pets are dead.");

        return success;
    }

    return false;
}

void Character::SummonPet(Character *owner, std::string petname)
{
    if (this->HasPet)
        this->SavePet();

    Database_Result res = this->world->db.Query("SELECT `name`, `owner_name`, `npcid`, "
    "`rebirth`, `level`, `exp`, `hp`, `tp`, "
    "`inventory` FROM `pets` WHERE `name` = '$' AND `owner_name` = '$'", petname.c_str(), owner->SourceName().c_str());

    if (!res.empty())
    {
        std::unordered_map<std::string, util::variant> row = res.front();

        if (owner->HasPet)
        {
            owner->PetTransfer();
            owner->pettransfer = false;
        }

        unsigned char index = owner->map->GenerateNPCIndex();

        if (index > 250)
            return;

        int petid = GetRow<int>(row, "npcid");

        NPC *npc = new NPC(owner->map, petid, owner->x, owner->y, 1, 1, index, true, true);
        owner->map->npcs.push_back(npc);
        npc->Spawn();

        npc->owner = owner;

        owner->pet = npc;
        owner->HasPet = true;

        owner->pet->name = GetRow<std::string>(row, "name");
        owner->pet->rebirth = GetRow<int>(row, "rebirth");
        owner->pet->level = GetRow<int>(row, "level");
        owner->pet->exp = GetRow<int>(row, "exp");
        owner->pet->hp = GetRow<int>(row, "hp");
        owner->pet->tp = GetRow<int>(row, "tp");
        owner->pet->tp = GetRow<int>(row, "tp");

        owner->pet->inventory = ItemUnserialize(row["inventory"]);

        if (owner->pet->hp <= 0)
        {
            owner->StatusMsg(util::ucfirst(owner->pet->name) + " is dead.");

            owner->PetTransfer();
            owner->pettransfer = false;
        }
        else
        {
            owner->StatusMsg(util::ucfirst(owner->pet->name) + " was summoned.");
        }

        owner->pet->CalculateStats();

        return;
    }
    else
    {
        owner->StatusMsg("You don't own any pets with the provided name.");
        return;
    }
}

bool Character::AddPet(Character *owner, int npcid)
{
    if (this->HasPet)
        this->SavePet();

    Database_Result res = this->world->db.Query("SELECT `name`, `owner_name`, `npcid`, "
    "`rebirth`, `level`, `exp`, `hp`, `tp`, "
    "`inventory` FROM `pets` WHERE `npcid` = '#' AND `owner_name` = '$'", npcid, owner->SourceName().c_str());

    if (!res.empty())
    {
        std::unordered_map<std::string, util::variant> row = res.front();

        if (owner->HasPet)
        {
            owner->PetTransfer();
            owner->pettransfer = false;
        }

        unsigned char index = owner->map->GenerateNPCIndex();

        if (index > 250)
            return false;

        int petid = GetRow<int>(row, "npcid");

        NPC *npc = new NPC(owner->map, petid, owner->x, owner->y, 1, 1, index, true, true);
        owner->map->npcs.push_back(npc);
        npc->Spawn();

        npc->owner = owner;

        owner->pet = npc;
        owner->HasPet = true;

        owner->pet->name = GetRow<std::string>(row, "name");
        owner->pet->rebirth = GetRow<int>(row, "rebirth");
        owner->pet->level = GetRow<int>(row, "level");
        owner->pet->exp = GetRow<int>(row, "exp");
        owner->pet->hp = GetRow<int>(row, "hp");
        owner->pet->tp = GetRow<int>(row, "tp");

        owner->pet->inventory = ItemUnserialize(row["inventory"]);

        if (owner->pet->hp <= 0)
        {
            owner->StatusMsg(util::ucfirst(owner->pet->name) + " is dead.");

            owner->PetTransfer();
            owner->pettransfer = false;
        }
        else
        {
            owner->StatusMsg(util::ucfirst(owner->pet->name) + " was summoned.");
        }

        owner->pet->CalculateStats();

        return true;
    }

    return false;
}

void Character::AddChatLog(std::string marker, std::string name, std::string msg)
{
	if (int(chat_log.size()) >= int(this->world->config["ReportChatLogSize"]))
		chat_log.pop_front();

	chat_log.push_back(marker + " " + util::ucfirst(name) + ": " + msg);
}

std::string Character::GetChatLogDump()
{
	std::string result;

	for (const std::string& line : chat_log)
	{
		result += line;
		result += "\r\n";
	}

	return result;
}

void Character::Send(const PacketBuilder &builder)
{
	this->player->Send(builder);
}

void Character::Logout()
{
	if (!this->online)
		return;

	this->CancelSpell();

	if (this->mapid == int(this->world->partymap_config[util::to_string(this->mapid) + ".map"]))
    {
        world->PartyMaps();

        if (this->party)
        {
            UTIL_FOREACH(this->party->members, member)
            {
                if (this->party->members.size() <= int64_t(int(this->world->partymap_config[util::to_string(this->mapid) + ".partyreq"])))
                    member->ServerMsg("Someone left the party, you have " + util::to_string(double(this->world->partymap_config["KickTimer"])) + " seconds to re-group.");
                else
                    member->ServerMsg("Someone left the party.");
            }
        }

        int kickMap = int(this->world->partymap_config[util::to_string(mapid) + ".kickmap"]);
        int kickX = int(this->world->partymap_config[util::to_string(mapid) + ".kickx"]);
        int kickY = int(this->world->partymap_config[util::to_string(mapid) + ".kicky"]);

        this->Warp(kickMap, kickX, kickY);
    }

	if (this->world->ctf == true)
    {
        Map *maps = this->world->GetMap(int(this->world->ctf_config["CTFMap"]));

        int blueflag = int(this->world->ctf_config["BlueFlag"]);
        int redflag = int(this->world->ctf_config["RedFlag"]);

        int bx = util::to_int(util::explode(',', this->world->ctf_config["BlueXY"])[0]);
        int by = util::to_int(util::explode(',', this->world->ctf_config["BlueXY"])[1]);

        int rx = util::to_int(util::explode(',', this->world->ctf_config["RedXY"])[0]);
        int ry = util::to_int(util::explode(',', this->world->ctf_config["RedXY"])[1]);

        if (this->bluehost == true && this->mapid == int(world->ctf_config["CTFMap"]) && this->world->atbluebase == false)
        {
            maps->AddItem(blueflag, 1, bx, by, this);

            UTIL_FOREACH(maps->characters, checkchar)
            {
                checkchar->Refresh();
            }

            if (this->HasItem(blueflag))
                this->RemoveItem(blueflag, 1);

            this->world->atbluebase = true;

            this->world->ServerMsg("The blue team's flag has been recovered!");
        }

        if (this->redhost == true && this->mapid == int(world->ctf_config["CTFMap"]) && this->world->atredbase == false)
        {
            maps->AddItem(redflag, 1, rx, ry, this);

            UTIL_FOREACH(maps->characters, checkchar)
            {
                checkchar->Refresh();
            }

            if (this->HasItem(redflag))
                this->RemoveItem(redflag, 1);

            this->world->atredbase = true;
            this->world->ServerMsg("The red team's flag has been recovered!");
        }
    }

    if (this->mapid == int(this->world->pvp_config["PVPMap"]))
    {
        if (this->oldmap == int(this->world->pvp_config["PVPMap"]))
            this->Warp(this->SpawnMap(), this->SpawnX(), this->SpawnY(), world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
        else if (this->oldmap != int(this->world->pvp_config["PVPMap"]))
            this->Warp(this->oldmap,this->oldx,this->oldy, world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
    }

	if (this->trading)
	{
		PacketBuilder builder(PACKET_TRADE, PACKET_CLOSE, 2);
		builder.AddShort(this->player->id);
		this->trade_partner->Send(builder);

		this->player->client->state = EOClient::Playing;
		this->trading = false;
		this->trade_inventory.clear();
		this->trade_agree = false;

		this->trade_partner->player->client->state = EOClient::Playing;
		this->trade_partner->trading = false;
		this->trade_partner->trade_inventory.clear();
		this->trade_agree = false;

		this->CheckQuestRules();
		this->trade_partner->CheckQuestRules();

		this->trade_partner->trade_partner = 0;
		this->trade_partner = 0;
	}

	if (this->party)
		this->party->Leave(this);

	if (this->arena)
		--this->arena->occupants;

	UTIL_FOREACH(this->unregister_npc, npc)
	{
		UTIL_IFOREACH(npc->damagelist, it)
		{
			if ((*it)->attacker == this)
			{
				npc->totaldamage -= (*it)->damage;
				npc->damagelist.erase(it);
				break;
			}
		}
	}

	UTIL_FOREACH(this->world->maps, map)
    {
        UTIL_FOREACH(map->npcs, npc)
        {
            if (npc->killowner)
            {
                if (npc->killowner == this)
                {
                    npc->killowner = 0;
                }
            }
        }
    }

    if (this->HasPet == true)
        this->PetTransfer();

	this->online = false;
	this->Save();
	this->world->Logout(this);
}

void Character::GiveRebirth(short rebirth)
{
    this->rebirth += rebirth;
    this->level = 0;
    this->exp = 0;

    if (!this->HasAchievement(std::string(this->world->achievements_config["ReachRebirth.Achievement." + util::to_string(this->rebirth)])))
    {
        if (std::string(this->world->achievements_config["ReachRebirth.Achievement." + util::to_string(this->rebirth)]) != "0")
        {
            this->GiveEXP(std::min(std::max(int(this->world->achievements_config["ReachRebirth.Reward." + util::to_string(this->rebirth)]), 0), int(this->world->config["MaxExp"])));

            if (std::string(this->world->achievements_config["ReachRebirth.Title." + util::to_string(this->rebirth)]) != "0")
                this->title = std::string(this->world->achievements_config["ReachRebirth.Title." + util::to_string(this->rebirth)]);

            if (std::string(this->world->achievements_config["ReachRebirth.Description." + util::to_string(this->rebirth)]) != "0")
                this->ServerMsg(std::string(this->world->achievements_config["ReachRebirth.Description." + util::to_string(this->rebirth)]));

            if (std::string(this->world->achievements_config["ReachRebirth.Announcement." + util::to_string(this->rebirth)]) == "yes")
                this->world->ServerMsg(util::ucfirst(this->SourceName()) + " earned the achievement '" + std::string(this->world->achievements_config["ReachRebirth.Achievement." + util::to_string(this->rebirth)]) + "'.");

            this->NewAchievement(std::string(this->world->achievements_config["ReachRebirth.Achievement." + util::to_string(this->rebirth)]));
        }
    }
}

void Character::GiveEXP(int exp)
{
    this->exp += exp;

    PacketBuilder builder;
    builder.SetID(PACKET_PARTY, PACKET_TARGET_GROUP);
    builder.AddShort(this->player->id);
    builder.AddInt(exp);
    this->Send(builder);

    this->CalculateTNL();
}

void Character::GiveItem(short item, int amount)
{
    this->AddItem(item, amount);

    PacketBuilder builder(PACKET_ITEM, PACKET_GET);
    builder.AddShort(0);
    builder.AddShort(item);
    builder.AddThree(amount);
    builder.AddChar(this->weight);
    builder.AddChar(this->maxweight);
    this->Send(builder);
}

void Character::RemoveItem(short item, int amount)
{
    this->DelItem(item, amount);

    PacketBuilder reply;
    reply.SetID(PACKET_ITEM, PACKET_JUNK);
    reply.AddShort(item);
    reply.AddThree(amount);
    reply.AddInt(this->HasItem(item));
    reply.AddChar(this->weight);
    reply.AddChar(this->maxweight);
    this->Send(reply);
}

void Character::CalculateTNL()
{
    bool level_up = false;

    while (this->level < static_cast<int>(this->world->config["MaxLevel"]) && this->exp >= this->world->exp_table[this->level+1])
    {
        level_up = true;
        ++this->level;

        this->statpoints += static_cast<int>(this->world->config["StatPerLevel"]);
        this->skillpoints += static_cast<int>(this->world->config["SkillPerLevel"]);

        this->CalculateStats();
    }

    PacketBuilder builder(PACKET_RECOVER, PACKET_REPLY);
    builder.AddInt(this->exp);
    builder.AddShort(this->karma);
    builder.AddChar(level_up ? this->level : 0);
    this->Send(builder);

    if (level_up)
    {
        this->Emote(EMOTE_LEVELUP, false);

        if (this->party)
        {
            UTIL_FOREACH(this->party->members, member)
            {
                this->party->RefreshMembers(member->player->character);
            }
        }

        this->UpdateStats();

        if (!this->HasAchievement(std::string(this->map->world->achievements_config["ReachLevel.Achievement." + util::to_string(this->level)])))
        {
            if (std::string(this->map->world->achievements_config["ReachLevel.Achievement." + util::to_string(this->level)]) != "0")
            {
                this->GiveEXP(std::min(std::max(int(this->map->world->achievements_config["ReachLevel.Reward." + util::to_string(this->level)]), 0), int(this->world->config["MaxExp"])));

                if (std::string(this->world->achievements_config["ReachLevel.Title." + util::to_string(this->level)]) != "0")
                    this->title = std::string(this->world->achievements_config["ReachLevel.Title." + util::to_string(this->level)]);

                if (std::string(this->map->world->achievements_config["ReachLevel.Description." + util::to_string(this->level)]) != "0")
                    this->ServerMsg(std::string(this->map->world->achievements_config["ReachLevel.Description." + util::to_string(this->level)]));

                if (std::string(this->world->achievements_config["ReachLevel.Announcement." + util::to_string(this->level)]) == "yes")
                    this->world->ServerMsg(util::ucfirst(this->SourceName()) + " earned the achievement '" + std::string(this->world->achievements_config["ReachLevel.Achievement." + util::to_string(this->level)]) + "'.");

                this->NewAchievement(std::string(this->map->world->achievements_config["ReachLevel.Achievement." + util::to_string(this->level)]));
            }
        }
    }
}

void Character::PacketRecover()
{
    PacketBuilder builder(PACKET_RECOVER, PACKET_PLAYER);
    builder.AddShort(this->hp);
    builder.AddShort(this->tp);
    builder.AddShort(0);
    this->Send(builder);
}

void Character::UpdateStats()
{
    PacketBuilder builder(PACKET_STATSKILL, PACKET_PLAYER, 32);
    builder.AddShort(this->statpoints);
    builder.AddShort(this->display_str);
    builder.AddShort(this->display_intl);
    builder.AddShort(this->display_wis);
    builder.AddShort(this->display_agi);
    builder.AddShort(this->display_con);
    builder.AddShort(this->display_cha);
    builder.AddShort(this->maxhp);
    builder.AddShort(this->maxtp);
    builder.AddShort(this->maxsp);
    builder.AddShort(this->maxweight);
    builder.AddShort(this->mindam);
    builder.AddShort(this->maxdam);
    builder.AddShort(this->accuracy);
    builder.AddShort(this->evade);
    builder.AddShort(this->armor);
    this->Send(builder);
}

void Character::SetClass(int id)
{
    this->clas = id;

    PacketBuilder builder(PACKET_RECOVER, PACKET_LIST, 32);
    builder.AddShort(this->clas);
    builder.AddShort(this->display_str);
    builder.AddShort(this->display_intl);
    builder.AddShort(this->display_wis);
    builder.AddShort(this->display_agi);
    builder.AddShort(this->display_con);
    builder.AddShort(this->display_cha);
    builder.AddShort(this->maxhp);
    builder.AddShort(this->maxtp);
    builder.AddShort(this->maxsp);
    builder.AddShort(this->maxweight);
    builder.AddShort(this->mindam);
    builder.AddShort(this->maxdam);
    builder.AddShort(this->accuracy);
    builder.AddShort(this->evade);
    builder.AddShort(this->armor);
    this->Send(builder);

    this->CalculateStats();
    this->UpdateStats();
}

void Character::UndoBuff()
{
    if (this->boosttimer > 0)
    {
        if (this->booststr > 0) this->booststr = 0;
        if (this->boostint > 0) this->boostint = 0;
        if (this->boostwis > 0) this->boostwis = 0;
        if (this->boostagi > 0) this->boostagi = 0;
        if (this->boostcon > 0) this->boostcon = 0;
        if (this->boostcha > 0) this->boostcha = 0;
        if (this->boostexp > 0) this->boostexp = 0;
        if (this->boostdrop > 0) this->boostdrop = 0;

        this->CalculateStats();
        this->PacketRecover();
        this->UpdateStats();

        this->boosttimer = 0;
        this->boosteffect = 0;

        this->ServerMsg("You are no longer boosted!");
    }
    else
    {
        this->ServerMsg("You are not boosted!");
    }
}

void Character::ResetTransfer()
{
    this->transfer_timer = 0;
    this->transfer_petid = 0;

    this->transfer_cost = "";
    this->transfer_petname = "";
    this->transfer_target = "";
    this->transfer_requester = "";

    this->transfer_pending = false;
}

std::string Character::ReplaceStrings(Character *from, std::string message)
{
    for (int position = message.find("[name]"); int(message.find("[name]")) != -1; position = message.find("[name]"))
        message.replace(position, 6, util::ucfirst(from->SourceName()));
    for (int position = message.find("[level]"); int(message.find("[level]")) != -1; position = message.find("[level]"))
        message.replace(position, 7, util::to_string(from->level));
    for (int position = message.find("[rebirth]"); int(message.find("[rebirth]")) != -1; position = message.find("[rebirth]"))
        message.replace(position, 9, util::to_string(from->rebirth));

    return message;
}

void Character::Save()
{
    const std::string & quest_data = (!this->quest_string.empty()) ? this->quest_string : QuestSerialize(this->quests, this->quests_inactive);

    int nointeract = this->nointeract;

    if (!(nointeract & NoInteractCustom))
        nointeract = 0;

    #ifdef DEBUG
	Console::Dbg("Saving character '%s' (session lasted %i minutes)", this->real_name.c_str(), int(std::time(0) - this->login_time) / 60);
    #endif

	this->world->db.Query("UPDATE `characters` SET `title` = '$', `guild_rankname` = '$', `home` = '$', `fiance` = '$', `partner` = '$', `admin` = #, `class` = #, `rebirth` = #, `immune` = #, `bounty` = #, `member` = #, `gender` = #, `race` = #, "
    "`oldhairstyle` = #, `hairstyle` = #, `haircolor` = #, `map` = #, `x` = #, `y` = #, `oldmap` = #, `oldx` = #, `oldy` = #, `direction` = #, `level` = #, `exp` = #, `flevel` = #, `fexp` = #, `mlevel` = #, `mexp` = #, "
    "`wlevel` = #, `wexp` = #, `clevel` = #, `cexp` = #, `lockerpin` = #, `hp` = #, `tp` = #, `str` = #, `int` = #, `wis` = #, `agi` = #, `con` = #, `cha` = #, `statpoints` = #, `skillpoints` = #, `karma` = #, `sitting` = #, "
    "`nointeract` = #, `bankmax` = #, `goldbank` = #, `usage` = #, `warn` = #, `inventory` = '$', `bank` = '$', `paperdoll` = '$', "
    "`spells` = '$', `guild` = '$', guild_rank = #, `quest` = '$', `achievements` = '$', `vars` = '$' WHERE `name` = '$'",

    this->title.c_str(), this->guild_rankname.c_str(), this->home.c_str(), this->fiance.c_str(), this->partner.c_str(), int(this->admin), this->clas, this->rebirth, this->immune, this->bounty, this->member, int(this->gender), int(this->race),
    this->oldhairstyle, this->hairstyle, this->haircolor, this->mapid, this->x, this->y, this->oldmap, this->oldx, this->oldy, int(this->direction), this->level, this->exp, this->flevel, this->fexp, this->mlevel, this->mexp,
    this->wlevel, this->wexp, this->clevel, this->cexp, this->lockerpin, this->hp, this->tp, this->str, this->intl, this->wis, this->agi, this->con, this->cha, this->statpoints, this->skillpoints, this->karma, int(this->sitting),
    nointeract, this->bankmax, this->goldbank, this->Usage(), this->warn, ItemSerialize(this->inventory).c_str(), ItemSerialize(this->bank).c_str(),
    DollSerialize(this->paperdoll).c_str(), SpellSerialize(this->spells).c_str(), (this->guild ? this->guild->tag.c_str() : ""),
    this->guild_rank, quest_data.c_str(), AchievementsSerialize(this->achievements).c_str(), "", this->real_name.c_str());
}

AdminLevel Character::SourceAccess() const
{
    return world->config["UseDutyAdmin"] ? player->Admin() : admin;
}

AdminLevel Character::SourceDutyAccess() const
{
	return admin;
}

std::string Character::SourceName() const
{
	return faux_name.empty() ? real_name : faux_name;
}

Character* Character::SourceCharacter()
{
	return this;
}

World* Character::SourceWorld()
{
	return this->world;
}

Character::~Character()
{
	this->Logout();
}
