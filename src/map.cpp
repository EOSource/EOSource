#include "map.hpp"

#include <algorithm>
#include <functional>
#include <set>

#include "console.hpp"
#include "timer.hpp"
#include "util.hpp"
#include "util/rpn.hpp"

#include "arena.hpp"
#include "character.hpp"
#include "eoclient.hpp"
#include "eodata.hpp"
#include "eoserver.hpp"
#include "npc.hpp"
#include "packet.hpp"
#include "party.hpp"
#include "player.hpp"
#include "quest.hpp"
#include "world.hpp"

static const char *safe_fail_filename;

static void safe_fail(int line)
{
	Console::Err("Invalid file / failed read/seek: %s -- %i", safe_fail_filename, line);
}

#define SAFE_SEEK(fh, offset, from) if (std::fseek(fh, offset, from) != 0) { std::fclose(fh); safe_fail(__LINE__); return false; }
#define SAFE_READ(buf, size, count, fh) if (std::fread(buf, size, count, fh) != static_cast<int>(count)) {  std::fclose(fh); safe_fail(__LINE__);return false; }

void map_timed_spikes(void *map_void)
{
    Map *map(static_cast<Map *>(map_void));

    std::vector<Character *> dcheck_chars;

    UTIL_FOREACH(map->characters, character)
    {
        if (map->GetSpec(character->x, character->y) == Map_Tile::Spikes1)
        {
            int amount = util::rand(int(map->world->config["TimedSpikeMinDamage"]), int(map->world->config["TimedSpikeMaxDamage"]));

            int limitamount = std::min(amount, int(character->hp));

            if (map->world->config["LimitDamage"])
                amount = limitamount;

            if (character->immune == false)
                character->hp -= limitamount;

            PacketBuilder builder(PACKET_EFFECT, PACKET_SPEC, 7);
            builder.AddChar(2);
            builder.AddShort(character->immune == false ? amount : 0);
            builder.AddShort(character->hp);
            builder.AddShort(character->maxhp);
            character->Send(builder);

            builder.Reset(10);
            builder.SetID(PACKET_AVATAR, PACKET_REPLY);
            builder.AddShort(0);
            builder.AddShort(character->player->id);
            builder.AddThree(character->immune == false ? amount : 0);
            builder.AddChar(character->direction);
            builder.AddChar(int(double(character->hp) / double(character->maxhp) * 100.0));
            builder.AddChar(character->hp == 0);

            UTIL_FOREACH(map->characters, mapchar)
            {
                if (!mapchar->InRange(character) || mapchar == character)
                    continue;

                if (!character->hidden)
                    mapchar->Send(builder);
                else if ((character->hidden && mapchar->admin >= static_cast<int>(map->world->admin_config["seehide"])) || character == mapchar)
                    mapchar->Send(builder);
            }

            dcheck_chars.push_back(character);

            continue;
        }

        PacketBuilder builder(PACKET_EFFECT, PACKET_REPORT, 1);
        builder.AddChar(0);
        character->Send(builder);

        if (character->party)
            character->party->UpdateHP(character);
    }

    UTIL_FOREACH(dcheck_chars, character)
    {
        if (character->hp == 0)
            character->DeathRespawn();
    }
}

void map_spawn_chests(void *map_void)
{
	Map *map(static_cast<Map *>(map_void));

	double current_time = Timer::GetTime();
	UTIL_FOREACH(map->chests, chest)
	{
		bool needs_update = false;

		std::vector<std::list<Map_Chest_Spawn>> spawns;
		spawns.resize(chest->slots);

		UTIL_FOREACH(chest->spawns, spawn)
		{
			if (spawn.last_taken + spawn.time*60.0 < current_time)
			{
				bool slot_used = false;

				UTIL_FOREACH(chest->items, item)
				{
					if (item.slot == spawn.slot)
					{
						slot_used = true;
					}
				}

				if (!slot_used)
				{
					spawns[spawn.slot - 1].emplace_back(spawn);
				}
			}
		}

		UTIL_FOREACH(spawns, slot_spawns)
		{
			if (!slot_spawns.empty())
			{
				const Map_Chest_Spawn& spawn = *std::next(slot_spawns.cbegin(), util::rand(0, slot_spawns.size() - 1));

				chest->AddItem(spawn.item.id, spawn.item.amount, spawn.slot);
				needs_update = true;

                #ifdef DEBUG
				Console::Dbg("Spawning chest item %i (x%i) on map %i", spawn.item.id, spawn.item.amount, map->id);
                #endif
			}
		}

		if (needs_update)
		{
			chest->Update(map, 0);
		}
	}
}

struct map_close_door_struct
{
	Map *map;
	unsigned char x, y;
};

void map_close_door(void *map_close_void)
{
	map_close_door_struct *close(static_cast<map_close_door_struct *>(map_close_void));

	close->map->CloseDoor(close->x, close->y);
}

struct map_evacuate_struct
{
	Map *map;
	int step;
};

void map_evacuate(void *map_evacuate_void)
{
	map_evacuate_struct *evac(static_cast<map_evacuate_struct *>(map_evacuate_void));

	int ticks = int(evac->map->world->config["EvacuateStep"]) / int(evac->map->world->config["EvacuateTick"]);

	if (evac->step > 0)
	{
		bool step = evac->step % ticks == 0;

		UTIL_FOREACH(evac->map->characters, character)
		{
			if (step)
				character->ServerMsg(character->world->i18n.Format("Evacuate", (evac->step / ticks) * int(evac->map->world->config["EvacuateStep"])));

			character->PlaySFX(int(evac->map->world->config["EvacuateSound"]));
		}

		--evac->step;
	}
	else
	{
		restart_loop:

		UTIL_FOREACH(evac->map->characters, character)
		{
			if (character->SourceAccess() < ADMIN_GUIDE)
			{
				character->world->Jail(0, character, false);
				goto restart_loop;
			}
		}

		evac->map->evacuate_lock = false;
	}
}

int Map_Chest::HasItem(short item_id) const
{
	UTIL_FOREACH_REF(this->items, item)
	{
		if (item.id == item_id)
		{
			return item.amount;
		}
	}

	return 0;
}

int Map_Chest::AddItem(short item_id, int amount, int slot)
{
	if (amount <= 0)
	{
		return 0;
	}

	if (slot == 0)
	{
		UTIL_FOREACH_REF(this->items, item)
		{
			if (item.id == item_id)
			{
				if (item.amount + amount < 0 || item.amount + amount > this->maxchest)
				{
					return 0;
				}

				item.amount += amount;
				return amount;
			}
		}
	}

	if (this->items.size() >= static_cast<std::size_t>(this->chestslots) || amount > this->maxchest)
	{
		return 0;
	}

	if (slot == 0)
	{
		int user_items = 0;

		UTIL_FOREACH(this->items, item)
		{
			if (item.slot == 0)
			{
				++user_items;
			}
		}

		if (user_items + this->slots >= this->chestslots)
		{
			return 0;
		}
	}

	Map_Chest_Item chestitem;
	chestitem.id = item_id;
	chestitem.amount = amount;
	chestitem.slot = slot;

	if (slot == 0)
	{
		this->items.push_back(chestitem);
	}
	else
	{
		this->items.push_front(chestitem);
	}

	return amount;
}

int Map_Chest::DelItem(short item_id)
{
	UTIL_IFOREACH(this->items, it)
	{
		if (it->id == item_id)
		{
			int amount = it->amount;

			if (it->slot)
			{
				double current_time = Timer::GetTime();

				UTIL_FOREACH_REF(this->spawns, spawn)
				{
					if (spawn.slot == it->slot)
					{
						spawn.last_taken = current_time;
					}
				}
			}

			this->items.erase(it);
			return amount;
		}
	}

	return 0;
}

int Map_Chest::DelSomeItem(short item_id, int amount)
{
	UTIL_IFOREACH(this->items, it)
	{
		if (it->id == item_id)
		{
			if (amount < it->amount)
			{
				it->amount -= amount;

				if (it->slot)
				{
					double current_time = Timer::GetTime();

					UTIL_FOREACH_REF(this->spawns, spawn)
					{
						if (spawn.slot == it->slot)
						{
							spawn.last_taken = current_time;
						}
					}

					it->slot = 0;
				}

				return it->amount;
			}
			else
			{
				return DelItem(item_id);
			}

			break;
		}
	}

	return 0;
}

void Map_Chest::Update(Map *map, Character *exclude) const
{
	PacketBuilder builder(PACKET_CHEST, PACKET_AGREE, this->items.size() * 5);

	UTIL_FOREACH(this->items, item)
	{
		builder.AddShort(item.id);
		builder.AddThree(item.amount);
	}

	UTIL_FOREACH(map->characters, character)
	{
		if (character == exclude)
		{
			continue;
		}

		if (util::path_length(character->x, character->y, this->x, this->y) <= 1)
		{
			character->Send(builder);
		}
	}
}

Map::Map(int id, World *world)
{
	this->id = id;
	this->world = world;
	this->exists = false;
	this->jukebox_protect = 0.0;
	this->arena = 0;
	this->evacuate_lock = false;

	this->LoadArena();

	this->Load();

	if (!this->chests.empty())
	{
		TimeEvent *event = new TimeEvent(map_spawn_chests, this, 60.0, Timer::FOREVER);
		this->world->timer.Register(event);
	}
}

void Map::LoadArena()
{
	std::list<Character *> update_characters;

	if (this->arena)
	{
		UTIL_FOREACH(this->arena->map->characters, character)
		{
			if (character->arena == this->arena)
			{
				update_characters.push_back(character);
			}
		}

		delete this->arena;
	}

	if (world->arenas_config[util::to_string(id) + ".enabled"])
	{
		std::string spawns_str = world->arenas_config[util::to_string(id) + ".spawns"];

		std::vector<std::string> spawns = util::explode(',', spawns_str);

		if (spawns.size() % 4 != 0)
		{
			Console::Wrn("Invalid arena spawn data for map %i", id);
			this->arena = 0;
		}
		else
		{
			this->arena = new Arena(this, static_cast<int>(world->arenas_config[util::to_string(id) + ".time"]), static_cast<int>(world->arenas_config[util::to_string(id) + ".block"]));
			this->arena->occupants = update_characters.size();

			int i = 1;
			Arena_Spawn *s = 0;
			UTIL_FOREACH(spawns, spawn)
			{
				util::trim(spawn);

				switch (i++ % 4)
				{
					case 1:
						s = new Arena_Spawn;
						s->sx = util::to_int(spawn);
						break;

					case 2:
						s->sy = util::to_int(spawn);
						break;

					case 3:
						s->dx = util::to_int(spawn);
						break;

					case 0:
						s->dy = util::to_int(spawn);
						this->arena->spawns.push_back(s);
						s = 0;
						break;
				}
			}

			if (s)
			{
				Console::Wrn("Invalid arena spawn string");
				delete s;
			}
		}
	}
	else
	{
		this->arena = 0;
	}

	UTIL_FOREACH(update_characters, character)
	{
		character->arena = this->arena;

		if (!this->arena)
		{
			character->Warp(character->map->id, character->map->relog_x, character->map->relog_y);
		}
	}
}

std::string Map::DecodeEMFString(std::string chars)
{
    reverse(chars.begin(), chars.end());

    bool flippy = (chars.length() % 2) == 1;

    for (std::size_t i = 0; i < chars.length(); ++i)
    {
        unsigned char c = chars[i];

        if (flippy)
        {
            if (c >= 0x22 && c <= 0x4F)
                c = (unsigned char)(0x71 - c);
            else if (c >= 0x50 && c <= 0x7E)
                c = (unsigned char)(0xCD - c);
        }
        else
        {
            if (c >= 0x22 && c <= 0x7E)
                c = (unsigned char)(0x9F - c);
        }

        chars[i] = c;
        flippy = !flippy;
    }

    return chars;
}

bool Map::Load()
{
	char namebuf[6];

	if (this->id < 0)
	{
		return false;
	}

	std::string filename = this->world->config["MapDir"];
	std::sprintf(namebuf, "%05i", this->id);
	filename.append(namebuf);
	filename.append(".emf");

	safe_fail_filename = filename.c_str();

	std::FILE *fh = std::fopen(filename.c_str(), "rb");

	if (!fh)
	{
		Console::Err("Could not load file: %s", filename.c_str());
		return false;
	}

    SAFE_SEEK(fh, 0x03, SEEK_SET);
    SAFE_READ(this->rid, sizeof(char), 4, fh);

    char buf[12];
    char tempname[25];

    int outersize;
    int innersize;

    SAFE_SEEK(fh, 0x7, SEEK_SET);
    SAFE_READ(tempname, sizeof(char), 24, fh);

    tempname[24] = '\0';

    this->name = this->DecodeEMFString(std::string(tempname));
    this->name = this->name.substr(0, this->name.find('ÿ'));

    if (this->name.empty()) this->name = "???";

	SAFE_SEEK(fh, 0x1F, SEEK_SET);
	SAFE_READ(buf, sizeof(char), 1, fh);
	this->pk = PacketProcessor::Number(buf[0]) == 3;

	SAFE_SEEK(fh, 0x25, SEEK_SET);
	SAFE_READ(buf, sizeof(char), 2, fh);
	this->width = PacketProcessor::Number(buf[0]) + 1;
	this->height = PacketProcessor::Number(buf[1]) + 1;

	SAFE_SEEK(fh, 0x20, SEEK_SET);
    SAFE_READ(buf, sizeof(char), 1, fh);
    this->effect = PacketProcessor::Number(buf[0]);

	this->tiles.resize(this->height * this->width);

	SAFE_SEEK(fh, 0x2A, SEEK_SET);
	SAFE_READ(buf, sizeof(char), 3, fh);
	this->scroll = PacketProcessor::Number(buf[0]);
	this->relog_x = PacketProcessor::Number(buf[1]);
	this->relog_y = PacketProcessor::Number(buf[2]);

	SAFE_SEEK(fh, 0x2E, SEEK_SET);
	SAFE_READ(buf, sizeof(char), 1, fh);
	outersize = PacketProcessor::Number(buf[0]);
	if (outersize)
	{
		SAFE_SEEK(fh, 8 * outersize, SEEK_CUR);
	}

	SAFE_READ(buf, sizeof(char), 1, fh);
	outersize = PacketProcessor::Number(buf[0]);
	if (outersize)
	{
		SAFE_SEEK(fh, 4 * outersize, SEEK_CUR);
	}

	SAFE_READ(buf, sizeof(char), 1, fh);
	outersize = PacketProcessor::Number(buf[0]);
	if (outersize)
	{
		SAFE_SEEK(fh, 12 * outersize, SEEK_CUR);
	}

	bool tilespike = false;

	SAFE_READ(buf, sizeof(char), 1, fh);
	outersize = PacketProcessor::Number(buf[0]);
	for (int i = 0; i < outersize; ++i)
	{
		SAFE_READ(buf, sizeof(char), 2, fh);
		unsigned char yloc = PacketProcessor::Number(buf[0]);
		innersize = PacketProcessor::Number(buf[1]);
		for (int ii = 0; ii < innersize; ++ii)
		{
			SAFE_READ(buf, sizeof(char), 2, fh);
			unsigned char xloc = PacketProcessor::Number(buf[0]);
			unsigned char spec = PacketProcessor::Number(buf[1]);

            if (spec == Map_Tile::Spikes1 && !tilespike)
            {
                if (util::to_int(this->world->config["TimedSpikeInterval"]) > 0)
                {
                    TimeEvent *event = new TimeEvent(map_timed_spikes, this, util::to_float(this->world->config["TimedSpikeInterval"]), Timer::FOREVER);
                    this->world->timer.Register(event);
                    tilespike = true;
                }
            }

			if (!this->InBounds(xloc, yloc))
			{
				Console::Wrn("Tile spec on map %i is outside of map bounds (%ix%i)", this->id, xloc, yloc);
				continue;
			}

			this->GetTile(xloc, yloc).tilespec = static_cast<Map_Tile::TileSpec>(spec);

			if (spec == Map_Tile::Chest)
			{
				Map_Chest chest;
				chest.maxchest = static_cast<int>(this->world->config["MaxChest"]);
				chest.chestslots = static_cast<int>(this->world->config["ChestSlots"]);
				chest.x = xloc;
				chest.y = yloc;
				chest.slots = 0;
				this->chests.push_back(std::make_shared<Map_Chest>(chest));
			}
		}
	}

	SAFE_READ(buf, sizeof(char), 1, fh);
	outersize = PacketProcessor::Number(buf[0]);
	for (int i = 0; i < outersize; ++i)
	{
		SAFE_READ(buf, sizeof(char), 2, fh);
		unsigned char yloc = PacketProcessor::Number(buf[0]);
		innersize = PacketProcessor::Number(buf[1]);
		for (int ii = 0; ii < innersize; ++ii)
		{
			Map_Warp newwarp;
			SAFE_READ(buf, sizeof(char), 8, fh);
			unsigned char xloc = PacketProcessor::Number(buf[0]);
			newwarp.map = PacketProcessor::Number(buf[1], buf[2]);
			newwarp.x = PacketProcessor::Number(buf[3]);
			newwarp.y = PacketProcessor::Number(buf[4]);
			newwarp.levelreq = PacketProcessor::Number(buf[5]);
			newwarp.spec = static_cast<Map_Warp::WarpSpec>(PacketProcessor::Number(buf[6], buf[7]));

			if (!this->InBounds(xloc, yloc))
			{
				Console::Wrn("Warp on map %i is outside of map bounds (%ix%i)", this->id, xloc, yloc);
				continue;
			}

			try
			{
				this->GetTile(xloc, yloc).warp = newwarp;
			}
			catch (...)
			{
				std::fclose(fh);
				safe_fail(__LINE__);
				return false;
			}
		}
	}

	SAFE_SEEK(fh, 0x2E, SEEK_SET);
	SAFE_READ(buf, sizeof(char), 1, fh);
	outersize = PacketProcessor::Number(buf[0]);
	int index = 0;
	for (int i = 0; i < outersize; ++i)
	{
		SAFE_READ(buf, sizeof(char), 8, fh);
		unsigned char x = PacketProcessor::Number(buf[0]);
		unsigned char y = PacketProcessor::Number(buf[1]);
		short npc_id = PacketProcessor::Number(buf[2], buf[3]);
		unsigned char spawntype = PacketProcessor::Number(buf[4]);
		short spawntime = PacketProcessor::Number(buf[5], buf[6]);
		unsigned char amount = PacketProcessor::Number(buf[7]);

		if (!this->world->enf->Get(npc_id))
		{
			Console::Wrn("An NPC spawn on map %i uses a non-existent NPC (#%i at %ix%i)", this->id, npc_id, x, y);
		}

		for (int ii = 0; ii < amount; ++ii)
		{
			if (!this->InBounds(x, y))
			{
				Console::Wrn("An NPC spawn on map %i is outside of map bounds (%s at %ix%i)", this->id, this->world->enf->Get(npc_id).name.c_str(), x, y);
				continue;
			}

			NPC *newnpc = new NPC(this, npc_id, x, y, spawntype, spawntime, index++);
			this->npcs.push_back(newnpc);

			newnpc->Spawn();
		}
	}

	SAFE_READ(buf, sizeof(char), 1, fh);
	outersize = PacketProcessor::Number(buf[0]);
	if (outersize)
	{
		SAFE_SEEK(fh, 4 * outersize, SEEK_CUR);
	}

	SAFE_READ(buf, sizeof(char), 1, fh);
	outersize = PacketProcessor::Number(buf[0]);
	for (int i = 0; i < outersize; ++i)
	{
		SAFE_READ(buf, sizeof(char), 12, fh);
		unsigned char x = PacketProcessor::Number(buf[0]);
		unsigned char y = PacketProcessor::Number(buf[1]);
		short slot = PacketProcessor::Number(buf[4]);
		short itemid = PacketProcessor::Number(buf[5], buf[6]);
		short time = PacketProcessor::Number(buf[7], buf[8]);
		int amount = PacketProcessor::Number(buf[9], buf[10], buf[11]);

		if (itemid != this->world->eif->Get(itemid).id)
		{
			Console::Wrn("A chest spawn on map %i uses a non-existent item (#%i at %ix%i)", this->id, itemid, x, y);
		}

		UTIL_FOREACH(this->chests, chest)
		{
			if (chest->x == x && chest->y == y)
			{
				Map_Chest_Spawn spawn;

				spawn.slot = slot+1;
				spawn.time = time;
				spawn.last_taken = Timer::GetTime();
				spawn.item.id = itemid;
				spawn.item.amount = amount;

				chest->spawns.push_back(spawn);
				chest->slots = std::max(chest->slots, slot+1);
				goto skip_warning;
			}
		}
		Console::Wrn("A chest spawn on map %i points to a non-chest (%s x%i at %ix%i)", this->id, this->world->eif->Get(itemid).name.c_str(), amount, x, y);
		skip_warning:
		;
	}

	SAFE_SEEK(fh, 0x00, SEEK_END);
	this->filesize = std::ftell(fh);

	std::fclose(fh);

	this->exists = true;

	return true;
}

void Map::Unload()
{
	this->exists = false;

	UTIL_FOREACH(this->npcs, npc)
	{
		UTIL_FOREACH(npc->damagelist, opponent)
		{
			opponent->attacker->unregister_npc.erase(
				std::remove(UTIL_RANGE(opponent->attacker->unregister_npc), npc),
				opponent->attacker->unregister_npc.end()
			);
		}

		delete npc;
	}

	this->npcs.clear();

	this->chests.clear();
	this->tiles.clear();
}

int Map::GenerateItemID() const
{
	int lowest_free_id = 1;

	restart_loop:

	UTIL_FOREACH(this->items, item)
	{
		if (item->uid == lowest_free_id)
		{
			lowest_free_id = item->uid + 1;
			goto restart_loop;
		}
	}

	return lowest_free_id;
}

unsigned char Map::GenerateNPCIndex() const
{
	unsigned char lowest_free_id = 1;

	restart_loop:

	UTIL_FOREACH(this->npcs, npc)
	{
		if (npc->index == lowest_free_id)
		{
			lowest_free_id = npc->index + 1;
			goto restart_loop;
		}
	}

	return lowest_free_id;
}

void Map::Immune()
{
    UTIL_FOREACH(this->characters, character)
    {
		if (character->immune == true)
        {
            PacketBuilder builder;
            builder.SetID(PACKET_EFFECT, PACKET_PLAYER);
            builder.AddShort(character->player->id);
            builder.AddThree(8);
            builder.AddShort(character->player->id);
            builder.AddThree(8);

			UTIL_FOREACH(this->characters, checkchar)
            {
                if (character->InRange(checkchar))
                    checkchar->Send(builder);
            }
        }
    }
}

void Map::DrainHP(Character *from)
{
    int amount = from->hp * double(this->world->config["HPdrainRate"]);
    amount = std::max(amount, 0);
    int limitamount = std::min(amount, int(from->hp));

    if (this->world->config["LimitDamage"])
        amount = limitamount;

    if (from->immune == false)
        from->hp -= limitamount;

    from->hp = std::min(from->hp, from->maxhp);

    PacketBuilder builder(PACKET_AVATAR, PACKET_REPLY, 10);
    builder.AddShort(0);
    builder.AddShort(from->player->id);
    builder.AddThree(from->immune == false ? amount : 0);
    builder.AddChar(from->direction);
    builder.AddChar(int(double(from->hp) / double(from->maxhp) * 100.0));
    builder.AddChar(from->hp == 0);
    from->Send(builder);

    UTIL_FOREACH(this->characters, character)
    {
        if (from->InRange(character))
        {
            if (!from->hidden)
                character->Send(builder);
            else if ((from->hidden && character->admin >= static_cast<int>(this->world->admin_config["seehide"])) || from == character)
                character->Send(builder);
        }
    }

    builder.Reset(5);
    builder.SetID(PACKET_EFFECT, PACKET_SPEC);
    builder.AddChar(1);
    builder.AddShort(0);
    builder.AddShort(from->tp);
    builder.AddShort(from->maxtp);
    from->Send(builder);

    from->PacketRecover();

    if (from->hp == 0)
        from->DeathRespawn();

    if (from->party)
        from->party->UpdateHP(from);
}

void Map::DrainTP(Character *from)
{
    int amount = from->maxtp * static_cast<double>(world->config["TPdrainRate"]);
    int limitamount = std::min(amount, int(from->tp));

    if (from->immune == false)
        from->tp -= limitamount;

    PacketBuilder builder;
    builder.SetID(PACKET_EFFECT, PACKET_SPEC);
    builder.AddChar(1);
    builder.AddShort(from->immune == false ? amount : 0);
    builder.AddShort(from->tp);
    builder.AddShort(from->maxtp);
    from->Send(builder);

    from->PacketRecover();
}

void Map::Enter(Character *character, WarpAnimation animation)
{
	this->characters.push_back(character);
	character->map = this;
	character->last_walk = Timer::GetTime();
	character->attacks = 0;
	character->CancelSpell();

	PacketBuilder builder(PACKET_PLAYERS, PACKET_AGREE, 63);

	builder.AddByte(255);
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
	builder.AddChar(animation);
	builder.AddByte(255);
	builder.AddChar(1);

	UTIL_FOREACH(this->characters, checkcharacter)
	{
		if (checkcharacter == character || !character->InRange(checkcharacter))
		{
			continue;
		}

		if (!character->hidden)
            checkcharacter->player->client->Send(builder);
        else if ((character->hidden && checkcharacter->admin >= static_cast<int>(this->world->admin_config["seehide"])) || character == checkcharacter)
            checkcharacter->player->client->Send(builder);
	}

	character->CheckQuestRules();
}

void Map::Leave(Character *character, WarpAnimation animation, bool silent)
{
    if (character->mapid == int(character->world->ctf_config["CTFMap"]))
    {
        if (world->ctf == true || world->ctfenabled == true)
        {
            if (character->blueteam == true || character->redteam == true)
                character->Undress(Character::Armor);
        }
    }

    if (character->mapid == int(character->world->pvp_config["PVPMap"]))
    {
        if (world->pvp == true || world->pvpenabled == true)
        {
            if (character->pvp_blueteam == true || character->pvp_redteam == true)
                character->Undress(Character::Armor);
        }
    }

	if (!silent)
	{
		PacketBuilder builder(PACKET_AVATAR, PACKET_REMOVE, 3);
		builder.AddShort(character->player->id);

		if (animation != WARP_ANIMATION_NONE)
		{
			builder.AddChar(animation);
		}

		UTIL_FOREACH(this->characters, checkcharacter)
		{
			if (checkcharacter == character || !character->InRange(checkcharacter))
			{
				continue;
			}

			if (!character->hidden)
            {
                checkcharacter->player->client->Send(builder);
            }
            else if ((character->hidden && checkcharacter->admin >= static_cast<int>(this->world->admin_config["seehide"])) || character == checkcharacter)
            {
                checkcharacter->player->client->Send(builder);
            }
		}
	}

    this->characters.erase(std::remove(UTIL_RANGE(this->characters), character), this->characters.end());

	character->map = 0;
}

void Map::Msg(Character *from, std::string message, bool echo)
{
	message = util::text_cap(message, static_cast<int>(this->world->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from->SourceName()) + "  "));

	PacketBuilder builder(PACKET_TALK, PACKET_PLAYER, 2 + message.length());
	builder.AddShort(from->player->id);
	builder.AddString(message);

	UTIL_FOREACH(this->characters, character)
	{
		if (!from->InRange(character))
 			continue;

        character->AddChatLog("", from->SourceName(), message);

		if (!echo && character == from)
			continue;

		character->Send(builder);
	}
}

void Map::Msg(NPC *from, std::string message)
{
	message = util::text_cap(message, static_cast<int>(this->world->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from->Data().name) + "  "));

	PacketBuilder builder(PACKET_NPC, PACKET_PLAYER, 4 + message.length());
	builder.AddByte(255);
	builder.AddByte(255);
	builder.AddShort(from->index);
	builder.AddChar(message.length());
	builder.AddString(message);

	UTIL_FOREACH(this->characters, character)
	{
		character->Send(builder);
	}
}

bool Map::Walk(Character *from, Direction direction, bool admin)
{
	int seedistance = this->world->config["SeeDistance"];

	unsigned char target_x = from->x;
	unsigned char target_y = from->y;

	switch (direction)
	{
		case DIRECTION_UP:
        target_y -= 1;

        if (target_y > from->y)
            return false;

        break;

		case DIRECTION_RIGHT:
        target_x += 1;

        if (target_x < from->x)
            return false;

        break;

		case DIRECTION_DOWN:
        target_y += 1;

        if (target_x < from->x)
            return false;

        break;

		case DIRECTION_LEFT:
        target_x -= 1;

        if (target_x > from->x)
            return false;

        break;
	}

	if (!this->InBounds(target_x, target_y))
		return false;

    if (!admin)
	{
		if (!this->Walkable(target_x, target_y))
			return false;

		if (this->Occupied(target_x, target_y, PlayerOnly) && (from->last_walk + double(this->world->config["GhostTimer"]) > Timer::GetTime()))
			return false;
	}

	const Map_Warp& warp = this->GetWarp(target_x, target_y);

	if (!admin)
	{
		if (!this->Walkable(target_x, target_y))
			return false;

		if (this->Occupied(target_x, target_y, PlayerOnly) && (from->last_walk + double(this->world->config["GhostTimer"]) > Timer::GetTime()))
			return false;
	}

	if (warp)
	{
		if (from->level >= warp.levelreq && (warp.spec == Map_Warp::NoDoor || warp.open))
		{
			Map* map = this->world->GetMap(warp.map);

			if (from->SourceAccess() < ADMIN_GUIDE && map->evacuate_lock && map->id != from->map->id)
			{
				from->StatusMsg(this->world->i18n.Format("EvacuateBlock"));
				from->Refresh();
			}
			else
			{
				from->Warp(warp.map, warp.x, warp.y);
			}

			return false;
		}

		if (!admin)
            return false;
	}

    from->last_walk = Timer::GetTime();
    from->attacks = 0;
    from->CancelSpell();

	from->direction = direction;

	from->x = target_x;
	from->y = target_y;

	int newx;
	int newy;
	int oldx;
	int oldy;

	std::vector<std::pair<int, int>> newcoords;
	std::vector<std::pair<int, int>> oldcoords;

	std::vector<Character *> adj_chars;
	std::vector<Character *> oldchars;
	std::vector<NPC *> newnpcs;
	std::vector<NPC *> oldnpcs;
	std::vector<std::shared_ptr<Map_Item>> newitems;

	switch (direction)
	{
		case DIRECTION_UP:
        for (int i = -seedistance; i <= seedistance; ++i)
        {
            newy = from->y - seedistance + std::abs(i);
            newx = from->x + i;
            oldy = from->y + seedistance + 1 - std::abs(i);
            oldx = from->x + i;

            newcoords.push_back(std::make_pair(newx, newy));
            oldcoords.push_back(std::make_pair(oldx, oldy));
        }
        break;

		case DIRECTION_RIGHT:
        for (int i = -seedistance; i <= seedistance; ++i)
        {
            newx = from->x + seedistance - std::abs(i);
            newy = from->y + i;
            oldx = from->x - seedistance - 1 + std::abs(i);
            oldy = from->y + i;

            newcoords.push_back(std::make_pair(newx, newy));
            oldcoords.push_back(std::make_pair(oldx, oldy));
        }
        break;

		case DIRECTION_DOWN:
        for (int i = -seedistance; i <= seedistance; ++i)
        {
            newy = from->y + seedistance - std::abs(i);
            newx = from->x + i;
            oldy = from->y - seedistance - 1 + std::abs(i);
            oldx = from->x + i;

            newcoords.push_back(std::make_pair(newx, newy));
            oldcoords.push_back(std::make_pair(oldx, oldy));
        }
        break;

		case DIRECTION_LEFT:
        for (int i = -seedistance; i <= seedistance; ++i)
        {
            newx = from->x - seedistance + std::abs(i);
            newy = from->y + i;
            oldx = from->x + seedistance + 1 - std::abs(i);
            oldy = from->y + i;

            newcoords.push_back(std::make_pair(newx, newy));
            oldcoords.push_back(std::make_pair(oldx, oldy));
        }
        break;
	}

	UTIL_FOREACH(this->characters, checkchar)
	{
		if (checkchar == from)
			continue;

		for (std::size_t i = 0; i < oldcoords.size(); ++i)
		{
			if (checkchar->x == oldcoords[i].first && checkchar->y == oldcoords[i].second)
			{
				oldchars.push_back(checkchar);
			}
			else if (checkchar->x == newcoords[i].first && checkchar->y == newcoords[i].second)
			{
				adj_chars.push_back(checkchar);
			}
		}
	}

	UTIL_FOREACH(this->npcs, checknpc)
	{
		if (!checknpc->alive)
			continue;

		for (std::size_t i = 0; i < oldcoords.size(); ++i)
		{
			if (checknpc->x == oldcoords[i].first && checknpc->y == oldcoords[i].second)
			{
				oldnpcs.push_back(checknpc);
			}
			else if (checknpc->x == newcoords[i].first && checknpc->y == newcoords[i].second)
			{
				newnpcs.push_back(checknpc);
			}
		}
	}

	UTIL_FOREACH(this->items, checkitem)
	{
		for (std::size_t i = 0; i < oldcoords.size(); ++i)
		{
			if (checkitem->x == newcoords[i].first && checkitem->y == newcoords[i].second)
				newitems.push_back(checkitem);
		}
	}

	PacketBuilder builder(PACKET_AVATAR, PACKET_REMOVE, 2);
	builder.AddShort(from->player->id);

	UTIL_FOREACH(oldchars, character)
	{
		PacketBuilder rbuilder(PACKET_AVATAR, PACKET_REMOVE, 2);
		rbuilder.AddShort(character->player->id);

		character->Send(builder);
		from->player->Send(rbuilder);
	}

	builder.Reset(62);
	builder.SetID(PACKET_PLAYERS, PACKET_AGREE);

	builder.AddByte(255);
	builder.AddBreakString(from->SourceName());
	builder.AddShort(from->player->id);
	builder.AddShort(from->mapid);
	builder.AddShort(from->x);
	builder.AddShort(from->y);
	builder.AddChar(from->direction);
	builder.AddChar(6);
	builder.AddString(from->PaddedGuildTag());
	builder.AddChar(from->level);
	builder.AddChar(from->gender);
	builder.AddChar(from->hairstyle);
	builder.AddChar(from->haircolor);
	builder.AddChar(from->race);
	builder.AddShort(from->maxhp);
	builder.AddShort(from->hp);
	builder.AddShort(from->maxtp);
	builder.AddShort(from->tp);

	// equipment

	from->AddPaperdollData(builder, "B000A0HSW");

	builder.AddChar(from->sitting);
	builder.AddChar(from->hidden);
	builder.AddByte(255);
	builder.AddChar(1);

	UTIL_FOREACH(adj_chars, character)
	{
		PacketBuilder rbuilder(PACKET_PLAYERS, PACKET_AGREE, 62);
		rbuilder.AddByte(255);
		rbuilder.AddBreakString(character->SourceName());
		rbuilder.AddShort(character->player->id);
		rbuilder.AddShort(character->mapid);
		rbuilder.AddShort(character->x);
		rbuilder.AddShort(character->y);
		rbuilder.AddChar(character->direction);
		rbuilder.AddChar(6);
		rbuilder.AddString(character->PaddedGuildTag());
		rbuilder.AddChar(character->level);
		rbuilder.AddChar(character->gender);
		rbuilder.AddChar(character->hairstyle);
		rbuilder.AddChar(character->haircolor);
		rbuilder.AddChar(character->race);
		rbuilder.AddShort(character->maxhp);
		rbuilder.AddShort(character->hp);
		rbuilder.AddShort(character->maxtp);
		rbuilder.AddShort(character->tp);

		// equipment

		character->AddPaperdollData(rbuilder, "B000A0HSW");

		rbuilder.AddChar(character->sitting);
		rbuilder.AddChar(character->hidden);
		rbuilder.AddByte(255);
		rbuilder.AddChar(1);

		if (!from->hidden)
            character->player->client->Send(builder);
        else if ((from->hidden && character->player->character->admin >= static_cast<int>(this->world->admin_config["seehide"])) || from == character)
            character->player->client->Send(builder);

        if (!character->hidden)
            from->player->client->Send(rbuilder);
        else if ((character->hidden && from->player->character->admin >= static_cast<int>(this->world->admin_config["seehide"])) || from == character)
            from->player->client->Send(rbuilder);
	}

	PacketBuilder builders;

	if (from->player->character->mapid == static_cast<int>(this->world->eosbot_config["bot.map"]) && static_cast<std::string>(this->world->eosbot_config["bot.enabled"]) != "no")
    {
        builders.SetID(PACKET_PLAYERS, PACKET_AGREE);
        builders.AddByte(255);

        if (static_cast<std::string>(this->world->eosbot_config["bot.name"]) == "0")
        {
            builder.AddBreakString("EOSbot");
        }
        else
        {
            builder.AddBreakString(static_cast<std::string>(this->world->eosbot_config["bot.name"]));
        }

        builders.AddShort(0);
        builders.AddShort(static_cast<int>(this->world->eosbot_config["bot.map"]));
        builders.AddShort(static_cast<int>(this->world->eosbot_config["bot.locx"]));
        builders.AddShort(static_cast<int>(this->world->eosbot_config["bot.locy"]));
        builders.AddChar(static_cast<int>(this->world->eosbot_config["bot.direction"]));
        builders.AddChar(6);
        builders.AddString(static_cast<std::string>(this->world->eosbot_config["bot.guildtag"]));
        builders.AddChar(static_cast<int>(this->world->eosbot_config["bot.level"]));
        builders.AddChar(static_cast<int>(this->world->eosbot_config["bot.gender"]));
        builders.AddChar(static_cast<int>(this->world->eosbot_config["bot.hairstyle"]));
        builders.AddChar(static_cast<int>(this->world->eosbot_config["bot.haircolor"]));
        builders.AddChar(static_cast<int>(this->world->eosbot_config["bot.race"]));
        builders.AddShort(0);
        builders.AddShort(0);
        builders.AddShort(0);
        builders.AddShort(0);
        builders.AddShort(static_cast<int>(this->world->eosbot_config["bot.boots"]));
        builders.AddShort(0);
        builders.AddShort(0);
        builders.AddShort(0);
        builders.AddShort(static_cast<int>(this->world->eosbot_config["bot.armor"]));
        builders.AddShort(0);
        builders.AddShort(static_cast<int>(this->world->eosbot_config["bot.hat"]));
        builders.AddShort(static_cast<int>(this->world->eosbot_config["bot.shield"]));
        builders.AddShort(static_cast<int>(this->world->eosbot_config["bot.weapon"]));
        builders.AddChar(static_cast<int>(this->world->eosbot_config["bot.sitting"]));
        builders.AddChar(static_cast<int>(this->world->eosbot_config["bot.hidden"]));
        builders.AddByte(255);
        builders.AddChar(1);

        from->Send(builders);
    }

	builder.Reset(5);
	builder.SetID(PACKET_WALK, PACKET_PLAYER);

	builder.AddShort(from->player->id);
	builder.AddChar(direction);
	builder.AddChar(from->x);
	builder.AddChar(from->y);

	UTIL_FOREACH(this->characters, character)
	{
		if (character == from || !from->InRange(character))
		{
			continue;
		}

		if (!from->hidden)
            character->player->client->Send(builder);
        else if ((from->hidden && character->admin >= static_cast<int>(this->world->admin_config["seehide"])) || from == character)
            character->player->client->Send(builder);
	}

	builder.Reset(2 + newitems.size() * 9);
	builder.SetID(PACKET_WALK, PACKET_REPLY);

	builder.AddByte(255);
	builder.AddByte(255);

	UTIL_FOREACH(newitems, item)
	{
		builder.AddShort(item->uid);
		builder.AddShort(item->id);
		builder.AddChar(item->x);
		builder.AddChar(item->y);
		builder.AddThree(item->amount);
	}

	from->player->Send(builder);

	builder.SetID(PACKET_APPEAR, PACKET_REPLY);

	UTIL_FOREACH(newnpcs, npc)
	{
		builder.Reset(8);
		builder.AddChar(0);
		builder.AddByte(255);
		builder.AddChar(npc->index);
		builder.AddShort(npc->id);
		builder.AddChar(npc->x);
		builder.AddChar(npc->y);
		builder.AddChar(npc->direction);

		from->player->Send(builder);
	}

	UTIL_FOREACH(oldnpcs, npc)
	{
		npc->RemoveFromView(from);
	}

	from->CheckQuestRules();

	return true;
}

bool Map::Walk(NPC *from, Direction direction)
{
	int seedistance = this->world->config["SeeDistance"];

	unsigned char target_x = from->x;
	unsigned char target_y = from->y;

	switch (direction)
	{
		case DIRECTION_UP:
        target_y -= 1;

        if (target_y > from->y)
        {
            return false;
        }

        break;

		case DIRECTION_RIGHT:
        target_x += 1;

        if (target_x < from->x)
        {
            return false;
        }

        break;

		case DIRECTION_DOWN:
        target_y += 1;

        if (target_x < from->x)
        {
            return false;
        }

        break;

		case DIRECTION_LEFT:
        target_x -= 1;

        if (target_x > from->x)
        {
            return false;
        }

        break;
	}

	if (!this->Walkable(target_x, target_y, Map_Tile::Wall))
        return false;

	if (!this->Walkable(target_x, target_y, !from->pet) || this->Occupied(target_x, target_y, Map::PlayerAndNPC))
        return false;

	from->x = target_x;
	from->y = target_y;

	int newx;
	int newy;
	int oldx;
	int oldy;

	std::vector<std::pair<int, int>> newcoords;
	std::vector<std::pair<int, int>> oldcoords;

	std::vector<Character *> adj_chars;
	std::vector<Character *> oldchars;

	switch (direction)
	{
        case DIRECTION_UP:
        for (int i = -seedistance; i <= seedistance; ++i)
        {
            newy = from->y - seedistance + std::abs(i);
            newx = from->x + i;
            oldy = from->y + seedistance + 1 - std::abs(i);
            oldx = from->x + i;

            newcoords.push_back(std::make_pair(newx, newy));
            oldcoords.push_back(std::make_pair(oldx, oldy));
        }
        break;

        case DIRECTION_RIGHT:
        for (int i = -seedistance; i <= seedistance; ++i)
        {
            newx = from->x + seedistance - std::abs(i);
            newy = from->y + i;
            oldx = from->x - seedistance - 1 + std::abs(i);
            oldy = from->y + i;

            newcoords.push_back(std::make_pair(newx, newy));
            oldcoords.push_back(std::make_pair(oldx, oldy));
        }
        break;

        case DIRECTION_DOWN:
        for (int i = -seedistance; i <= seedistance; ++i)
        {
            newy = from->y + seedistance - std::abs(i);
            newx = from->x + i;
            oldy = from->y - seedistance - 1 + std::abs(i);
            oldx = from->x + i;

            newcoords.push_back(std::make_pair(newx, newy));
            oldcoords.push_back(std::make_pair(oldx, oldy));
        }
        break;

        case DIRECTION_LEFT:
        for (int i = -seedistance; i <= seedistance; ++i)
        {
            newx = from->x - seedistance + std::abs(i);
            newy = from->y + i;
            oldx = from->x + seedistance + 1 - std::abs(i);
            oldy = from->y + i;

            newcoords.push_back(std::make_pair(newx, newy));
            oldcoords.push_back(std::make_pair(oldx, oldy));
        }
        break;
	}

	from->direction = direction;

	UTIL_FOREACH(this->characters, checkchar)
	{
		for (std::size_t i = 0; i < oldcoords.size(); ++i)
		{
			if (checkchar->x == oldcoords[i].first && checkchar->y == oldcoords[i].second)
			{
				oldchars.push_back(checkchar);
			}
			else if (checkchar->x == newcoords[i].first && checkchar->y == newcoords[i].second)
			{
				adj_chars.push_back(checkchar);
			}
		}
	}

	PacketBuilder builder(PACKET_APPEAR, PACKET_REPLY, 8);
	builder.AddChar(0);
	builder.AddByte(255);
	builder.AddChar(from->index);
	builder.AddShort(from->id);
	builder.AddChar(from->x);
	builder.AddChar(from->y);
	builder.AddChar(from->direction);

	UTIL_FOREACH(adj_chars, character)
	{
		character->Send(builder);
	}

	builder.Reset(7);
	builder.SetID(PACKET_NPC, PACKET_PLAYER);

	builder.AddChar(from->index);
	builder.AddChar(from->x);
	builder.AddChar(from->y);
	builder.AddChar(from->direction);
	builder.AddByte(255);
	builder.AddByte(255);
	builder.AddByte(255);

	UTIL_FOREACH(this->characters, character)
	{
		if (!character->InRange(from))
		{
			continue;
		}

		character->Send(builder);
	}

	UTIL_FOREACH(oldchars, character)
	{
		from->RemoveFromView(character);
	}

	return true;
}

void Map::Attack(Character *from, Direction direction)
{
	from->direction = direction;

    if (from->paperdoll[Character::Weapon] > 0)
    {
        int target_x = from->x;
        int target_y = from->y;

        switch (from->direction)
        {
            case DIRECTION_UP:
            target_y -= 1;
            break;

            case DIRECTION_RIGHT:
            target_x += 1;
            break;

            case DIRECTION_DOWN:
            target_y += 1;
            break;

            case DIRECTION_LEFT:
            target_x -= 1;
            break;
        }

        if (from->paperdoll[Character::Weapon] == util::to_int(this->world->fishing_config["FishingWeapon"]))
        {
            if (from->map->GetSpec(target_x, target_y) == Map_Tile::Fishing)
            {
                if (from->HasItem(util::to_int(this->world->fishing_config["FishingBait"])))
                    this->Fish(from);

                if (!from->HasItem(util::to_int(this->world->fishing_config["FishingBait"])))
                    from->StatusMsg("You don't have any fishing bait.");
            }
        }
        else if (from->paperdoll[Character::Weapon] == util::to_int(this->world->mining_config["MiningWeapon"]))
        {
            if (from->map->GetSpec(target_x, target_y) == Map_Tile::Mining)
                this->Mine(from);
        }
        else if (from->paperdoll[Character::Weapon] == util::to_int(this->world->woodcutting_config["WoodcuttingWeapon"]))
        {
            if (from->map->GetSpec(target_x, target_y) == Map_Tile::Woodcutting)
                this->Wood(from);
        }
    }

	from->attacks += 1;

	from->CancelSpell();

	if (from->arena)
		from->arena->Attack(from, direction);

	int wep_graphic = this->world->eif->Get(from->paperdoll[Character::Weapon]).dollgraphic;
	bool is_instrument = (wep_graphic != 0 && this->world->IsInstrument(wep_graphic));

	if (!is_instrument && (this->pk || (this->world->config["GlobalPK"] && !this->world->PKExcept(this->id))))
	{
		if (this->AttackPK(from, direction))
			return;
	}

	PacketBuilder builder(PACKET_ATTACK, PACKET_PLAYER, 3);
	builder.AddShort(from->player->id);
	builder.AddChar(direction);

	UTIL_FOREACH(this->characters, character)
	{
		if (character == from || !from->InRange(character))
			continue;

		if (!from->hidden)
		    character->player->client->Send(builder);
        else if ((from->hidden && character->admin >= static_cast<int>(this->world->admin_config["seehide"])) || from == character)
            character->player->client->Send(builder);
	}

	if (is_instrument)
		return;

    if (!from->CanInteractCombat())
        return;

	int target_x = from->x;
	int target_y = from->y;

	int range = 1;

	if (this->world->eif->Get(from->paperdoll[Character::Weapon]).subtype == EIF::Ranged)
		range = static_cast<int>(this->world->config["RangedDistance"]);

	for (int i = 0; i < range; ++i)
	{
		switch (from->direction)
		{
			case DIRECTION_UP:
            target_y -= 1;
            break;

			case DIRECTION_RIGHT:
            target_x += 1;
            break;

			case DIRECTION_DOWN:
            target_y += 1;
            break;

			case DIRECTION_LEFT:
            target_x -= 1;
            break;
		}

		if (this->world->eif->Get(from->paperdoll[Character::Weapon]).unkb && this->world->config["AOEWeapons"])
        {
            int amount = util::rand(from->mindam, from->maxdam);

            UTIL_FOREACH(this->npcs, npc)
            {
                if (npc->Data().type == ENF::Passive || npc->Data().type == ENF::Aggressive)
                {
                    int distance = util::path_length(target_x, target_y, npc->x, npc->y);

                    if ((this->pk == false && npc->pet) || from == npc->owner)
                        return;

                    if (distance <= this->world->eif->Get(from->paperdoll[Character::Weapon]).unkb && npc->alive)
                    {
                        if (distance >= 1)
                            npc->Damage(from,amount);
                    }
                }
            }
        }

		UTIL_FOREACH(this->npcs, npc)
		{
            if ((npc->Data().type == ENF::Passive || npc->Data().type == ENF::Aggressive || npc->pet || from->SourceDutyAccess() >= static_cast<int>(this->world->admin_config["killnpc"])) && npc->alive && npc->x == target_x && npc->y == target_y)
			{
			    if ((this->pk == false && npc->pet) || from == npc->owner)
			        return;

				int amount = util::rand(from->mindam, from->maxdam);
				double rand = util::rand(0.0, 1.0);
				bool critical = std::abs(int(npc->direction) - from->direction) != 2 || rand < static_cast<double>(this->world->config["CriticalRate"]);

				std::unordered_map<std::string, double> formula_vars;

				from->FormulaVars(formula_vars);
				npc->FormulaVars(formula_vars, "target_");
				formula_vars["modifier"] = this->world->config["MobRate"];
				formula_vars["damage"] = amount;
				formula_vars["critical"] = critical;

				amount = rpn_eval(rpn_parse(this->world->formulas_config["damage"]), formula_vars);
				double hit_rate = rpn_eval(rpn_parse(this->world->formulas_config["hit_rate"]), formula_vars);

				if (rand > hit_rate)
				{
					amount = 0;
				}

				amount = std::max(amount, 0);

				int limitamount = std::min(amount, int(npc->hp));

				if (this->world->config["LimitDamage"])
				{
					amount = limitamount;
				}

				for (int i = 0 ; i < static_cast<int>(this->world->shaving_config["Amount"]) ; i++)
                {
                    int random = util::rand(1,100);

                    int npcid = static_cast<int>(this->world->shaving_config[util::to_string(i+1) + ".NPCID"]);
                    int weapon = static_cast<int>(this->world->shaving_config[util::to_string(i+1) + ".WeaponID"]);

                    if (npc->alive && npc->id == npcid)
                    {
                        if (from->paperdoll[Character::Weapon] == weapon)
                        {
                            int chance = static_cast<int>(this->world->shaving_config[util::to_string(i+1) + ".Chance"]);

                            if (random > 100 - chance)
                            {
                                int Reward = static_cast<int>(this->world->shaving_config[util::to_string(i+1) + ".RewardID"]);

                                int MinAmount = static_cast<int>(this->world->shaving_config[util::to_string(i+1) + ".MinAmountID"]);
                                int MaxAmount = static_cast<int>(this->world->shaving_config[util::to_string(i+1) + ".MaxAmountID"]);

                                int Amount = util::rand(MinAmount, MaxAmount);

                                from->GiveItem(Reward, Amount);
                                    from->StatusMsg(from->world->i18n.Format("ShaveSuccess", this->world->enf->Get(npcid).name, util::to_string(Amount), this->world->eif->Get(Reward).name));
                            }
                            else
                            {
                                from->StatusMsg(from->world->i18n.Format("ShaveFail"));
                            }

                            return;
                        }
                    }
                }

				npc->Damage(from, amount);

				return;
			}
		}

		if (!this->Walkable(target_x, target_y, true))
		{
			return;
		}
	}
}

int AntiGank(int minlevel ,int maxlevel)
{
	if (minlevel >= maxlevel)
	{
		return minlevel - maxlevel;
	}
	else
	{
		return maxlevel - minlevel;
	}
}

bool Map::AttackPK(Character *from, Direction direction)
{
    if (!from->CanInteractCombat())
        return false;

	(void)direction;

	int target_x = from->x;
	int target_y = from->y;

	int range = 1;

	if (this->world->eif->Get(from->paperdoll[Character::Weapon]).subtype == EIF::Ranged)
		range = static_cast<int>(this->world->config["RangedDistance"]);

	for (int i = 0; i < range; ++i)
	{
		switch (from->direction)
		{
			case DIRECTION_UP:
            target_y --;
            break;

			case DIRECTION_RIGHT:
            target_x ++;
            break;

			case DIRECTION_DOWN:
            target_y ++;
            break;

			case DIRECTION_LEFT:
            target_x --;
            break;
		}

		Character *character;

        UTIL_FOREACH(this->characters, pkcharacter)
		{
		    if (pkcharacter->mapid == this->id && !pkcharacter->nowhere && pkcharacter->x == target_x && pkcharacter->y == target_y)
            {
                character = pkcharacter;

                if (from->player->character->party)
                {
                    UTIL_FOREACH(from->player->character->party->members, member)
                    {
                        if (member->player->character == character->player->character)
                        {
                            from->player->character->StatusMsg("You can't attack your own party members!");
                            return false;
                        }
                    }
                }

                if (this->world->config["pkguildprotection"])
                {
                    if (from->player->character->guild)
                    {
                        UTIL_FOREACH(from->player->character->guild->members, member)
                        {
                            Character *guildmember = this->world->GetCharacter(member->name);

                            if (guildmember->player->character == character->player->character)
                            {
                                from->player->character->StatusMsg("You can't attack your own guild members!");
                                return false;
                            }
                        }
                    }
                }

                if (this->world->config["racewars"])
                {
                    if ((from->race == 0 || from->race == 1 || from->race == 2) && (character->race == from->race))
                    {
                        from->StatusMsg("You cannot attack your own allies!");
                        return false;
                    }
                    else if (from->race == SKIN_ORC && character->race == SKIN_ORC)
                    {
                        from->StatusMsg("You cannot attack your own allies!");
                        return false;
                    }
                }

                if (this->world->config["classwars"])
                {
                    if (from->clas == character->clas)
                    {
                        from->StatusMsg("You cannot attack your own allies!");
                        return false;
                    }
                }

                if (this->world->config["AntiGank"])
                {
                    int AntiGank(int minlevel, int maxlevel);

                    if (AntiGank(character->level, from->level) >= int(this->world->config["AntiGankLevel"]))
                    {
                        from->StatusMsg("Cannot attack target - Not within level range [" + util::to_string(static_cast<int>(this->world->config["AntiGankLevel"])) + "]");
                        return false;
                    }
                }

                if (from->mapid == int(this->world->pvp_config["PVPMap"]))
                {
                    if (world->pvp)
                    {
                        if ((from->pvp_blueteam && character->pvp_blueteam) || (from->pvp_redteam && character->pvp_redteam))
                        {
                            from->StatusMsg("You cannot attack your own team members!");
                            return false;
                        }
                    }
                    else if (!world->pvp)
                    {
                        from->StatusMsg("PVP has not started yet!");
                        return false;
                    }
                }

                int amount = util::rand(from->mindam, from->maxdam) * static_cast<double>(this->world->config["PKRate"]);

                double rand = util::rand(0.0, 1.0);

                bool critical = std::abs(int(character->direction) - from->direction) != 2 || rand < static_cast<double>(this->world->config["CriticalRate"]);

                std::unordered_map<std::string, double> formula_vars;

                from->FormulaVars(formula_vars);
                character->FormulaVars(formula_vars, "target_");
                formula_vars["modifier"] = this->world->config["PKRate"];
                formula_vars["damage"] = amount;
                formula_vars["critical"] = critical;

                amount = rpn_eval(rpn_parse(this->world->formulas_config["damage"]), formula_vars);
                double hit_rate = rpn_eval(rpn_parse(this->world->formulas_config["hit_rate"]), formula_vars);

                if (rand > hit_rate)
                    amount = 0;

                amount = std::max(amount, 0);

                int limitamount = std::min(amount, int(character->hp));

                if (this->world->config["LimitDamage"])
                    amount = limitamount;

                for (int i = 0 ; i < static_cast<int>(this->world->effects_config["WAmount"]) ; i++)
                {
                    int itemid = static_cast<int>(this->world->effects_config[util::to_string(i+1) + ".ItemID"]);
                    int effectid = static_cast<int>(this->world->effects_config[util::to_string(i+1) + ".PlayerEffect"]);

                    if (from->paperdoll[Character::Weapon] == itemid)
                    {
                        character->Effect(effectid, true);

                        if (itemid > 0)
                            from->PacketRecover();
                    }
                }

                if (character->immune == false)
                    character->hp -= limitamount;

                PacketBuilder from_builder(PACKET_AVATAR, PACKET_REPLY, 10);
				from_builder.AddShort(0);
				from_builder.AddShort(character->player->id);
				from_builder.AddThree(character->immune == false ? amount : 0);
				from_builder.AddChar(from->direction);
				from_builder.AddChar(util::clamp<int>(double(character->hp) / double(character->maxhp) * 100.0, 0, 100));
				from_builder.AddChar(character->hp == 0);

				PacketBuilder builder(PACKET_AVATAR, PACKET_REPLY, 10);
				builder.AddShort(from->player->id);
				builder.AddShort(character->player->id);
				builder.AddThree(character->immune == false ? amount : 0);
				builder.AddChar(from->direction);
				builder.AddChar(util::clamp<int>(double(character->hp) / double(character->maxhp) * 100.0, 0, 100));
				builder.AddChar(character->hp == 0);

				from->Send(from_builder);

				UTIL_FOREACH(this->characters, checkchar)
				{
					if (from != checkchar && character->InRange(checkchar))
						checkchar->Send(builder);
				}

				character->PacketRecover();

                if (character->hp == 0)
                {
                    UTIL_FOREACH(from->quests, q)
					{
						if (!q.second || q.second->GetQuest()->Disabled())
							continue;

						q.second->KilledPlayer();
					}

                    if (!from->HasAchievement(std::string(this->world->achievements_config["PlayerKill.Achievement"])))
                    {
                        if (std::string(this->world->achievements_config["PlayerKill.Achievement"]) != "0")
                        {
                            from->GiveEXP(std::min(std::max(int(this->world->achievements_config["PlayerKill.Reward"]), 0), int(from->world->config["MaxExp"])));

                            if (std::string(this->world->achievements_config["PlayerKill.Title"]) != "0")
                                from->title = std::string(this->world->achievements_config["PlayerKill.Title"]);

                            if (std::string(this->world->achievements_config["PlayerKill.Description"]) != "0")
                                from->ServerMsg(std::string(this->world->achievements_config["PlayerKill.Description"]));

                            if (std::string(from->world->achievements_config["PlayerKill.Announcement"]) == "yes")
                                from->world->ServerMsg(util::ucfirst(from->SourceName()) + " earned the achievement '" + std::string(from->world->achievements_config["PlayerKill.Achievement"]) + "'.");

                            from->NewAchievement(std::string(this->world->achievements_config["PlayerKill.Achievement"]));
                        }
                    }

                    if (from->world->pvp == true)
                    {
                        if (from->mapid == int(from->world->pvp_config["PVPMap"]))
                        {
                            Map *maps = world->GetMap(int(from->world->pvp_config["PVPMap"]));

                            int KillCounter = int(from->world->pvp_config["KillsToWin"]);
                            int KillRewardID = int(from->world->pvp_config["KillRewardID"]);
                            int KillRewardAmount = int(from->world->pvp_config["KillRewardAmount"]);

                            int WinRewardID = int(from->world->pvp_config["WinRewardID"]);
                            int WinRewardAmount = int(from->world->pvp_config["WinRewardAmount"]);

                            if (from->pvp_blueteam == true)
                            {
                                if (character->pvp_redteam)
                                    world->pvp_bluekillcount++;

                                UTIL_FOREACH(maps->characters, ch)
                                {
                                    ch->ServerMsg("Someone of the red team has been killed, the blue team's current current kill count is " + util::to_string(world->pvp_bluekillcount) + "/" + util::to_string(KillCounter));
                                }
                            }
                            else if (from->pvp_redteam == true)
                            {
                                if (character->pvp_blueteam)
                                    world->pvp_redkillcount++;

                                UTIL_FOREACH(maps->characters, ch)
                                {
                                    ch->ServerMsg("Someone of the blue team has been killed, the red team's current current kill count is " + util::to_string(world->pvp_redkillcount) + "/" + util::to_string(KillCounter));
                                }
                            }

                            if (KillRewardID > 0 && KillRewardAmount > 0)
                            {
                                from->GiveItem(KillRewardID, KillRewardAmount);
                                from->StatusMsg("You have been rewarded with " + util::to_string(KillRewardAmount) + " " + from->world->eif->Get(int(KillRewardID)).name + " for killing an enemy.");
                            }

                            if (world->pvp_bluekillcount >= KillCounter)
                            {
                                UTIL_FOREACH(maps->characters, ch)
                                {
                                    ch->ServerMsg("PVP has ended, the blue team has won, congratulations!");
                                }

                                UTIL_FOREACH(from->map->characters, checkchar)
                                {
                                    if (checkchar->pvp_blueteam)
                                    {
                                        if (WinRewardID > 0 && WinRewardAmount > 0)
                                        {
                                            checkchar->GiveItem(WinRewardID, WinRewardAmount);
                                            checkchar->StatusMsg("You have been rewarded with " + util::to_string(WinRewardAmount) + " " + from->world->eif->Get(int(WinRewardID)).name);
                                        }
                                    }
                                }
                            }
                            else if (world->pvp_redkillcount >= KillCounter)
                            {
                                UTIL_FOREACH(maps->characters, ch)
                                {
                                    ch->ServerMsg("PVP has ended, the red team has won, congratulations!");
                                }

                                UTIL_FOREACH(from->map->characters, checkchar)
                                {
                                    if (checkchar->pvp_redteam)
                                    {
                                        if (WinRewardID > 0 && WinRewardAmount > 0)
                                        {
                                            checkchar->GiveItem(WinRewardID, WinRewardAmount);
                                            checkchar->StatusMsg("You have been rewarded with " + util::to_string(WinRewardAmount) + " " + from->world->eif->Get(int(WinRewardID)).name);
                                        }
                                    }
                                }
                            }

                            if (world->pvp_bluekillcount >= KillCounter || world->pvp_redkillcount >= KillCounter)
                            {
                                UTIL_FOREACH(from->world->characters, ch)
                                {
                                    if (ch->pvp_redteam || ch->pvp_blueteam)
                                    {
                                        ch->Undress(Character::Armor);

                                        ch->pvp_redteam = false;
                                        ch->pvp_blueteam = false;

                                        ch->Warp(ch->oldmap, ch->oldx, ch->oldy, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                                    }
                                }

                                world->ResetPVP();
                            }
                        }
                    }

                    if (character->bounty > 0)
                    {
                        from->GiveItem(1, character->bounty);
                        this->world->ServerMsg(util::ucfirst(from->SourceName()) + " has claimed " + character->SourceName() + "'s bounty of " + util::to_string(character->bounty) + " " + this->world->eif->Get(1).name);
                        character->bounty = 0;
                    }

                    if (int(this->world->config["PKExp"]) > 0)
                        from->GiveEXP(std::min(std::max(int(this->world->config["PKExp"]), 0), int(from->world->config["MaxExp"])));

                    int KillItem = util::to_int(util::explode(',', from->world->config["PKItem"])[0]);
                    int KillAmount = util::to_int(util::explode(',', from->world->config["PKItem"])[1]);

                    if (KillItem > 0 && KillAmount > 0)
                        from->GiveItem(KillItem, KillAmount);

                    character->DeathRespawn();
                }

                if (character->party)
                    character->party->UpdateHP(character);

                return true;
            }
		}

		if (!this->Walkable(target_x, target_y, true))
			return false;
	}

	return false;
}

void Map::Fish(Character *from)
{
    std::vector<Fish_Drop> drops;
    Fish_Drop *drop = 0;

    UTIL_FOREACH(this->world->fish_drops, checkdrop)
    {
        if (checkdrop->levelreq <= from->flevel)
        {
            if ((double(util::rand(0,10000)) / 100.0) < (checkdrop->chance + (from->flevel - checkdrop->levelreq) / 4))
                drops.push_back(*checkdrop);
        }
    }

    if (drops.size() > 0)
        drop = &drops[util::rand(0, drops.size() -1)];

    if (from->DelItem(util::to_int(this->world->fishing_config["FishingBait"]),1))
    {
        PacketBuilder builder;
        builder.SetID(PACKET_ITEM, PACKET_REPLY);
        builder.AddChar(EIF::Teleport);
        builder.AddShort(util::to_int(this->world->fishing_config["FishingBait"]));
        builder.AddInt(from->player->character->HasItem(util::to_int(this->world->fishing_config["FishingBait"])));
        builder.AddChar(from->player->character->weight);
        builder.AddChar(from->player->character->maxweight);
        from->Send(builder);
    }

    PacketBuilder builder;

    if (drop)
    {
        from->StatusMsg(from->world->i18n.Format("FishSuccess", this->world->eif->Get(drop->item).name));
        from->fexp = std::min(from->fexp + drop->exp, util::to_int(this->world->config["MaxExp"]));

        bool level_up = false;
        while (from->flevel < static_cast<int>(this->world->config["MaxLevel"]) && from->fexp >= this->world->exp_table[from->flevel+1])
        {
            level_up = true;
            ++from->flevel;
        }

        builder.SetID(PACKET_RECOVER, PACKET_REPLY);
        builder.AddInt(from->fexp);
        builder.AddShort(from->karma);

        if (!from->fishing_exp)
        {
            from->fishing_exp = true;
            builder.AddChar(from->flevel);
        }
        else
        {
            if (level_up)
            {
                builder.AddChar(from->flevel);
                PacketBuilder reply(PACKET_ITEM, PACKET_ACCEPT);
                reply.AddShort(from->player->id);

                UTIL_FOREACH(this->characters, character)
                {
                    if (character != from && character->InRange(from))
                        character->Send(reply);
                }
            }
            else
            {
                builder.AddChar(0);
            }
        }

        from->Send(builder);

        if (from->AddItem(drop->item, 1))
        {
            from->CalculateStats();

            PacketBuilder builder(PACKET_ITEM, PACKET_OBTAIN);
            builder.AddShort(drop->item);
            builder.AddThree(1);
            builder.AddChar(from->weight);
            from->Send(builder);
        }
    }
    else
    {
        from->StatusMsg(from->world->i18n.Format("FishFail"));
    }

    /*builder.Reset();
    builder.SetID(PACKET_ATTACK,PACKET_PLAYER);
    builder.AddShort(from->player->id);
    builder.AddChar(from->direction);

    UTIL_FOREACH(this->characters, character)
    {
        if (from->InRange(character))
            character->Send(builder);
    }*/
}

void Map::Mine(Character *from)
{
    std::vector<Mine_Drop> drops;
    Mine_Drop *drop = 0;

    UTIL_FOREACH(this->world->mine_drops, checkdrop)
    {
        if (checkdrop->levelreq <= from->mlevel)
        {
            if ((double(util::rand(0,10000)) / 100.0) < (checkdrop->chance + (from->mlevel - checkdrop->levelreq) / 4))
                drops.push_back(*checkdrop);
        }
    }

    if (drops.size() > 0)
        drop = &drops[util::rand(0, drops.size()-1)];

    PacketBuilder builder;

    if (drop)
    {
        from->StatusMsg(from->world->i18n.Format("MineSuccess", this->world->eif->Get(drop->item).name));
        from->mexp = std::min(from->mexp + drop->exp, util::to_int(this->world->config["MaxExp"]));

        bool level_up = false;

        while (from->mlevel < static_cast<int>(this->world->config["MaxLevel"]) && from->mexp >= this->world->exp_table[from->mlevel+1])
        {
            level_up = true;
            ++from->mlevel;
        }

        builder.SetID(PACKET_RECOVER, PACKET_REPLY);
        builder.AddInt(from->mexp);
        builder.AddShort(from->karma);

        if (!from->mining_exp)
        {
            from->mining_exp = true;
            builder.AddChar(from->mlevel);
        }
        else
        {
            if (level_up)
            {
                builder.AddChar(from->mlevel);
                PacketBuilder reply(PACKET_ITEM, PACKET_ACCEPT);
                reply.AddShort(from->player->id);

                UTIL_FOREACH(this->characters, character)
                {
                    if (character != from && character->InRange(from))
                        character->Send(reply);
                }
            }
            else
            {
                builder.AddChar(0);
            }
        }

        from->Send(builder);

        if (from->AddItem(drop->item, 1))
        {
            from->CalculateStats();

            PacketBuilder builder(PACKET_ITEM, PACKET_OBTAIN);
            builder.AddShort(drop->item);
            builder.AddThree(1);
            builder.AddChar(from->weight);
            from->Send(builder);
        }
    }
    else
    {
        from->StatusMsg(from->world->i18n.Format("MineFail"));
    }

    /*builder.Reset();
    builder.SetID(PACKET_ATTACK, PACKET_PLAYER);
    builder.AddShort(from->player->id);
    builder.AddChar(from->direction);

    UTIL_FOREACH(this->characters, character)
    {
        if (from->InRange(character))
            character->Send(builder);
    }*/
}

void Map::Wood(Character *from)
{
    std::vector<Wood_Drop> drops;
    Wood_Drop *drop = 0;

    UTIL_FOREACH(this->world->wood_drops, checkdrop)
    {
        if (checkdrop->levelreq <= from->wlevel)
        {
            if ((double(util::rand(0,10000)) / 100.0) < (checkdrop->chance + (from->wlevel - checkdrop->levelreq) / 4))
                drops.push_back(*checkdrop);
        }
    }

    if (drops.size() > 0)
        drop = &drops[util::rand(0, drops.size()-1)];

    PacketBuilder builder;

    if (drop)
    {
        from->StatusMsg(from->world->i18n.Format("WoodcuttingSuccess", this->world->eif->Get(drop->item).name));
        from->wexp = std::min(from->wexp + drop->exp, util::to_int(this->world->config["MaxExp"]));

        bool level_up = false;
        while (from->wlevel < static_cast<int>(this->world->config["MaxLevel"]) && from->wexp >= this->world->exp_table[from->wlevel+1])
        {
            level_up = true;
            ++from->wlevel;
        }

        builder.SetID(PACKET_RECOVER, PACKET_REPLY);
        builder.AddInt(from->wexp);
        builder.AddShort(from->karma);

        if (!from->woodcutting_exp)
        {
            from->woodcutting_exp = true;
            builder.AddChar(from->wlevel);
        }
        else
        {
            if (level_up)
            {
                builder.AddChar(from->wlevel);
                PacketBuilder reply(PACKET_ITEM, PACKET_ACCEPT);
                reply.AddShort(from->player->id);

                UTIL_FOREACH(this->characters, character)
                {
                    if (character != from && character->InRange(from))
                    {
                        character->Send(reply);
                    }
                }
            }
            else
            {
                builder.AddChar(0);
            }
        }

        from->Send(builder);

        if (from->AddItem(drop->item, 1))
        {
            from->CalculateStats();

            PacketBuilder builder(PACKET_ITEM, PACKET_OBTAIN);
            builder.AddShort(drop->item);
            builder.AddThree(1);
            builder.AddChar(from->weight);
            from->Send(builder);
        }
    }
    else
    {
        from->StatusMsg(from->world->i18n.Format("WoodcuttingFail"));
    }

    /*builder.Reset();
    builder.SetID(PACKET_ATTACK,PACKET_PLAYER);
    builder.AddShort(from->player->id);
    builder.AddChar(from->direction);

    UTIL_FOREACH(this->characters, character)
    {
        if (from->InRange(character))
            character->Send(builder);
    }*/
}

void Map::Face(Character *from, Direction direction)
{
	from->direction = direction;

	from->CancelSpell();

	PacketBuilder builder(PACKET_FACE, PACKET_PLAYER, 3);
	builder.AddShort(from->player->id);
	builder.AddChar(direction);

	UTIL_FOREACH(this->characters, character)
	{
		if (character == from || !from->InRange(character))
		{
			continue;
		}

		character->Send(builder);
	}
}

void Map::Sit(Character *from, SitState sit_type)
{
	from->sitting = sit_type;

	from->CancelSpell();

	PacketBuilder builder((sit_type == SIT_CHAIR) ? PACKET_CHAIR : PACKET_SIT, PACKET_PLAYER, 6);
	builder.AddShort(from->player->id);
	builder.AddChar(from->x);
	builder.AddChar(from->y);
	builder.AddChar(from->direction);
	builder.AddChar(0);

	UTIL_FOREACH(this->characters, character)
	{
		if (character == from || !from->InRange(character))
		{
			continue;
		}

		character->Send(builder);
	}
}

void Map::Stand(Character *from)
{
	from->sitting = SIT_STAND;

	from->CancelSpell();

	PacketBuilder builder(PACKET_SIT, PACKET_REMOVE, 4);
	builder.AddShort(from->player->id);
	builder.AddChar(from->x);
	builder.AddChar(from->y);

	UTIL_FOREACH(this->characters, character)
	{
		if (character == from || !from->InRange(character))
		{
			continue;
		}

		character->Send(builder);
	}
}

void Map::Emote(Character *from, enum Emote emote, bool echo)
{
	PacketBuilder builder(PACKET_EMOTE, PACKET_PLAYER, 3);
	builder.AddShort(from->player->id);
	builder.AddChar(emote);

	UTIL_FOREACH(this->characters, character)
	{
		if (!echo && (character == from || !from->InRange(character)))
		{
			continue;
		}

		if (!from->hidden)
            character->player->client->Send(builder);
        else if ((from->hidden && character->admin >= static_cast<int>(this->world->admin_config["seehide"])) || from == character)
            character->player->client->Send(builder);
	}
}

bool Map::Occupied(unsigned char x, unsigned char y, Map::OccupiedTarget target) const
{
	if (!InBounds(x, y))
	{
		return false;
	}

	if (target != Map::NPCOnly)
	{
		UTIL_FOREACH(this->characters, character)
		{
			if (character->x == x && character->y == y && character->CanInteractCombat())
			{
				return true;
			}
		}
	}

	if (target != Map::PlayerOnly)
	{
		UTIL_FOREACH(this->npcs, npc)
		{
			if (npc->alive && npc->x == x && npc->y == y)
			{
				return true;
			}
		}
	}

	return false;
}

Map::~Map()
{
	this->Unload();
}

bool Map::OpenDoor(Character *from, unsigned char x, unsigned char y)
{
	if (!this->InBounds(x, y) || (from && !from->InRange(x, y)))
	{
		return false;
	}

	if (Map_Warp& warp = this->GetWarp(x, y))
	{
		if (warp.spec == Map_Warp::NoDoor || warp.open)
		{
			return false;
		}

		if (from && warp.spec > Map_Warp::Door)
		{
			if (!from->CanInteractDoors() || !from->HasItem(this->world->eif->GetKey(warp.spec - static_cast<int>(Map_Warp::Door) + 1)))
			{
				return false;
			}
		}

		PacketBuilder builder(PACKET_DOOR, PACKET_OPEN, 3);
		builder.AddChar(x);
		builder.AddShort(y);

		UTIL_FOREACH(this->characters, character)
		{
			if (character->InRange(x, y))
			{
				character->Send(builder);
			}
		}

		warp.open = true;

		map_close_door_struct *close = new map_close_door_struct;
		close->map = this;
		close->x = x;
		close->y = y;

		TimeEvent *event = new TimeEvent(map_close_door, close, this->world->config["DoorTimer"], 1);
		this->world->timer.Register(event);

		return true;
	}

	return false;
}

void Map::CloseDoor(unsigned char x, unsigned char y)
{
	if (!this->InBounds(x, y))
		return;

	if (Map_Warp& warp = this->GetWarp(x, y))
	{
		if (warp.spec == Map_Warp::NoDoor || !warp.open)
		{
			return;
		}

		warp.open = false;
	}
}

void Map::SpellSelf(Character *from, unsigned short spell_id)
{
    if (!from->CanInteractCombat())
        return;

	const ESF_Data& spell = from->world->esf->Get(spell_id);

	if (!spell || spell.type != ESF::Heal || from->tp < spell.tp)
		return;

	from->tp -= spell.tp;

	int hpgain = spell.hp;

	if (this->world->config["LimitDamage"])
		hpgain = std::min(hpgain, from->maxhp - from->hp);

	hpgain = std::max(hpgain, 0);

	from->hp += hpgain;

	from->hp = std::min(from->hp, from->maxhp);

	PacketBuilder builder(PACKET_SPELL, PACKET_TARGET_SELF, 15);
	builder.AddShort(from->player->id);
	builder.AddShort(spell_id);
	builder.AddInt(spell.hp);
	builder.AddChar(util::clamp<int>(double(from->hp) / double(from->maxhp) * 100.0, 0, 100));

	UTIL_FOREACH(this->characters, character)
	{
		if (character != from && from->InRange(character))
			character->Send(builder);
	}

	builder.AddShort(from->hp);
	builder.AddShort(from->tp);
	builder.AddShort(1);

	from->Send(builder);
}

void Map::SpellAttack(Character *from, NPC *npc, unsigned short spell_id)
{
    if (!from->CanInteractCombat())
        return;

	const ESF_Data& spell = from->world->esf->Get(spell_id);

	if (!spell || spell.type != ESF::Damage || from->tp < spell.tp)
		return;

	if ((npc->Data().type == ENF::Passive || npc->Data().type == ENF::Aggressive || npc->pet) && npc->alive)
	{
	    if ((this->pk == false && npc->pet) || from == npc->owner)
            return;

	    from->tp -= spell.tp;

		int amount = util::rand(from->mindam, from->maxdam) + util::rand(spell.mindam, spell.maxdam);
		double rand = util::rand(0.0, 1.0);

		bool critical = rand < static_cast<double>(this->world->config["CriticalRate"]);

		std::unordered_map<std::string, double> formula_vars;

		from->FormulaVars(formula_vars);
		npc->FormulaVars(formula_vars, "target_");
		formula_vars["modifier"] = this->world->config["MobRate"];
		formula_vars["damage"] = amount;
		formula_vars["critical"] = critical;

		amount = rpn_eval(rpn_parse(this->world->formulas_config["damage"]), formula_vars);
		double hit_rate = rpn_eval(rpn_parse(this->world->formulas_config["hit_rate"]), formula_vars);

		if (rand > hit_rate)
		{
			amount = 0;
		}

		amount = std::max(amount, 0);

		int limitamount = std::min(amount, int(npc->hp));

		if (this->world->config["LimitDamage"])
			amount = limitamount;

		npc->Damage(from, amount, spell_id);
	}
}

void Map::SpellAttackPK(Character *from, Character *victim, unsigned short spell_id)
{
    if (!from->CanInteractCombat())
        return;

	const ESF_Data& spell = from->world->esf->Get(spell_id);

	if (!spell || (spell.type != ESF::Heal && spell.type != ESF::Damage) || from->tp < spell.tp)
	    return;

    if (from->world->esf->Get(spell_id).unkb > 0 && victim && this->world->config["WarpSpells"])
        from->Warp(victim->mapid, victim->x, victim->y, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);

    for (int i = 0; i < int(from->world->buffspells_config["Amount"]); i++)
    {
        if (spell_id == int(from->world->buffspells_config[util::to_string(i+1) + ".BuffSpell"]))
        {
            if (!victim)
                return;

            int effect = from->world->buffspells_config[util::to_string(i+1) + ".BuffEffect"];
            int str = from->world->buffspells_config[util::to_string(i+1) + ".BuffSTR"];
            int lnt = from->world->buffspells_config[util::to_string(i+1) + ".BuffINT"];
            int wis = from->world->buffspells_config[util::to_string(i+1) + ".BuffWIS"];
            int agi = from->world->buffspells_config[util::to_string(i+1) + ".BuffAGI"];
            int con = from->world->buffspells_config[util::to_string(i+1) + ".BuffCON"];
            int cha = from->world->buffspells_config[util::to_string(i+1) + ".BuffCHA"];
            int time = from->world->buffspells_config[util::to_string(i+1) + ".BuffTime"];
            double exp = from->world->buffspells_config[util::to_string(i+1) + ".EXPRate"];
            double drop = from->world->buffspells_config[util::to_string(i+1) + ".DropRate"];

            if (from->map->pk && !from->world->buffspells_config[util::to_string(i+1) + ".AllowInPK"])
            {
                if (victim->SourceName() != from->SourceName())
                {
                    from->ServerMsg("You can't cast this spell on other players right now.");
                    return;
                }
            }

            if (victim->boosttimer < Timer::GetTime())
            {
                victim->boosttimer = Timer::GetTime() + int(time);

                if (effect > 0) victim->boosteffect = effect;
                if (str > 0) victim->booststr += str;
                if (lnt > 0) victim->boostint += lnt;
                if (wis > 0) victim->boostwis += wis;
                if (agi > 0) victim->boostagi += agi;
                if (con > 0) victim->boostcon += con;
                if (cha > 0) victim->boostcha += cha;
                if (exp > 0) victim->boostexp = exp;
                if (drop > 0) victim->boostdrop = drop;

                victim->CalculateStats();
                victim->PacketRecover();
                victim->UpdateStats();

                from->ServerMsg("You have boosted " + util::ucfirst(victim->SourceName()) + " for " + util::to_string(time) + " seconds.");

                if (victim->SourceName() != from->SourceName())
                    victim->ServerMsg("You have been boosted by " + util::ucfirst(from->SourceName()) + " for " + util::to_string(time) + " seconds.");

                if (from->world->buffspells_config[util::to_string(i+1) + ".DeleteSpell"])
                {
                    from->DelSpell(spell_id);

                    PacketBuilder reply(PACKET_STATSKILL, PACKET_REMOVE, 2);
                    reply.AddShort(spell_id);
                    from->Send(reply);
                }
            }
            else
            {
                from->ServerMsg(util::ucfirst(victim->SourceName()) + " is already boosted!");
            }
        }
    }

    for (int i = 0; i < int(from->world->spells_config["FreezeSpellAmount"]); i++)
    {
        if (spell_id == int(from->world->spells_config[util::to_string(i+1) + ".FreezeSpell"]))
        {
            if (from->map->pk)
            {
                if (victim->freezetimer < Timer::GetTime())
                {
                    victim->freezetimer = Timer::GetTime() + int(from->world->spells_config[util::to_string(i+1) + ".FreezeTimer"]);

                    if (victim->admin == ADMIN_PLAYER)
                    {
                        PacketBuilder builder;
                        builder.SetID(PACKET_WALK, PACKET_CLOSE);
                        victim->Send(builder);
                    }

                    from->ServerMsg("You have frozen " + util::ucfirst(victim->SourceName()) + " for " + util::to_string(int(from->world->spells_config[util::to_string(i+1) + ".FreezeTimer"])) + " seconds.");
                    victim->ServerMsg("You are frozen by " + util::ucfirst(from->SourceName()) + " for " + util::to_string(int(from->world->spells_config[util::to_string(i+1) + ".FreezeTimer"])) + " seconds.");
                }
                else
                {
                    from->ServerMsg(util::ucfirst(victim->SourceName()) + " is already frozen!");
                }
            }
            else
            {
                from->ServerMsg("This spell can only be used in PK zones.");
            }
        }
    }

    for (int i = 0; i < int(from->world->poison_config["Amount"]); i++)
    {
        if (spell_id == int(from->world->poison_config[util::to_string(i+1) + ".PoisonSpell"]))
        {
            if (from->map->pk)
            {
                int time = int(from->world->poison_config[util::to_string(i+1) + ".PoisonTimer"]);
                int hp = int(from->world->poison_config[util::to_string(i+1) + ".PoisonHP"]);
                int tp = int(from->world->poison_config[util::to_string(i+1) + ".PoisonTP"]);
                int effect = int(from->world->poison_config[util::to_string(i+1) + ".PoisonEffect"]);

                if (victim->poisontimer < Timer::GetTime())
                {
                    from->ServerMsg("You poisoned " + util::ucfirst(victim->SourceName()) + " for " + util::to_string(time) + " seconds.");

                    victim->poisontimer = Timer::GetTime() + int(time);
                    victim->poisonhp = hp;
                    victim->poisontp = tp;
                    victim->poisoneffect = effect;

                    victim->ServerMsg(util::ucfirst(from->SourceName()) + " poisoned you for " + util::to_string(time) + " seconds.");
                }
                else
                {
                    from->ServerMsg(util::ucfirst(victim->SourceName()) + " is already poisoned.");
                }
            }
            else
            {
                from->ServerMsg("This spell can only be used in PK zones.");
            }
        }
    }

	if (spell.type == ESF::Damage && (from->map->pk || (this->world->config["GlobalPK"] && !this->world->PKExcept(this->id))))
	{
		from->tp -= spell.tp;

		int amount = (util::rand(from->mindam, from->maxdam) + util::rand(spell.mindam, spell.maxdam)) * double(this->world->config["PKRate"]);;
		double rand = util::rand(0.0, 1.0);
		bool critical = rand < static_cast<double>(this->world->config["CriticalRate"]);

		std::unordered_map<std::string, double> formula_vars;

		from->FormulaVars(formula_vars);
		victim->FormulaVars(formula_vars, "target_");
		formula_vars["modifier"] = this->world->config["PKRate"];
		formula_vars["damage"] = amount;
		formula_vars["critical"] = critical;

		amount = rpn_eval(rpn_parse(this->world->formulas_config["damage"]), formula_vars);
		double hit_rate = rpn_eval(rpn_parse(this->world->formulas_config["hit_rate"]), formula_vars);

		if (rand > hit_rate)
			amount = 0;

        if (from->player->character->party)
        {
            UTIL_FOREACH(from->player->character->party->members, member)
            {
                if (member->player->character == victim->player->character)
                {
                    from->player->character->StatusMsg("You can't attack your own party members!");
                    return;
                }
            }
        }

        if (this->world->config["pkguildprotection"])
        {
            if (from->player->character->guild)
            {
                UTIL_FOREACH(from->player->character->guild->members, member)
                {
                    Character *guildmember = this->world->GetCharacter(member->name);

                    if (guildmember->player->character == victim->player->character)
                    {
                        from->player->character->StatusMsg("You can't attack your own guild members!");
                        return;
                    }
                }
            }
        }

        if (this->world->config["racewars"])
        {
            if ((from->race == 0 || from->race == 1 || from->race == 2) && (victim->race == from->race))
            {
                from->StatusMsg("You cannot attack your own allies!");
                return;
            }
            else if (from->race == SKIN_ORC && victim->race == SKIN_ORC)
            {
                from->StatusMsg("You cannot attack your own allies!");
                return;
            }
        }

        if (this->world->config["classwars"])
        {
            if (from->clas == victim->clas)
            {
                from->StatusMsg("You cannot attack your own allies!");
                return;
            }
        }

        if (this->world->config["AntiGank"])
        {
            int AntiGank(int minlevel,int maxlevel);

            if (AntiGank(victim->level, from->level) >= int(this->world->config["AntiGankLevel"]))
            {
                from->StatusMsg("Cannot attack target - Not within level range [" + util::to_string(static_cast<int>(this->world->config["AntiGankLevel"])) + "]");
                return;
            }
        }

        if (from->mapid == int(this->world->pvp_config["PVPMap"]))
        {
            if (world->pvp)
            {
                if ((from->pvp_blueteam && victim->pvp_blueteam) || (from->pvp_redteam && victim->pvp_redteam))
                {
                    from->StatusMsg("You cannot attack your own team members!");
                    return;
                }
            }
            else if (!world->pvp)
            {
                from->StatusMsg("PVP has not started yet!");
                return;
            }
        }

		amount = std::max(amount, 0);

		int limitamount = std::min(amount, int(victim->hp));

		if (this->world->config["LimitDamage"])
			amount = limitamount;

        if (victim->immune == false)
            victim->hp -= limitamount;

        PacketBuilder builder(PACKET_AVATAR, PACKET_ADMIN, 12);
        builder.AddShort(from->player->id);
        builder.AddShort(victim->player->id);
        builder.AddThree(victim->immune == false ? amount : 0);
        builder.AddChar(from->direction);
        builder.AddChar(util::clamp<int>(double(victim->hp) / double(victim->maxhp) * 100.0, 0, 100));
        builder.AddChar(victim->hp == 0);
        builder.AddShort(spell_id);

        UTIL_FOREACH(this->characters, character)
        {
            if (victim->InRange(character))
                character->Send(builder);
        }

		if (victim->hp == 0)
		{
            UTIL_FOREACH(from->quests, q)
			{
				if (!q.second || q.second->GetQuest()->Disabled())
					continue;

				q.second->KilledPlayer();
			}

		    if (!from->HasAchievement(std::string(this->world->achievements_config["PlayerKill.Achievement"])))
            {
                if (std::string(this->world->achievements_config["PlayerKill.Achievement"]) != "0")
                {
                    from->GiveEXP(std::min(std::max(int(this->world->achievements_config["PlayerKill.Reward"]), 0), int(from->world->config["MaxExp"])));

                    if (std::string(this->world->achievements_config["PlayerKill.Description"]) != "0")
                        from->ServerMsg(std::string(this->world->achievements_config["PlayerKill.Description"]));

                    if (std::string(from->world->achievements_config["PlayerKill.Announcement"]) == "yes")
                        from->world->ServerMsg(util::ucfirst(from->SourceName()) + " earned the achievement '" + std::string(from->world->achievements_config["PlayerKill.Achievement"]) + "'.");

                    from->NewAchievement(std::string(this->world->achievements_config["PlayerKill.Achievement"]));
                }
            }

            if (from->world->pvp == true)
            {
                if (from->mapid == int(from->world->pvp_config["PVPMap"]))
                {
                    Map *maps = world->GetMap(int(world->pvp_config["PVPMap"]));

                    int KillCounter = int(from->world->pvp_config["KillsToWin"]);
                    int KillRewardID = int(from->world->pvp_config["KillRewardID"]);
                    int KillRewardAmount = int(from->world->pvp_config["KillRewardAmount"]);

                    int WinRewardID = int(from->world->pvp_config["WinRewardID"]);
                    int WinRewardAmount = int(from->world->pvp_config["WinRewardAmount"]);

                    if (from->pvp_blueteam == true)
                    {
                        if (victim->pvp_redteam)
                            world->pvp_bluekillcount++;

                        UTIL_FOREACH(maps->characters, ch)
                        {
                            ch->ServerMsg("Someone of the red team has been killed, the blue team's current current kill count is " + util::to_string(world->pvp_bluekillcount) + "/" + util::to_string(KillCounter));
                        }
                    }
                    else if (from->pvp_redteam == true)
                    {
                        if (victim->pvp_blueteam)
                            world->pvp_redkillcount++;

                        UTIL_FOREACH(maps->characters, ch)
                        {
                            ch->ServerMsg("Someone of the blue team has been killed, the red team's current current kill count is " + util::to_string(world->pvp_redkillcount) + "/" + util::to_string(KillCounter));
                        }
                    }

                    if (KillRewardID > 0 && KillRewardAmount > 0)
                    {
                        from->GiveItem(KillRewardID, KillRewardAmount);
                        from->StatusMsg("You have been rewarded with " + util::to_string(KillRewardAmount) + " " + from->world->eif->Get(KillRewardID).name + " for killing an enemy.");
                    }

                    if (world->pvp_bluekillcount >= KillCounter)
                    {
                        UTIL_FOREACH(maps->characters, ch)
                        {
                            ch->ServerMsg("PVP has ended, the blue team has won, congratulations!");
                        }

                        UTIL_FOREACH(from->map->characters, checkchar)
                        {
                            if (checkchar->pvp_blueteam)
                            {
                                if (WinRewardID > 0 && WinRewardAmount > 0)
                                {
                                    checkchar->GiveItem(WinRewardID, WinRewardAmount);
                                    checkchar->ServerMsg("You have been rewarded with " + util::to_string(WinRewardAmount) + " " + from->world->eif->Get(WinRewardID).name);
                                }
                            }
                        }
                    }
                    else if (world->pvp_redkillcount >= KillCounter)
                    {
                        UTIL_FOREACH(maps->characters, ch)
                        {
                            ch->ServerMsg("PVP has ended, the red team has won, congratulations!");
                        }

                        UTIL_FOREACH(from->map->characters, checkchar)
                        {
                            if (checkchar->pvp_redteam)
                            {
                                if (WinRewardID > 0 && WinRewardAmount > 0)
                                {
                                    checkchar->GiveItem(WinRewardID, WinRewardAmount);
                                    checkchar->ServerMsg("You have been rewarded with " + util::to_string(WinRewardAmount) + " " + from->world->eif->Get(WinRewardID).name);
                                }
                            }
                        }
                    }

                    if (world->pvp_bluekillcount >= KillCounter || world->pvp_redkillcount >= KillCounter)
                    {
                        UTIL_FOREACH(from->world->characters, ch)
                        {
                            if (ch->pvp_redteam || ch->pvp_blueteam)
                            {
                                ch->Undress(Character::Armor);

                                ch->pvp_redteam = false;
                                ch->pvp_blueteam = false;

                                ch->Warp(ch->oldmap, ch->oldx, ch->oldy, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                            }
                        }

                        world->ResetPVP();
                    }
                }
            }

            if (victim->bounty > 0)
            {
                from->GiveItem(1, victim->bounty);
                this->world->ServerMsg(from->SourceName() + " has claimed " + victim->SourceName() + "'s bounty of " + util::to_string(victim->bounty) + " " + this->world->eif->Get(1).name);
                victim->bounty = 0;
            }

            if (int(this->world->config["PKExp"]) > 0)
                from->GiveEXP(std::min(std::max(int(this->world->config["PKExp"]), 0), int(from->world->config["MaxExp"])));

            int KillItem = util::to_int(util::explode(',', from->world->config["PKItem"])[0]);
            int KillAmount = util::to_int(util::explode(',', from->world->config["PKItem"])[1]);

            if (KillItem > 0 && KillAmount > 0)
                from->GiveItem(KillItem, KillAmount);

			victim->DeathRespawn();
		}

        builder.Reset(4);
		builder.SetID(PACKET_RECOVER, PACKET_PLAYER);
		builder.AddShort(victim->hp);
		builder.AddShort(victim->tp);
		victim->Send(builder);

		if (victim->party)
			victim->party->UpdateHP(victim);
	}
	else if (spell.type == ESF::Heal)
	{
		from->tp -= spell.tp;

		int hpgain = spell.hp;

		if (this->world->config["LimitDamage"])
		{
		    hpgain = std::min(hpgain, victim->maxhp - victim->hp);
		}

		hpgain = std::max(hpgain, 0);

		victim->hp += hpgain;

		if (!this->world->config["LimitDamage"])
		{
		    victim->hp = std::min(victim->hp, victim->maxhp);
		}

		PacketBuilder builder(PACKET_SPELL, PACKET_TARGET_OTHER, 18);
		builder.AddShort(victim->player->id);
		builder.AddShort(from->player->id);
		builder.AddChar(from->direction);
		builder.AddShort(spell_id);
		builder.AddInt(spell.hp);
		builder.AddChar(util::clamp<int>(double(victim->hp) / double(victim->maxhp) * 100.0, 0, 100));

		UTIL_FOREACH(this->characters, character)
		{
			if (character != victim && victim->InRange(character))
			{
			    character->Send(builder);
			}
		}

		builder.AddShort(victim->hp);
		victim->Send(builder);

		if (victim->party)
			victim->party->UpdateHP(victim);
	}

	PacketBuilder builder(PACKET_RECOVER, PACKET_PLAYER, 4);
	builder.AddShort(from->hp);
	builder.AddShort(from->tp);
	from->Send(builder);
}

void Map::SpellGroup(Character *from, unsigned short spell_id)
{
    if (!from->CanInteractCombat())
        return;

	const ESF_Data& spell = from->world->esf->Get(spell_id);

	if (!spell || spell.type != ESF::Heal || !from->party || from->tp < spell.tp)
	    return;

	from->tp -= spell.tp;

	int hpgain = spell.hp;

	if (this->world->config["LimitDamage"])
	{
	    hpgain = std::min(hpgain, from->maxhp - from->hp);
	}

	hpgain = std::max(hpgain, 0);

	from->hp += hpgain;

	from->hp = std::min(from->hp, from->maxhp);

	std::set<Character *> in_range;

	PacketBuilder builder(PACKET_SPELL, PACKET_TARGET_GROUP, 8 + from->party->members.size() * 10);
	builder.AddShort(spell_id);
	builder.AddShort(from->player->id);
	builder.AddShort(from->tp);
	builder.AddShort(spell.hp);

	UTIL_FOREACH(from->party->members, member)
	{
		if (member->map != from->map)
		{
		    continue;
		}

		int hpgain = spell.hp;

		if (this->world->config["LimitDamage"])
		{
		    hpgain = std::min(hpgain, member->maxhp - member->hp);
		}

		hpgain = std::max(hpgain, 0);

		member->hp += hpgain;

		if (this->world->config["LimitDamage"])
		{
		    member->hp = std::min(member->hp, member->maxhp);
		}

		builder.AddByte(255);
		builder.AddByte(255);
		builder.AddByte(255);
		builder.AddByte(255);
		builder.AddByte(255);

		builder.AddShort(member->player->id);
		builder.AddChar(util::clamp<int>(double(member->hp) / double(member->maxhp) * 100.0, 0, 100));
		builder.AddShort(member->hp);

		UTIL_FOREACH(this->characters, character)
		{
			if (member->InRange(character))
			{
			    in_range.insert(character);
			}
		}
	}

	UTIL_FOREACH(in_range, character)
	{
		character->Send(builder);
	}
}

std::shared_ptr<Map_Item> Map::AddItem(short id, int amount, unsigned char x, unsigned char y, Character *from)
{
	std::shared_ptr<Map_Item> newitem(std::make_shared<Map_Item>(0, id, amount, x, y, 0, 0));

	if (from || (from && from->SourceAccess() <= ADMIN_GM))
	{
		int ontile = 0;
		int onmap = 0;

		UTIL_FOREACH(this->items, item)
		{
			++onmap;
			if (item->x == x && item->y == y)
			{
				++ontile;
			}
		}

		if (ontile >= static_cast<int>(this->world->config["MaxTile"]) || onmap >= static_cast<int>(this->world->config["MaxMap"]))
		{
			return newitem;
		}
	}

	newitem->uid = GenerateItemID();

	PacketBuilder builder(PACKET_ITEM, PACKET_ADD, 9);
	builder.AddShort(id);
	builder.AddShort(newitem->uid);
	builder.AddThree(amount);
	builder.AddChar(x);
	builder.AddChar(y);

	UTIL_FOREACH(this->characters, character)
	{
		if ((from && character == from) || !character->InRange(*newitem))
		{
			continue;
		}

		character->Send(builder);
	}

	this->items.push_back(newitem);
	return newitem;
}

std::shared_ptr<Map_Item> Map::GetItem(short uid)
{
	UTIL_FOREACH(this->items, item)
	{
		if (item->uid == uid)
		{
			return item;
		}
	}

	return std::shared_ptr<Map_Item>();
}

std::shared_ptr<const Map_Item> Map::GetItem(short uid) const
{
	UTIL_FOREACH(this->items, item)
	{
		if (item->uid == uid)
		{
			return item;
		}
	}

	return std::shared_ptr<Map_Item>();
}

void Map::DelItem(short uid, Character *from)
{
	UTIL_IFOREACH(this->items, it)
	{
		if ((*it)->uid == uid)
		{
			this->DelItem(it, from);
			break;
		}
	}
}

std::list<std::shared_ptr<Map_Item>>::iterator Map::DelItem(std::list<std::shared_ptr<Map_Item>>::iterator it, Character *from)
{
	PacketBuilder builder(PACKET_ITEM, PACKET_REMOVE, 2);
	builder.AddShort((*it)->uid);

	UTIL_FOREACH(this->characters, character)
	{
		if ((from && character == from) || !character->InRange(**it))
		{
			continue;
		}

		character->Send(builder);
	}

	return this->items.erase(it);
}

void Map::DelSomeItem(short uid, int amount, Character *from)
{
	if (amount < 0)
		return;

	UTIL_IFOREACH(this->items, it)
	{
		if ((*it)->uid == uid)
		{
			if (amount < (*it)->amount)
			{
				(*it)->amount -= amount;

				PacketBuilder builder(PACKET_ITEM, PACKET_REMOVE, 2);
				builder.AddShort((*it)->uid);

				UTIL_FOREACH(this->characters, character)
				{
					if ((from && character == from) || !character->InRange(**it))
					{
						continue;
					}

					character->Send(builder);
				}

				builder.Reset(9);
				builder.SetID(PACKET_ITEM, PACKET_ADD);
				builder.AddShort((*it)->id);
				builder.AddShort((*it)->uid);
				builder.AddThree((*it)->amount);
				builder.AddChar((*it)->x);
				builder.AddChar((*it)->y);

				UTIL_FOREACH(this->characters, character)
				{
					if (!character->InRange(**it))
						continue;

					character->Send(builder);
				}
			}
			else
			{
				this->DelItem(it, from);
			}

			break;
		}
	}
}

bool Map::InBounds(unsigned char x, unsigned char y) const
{
	return !(x >= this->width || y >= this->height);
}

bool Map::Walkable(unsigned char x, unsigned char y, bool npc) const
{
	if (!InBounds(x, y) || !this->GetTile(x, y).Walkable(npc))
		return false;

	if (this->world->config["GhostArena"] && this->GetTile(x, y).tilespec == Map_Tile::Arena && this->Occupied(x, y, PlayerAndNPC))
		return false;

	return true;
}

Map_Tile& Map::GetTile(unsigned char x, unsigned char y)
{
	if (!InBounds(x, y))
		throw std::out_of_range("Map tile out of range");

	return this->tiles[y * this->width + x];
}

const Map_Tile& Map::GetTile(unsigned char x, unsigned char y) const
{
	if (!InBounds(x, y))
		throw std::out_of_range("Map tile out of range");

	return this->tiles[y * this->width + x];
}

Map_Tile::TileSpec Map::GetSpec(unsigned char x, unsigned char y) const
{
	if (!InBounds(x, y))
		return Map_Tile::None;

	return this->GetTile(x, y).tilespec;
}

Map_Warp& Map::GetWarp(unsigned char x, unsigned char y)
{
	return this->GetTile(x, y).warp;
}

const Map_Warp& Map::GetWarp(unsigned char x, unsigned char y) const
{
	return this->GetTile(x, y).warp;
}

std::vector<Character *> Map::CharactersInRange(unsigned char x, unsigned char y, unsigned char range)
{
	std::vector<Character *> characters;

	UTIL_FOREACH(this->characters, character)
	{
		if (util::path_length(character->x, character->y, x, y) <= range)
			characters.push_back(character);
	}

	return characters;
}

std::vector<NPC *> Map::NPCsInRange(unsigned char x, unsigned char y, unsigned char range)
{
	std::vector<NPC *> npcs;

	UTIL_FOREACH(this->npcs, npc)
	{
		if (util::path_length(npc->x, npc->y, x, y) <= range)
			npcs.push_back(npc);
	}

	return npcs;
}

void Map::Effect(MapEffect effect, unsigned char param)
{
	PacketBuilder builder(PACKET_EFFECT, PACKET_USE, 2);
	builder.AddChar(effect);
	builder.AddChar(param);

	UTIL_FOREACH(this->characters, character)
	{
		character->Send(builder);
	}
}

bool Map::Evacuate()
{
	if (!this->evacuate_lock)
	{
		this->evacuate_lock = true;

		map_evacuate_struct *evac = new map_evacuate_struct;
		evac->map = this;
		evac->step = int(evac->map->world->config["EvacuateLength"]) / int(evac->map->world->config["EvacuateTick"]);

		TimeEvent *event = new TimeEvent(map_evacuate, evac, this->world->config["EvacuateTick"], evac->step);
		this->world->timer.Register(event);

		map_evacuate(evac);
		return true;
	}
	else
	{
		return false;
	}
}

bool Map::Reload()
{
	char namebuf[6];
	char checkrid[4];

	std::string filename = this->world->config["MapDir"];
	std::sprintf(namebuf, "%05i", this->id);
	filename.append(namebuf);
	filename.append(".emf");

	std::FILE *fh = std::fopen(filename.c_str(), "rb");

	if (!fh)
	{
		return false;
	}

	SAFE_SEEK(fh, 0x03, SEEK_SET);
	SAFE_READ(checkrid, sizeof(char), 4, fh);

	if (this->rid[0] == checkrid[0] && this->rid[1] == checkrid[1]
	 && this->rid[2] == checkrid[2] && this->rid[3] == checkrid[3])
	{
		return true;
	}

	std::list<Character *> temp = this->characters;

	this->Unload();

	if (!this->Load())
		return false;

	this->characters = temp;

	UTIL_FOREACH(temp, character)
	{
		character->player->client->Upload(FILE_MAP, character->mapid, INIT_MAP_MUTATION);
		character->Refresh(); // TODO: Find a better way to reload NPCs
	}

	this->exists = true;

	return true;
}

Character *Map::GetCharacter(std::string name)
{
	name = util::lowercase(name);

	UTIL_FOREACH(this->characters, character)
	{
		if (character->SourceName().compare(name) == 0)
		{
			return character;
		}
	}

	return 0;
}

Character *Map::GetCharacterPID(unsigned int id)
{
	UTIL_FOREACH(this->characters, character)
	{
		if (character->player->id == id)
		{
			return character;
		}
	}

	return 0;
}

Character *Map::GetCharacterCID(unsigned int id)
{
	UTIL_FOREACH(this->characters, character)
	{
		if (character->id == id)
		{
			return character;
		}
	}

	return 0;
}

NPC *Map::GetNPCIndex(unsigned char index)
{
	UTIL_FOREACH(this->npcs, npc)
	{
		if (npc->index == index)
		{
			return npc;
		}
	}

	return 0;
}
