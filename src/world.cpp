#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <map>
#include <stdexcept>
#include <fstream>

#include "console.hpp"
#include "database.hpp"
#include "hash.hpp"
#include "util.hpp"

#include "character.hpp"
#include "command_source.hpp"
#include "config.hpp"
#include "eoclient.hpp"
#include "eodata.hpp"
#include "eoserver.hpp"
#include "guild.hpp"
#include "map.hpp"
#include "npc.hpp"
#include "packet.hpp"
#include "party.hpp"
#include "player.hpp"
#include "quest.hpp"
#include "chat.hpp"
#include "commands.hpp"

#include "eoplus.hpp"

#include "commands/admins/commands.hpp"
#include "commands/players/commands.hpp"

#include "handlers/handlers.hpp"

void world_execute_weddings(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    double now = Timer::GetTime();

    UTIL_FOREACH(world->maps, map)
    {
        UTIL_FOREACH(map->npcs, npc)
        {
            if (npc->marriage && npc->marriage->request_accepted)
            {
                if (!npc->marriage->partner[0] || !npc->marriage->partner[1])
                {
                    npc->ShowDialog(world->i18n.Format("WeddingError"));
                    npc->marriage = 0;

                    continue;
                }
                else if (!npc->marriage->partner[0]->online || !npc->marriage->partner[1]->online)
                {
                    npc->ShowDialog(world->i18n.Format("WeddingMissingPartner"));
                    npc->marriage = 0;

                    continue;
                }
                else if (npc->marriage->partner[0]->map != npc->map || npc->marriage->partner[1]->map != npc->map)
                {
                    npc->ShowDialog(world->i18n.Format("WeddingMissingPartner"));
                    npc->marriage = 0;

                    continue;
                }
                else if ((npc->marriage->state == 5 && npc->marriage->partner_accepted[0]) || (npc->marriage->state == 8 && npc->marriage->partner_accepted[1]))
                {
                    ++npc->marriage->state;
                }
                else if (npc->marriage->last_execution + (util::to_int(world->config["PriestDialogInterval"])) <= now)
                {
                    switch (npc->marriage->state)
                    {
                        case 1:
                        {
                            npc->ShowDialog(world->i18n.Format("WeddingText1", util::ucfirst(npc->marriage->partner[0]->SourceName()), util::ucfirst(npc->marriage->partner[1]->SourceName())));
                            ++npc->marriage->state;
                        }
                        break;

                        case 2:
                        {
                            npc->ShowDialog(world->i18n.Format("WeddingText2"));
                            ++npc->marriage->state;
                        }
                        break;

                        case 3:
                        {
                            npc->ShowDialog(world->i18n.Format("WeddingText3", util::ucfirst(npc->marriage->partner[0]->SourceName()), util::ucfirst(npc->marriage->partner[1]->SourceName())));
                            ++npc->marriage->state;
                        }
                        break;

                        case 4:
                        {
                            PacketBuilder builder(PACKET_PRIEST, PACKET_REPLY);
                            builder.AddChar(PRIEST_REQUEST);
                            npc->marriage->partner[0]->player->client->Send(builder);
                            ++npc->marriage->state;
                        }
                        break;

                        case 5:
                        {
                            ++npc->marriage->state;
                        }
                        break;

                        case 6:
                        {
                            npc->ShowDialog(world->i18n.Format("WeddingText3", util::ucfirst(npc->marriage->partner[1]->SourceName()), util::ucfirst(npc->marriage->partner[0]->SourceName())));
                            ++npc->marriage->state;
                        }
                        break;

                        case 7:
                        {
                            PacketBuilder builder(PACKET_PRIEST, PACKET_REPLY);
                            builder.AddChar(PRIEST_REQUEST);
                            npc->marriage->partner[1]->player->client->Send(builder);
                            ++npc->marriage->state;
                        }
                        break;

                        case 8:
                        {
                            ++npc->marriage->state;
                        }
                        break;

                        case 9:
                        {
                            npc->ShowDialog(world->i18n.Format("WeddingRing1"));
                            ++npc->marriage->state;
                        }
                        break;

                        case 10:
                        {
                            PacketBuilder builder(PACKET_ITEM, static_cast<PacketAction>(26));
                            builder.AddShort(util::to_int(world->config["WeddingRing"]));
                            builder.AddThree(1);
                            for (int i = 0; i < 2; ++i)
                            {
                                npc->marriage->partner[i]->AddItem(util::to_int(world->config["WeddingRing"]), 1);
                                npc->marriage->partner[i]->player->client->Send(builder);
                            }
                            ++npc->marriage->state;
                        }

                        case 11:
                        {
                            npc->ShowDialog(world->i18n.Format("WeddingRing2"));
                            ++npc->marriage->state;
                        }
                        break;

                        case 12:
                        {
                            int effect = util::to_int(util::explode(',', world->config["WeddingEffects"])[0]);

                            for (int i = 0; i < 2; ++i)
                                npc->marriage->partner[i]->Effect(effect);

                            npc->ShowDialog(world->i18n.Format("WeddingFinish1", util::ucfirst(npc->marriage->partner[0]->SourceName()), util::ucfirst(npc->marriage->partner[1]->SourceName())));
                            ++npc->marriage->state;
                        }
                        break;

                        case 13:
                        {
                            int effect = util::to_int(util::explode(',', world->config["WeddingEffects"])[1]);

                            for (int i = 0; i < 2; ++i)
                                npc->marriage->partner[i]->Effect(effect);

                            ++npc->marriage->state;
                        }
                        break;

                        case 14:
                        {
                            npc->ShowDialog(world->i18n.Format("WeddingFinish2"));

                            PacketBuilder reply;
                            reply.SetID(PACKET_JUKEBOX, PACKET_USE);
                            reply.AddShort(util::rand(7, 20));

                            UTIL_FOREACH(npc->marriage->partner[0]->map->characters, character)
                            {
                                character->player->client->Send(reply);
                            }

                            int effect = util::to_int(util::explode(',', world->config["WeddingEffects"])[2]);

                            for (int i = 0; i < 2; ++i)
                                npc->marriage->partner[i]->Effect(effect);

                            npc->marriage->partner[0]->partner = npc->marriage->partner[1]->SourceName();
                            npc->marriage->partner[0]->fiance = "";

                            npc->marriage->partner[1]->partner = npc->marriage->partner[0]->SourceName();
                            npc->marriage->partner[1]->fiance = "";
                            npc->marriage = 0;
                        }
                        break;

                        default:

                        Console::Err("Invalid state for marriage ceremony.");
                        npc->marriage = 0;
                    }

                    if (npc->marriage)
                    npc->marriage->last_execution = Timer::GetTime();
                }
            }
        }
    }
}

void World::UpdateConfig()
{
    this->timer.SetMaxDelta(this->config["ClockMaxDelta"]);

	double rate_face = this->config["PacketRateFace"];
	double rate_walk = this->config["PacketRateWalk"];
	double rate_attack = this->config["PacketRateAttack"];

	Handlers::SetDelay(PACKET_FACE, PACKET_PLAYER, rate_face);

	Handlers::SetDelay(PACKET_WALK, PACKET_ADMIN, rate_walk);
	Handlers::SetDelay(PACKET_WALK, PACKET_PLAYER, rate_walk);
	Handlers::SetDelay(PACKET_WALK, PACKET_SPEC, rate_walk);

	Handlers::SetDelay(PACKET_ATTACK, PACKET_USE, rate_attack);

	std::array<double, 7> npc_speed_table;

	std::vector<std::string> rate_list = util::explode(',', this->config["NPCMovementRate"]);

	for (std::size_t i = 0; i < std::min<std::size_t>(7, rate_list.size()); ++i)
	{
		if (i < rate_list.size())
			npc_speed_table[i] = util::tdparse(rate_list[i]);
		else
			npc_speed_table[i] = 1.0;
	}

	NPC::SetSpeedTable(npc_speed_table);

	this->i18n.SetLangFile(this->config["ServerLanguage"]);

	this->instrument_ids.clear();

	std::vector<std::string> instrument_list = util::explode(',', this->config["InstrumentItems"]);

	this->instrument_ids.reserve(instrument_list.size());

	for (std::size_t i = 0; i < instrument_list.size(); ++i)
	{
		this->instrument_ids.push_back(int(util::tdparse(instrument_list[i])));
	}

	if (this->db.Pending() && !this->config["TimedSave"])
        this->CommitDB();
}

void world_spawn_npcs(void *world_void)
{
	World *world(static_cast<World *>(world_void));

	double spawnrate = world->config["SpawnRate"];
	double current_time = Timer::GetTime();

	UTIL_FOREACH(world->maps, map)
	{
		UTIL_FOREACH(map->npcs, npc)
		{
			if ((!npc->alive && npc->dead_since + (double(npc->spawn_time) * spawnrate) < current_time)
            && (!npc->Data().child || (npc->parent && npc->parent->alive && world->config["RespawnBossChildren"])))
			{
                #ifdef DEBUG
				Console::Dbg("Spawning NPC %i on map %i", npc->id, map->id);
                #endif

                npc->killowner = 0;
				npc->Spawn();
			}
		}
	}
}

void world_act_npcs(void *world_void)
{
	World *world(static_cast<World *>(world_void));

	double current_time = Timer::GetTime();
	UTIL_FOREACH(world->maps, map)
	{
		UTIL_FOREACH(map->npcs, npc)
		{
			if (npc->alive && npc->last_act + npc->act_speed < current_time)
				npc->Act();
		}
	}
}

void world_recover(void *world_void)
{
	World *world(static_cast<World *>(world_void));

	UTIL_FOREACH(world->characters, character)
	{
		if (character->hp != character->maxhp)
		{
			if (character->sitting != SIT_STAND)
			{
			    character->hp += character->maxhp * double(world->config["SitHPRecoverRate"]);
			}
			else
			{
			    character->hp += character->maxhp * double(world->config["HPRecoverRate"]);
			}

			character->hp = std::min(character->hp, character->maxhp);
			character->PacketRecover();

			if (character->party)
				character->party->UpdateHP(character);
		}

		if (character->tp != character->maxtp)
		{
			if (character->sitting != SIT_STAND)
			{
			    character->tp += character->maxtp * double(world->config["SitTPRecoverRate"]);
			}
			else
			{
			    character->tp += character->maxtp * double(world->config["TPRecoverRate"]);
			}

			character->tp = std::min(character->tp, character->maxtp);
			character->PacketRecover();
		}
	}
}

void world_npc_recover(void *world_void)
{
	World *world(static_cast<World *>(world_void));

	UTIL_FOREACH(world->maps, map)
	{
		UTIL_FOREACH(map->npcs, npc)
		{
			if (npc->alive && npc->hp < npc->Data().hp)
			{
				npc->hp += npc->Data().hp * double(world->config["NPCRecoverRate"]);
				npc->hp = std::min(npc->hp, npc->Data().hp);
			}

            if (npc->alive && npc->tp < npc->maxtp && npc->pet)
            {
				npc->tp += npc->maxtp * double(world->config["NPCRecoverRate"]);
				npc->tp = std::min(npc->tp, npc->maxtp);
			}
		}
	}
}

void world_startdevil(void *world_void)
{
	World *world(static_cast<World *>(world_void));

	if (world->DevilGateEnabled == false)
    {
        world->devil = true;
        world->DevilTimer();

        world->ServerMsg("The devil gate has now been opened, type " + std::string(world->config["PlayerPrefix"]) + "devil to enter. The gate will close in " + util::to_string(int(world->devilgate_config["StartTimer"])) + " seconds.");
    }
    else
    {
        Console::Wrn("Failed to open devilgate, event still in progress.");
    }
}

void world_pvp_autostart(void *world_void)
{
	World *world(static_cast<World *>(world_void));

	if (world->pvp == false)
    {
        world->pvpenabled = true;
        world->PVPTimer();

        world->ServerMsg("PVP is now accessable, type " + std::string(world->config["PlayerPrefix"]) + "pvp to enter. Access will be denied in " + util::to_string(int(world->pvp_config["StartTimer"])) + " seconds.");
    }
    else
    {
        Console::Wrn("Failed to start PVP, event still in progress.");
    }
}

void world_warp_suck(void *world_void)
{
	struct Warp_Suck_Action
	{
		Character *character;
		short map;
		unsigned char x;
		unsigned char y;

		Warp_Suck_Action(Character *character, short map, unsigned char x, unsigned char y)
			: character(character)
			, map(map)
			, x(x)
			, y(y)
		{ }
	};

	std::vector<Warp_Suck_Action> actions;

	World *world(static_cast<World *>(world_void));

	double now = Timer::GetTime();
	double delay = world->config["WarpSuck"];

	UTIL_FOREACH(world->maps, map)
	{
		UTIL_FOREACH(map->characters, character)
		{
			if (character->last_walk + delay >= now)
				continue;

			auto check_warp = [&](bool test, unsigned char x, unsigned char y)
			{
				if (!test || !map->InBounds(x, y))
					return;

				const Map_Warp& warp = map->GetWarp(x, y);

				if (!warp || warp.levelreq > character->level || (warp.spec != Map_Warp::Door && warp.spec != Map_Warp::NoDoor))
					return;

				actions.push_back({character, warp.map, warp.x, warp.y});
			};

			character->last_walk = now;

			check_warp(true,                       character->x,     character->y);
			check_warp(character->x > 0,           character->x - 1, character->y);
			check_warp(character->x < map->width,  character->x + 1, character->y);
			check_warp(character->y > 0,           character->x,     character->y - 1);
			check_warp(character->y < map->height, character->x,     character->y + 1);
		}
	}

	UTIL_FOREACH(actions, act)
	{
		if (act.character->SourceAccess() < ADMIN_GUIDE && world->GetMap(act.map)->evacuate_lock)
		{
			act.character->StatusMsg(world->i18n.Format("EvacuateBlock"));
			act.character->Refresh();
		}
		else
		{
			act.character->Warp(act.map, act.x, act.y);
		}
	}
}

bool World::SpawnDevilNPC(int id)
{
    unsigned char npc_index = this->GetMap(int(this->devilgate_config["DevilMap"]))->GenerateNPCIndex();

    if (npc_index > 250)
    {
        return false;
    }

    if (id < 0 || std::size_t(id) > this->enf->data.size())
    {
        return false;
    }

    NPC *npc = new NPC(this->GetMap(int(this->devilgate_config["DevilMap"])), id, util::rand(int(this->devilgate_config["DevilSpawnX.1"]), int(this->devilgate_config["DevilSpawnX.2"])), util::rand(int(this->devilgate_config["DevilSpawnY.1"]), int(this->devilgate_config["DevilSpawnY.2"])), 1, 2, npc_index, true);
    this->GetMap(int(this->devilgate_config["DevilMap"]))->npcs.push_back(npc);
    npc->Spawn();

    return true;
}

void world_devilgate(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    if (world->DevilGateEnabled == true)
    {
        if (world->wave != int(world->devilgate_config["MaxWaves"]) + 1)
        {
            Map *maps = world->GetMap(int(world->devilgate_config["DevilMap"]));

            int counter = 0;

            UTIL_FOREACH(maps->npcs, npc)
            {
                if (npc->alive)
                    counter ++;
            }

            world->charactercounter = 0;

            UTIL_FOREACH(maps->characters, character)
            {
                if (character)
                    world->charactercounter ++;
            }

            if (world->charactercounter == 0)
            {
                for (int i = 0; i <= 5; i++)
                {
                    UTIL_FOREACH(maps->npcs, npc) { maps->npcs.erase(std::remove(maps->npcs.begin(), maps->npcs.end(), npc), maps->npcs.end()); }
                }

                world->DevilGateEnabled = false;
                world->devil = false;

                world->ServerMsg("The devil gate has ended due to inactivity!");
            }

            if (counter == 0)
            {
                world->wave ++;

                for (world->WaveNPCs = 1; world->WaveNPCs <=  int(world->devilgate_config[util::to_string(world->wave) + ".NPCAmount"]); world->WaveNPCs ++)
                {
                    world->SpawnDevilNPC(int(world->devilgate_config[util::to_string(world->wave) + ".WaveNPC"]));
                }

                UTIL_FOREACH(maps->characters, character)
                {
                    if (character->mapid == int(character->world->devilgate_config["DevilMap"]))
                    {
                        if (world->wave != static_cast<int>(world->devilgate_config["MaxWaves"]) + 1)
                            character->ServerMsg("Wave " + util::to_string(world->wave) + " has started!");

                        if (character->world->devilgate_config["AllowEarthquakes"])
                            character->map->Effect(MAP_EFFECT_QUAKE, 4);
                    }
                }
            }
        }
        else
        {
            UTIL_FOREACH(world->characters, character)
            {
                if (character->mapid == int(character->world->devilgate_config["DevilMap"]))
                    character->ServerMsg("Devil gate ending in " + util::to_string(int(world->devilgate_config["EndingTimer"])) + " seconds!");
            }

            world->DevilGateEnabled = false;
            world->DevilGateEnd();
        }
    }
}

void world_disablectf(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    Map *maps = world->GetMap(int(world->ctf_config["CTFMap"]));

    int blueflag = int(world->ctf_config["BlueFlag"]);
    int redflag = int(world->ctf_config["RedFlag"]);

    bool DeletedFlags = false;

    if (world->ctf == true)
    {
        int charactercounter = 0;

        UTIL_FOREACH(maps->characters, character)
        {
            if (character)
            {
                if (character->redteam == true || character->blueteam == true)
                    charactercounter ++;
            }
        }

        if (charactercounter <= 1)
        {
            if (DeletedFlags == false)
            {
                restart_loop:
                UTIL_FOREACH(maps->items, item)
                {
                    maps->DelItem(item->uid, 0);
                    DeletedFlags = true;
                    goto restart_loop;
                }
            }

            world->ResetCTF();

            UTIL_FOREACH(world->characters, checkchar)
            {
                checkchar->redteam = false;
                checkchar->blueteam = false;

                checkchar->bluehost = false;
                checkchar->redhost = false;

                if (checkchar->mapid == int(world->ctf_config["CTFMap"]))
                {
                    checkchar->Undress(Character::Armor);

                    if (checkchar->HasItem(blueflag))
                        checkchar->RemoveItem(blueflag, 1);

                    if (checkchar->HasItem(redflag))
                        checkchar->RemoveItem(redflag, 1);

                    if (checkchar->oldmap == int(world->ctf_config["CTFMap"]))
                        checkchar->Warp(checkchar->SpawnMap(), checkchar->SpawnX(), checkchar->SpawnY(), world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                    else if (checkchar->oldmap != int(world->ctf_config["CTFMap"]))
                        checkchar->Warp(checkchar->oldmap, checkchar->oldx, checkchar->oldy, world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                }
            }

            world->ServerMsg("Capture The Flag has ended due to inactivity!");
        }
    }
}

void world_disablepvp(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    Map *maps = world->GetMap(int(world->pvp_config["PVPMap"]));

    if (world->pvp == true)
    {
        int charactercounter = 0;

        UTIL_FOREACH(maps->characters, character)
        {
            if (character)
            {
                if (character->pvp_redteam == true || character->pvp_blueteam == true)
                    charactercounter ++;
            }
        }

        if (charactercounter <= 1)
        {
            world->ResetPVP();

            UTIL_FOREACH(world->characters, checkchar)
            {
                checkchar->pvp_redteam = false;
                checkchar->pvp_blueteam = false;

                if (checkchar->mapid == int(world->pvp_config["PVPMap"]))
                {
                    checkchar->Undress(Character::Armor);

                    if (checkchar->oldmap == int(world->pvp_config["PVPMap"]))
                        checkchar->Warp(checkchar->SpawnMap(), checkchar->SpawnX(), checkchar->SpawnY(), world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                    else if (checkchar->oldmap != int(world->pvp_config["PVPMap"]))
                        checkchar->Warp(checkchar->oldmap, checkchar->oldx, checkchar->oldy, world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                }
            }

            world->ServerMsg("PVP has ended due to inactivity!");
        }
    }
}

void world_endgate(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        if (character->mapid == int(character->world->devilgate_config["DevilMap"]))
        {
            if (character->oldmap == int(character->world->devilgate_config["DevilMap"]))
                character->Warp(character->SpawnMap(), character->SpawnX(), character->SpawnY(), world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
            else if (character->oldmap != int(character->world->devilgate_config["DevilMap"]))
                character->Warp(character->oldmap,character->oldx,character->oldy, world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);

            if (static_cast<int>(world->devilgate_config["RewardID"]) != 0)
            {
                character->AddItem(int(world->devilgate_config["RewardID"]), int(world->devilgate_config["RewardAmount"]));

                PacketBuilder builder(PACKET_ITEM, PACKET_GET);
                builder.AddShort(0);
                builder.AddShort(int(world->devilgate_config["RewardID"]));
                builder.AddThree(int(world->devilgate_config["RewardAmount"]));
                builder.AddChar(character->weight);
                builder.AddChar(character->maxweight);
                character->Send(builder);

                world->DevilGateEnabled = false;
                world->devil = false;

                character->ServerMsg("The devil gate is now closed. You have been rewarded with " + util::to_string(int(world->devilgate_config["RewardAmount"])) + " " + world->eif->Get(int(world->devilgate_config["RewardID"])).name);
            }
            else
            {
                character->ServerMsg("The devil gate is now closed, congratulations!");
            }
        }
    }
}

void world_transfer_request(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        if (character->transfer_timer < Timer::GetTime())
        {
            if (character->transfer_timer > 0)
            {
                character->ServerMsg("No response, the transfer request has been declined.");
                    character->ResetTransfer();
            }
        }
    }
}

void world_effects(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        for (std::size_t i = 0; i < character->paperdoll.size(); ++i)
        {
            if (character->paperdoll[i] > 0 && i > 0)
            {
                if (int(world->timedeffects_config[util::to_string(character->paperdoll[i]) + ".paperdoll"]) >= 1)
                    character->Effect(int(world->timedeffects_config[util::to_string(character->paperdoll[i]) + ".paperdoll"]));
            }
        }

        UTIL_FOREACH(character->inventory, item)
        {
            if (item.id > 0)
            {
                if (int(world->timedeffects_config[util::to_string(item.id) + ".inventory"]) >= 1)
                    character->Effect(int(world->timedeffects_config[util::to_string(item.id) + ".inventory"]));
            }
        }
    }
}

void world_deviltimer(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        if (world->devil == true && world->DevilGateEnabled == false)
        {
            world->ServerMsg("The devil gate is now closed, let the game begin!");

            if (character->mapid == int(character->world->devilgate_config["DevilMap"]))
            {
                world->ServerMsg("Wave 1 has started!");

                if (character->world->devilgate_config["AllowEarthquakes"])
                    character->map->Effect(MAP_EFFECT_QUAKE, 4);
            }

            world->devil = false;
            world->wave = 1;

            for (world->WaveNPCs = 1; world->WaveNPCs <= int(world->devilgate_config["1.NPCAmount"]); world->WaveNPCs ++)
            {
                character->world->SpawnDevilNPC(int(world->devilgate_config["1.WaveNPC"]));
            }

            world->DevilGateEnabled = true;
        }
    }
}

void world_startctf(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    if (world->ctf == false)
    {
        world->bluecounter = 0;
        world->redcounter = 0;

        world->ServerMsg("Capture the flag can now be entered, type " + std::string(world->config["PlayerPrefix"]) + "ctf to join. The event will start in " + util::to_string(int(world->ctf_config["StartTimer"])) + " seconds.");

        world->ctfenabled = true;
        world->CTFTimer();
    }
}

void world_ctftimer(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    Map *maps = world->GetMap(int(world->ctf_config["CTFMap"]));

    UTIL_FOREACH(world->characters, character)
    {
        if (world->ctfenabled == true)
        {
            if (world->ctf == false)
            {
                int blueflag = int(world->ctf_config["BlueFlag"]);
                int redflag = int(world->ctf_config["RedFlag"]);

                int bx = util::to_int(util::explode(',', world->ctf_config["BlueXY"])[0]);
                int by = util::to_int(util::explode(',', world->ctf_config["BlueXY"])[1]);

                int rx = util::to_int(util::explode(',', world->ctf_config["RedXY"])[0]);
                int ry = util::to_int(util::explode(',', world->ctf_config["RedXY"])[1]);

                world->ctfenabled = false;
                world->ctf = true;

                if (bx > 0 && by > 0 && rx > 0 && ry > 0)
                {
                    maps->AddItem(blueflag, 1, bx, by, character);
                    maps->AddItem(redflag, 1, rx, ry, character);
                }

                if (character->mapid == int(world->ctf_config["CTFMap"]))
                    character->Refresh();

                world->ServerMsg("Capture The Flag has started, let the game begin!");
            }
        }
    }
}

void world_pvptimer(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    if (world->pvp == false && world->pvpenabled == true)
    {
        world->ServerMsg("PVP is no longer accessable, may the strongest team survive.");

        world->pvpenabled = false;
        world->pvp = true;
    }
}

void world_ctfdelay(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    int RewardID = int(world->ctf_config["RewardID"]);
    int AmountID = int(world->ctf_config["AmountID"]);

    Map *maps = world->GetMap(int(world->ctf_config["CTFMap"]));

    bool DeletedFlags = false;

    UTIL_FOREACH(world->characters, character)
    {
        if (world->bluecounter < int(world->ctf_config["RoundsToWin"]) && world->redcounter < int(world->ctf_config["RoundsToWin"]))
        {
            int fbarmor = world->ctf_config["FemaleBlueTeamArmor"];
            int frarmor = world->ctf_config["FemaleRedTeamArmor"];

            int mbarmor = world->ctf_config["MaleBlueTeamArmor"];
            int mrarmor = world->ctf_config["MaleRedTeamArmor"];

            const EIF_Data &fblue = world->eif->Get(fbarmor);
            const EIF_Data &fred = world->eif->Get(frarmor);

            const EIF_Data &mblue = world->eif->Get(mbarmor);
            const EIF_Data &mred = world->eif->Get(mrarmor);

            if (character->blueteam == true)
            {
                if (character->gender == 0)
                    character->Dress(Character::Armor, fblue.dollgraphic);
                else if (character->gender == 1)
                    character->Dress(Character::Armor, mblue.dollgraphic);
            }
            else if (character->redteam == true)
            {
                if (character->gender == 0)
                    character->Dress(Character::Armor, fred.dollgraphic);
                else if (character->gender == 1)
                    character->Dress(Character::Armor, mred.dollgraphic);
            }

            if (character->mapid == int(world->ctf_config["CTFMap"]))
                character->ServerMsg("Team blue: " + util::to_string(world->bluecounter) + " wins! " + "Team red: " + util::to_string(world->redcounter) + " wins! - Let the next round begin!");
        }
        else
        {
            if (world->bluecounter == int(world->ctf_config["RoundsToWin"]))
            {
                if (character->blueteam == true)
                    character->GiveItem(RewardID, AmountID);

                character->ServerMsg("Congratulations! The blue team has won Capture The Flag and has been rewarded!");
            }

            if (world->redcounter == int(world->ctf_config["RoundsToWin"]))
            {
                if (character->redteam == true)
                    character->GiveItem(RewardID, AmountID);

                character->ServerMsg("Congratulations! The red team has won Capture The Flag and has been rewarded!");
            }

            DeletedFlags = false;

            if (DeletedFlags == false)
            {
                restart_loop:
                UTIL_FOREACH(maps->items, item)
                {
                    maps->DelItem(item->uid, 0);
                    DeletedFlags = true;
                    goto restart_loop;
                }
            }

            if (character->mapid == int(world->ctf_config["CTFMap"]))
            {
                if (character->blueteam == true || character->redteam == true)
                    character->Warp(character->oldmap, character->oldx, character->oldy, world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);

                if (character->blueteam == true || character->redteam == true)
                    character->Undress(Character::Armor);
            }

            character->bluehost = false;
            character->redhost = false;

            character->blueteam = false;
            character->redteam = false;

            world->ResetCTF();
        }
    }
}

void world_event(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    restart_loop:

    Character *character = world->GetCharacterPID(util::rand(0, world->characters.size()));

    if (character)
    {
        if (character->admin <= int(world->event_config["MaxAdminLevel"]))
        {
            if (character->mapid != int(world->config["JailMap"]) && character->mapid != int(world->config["WallMap"]))
            {
                int M = util::to_int(util::explode(',', world->event_config["EventLocation"])[0]);
                int X = util::to_int(util::explode(',', world->event_config["EventLocation"])[1]);
                int Y = util::to_int(util::explode(',', world->event_config["EventLocation"])[2]);

                if (M > 0 && X > 0 && Y > 0)
                {
                    if (character->mapid != M)
                    {
                        character->oldmap = character->mapid;
                        character->oldx = character->x;
                        character->oldy = character->y;
                    }

                    character->event = Timer::GetTime() + double(world->event_config["DisqualifyTimer"]);
                    character->Warp(M, X, Y, world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);

                    world->ServerMsg("Attention!! " + character->SourceName() + " has been warped to the event room!");
                }
            }
            else
            {
                goto restart_loop;
            }
        }
        else
        {
            goto restart_loop;
        }
    }
}

void world_eventprotection(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        if (character->event < Timer::GetTime() && character->event != 0)
        {
            int M = util::to_int(util::explode(',', world->event_config["EventLocation"])[0]);
            int X = util::to_int(util::explode(',', world->event_config["EventLocation"])[1]);
            int Y = util::to_int(util::explode(',', world->event_config["EventLocation"])[2]);

            if (character->mapid == M && character->x == X && character->y == Y)
            {
                character->Warp(character->oldmap, character->oldx, character->oldy, world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);

                world->ServerMsg("Attention!! " + util::ucfirst(character->SourceName()) + " has been disqualified for being AFK [Away from keyboard] - Event System");
            }

            character->event = 0;
        }
    }
}

void world_despawn_items(void *world_void)
{
	World *world = static_cast<World *>(world_void);

	UTIL_FOREACH(world->maps, map)
	{
		restart_loop:
		UTIL_FOREACH(map->items, item)
		{
			if (item->unprotecttime < (Timer::GetTime() - static_cast<double>(world->config["ItemDespawnRate"])))
			{
			    if (map->id != util::to_int(world->ctf_config["CTFMap"]))
                    map->DelItem(item->uid, 0);

				goto restart_loop;
			}
		}
	}
}

void world_speak_npcs(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->maps, map)
    {
        UTIL_FOREACH(map->npcs, npc)
        {
            double timeinterval = util::to_float(world->npcs_config[util::to_string(npc->id) + ".interval"]), current_time = Timer::GetTime();

            if (timeinterval > 0 && (current_time - npc->last_chat) >= timeinterval)
            {
                std::vector<std::string> chats;

                for (int i = 1; std::string(world->npcs_config[util::to_string(npc->id) + ".chat" + util::to_string(i)]) != "0"; ++i)
                    chats.push_back(world->npcs_config[util::to_string(npc->id) + ".chat" + util::to_string(i)]);

                if (util::rand(0, 100) <= (util::to_int(world->npcs_config[util::to_string(npc->id) + ".frequency"])) && chats.size() > 0)
                {
                    npc->ShowDialog(chats.at(util::rand(0, chats.size() - 1)));
                    npc->last_chat = current_time;
                }
                else
                {
                    npc->last_chat = current_time;
                }
            }
        }
    }
}

void world_timed_save(void *world_void)
{
	World *world = static_cast<World *>(world_void);

	UTIL_FOREACH(world->characters, character)
	{
		character->Save();

		if (character->HasPet)
            character->SavePet();
	}

	world->guildmanager->SaveAll();

	world->CommitDB();
	world->BeginDB();
}

void world_mapeffects(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, from)
    {
        if (from->map->effect == 1)
            from->map->DrainHP(from);
        else if (from->map->effect == 2)
            from->map->DrainTP(from);
        else if (from->map->effect == 3)
            from->map->Effect(MAP_EFFECT_QUAKE, 2);
        else if (from->map->effect == 4)
            from->map->Effect(MAP_EFFECT_QUAKE, 4);
        else if (from->map->effect == 5)
            from->map->Effect(MAP_EFFECT_QUAKE, 6);
        else if (from->map->effect == 6)
            from->map->Effect(MAP_EFFECT_QUAKE, 8);
    }
}

void world_immune(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->maps, map)
    {
        map->Immune();
    }
}

void world_eosbot(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    double timeinterval = util::to_float(world->eosbot_config["interval"]);
    double current_time = Timer::GetTime();

    if (timeinterval > 0 && (current_time - world->last_chat) >= timeinterval)
    {
        std::vector<std::string> chats;

        for (int i = 1; std::string(world->eosbot_config["chat" + util::to_string(i)]) != "0"; ++i)
        {
            chats.push_back(world->eosbot_config["chat" + util::to_string(i)]);
        }

        if (chats.size() > 0)
        {
            PacketBuilder builder;
            builder.SetID(PACKET_TALK, PACKET_PLAYER);
            builder.AddShort(0);
            builder.AddString(chats.at(util::rand(0, chats.size() - 1)));

            UTIL_FOREACH(world->characters, character)
            {
                if (character->mapid == int(world->eosbot_config["bot.map"]))
                {
                    character->Send(builder);
                }
            }

            world->last_chat = current_time;
        }
        else
        {
            world->last_chat = current_time;
        }
    }
}

void world_cooking(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        if (character->cooking < Timer::GetTime() && character->cooking != 0)
        {
            int random = util::rand(1,100);

            int itemid = world->cooking_config[util::to_string(character->cookid) + ".CookItemID"];
            int chance = world->cooking_config[util::to_string(character->cookid) + ".RewardChance"];

            if (random > 100 - chance)
            {
                bool level_up = false;

                int reward = world->cooking_config[util::to_string(character->cookid) + ".RewardID"];
                int amount = world->cooking_config[util::to_string(character->cookid) + ".AmountID"];
                int EXPRew = world->cooking_config[util::to_string(character->cookid) + ".CookingEXP"];

                character->GiveItem(reward, amount);

                character->cexp += EXPRew;
                character->cexp = std::min(character->cexp, util::to_int(world->config["MaxExp"]));

                while (character->clevel < int(world->config["MaxLevel"]) && character->cexp >= world->exp_table[character->clevel +1])
                {
                    level_up = true;
                    ++character->clevel;
                }

                PacketBuilder builder(PACKET_RECOVER, PACKET_REPLY);
                builder.AddInt(character->cexp);
                builder.AddShort(character->karma);

                if (!character->cooking_exp)
                {
                    character->cooking_exp = true;
                    builder.AddChar(character->clevel);
                }

                if (level_up)
                {
                    builder.AddChar(character->clevel);
                    PacketBuilder reply(PACKET_ITEM, PACKET_ACCEPT);
                    reply.AddShort(character->id);

                    UTIL_FOREACH(character->world->characters, from)
                    {
                        if (from != character && from->InRange(character))
                            from->Send(reply);
                    }
                }

                builder.AddChar(0);
                character->Send(builder);

                character->ServerMsg(world->i18n.Format("Cooking-Success", world->eif->Get(itemid).name));
            }
            else
            {
                character->ServerMsg(world->i18n.Format("Cooking-Fail", world->eif->Get(itemid).name));
            }

            character->cooking = 0;
            character->cookid = 0;

            character->StatusMsg(world->i18n.Format("Cooking-Done"));
        }
    }
}

void world_boost(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        if (character->boosttimer < Timer::GetTime())
        {
            if (character->boosttimer > 0)
                character->UndoBuff();
        }
    }
}

void world_boosttimer(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        if (character->boosttimer > 0 && character->boosteffect > 0)
            character->Effect(character->boosteffect);
    }
}

void world_hidespell(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        if (character->hidetimer < Timer::GetTime())
        {
            if (character->hidetimer > 0)
            {
                if (character->hidden)
                {
                    character->Unhide();

                    PacketBuilder builder(PACKET_ADMININTERACT, PACKET_AGREE);
                    builder.AddShort(character->player->id);
                    character->Send(builder);
                }

                character->hidetimer = 0;
                character->ServerMsg("You are no longer hidden.");
            }
        }
    }
}

void world_freezespell(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        if (character->freezetimer < Timer::GetTime())
        {
            if (character->freezetimer > 0)
            {
                PacketBuilder builder;
                builder.SetID(PACKET_WALK, PACKET_OPEN);
                character->Send(builder);

                character->freezetimer = 0;
                character->ServerMsg("You are no longer frozen.");
            }
        }
    }
}

void world_regenerating(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        if (character->regenerateid > 0)
        {
            int RegenHP = world->eif->Get(character->regenerateid).unkd;
            int RegenTP = world->eif->Get(character->regenerateid).unke;

            RegenHP = std::max(RegenHP, 0);
            RegenTP = std::max(RegenTP, 0);

            if (RegenHP >= 1) character->hp += RegenHP;
            if (RegenTP >= 1) character->tp += RegenTP;

            if (!world->config["LimitDamage"])
            {
                character->hp = std::min(character->hp, character->maxhp);
                character->tp = std::min(character->tp, character->maxtp);
            }

            PacketBuilder builder(PACKET_RECOVER, PACKET_AGREE);
            builder.AddShort(character->id);
            builder.AddInt(RegenHP);
            builder.AddChar(int(double(character->hp) / double(character->maxhp) * 100.0));
            character->Send(builder);

            if (character->hp > character->maxhp) character->hp = character->maxhp;
            if (character->tp > character->maxtp) character->tp = character->maxtp;

            character->PacketRecover();
        }

        if (character->regeneratetimer < Timer::GetTime())
        {
            if (character->regeneratetimer > 0)
            {
                character->regenerateid = 0;
                character->regeneratetimer = 0;

                character->ServerMsg("Your stats are no longer being regenerated!");
            }
        }
    }
}

void world_poison(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        if (character->poisontimer > 0)
        {
            int PoisonHP = character->poisonhp;
            int PoisonTP = character->poisontp;

            int amount = PoisonHP;
            int limitamount = std::min(amount, int(character->hp));

            if (character->world->config["LimitDamage"])
                amount = limitamount;

            if (character->immune == false)
            {
                character->hp -= limitamount;

                if (PoisonTP > 0)
                    character->tp -= PoisonTP;
            }

            PacketBuilder builder(PACKET_AVATAR, PACKET_REPLY);
            builder.AddShort(0);
            builder.AddShort(character->player->id);
            builder.AddThree(character->immune == false ? amount : 0);
            builder.AddChar(character->direction);
            builder.AddChar(int(double(character->hp) / double(character->maxhp) * 100.0));
            builder.AddChar(character->hp == 0);
            character->Send(builder);

            UTIL_FOREACH(character->map->characters, mapchar)
            {
                if (!mapchar->InRange(character) || mapchar == character)
                    continue;

                if (!character->hidden)
                    mapchar->Send(builder);
                else if ((character->hidden && mapchar->admin >= static_cast<int>(character->world->admin_config["seehide"])) || character == mapchar)
                    mapchar->Send(builder);
            }

            character->Effect(character->poisoneffect);

            character->PacketRecover();

            if (character->hp == 0)
                character->DeathRespawn();
        }

        if (character->poisontimer < Timer::GetTime())
        {
            if (character->poisontimer > 0)
            {
                character->poisontimer = 0;
                    character->ServerMsg("You are no longer poisoned.");
            }
        }
    }
}

void world_spawnflags(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    int blueflag = int(world->ctf_config["BlueFlag"]);
    int redflag = int(world->ctf_config["RedFlag"]);

    UTIL_FOREACH(world->characters, character)
    {
        if (character->mapid != int(world->ctf_config["CTFMap"]))
        {
            if (character->blueteam == true)
            {
                if (character->HasItem(redflag))
                    character->RemoveItem(redflag, 1);

                character->blueteam = false;
                world->bluemembers --;
            }

            if (character->redteam == true)
            {
                if (character->HasItem(blueflag))
                    character->RemoveItem(blueflag, 1);

                character->redteam = false;
                world->redmembers --;
            }
        }
    }

    if (world->ctf == true)
    {
        Map *maps = world->GetMap(int(world->ctf_config["CTFMap"]));

        int bx = util::to_int(util::explode(',', world->ctf_config["BlueXY"])[0]);
        int by = util::to_int(util::explode(',', world->ctf_config["BlueXY"])[1]);

        int rx = util::to_int(util::explode(',', world->ctf_config["RedXY"])[0]);
        int ry = util::to_int(util::explode(',', world->ctf_config["RedXY"])[1]);

        UTIL_FOREACH(world->characters, character)
        {
            if (character->bluehost == true && character->mapid != int(world->ctf_config["CTFMap"]) && world->atbluebase == false)
            {
                maps->AddItem(blueflag, 1, bx, by, character);

                UTIL_FOREACH(maps->characters, checkchar)
                {
                    checkchar->Refresh();
                }

                world->atbluebase = true;
                world->ServerMsg("The blue team's flag has been recovered!");
            }

            if (character->redhost == true && character->mapid != int(world->ctf_config["CTFMap"]) && world->atredbase == false)
            {
                maps->AddItem(redflag, 1, rx, ry, character);

                UTIL_FOREACH(maps->characters, checkchar)
                {
                    checkchar->Refresh();
                }

                world->atredbase = true;
                world->ServerMsg("The red team's flag has been recovered!");
            }
        }
    }
}

void world_partymaps(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        if (character->mapid == int(world->partymap_config[util::to_string(character->mapid) + ".map"]))
        {
            int mapid = character->mapid;

            int partyreq = int(world->partymap_config[util::to_string(mapid) + ".partyreq"]);
            int kickMap = int(world->partymap_config[util::to_string(mapid) + ".kickmap"]);
            int kickX = int(world->partymap_config[util::to_string(mapid) + ".kickx"]);
            int kickY = int(world->partymap_config[util::to_string(mapid) + ".kicky"]);

            if (character->party)
            {
                if (character->party->members.size() < int64_t(partyreq))
                {
                    UTIL_FOREACH(character->party->members, member)
                    {
                        member->Warp(kickMap, kickX, kickY);
                        member->ServerMsg("You were removed from the map because someone left the party.");
                    }
                }
            }
            else
            {
                character->Warp(kickMap, kickX, kickY);
                character->ServerMsg("You were removed from the map because you left the party.");
            }
        }
    }
}

void world_timedmessage(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    std::string message = world->message_config["TimedMessage"];

    if (message != "0")
        world->ServerMsg(message);
}

void world_restart(void *world_void)
{
    World *world = static_cast<World *>(world_void);

    UTIL_FOREACH(world->characters, character)
    {
        character->Save();

        if (character->HasPet)
            character->SavePet();
    }

    world->guildmanager->SaveAll();

    world->CommitDB();
	world->BeginDB();

    std::exit(0);
}

World::World(std::array<std::string, 6> dbinfo, const Config &eoserv_config, const Config &admin_config) : i18n(eoserv_config.find("ServerLanguage")->second), admin_count(0)
{
    this->global = true;
    this->last_chat = 0;

	this->config = eoserv_config;
	this->admin_config = admin_config;

	Database::Engine engine;

	if (util::lowercase(dbinfo[0]).compare("sqlite") == 0)
	{
		engine = Database::SQLite;
	}
	else
	{
		engine = Database::MySQL;
	}

	this->db.Connect(engine, dbinfo[1], util::to_int(dbinfo[5]), dbinfo[2], dbinfo[3], dbinfo[4]);
	this->BeginDB();

	try
	{
		this->drops_config.Read(this->config["DropsFile"]);
		this->shops_config.Read(this->config["ShopsFile"]);
		this->arenas_config.Read(this->config["ArenasFile"]);
		this->formulas_config.Read(this->config["FormulasFile"]);
		this->home_config.Read(this->config["HomeFile"]);
		this->skills_config.Read(this->config["SkillsFile"]);
		this->npcs_config.Read(this->config["NPCsFile"]);

		this->eosbot_config.Read(this->config["EOSbotFile"]);
		this->achievements_config.Read(this->config["AchievementsFile"]);
		this->message_config.Read(this->config["MessageFile"]);
		this->event_config.Read(this->config["EventFile"]);
		this->devilgate_config.Read(this->config["DevilGateFile"]);
		this->pets_config.Read(this->config["PetsFile"]);
		this->harvesting_config.Read(this->config["HarvestingFile"]);
		this->chatlogs_config.Read(this->config["ChatlogsFile"]);
		this->fishing_config.Read(this->config["FishingFile"]);
		this->mining_config.Read(this->config["MiningFile"]);
		this->woodcutting_config.Read(this->config["WoodcuttingFile"]);
		this->cooking_config.Read(this->config["CookingFile"]);
		this->effects_config.Read(this->config["EffectsFile"]);
		this->timedeffects_config.Read(this->config["TimedEffectsFile"]);
		this->members_config.Read(this->config["MembersFile"]);
		this->warps_config.Read(this->config["WarpsFile"]);
		this->giftbox1_config.Read(this->config["Giftbox1File"]);
		this->giftbox2_config.Read(this->config["Giftbox2File"]);
		this->shaving_config.Read(this->config["ShavingFile"]);
		this->tilemessage_config.Read(this->config["TileMessageFile"]);
		this->partymap_config.Read(this->config["PartyMapsFile"]);
		this->ctf_config.Read(this->config["CTFFile"]);
		this->buffspells_config.Read(this->config["BuffSpellsFile"]);
		this->cursefilter_config.Read(this->config["CurseFilterFile"]);
		this->buffitems_config.Read(this->config["BuffItemsFile"]);
		this->commands_config.Read(this->config["CommandsFile"]);
		this->equipment_config.Read(this->config["EquipmentFile"]);
		this->spells_config.Read(this->config["SpellsFile"]);
		this->poison_config.Read(this->config["PoisonFile"]);
		this->pvp_config.Read(this->config["PVPFile"]);
		this->shrines_config.Read(this->config["ShrinesFile"]);
	}
	catch (std::runtime_error &e)
	{
		Console::Wrn(e.what());
	}

	this->UpdateConfig();
	this->LoadHome();

	this->LoadWlist();

	this->eif = new EIF(this->config["EIF"]);
	this->enf = new ENF(this->config["ENF"]);
	this->esf = new ESF(this->config["ESF"]);
	this->ecf = new ECF(this->config["ECF"]);

	this->maps.resize(static_cast<int>(this->config["Maps"]));

	int loaded = 0;
	int npcs = 0;

	for (int i = 0; i < static_cast<int>(this->config["Maps"]); ++i)
	{
		this->maps[i] = new Map(i + 1, this);

		if (this->maps[i]->exists)
		{
			npcs += this->maps[i]->npcs.size();
			++loaded;
		}
	}

	this->DevilGateEnabled = false;
	this->devil = false;
	this->ctf = false;
	this->pvp = false;
	this->pvpenabled = false;
	this->ctfenabled = false;

    this->pvp_bluekillcount = 0;
    this->pvp_redkillcount = 0;

    this->pvp_bluemembers = 0;
    this->pvp_redmembers = 0;

	this->bluecounter = 0;
	this->redcounter = 0;

	this->bluemembers = 0;
	this->redmembers = 0;

	this->atbluebase = true;
	this->atredbase = true;

	this->charactercounter = 0;

	#ifdef GUI
    int mapsize = this->maps.size();
    Chat::Info(util::to_string(loaded) + "/" + util::to_string(mapsize) + " maps loaded.", 234,230,157);
    Chat::Info(util::to_string(npcs) + " NPCs loaded.", 234,230,157);
    #else
    Console::GreenOut("%i/%i maps loaded.", loaded, this->maps.size());
	Console::GreenOut("%i NPCs loaded.", npcs);
    #endif

    loaded = 0;
    std::FILE *nafh = std::fopen(static_cast<std::string>(this->config["AdminsFile"]).c_str(), "rb");

    if (nafh)
    {
        char nabuf[4096] = "";
        std::string buff;
        bool naeof = (nafh == 0);

        while (!naeof)
        {
            std::fgets(nabuf, 4096, nafh);

            buff = util::trim(nabuf);
            this->namelist.push_back(buff);

            naeof = std::feof(nafh);
            ++loaded;
        }
    }

	short max_quest = static_cast<int>(this->config["Quests"]);

	UTIL_FOREACH(this->enf->data, npc)
	{
		if (npc.type == ENF::Quest)
			max_quest = std::max(max_quest, npc.vendor_id);
	}

	for (short i = 0; i <= max_quest; ++i)
	{
		try
		{
			std::shared_ptr<Quest> q = std::make_shared<Quest>(i, this);
			this->quests.insert(std::make_pair(i, std::move(q)));
		}
		catch (...)
		{

		}
	}

	#ifdef GUI
	int questsize = this->quests.size();
    Chat::Info(util::to_string(questsize) + "/" + util::to_string(max_quest) + " quests loaded.", 234,230,157);
    #else
    Console::GreenOut("%i/%i quests loaded.", this->quests.size(), max_quest);
    #endif

    this->commands.resize(static_cast<int>(this->config["Commands"]));
    loaded = 0;
    for (int i = 1; i <= static_cast<int>(this->config["Commands"]); ++i)
    {
        this->commands[i-1] = new Command(i, this);

        if (this->commands[i-1]->exists)
            ++loaded;
    }

    #ifdef GUI
    Chat::Info(util::to_string(loaded) + " commands loaded.",234,230,157);
    #else
    Console::GreenOut("%i commands loaded.", loaded);
    #endif

	this->last_character_id = 0;

	TimeEvent *event = new TimeEvent(world_execute_weddings, this, 1.0, Timer::FOREVER);
    this->timer.Register(event);

	event = new TimeEvent(world_spawn_npcs, this, 1.0, Timer::FOREVER);
	this->timer.Register(event);

    event = new TimeEvent(world_mapeffects, this, 15.0, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_immune, this, 1.00, Timer::FOREVER);
    this->timer.Register(event);

	event = new TimeEvent(world_act_npcs, this, 0.05, Timer::FOREVER);
	this->timer.Register(event);

	event = new TimeEvent(world_devilgate, this, 0.5, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_disablectf, this, 1.0, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_disablepvp, this, 1.0, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_effects, this, 2.0, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_speak_npcs, this, 1.00, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_eosbot, this, 1.00, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_cooking, this, 1.00, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_eventprotection, this, 1.00, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_boost, this, 1.00, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_hidespell, this, 1.00, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_freezespell, this, 1.00, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_transfer_request, this, 1.00, Timer::FOREVER);
    this->timer.Register(event);

    event = new TimeEvent(world_spawnflags, this, 1.00, Timer::FOREVER);
    this->timer.Register(event);

    if (this->config["RegenerationItems"])
    {
        event = new TimeEvent(world_regenerating, this, 2.00, Timer::FOREVER);
        this->timer.Register(event);
    }

    if (int(this->poison_config["PoisonTick"]) > 0)
    {
        event = new TimeEvent(world_poison, this, static_cast<double>(this->poison_config["PoisonTick"]), Timer::FOREVER);
        this->timer.Register(event);
    }

    if (int(this->config["BuffEffectTimer"]) > 0)
    {
        event = new TimeEvent(world_boosttimer, this, static_cast<double>(this->config["BuffEffectTimer"]), Timer::FOREVER);
        this->timer.Register(event);
    }

    if (int(this->message_config["TimedMessageTimer"]) > 0)
    {
        event = new TimeEvent(world_timedmessage, this, static_cast<double>(this->message_config["TimedMessageTimer"]), Timer::FOREVER);
        this->timer.Register(event);
    }

	if (int(this->config["RecoverSpeed"]) > 0)
	{
		event = new TimeEvent(world_recover, this, static_cast<double>(this->config["RecoverSpeed"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (int(this->config["NPCRecoverSpeed"]) > 0)
	{
		event = new TimeEvent(world_npc_recover, this, static_cast<double>(this->config["NPCRecoverSpeed"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (int(this->devilgate_config["AutoStartTimer"]) > 0)
	{
		event = new TimeEvent(world_startdevil, this, static_cast<double>(this->devilgate_config["AutoStartTimer"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (int(this->pvp_config["AutoStartTimer"]) > 0)
	{
		event = new TimeEvent(world_pvp_autostart, this, static_cast<double>(this->pvp_config["AutoStartTimer"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (int(this->ctf_config["AutoStartTimer"]) > 0)
	{
		event = new TimeEvent(world_startctf, this, static_cast<double>(this->ctf_config["AutoStartTimer"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (int(this->config["WarpSuck"]) > 0)
	{
		event = new TimeEvent(world_warp_suck, this, 1.0, Timer::FOREVER);
		this->timer.Register(event);
	}

	if (this->config["ItemDespawn"])
	{
		event = new TimeEvent(world_despawn_items, this, static_cast<double>(this->config["ItemDespawnCheck"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (this->config["TimedSave"])
	{
		event = new TimeEvent(world_timed_save, this, static_cast<double>(this->config["TimedSave"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	if (int(this->event_config["EventTimer"]) > 0)
	{
		event = new TimeEvent(world_event, this, static_cast<double>(this->event_config["EventTimer"]), Timer::FOREVER);
		this->timer.Register(event);
	}

	exp_table[0] = 0;

	for (std::size_t i = 1; i < this->exp_table.size(); ++i)
	{
		exp_table[i] = int(util::round(std::pow(double(i), 3.0) * 133.1));
	}

	for (std::size_t i = 0; i < this->boards.size(); ++i)
	{
		this->boards[i] = new Board(i);
	}

	this->guildmanager = new GuildManager(this);

	this->LoadFish();
	this->LoadMine();
	this->LoadWood();
}

void World::BeginDB()
{
	if (this->config["TimedSave"])
		this->db.BeginTransaction();
}

void World::CommitDB()
{
	if (this->db.Pending())
		this->db.Commit();
}

void World::UpdateAdminCount(int admin_count)
{
	this->admin_count = admin_count;

	if (admin_count == 0 && this->config["FirstCharacterAdmin"])
	{
		#ifdef GUI
		Chat::Info("There are no admin characters!",200,20,20);
		Chat::Info("The next character created will be given HGM status!",200,20,20);
		#else
		Console::Out("There are no admin characters!");
		Console::Out("The next character created will be given HGM status!");
		#endif
	}
}

void World::AdminCommands(std::string command, const std::vector<std::string>& arguments, Command_Source* from)
{
    std::unique_ptr<System_Command_Source> system_source;

    if (!from)
    {
        system_source.reset(new System_Command_Source(this));
        from = system_source.get();
    }

    Commands::Handle(util::lowercase(command), arguments, from);
}

void World::PlayerCommands(std::string command, const std::vector<std::string>& arguments, Command_Source* from)
{
    std::unique_ptr<System_Command_Source> system_source;

    if (!from)
    {
        system_source.reset(new System_Command_Source(this));
        from = system_source.get();
    }

    PlayerCommands::Handle(util::lowercase(command), arguments, from);
}

void World::LoadHome()
{
	this->homes.clear();

	std::unordered_map<std::string, Home *> temp_homes;

	UTIL_FOREACH(this->home_config, hc)
	{
		std::vector<std::string> parts = util::explode('.', hc.first);

		if (parts.size() < 2)
		{
			continue;
		}

		if (parts[0] == "level")
		{
			int level = util::to_int(parts[1]);

			std::unordered_map<std::string, Home *>::iterator home_iter = temp_homes.find(hc.second);

			if (home_iter == temp_homes.end())
			{
				Home *home = new Home;
				home->id = static_cast<std::string>(hc.second);
				temp_homes[hc.second] = home;
				home->level = level;
			}
			else
			{
				home_iter->second->level = level;
			}

			continue;
		}

		Home *&home = temp_homes[parts[0]];

		if (!home)
		{
			temp_homes[parts[0]] = home = new Home;
			home->id = parts[0];
		}

		if (parts[1] == "name")
		{
			home->name = home->name = static_cast<std::string>(hc.second);
		}
		else if (parts[1] == "location")
		{
			std::vector<std::string> locparts = util::explode(',', hc.second);
			home->map = locparts.size() >= 1 ? util::to_int(locparts[0]) : 1;
			home->x = locparts.size() >= 2 ? util::to_int(locparts[1]) : 0;
			home->y = locparts.size() >= 3 ? util::to_int(locparts[2]) : 0;
		}
	}

	UTIL_FOREACH(temp_homes, home)
	{
		this->homes.push_back(home.second);
	}
}

int World::FindMap(std::string mapname)
{
    UTIL_FOREACH(this->maps, map)
    {
        if (int(util::lowercase(map->name).find(util::lowercase(mapname))) != -1 || mapname == util::to_string(map->id))
            return map->id;
    }

    return 0;
}

int World::GenerateCharacterID()
{
	return ++this->last_character_id;
}

int World::GeneratePlayerID()
{
	unsigned int lowest_free_id = 1;
	restart_loop:
	UTIL_FOREACH(this->server->clients, client)
	{
		EOClient *eoclient = static_cast<EOClient *>(client);

		if (eoclient->id == lowest_free_id)
		{
			lowest_free_id = eoclient->id + 1;
			goto restart_loop;
		}
	}
	return lowest_free_id;
}

void World::Login(Character *character)
{
	this->characters.push_back(character);

	if (this->GetMap(character->mapid)->relog_x || this->GetMap(character->mapid)->relog_y)
	{
		character->x = this->GetMap(character->mapid)->relog_x;
		character->y = this->GetMap(character->mapid)->relog_y;
	}

	if (character->mapid == int(character->world->devilgate_config["DevilMap"]))
        character->Warp(character->oldmap, character->oldx, character->oldy, WARP_ANIMATION_NONE);

    if (character->mapid == int(character->world->ctf_config["CTFMap"]))
    {
        character->Undress(Character::Armor);

        character->RemoveItem(int(character->world->ctf_config["BlueFlag"]), 1);
        character->RemoveItem(int(character->world->ctf_config["RedFlag"]), 1);

        character->Warp(character->oldmap, character->oldx, character->oldy, WARP_ANIMATION_NONE);
    }

	Map* map = this->GetMap(character->mapid);

	if (character->sitting == SIT_CHAIR)
	{
		Map_Tile::TileSpec spec = map->GetSpec(character->x, character->y);

		if (spec == Map_Tile::ChairDown)
			character->direction = DIRECTION_DOWN;
		else if (spec == Map_Tile::ChairUp)
			character->direction = DIRECTION_UP;
		else if (spec == Map_Tile::ChairLeft)
			character->direction = DIRECTION_LEFT;
		else if (spec == Map_Tile::ChairRight)
			character->direction = DIRECTION_RIGHT;
		else if (spec == Map_Tile::ChairDownRight)
			character->direction = character->direction == DIRECTION_RIGHT ? DIRECTION_RIGHT : DIRECTION_DOWN;
		else if (spec == Map_Tile::ChairUpLeft)
			character->direction = character->direction == DIRECTION_LEFT ? DIRECTION_LEFT : DIRECTION_UP;
		else if (spec != Map_Tile::ChairAll)
			character->sitting = SIT_STAND;
	}

	map->Enter(character);

	character->Login();
}

void World::Logout(Character *character)
{
	if (this->GetMap(character->mapid)->exists)
		this->GetMap(character->mapid)->Leave(character);

	this->characters.erase(std::remove(UTIL_RANGE(this->characters), character),this->characters.end());
}

void World::Msg(Command_Source *from, std::string message, bool echo)
{
	std::string from_str = from ? from->SourceName() : "server";

	message = util::text_cap(message, static_cast<int>(this->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from_str) + "  "));

	PacketBuilder builder(PACKET_TALK, PACKET_MSG, 2 + from_str.length() + message.length());
	builder.AddBreakString(from_str);
	builder.AddBreakString(message);

	UTIL_FOREACH(this->characters, character)
	{
		character->AddChatLog("~", from_str, message);

		if (!echo && character == from)
		{
			continue;
		}

		character->Send(builder);
	}
}

void World::AdminMsg(Command_Source *from, std::string message, int minlevel, bool echo)
{
	std::string from_str = from ? from->SourceName() : "server";

	message = util::text_cap(message, static_cast<int>(this->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from_str) + "  "));

	PacketBuilder builder(PACKET_TALK, PACKET_ADMIN, 2 + from_str.length() + message.length());
	builder.AddBreakString(from_str);
	builder.AddBreakString(message);

	UTIL_FOREACH(this->characters, character)
	{
		character->AddChatLog("+", from_str, message);

		if ((!echo && character == from) || character->SourceAccess() < minlevel)
		{
			continue;
		}

		character->Send(builder);
	}
}

void World::AnnounceMsg(Command_Source *from, std::string message, bool echo)
{
	std::string from_str = from ? from->SourceName() : "server";

	message = util::text_cap(message, static_cast<int>(this->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from_str) + "  "));

	PacketBuilder builder(PACKET_TALK, PACKET_ANNOUNCE, 2 + from_str.length() + message.length());
	builder.AddBreakString(from_str);
	builder.AddBreakString(message);

	UTIL_FOREACH(this->characters, character)
	{
		character->AddChatLog("@", from_str, message);

		if (!echo && character == from)
		{
			continue;
		}

		character->Send(builder);
	}
}

void World::ServerMsg(std::string message)
{
	message = util::text_cap(message, static_cast<int>(this->config["ChatMaxWidth"]) - util::text_width("Server  "));

	PacketBuilder builder(PACKET_TALK, PACKET_SERVER, message.length());
	builder.AddString(message);

	UTIL_FOREACH(this->characters, character)
	{
		character->Send(builder);
	}
}

void World::StatusMsg(std::string message)
{
    PacketBuilder builder;
    builder.SetID(PACKET_MESSAGE, PACKET_OPEN);
    builder.AddString(message);

    UTIL_FOREACH(this->characters, character)
    {
        character->Send(builder);
    }
}

void World::AdminReport(Character *from, std::string reportee, std::string message)
{
	message = util::text_cap(message, static_cast<int>(this->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from->SourceName()) + "  reports: " + reportee + ", "));

	PacketBuilder builder(PACKET_ADMININTERACT, PACKET_REPLY, 5 + from->SourceName().length() + message.length() + reportee.length());
	builder.AddChar(2); // message type
	builder.AddByte(255);
	builder.AddBreakString(from->SourceName());
	builder.AddBreakString(message);
	builder.AddBreakString(reportee);

	UTIL_FOREACH(this->characters, character)
	{
		if (character->SourceAccess() >= static_cast<int>(this->admin_config["reports"]))
		{
			character->Send(builder);
		}
	}

	short boardid = static_cast<int>(this->config["AdminBoard"]) - 1;

	if (static_cast<std::size_t>(boardid) < this->boards.size())
	{
		std::string chat_log_dump;
		Board *admin_board = this->boards[boardid];

		Board_Post *newpost = new Board_Post;
		newpost->id = ++admin_board->last_id;
		newpost->author = from->SourceName();
		newpost->author_admin = from->admin;
		newpost->subject = std::string(" [Report] ") + util::ucfirst(from->SourceName()) + " reports: " + reportee;
		newpost->body = message;
		newpost->time = Timer::GetTime();

		if (int(this->config["ReportChatLogSize"]) > 0)
		{
			chat_log_dump = from->GetChatLogDump();
			newpost->body += "\r\n\r\n";
			newpost->body += chat_log_dump;
		}

		if (this->config["LogReports"])
		{
			try
			{
				this->db.Query("INSERT INTO `reports` (`reporter`, `reported`, `reason`, `time`, `chat_log`) VALUES ('$', '$', '$', #, '$')",
					from->SourceName().c_str(),
					reportee.c_str(),
					message.c_str(),
					int(std::time(0)),
					chat_log_dump.c_str()
				);
			}
			catch (Database_Exception& e)
			{
				Console::Err("Could not save report to database.");
				Console::Err("%s", e.error());
			}
		}

		admin_board->posts.push_front(newpost);

		if (admin_board->posts.size() > static_cast<std::size_t>(static_cast<int>(this->config["AdminBoardLimit"])))
		{
			admin_board->posts.pop_back();
		}
	}
}

void World::AdminRequest(Character *from, std::string message)
{
	message = util::text_cap(message, static_cast<int>(this->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from->SourceName()) + "  needs help: "));

	PacketBuilder builder(PACKET_ADMININTERACT, PACKET_REPLY, 4 + from->SourceName().length() + message.length());
	builder.AddChar(1); // message type
	builder.AddByte(255);
	builder.AddBreakString(from->SourceName());
	builder.AddBreakString(message);

	UTIL_FOREACH(this->characters, character)
	{
		if (character->SourceAccess() >= static_cast<int>(this->admin_config["reports"]))
		{
			character->Send(builder);
		}
	}

	short boardid = static_cast<int>(this->server->world->config["AdminBoard"]) - 1;

	if (static_cast<std::size_t>(boardid) < this->server->world->boards.size())
	{
		Board *admin_board = this->server->world->boards[boardid];

		Board_Post *newpost = new Board_Post;
		newpost->id = ++admin_board->last_id;
		newpost->author = from->SourceName();
		newpost->author_admin = from->admin;
		newpost->subject = std::string(" [Request] ") + util::ucfirst(from->SourceName()) + " needs help";
		newpost->body = message;
		newpost->time = Timer::GetTime();

		admin_board->posts.push_front(newpost);

		if (admin_board->posts.size() > static_cast<std::size_t>(static_cast<int>(this->server->world->config["AdminBoardLimit"])))
		{
			admin_board->posts.pop_back();
		}
	}
}

void World::Rehash()
{
	try
	{
		this->config.Read("config.ini");
		this->admin_config.Read("admin.ini");

		this->drops_config.Read(this->config["DropsFile"]);
		this->shops_config.Read(this->config["ShopsFile"]);
		this->arenas_config.Read(this->config["ArenasFile"]);
		this->formulas_config.Read(this->config["FormulasFile"]);
		this->home_config.Read(this->config["HomeFile"]);
		this->skills_config.Read(this->config["SkillsFile"]);
		this->npcs_config.Read(this->config["NPCsFile"]);

		this->eosbot_config.Read(this->config["EOSbotFile"]);
		this->achievements_config.Read(this->config["AchievementsFile"]);
		this->message_config.Read(this->config["MessageFile"]);
		this->event_config.Read(this->config["EventFile"]);
		this->devilgate_config.Read(this->config["DevilGateFile"]);
		this->pets_config.Read(this->config["PetsFile"]);
		this->harvesting_config.Read(this->config["HarvestingFile"]);
		this->chatlogs_config.Read(this->config["ChatlogsFile"]);
		this->fishing_config.Read(this->config["FishingFile"]);
		this->mining_config.Read(this->config["MiningFile"]);
		this->woodcutting_config.Read(this->config["WoodcuttingFile"]);
		this->cooking_config.Read(this->config["CookingFile"]);
		this->effects_config.Read(this->config["EffectsFile"]);
		this->timedeffects_config.Read(this->config["TimedEffectsFile"]);
		this->members_config.Read(this->config["MembersFile"]);
		this->warps_config.Read(this->config["WarpsFile"]);
		this->giftbox1_config.Read(this->config["Giftbox1File"]);
		this->giftbox2_config.Read(this->config["Giftbox2File"]);
		this->shaving_config.Read(this->config["ShavingFile"]);
		this->tilemessage_config.Read(this->config["TileMessageFile"]);
		this->partymap_config.Read(this->config["PartyMapsFile"]);
		this->ctf_config.Read(this->config["CTFFile"]);
		this->buffspells_config.Read(this->config["BuffSpellsFile"]);
		this->cursefilter_config.Read(this->config["CurseFilterFile"]);
		this->buffitems_config.Read(this->config["BuffItemsFile"]);
		this->commands_config.Read(this->config["CommandsFile"]);
		this->equipment_config.Read(this->config["EquipmentFile"]);
		this->spells_config.Read(this->config["SpellsFile"]);
		this->poison_config.Read(this->config["PoisonFile"]);
		this->pvp_config.Read(this->config["PVPFile"]);
		this->shrines_config.Read(this->config["ShrinesFile"]);
	}
	catch (std::runtime_error &e)
	{
		Console::Err(e.what());
	}

    this->UpdateConfig();
	this->LoadHome();
	this->LoadFish();
	this->LoadMine();
	this->LoadWood();
    this->LoadWlist();

	UTIL_FOREACH(this->maps, map)
	{
		map->LoadArena();

		UTIL_FOREACH(map->npcs, npc)
		{
			npc->LoadShopDrop();
		}
	}

	UTIL_FOREACH(this->commands, command)
    {
        command->Reload(this);
    }
}

void World::LoadFish()
{
    this->fish_drops.clear();

    UTIL_FOREACH(this->fishing_config, hc)
    {
        std::vector<std::string> parts = util::explode(',', static_cast<std::string>(hc.second));

        if (parts.size() != 3)
        {
            continue;
        }

        Fish_Drop *drop(new Fish_Drop);
        drop->item = util::to_int(hc.first);
        drop->chance = util::to_float(parts[0]);
        drop->levelreq = util::to_int(parts[1]);
        drop->exp = util::to_int(parts[2]);
        this->fish_drops.push_back(drop);
    }
}

void World::LoadMine()
{
    this->mine_drops.clear();

    UTIL_FOREACH(this->mining_config, hc)
    {
        std::vector<std::string> parts = util::explode(',', static_cast<std::string>(hc.second));

        if (parts.size() != 3)
        {
            continue;
        }

        Mine_Drop *drop(new Mine_Drop);
        drop->item = util::to_int(hc.first);
        drop->chance = util::to_float(parts[0]);
        drop->levelreq = util::to_int(parts[1]);
        drop->exp = util::to_int(parts[2]);
        this->mine_drops.push_back(drop);
    }
}

void World::LoadWood()
{
    this->wood_drops.clear();

    UTIL_FOREACH(this->woodcutting_config, hc)
    {
        std::vector<std::string> parts = util::explode(',', static_cast<std::string>(hc.second));

        if (parts.size() != 3)
        {
            continue;
        }

        Wood_Drop *drop(new Wood_Drop);
        drop->item = util::to_int(hc.first);
        drop->chance = util::to_float(parts[0]);
        drop->levelreq = util::to_int(parts[1]);
        drop->exp = util::to_int(parts[2]);
        this->wood_drops.push_back(drop);
    }
}

void World::ReloadPub(bool quiet)
{
    auto eif_id = this->eif->rid;
	auto enf_id = this->enf->rid;
	auto esf_id = this->esf->rid;
	auto ecf_id = this->ecf->rid;

	this->eif->Read(this->config["EIF"]);
	this->enf->Read(this->config["ENF"]);
	this->esf->Read(this->config["ESF"]);
	this->ecf->Read(this->config["ECF"]);

	if (eif_id != this->eif->rid || enf_id != this->enf->rid || esf_id != this->esf->rid || ecf_id != this->ecf->rid)
	{
		if (!quiet)
		{
			UTIL_FOREACH(this->characters, character)
			{
				character->ServerMsg("The server has been reloaded, please log out and in again.");
			}
		}
	}
}

void World::ReloadQuests()
{
	UTIL_FOREACH(this->characters, c)
	{
		UTIL_FOREACH(c->quests, q)
		{
		    if (!q.second)
				continue;

			short quest_id = q.first;
            std::string quest_name = q.second->StateName();
            std::string progress = q.second->SerializeProgress();

            c->quests_inactive.insert({quest_id, quest_name, progress});
		}
	}

	UTIL_FOREACH(this->characters, c)
	{
		c->quests.clear();
	}

	short max_quest = static_cast<int>(this->config["Quests"]);

    UTIL_FOREACH(this->enf->data, npc)
    {
        if (npc.type == ENF::Quest)
            max_quest = std::max(max_quest, npc.vendor_id);
    }

    for (short i = 0; i <= max_quest; ++i)
    {
		try
		{
			std::shared_ptr<Quest> q = std::make_shared<Quest>(i, this);
            this->quests[i] = std::move(q);
		}
		catch (...)
		{
            this->quests.erase(i);
		}
	}

	UTIL_IFOREACH(this->quests, it)
    {
        if (it->first > max_quest)
        {
            try
            {
                std::shared_ptr<Quest> q = std::make_shared<Quest>(it->first, this);
                std::swap(it->second, q);
            }
            catch (...)
            {
                it = this->quests.erase(it);
            }
        }
    }

	UTIL_FOREACH(this->characters, c)
	{
		c->quests.clear();

		UTIL_FOREACH(c->quests_inactive, state)
		{
			auto quest_it = this->quests.find(state.quest_id);

			if (quest_it == this->quests.end())
			{
				Console::Wrn("Quest not found: %i. Marking as inactive.", state.quest_id);
				continue;
			}

			Quest* quest = quest_it->second.get();
			auto quest_context(std::make_shared<Quest_Context>(c, quest));

			try
            {
                quest_context->SetState(state.quest_state, false);
                quest_context->UnserializeProgress(UTIL_CRANGE(state.quest_progress));
            }
            catch (EOPlus::Runtime_Error& ex)
            {
                Console::Wrn(ex.what());
                Console::Wrn("Could not resume quest: %i. Marking as inactive.", state.quest_id);

                if (!c->quests_inactive.insert(std::move(state)).second)
                    Console::Wrn("Duplicate inactive quest record dropped for quest: %i", state.quest_id);

                continue;
            }

            auto result = c->quests.insert(std::make_pair(state.quest_id, std::move(quest_context)));

			if (!result.second)
            {
                Console::Wrn("Duplicate quest record dropped for quest: %i", state.quest_id);
                continue;
            }

			result.first->second->SetState(state.quest_state, false);
			result.first->second->UnserializeProgress(UTIL_CRANGE(state.quest_progress));
		}
	}

	UTIL_FOREACH(this->characters, c)
	{
		c->CheckQuestRules();
	}

	Console::GreenOut("%i/%i quests loaded.", this->quests.size(), max_quest);
}

Character *World::GetCharacter(std::string name)
{
	name = util::lowercase(name);

	UTIL_FOREACH(this->characters, character)
	{
		if (character->SourceName() == name)
		{
			return character;
		}
	}

	return 0;
}

Character *World::GetCharacterReal(std::string real_name)
{
    real_name = util::lowercase(real_name);

    UTIL_FOREACH(this->characters, character)
    {
        if (character->real_name == real_name)
        {
            return character;
        }
    }

    return 0;
}

Character *World::GetCharacterPID(unsigned int id)
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

Character *World::GetCharacterCID(unsigned int id)
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

Map *World::GetMap(short id)
{
	try
	{
		return this->maps.at(id - 1);
	}
	catch (...)
	{
		try
		{
			return this->maps.at(0);
		}
		catch (...)
		{
			throw std::runtime_error("Map #" + util::to_string(id) + " and fallback map #1 are unavailable");
		}
	}
}

Home *World::GetHome(Character *character)
{
	Home *home = 0;

	static Home *null_home = new Home;

	UTIL_FOREACH(this->homes, h)
	{
		if (h->id == character->home)
			return h;
	}

	int current_home_level = -2;

	UTIL_FOREACH(this->homes, h)
	{
		if (h->level <= character->level && h->level > current_home_level)
		{
			home = h;
			current_home_level = h->level;
		}
	}

	if (!home)
		home = null_home;

	return home;
}

Home *World::GetHome(std::string id)
{
	UTIL_FOREACH(this->homes, h)
	{
		if (h->id == id)
		{
			return h;
		}
	}

	return 0;
}

bool World::CharacterExists(std::string name)
{
	Database_Result res = this->db.Query("SELECT 1 FROM `characters` WHERE `name` = '$'", name.c_str());
	return !res.empty();
}

Character *World::CreateCharacter(Player *player, std::string name, Gender gender, int hairstyle, int haircolor, Skin race)
{
	char buffer[1024];
	std::string startmapinfo;
	std::string startmapval;

	if (static_cast<int>(this->config["StartMap"]))
	{
	    using namespace std;
		startmapinfo = ", `map`, `x`, `y`";
		snprintf(buffer, 1024, ",%i,%i,%i", static_cast<int>(this->config["StartMap"]), static_cast<int>(this->config["StartX"]), static_cast<int>(this->config["StartY"]));
		startmapval = buffer;
	}

	this->db.Query("INSERT INTO `characters` (`name`, `account`, `gender`, `hairstyle`, `haircolor`, `race`, `inventory`, `bank`, `paperdoll`, `spells`, `quest`, `vars`@) VALUES ('$','$',#,#,#,#,'$','','$','$','',''@)",
		startmapinfo.c_str(), name.c_str(), player->username.c_str(), gender, hairstyle, haircolor, race,
		static_cast<std::string>(this->config["StartItems"]).c_str(), static_cast<std::string>(gender?this->config["StartEquipMale"]:this->config["StartEquipFemale"]).c_str(),
		static_cast<std::string>(this->config["StartSpells"]).c_str(), startmapval.c_str());

	return new Character(name, this);
}

void World::DeleteCharacter(std::string name)
{
	this->db.Query("DELETE FROM `pets` WHERE owner_name = '$'", name.c_str());
    this->db.Query("DELETE FROM `characters` WHERE name = '$'", name.c_str());
}

Player *World::Login(const std::string& username, util::secure_string&& password)
{
	if (LoginCheck(username, std::move(password)) == LOGIN_WRONG_USERPASS)
		return 0;

	return new Player(username, this);
}

Player *World::Login(std::string username)
{
	return new Player(username, this);
}

LoginReply World::LoginCheck(const std::string& username, util::secure_string&& password)
{
	{
		util::secure_string password_buffer(std::move(std::string(this->config["PasswordSalt"]) + username + password.str()));
		password = sha256(password_buffer.str());
	}

	Database_Result res = this->db.Query("SELECT 1 FROM `accounts` WHERE `username` = '$' AND `password` = '$'", username.c_str(), password.str().c_str());

	if (res.empty())
	{
		return LOGIN_WRONG_USERPASS;
	}
	else if (this->PlayerOnline(username))
	{
		return LOGIN_LOGGEDIN;
	}
	else
	{
		return LOGIN_OK;
	}
}

bool World::CreatePlayer(const std::string& username, util::secure_string&& password,
	const std::string& fullname, const std::string& location, const std::string& email,
	const std::string& computer, const std::string& hdid, const std::string& ip)
{
	{
		util::secure_string password_buffer(std::move(std::string(this->config["PasswordSalt"]) + username + password.str()));
		password = sha256(password_buffer.str());
	}

	Database_Result result = this->db.Query("INSERT INTO `accounts` (`username`, `password`, `fullname`, `location`, `email`, `computer`, `hdid`, `regip`, `created`) VALUES ('$','$','$','$','$','$','$','$',#)",
		username.c_str(), password.str().c_str(), fullname.c_str(), location.c_str(), email.c_str(), computer.c_str(), hdid.c_str(), ip.c_str(), int(std::time(0)));

	return !result.Error();
}

bool World::PlayerExists(std::string username)
{
	Database_Result res = this->db.Query("SELECT 1 FROM `accounts` WHERE `username` = '$'", username.c_str());
	return !res.empty();
}

bool World::PlayerOnline(std::string username)
{
	if (!Player::ValidName(username))
	{
		return false;
	}

	UTIL_FOREACH(this->server->clients, client)
	{
		EOClient *eoclient = static_cast<EOClient *>(client);

		if (eoclient->player)
		{
			if (eoclient->player->username.compare(username) == 0)
			{
				return true;
			}
		}
	}

	return false;
}

void World::ResetCTF()
{
    this->redcounter = 0;
    this->bluecounter = 0;

    this->bluemembers = 0;
    this->redmembers = 0;

    this->atbluebase = true;
    this->atredbase = true;

    if (this->ctf)
        this->ctf = false;

    if (this->ctfenabled)
        this->ctfenabled = false;
}

void World::ResetPVP()
{
    this->pvp_redkillcount = 0;
    this->pvp_bluekillcount = 0;

    this->pvp_bluemembers = 0;
    this->pvp_redmembers = 0;

    if (this->pvp)
        this->pvp = false;

    if (this->pvpenabled)
        this->pvpenabled = false;
}

void World::Mute(Command_Source *from, Character *victim, bool announce)
{
    if (announce)
		this->ServerMsg(i18n.Format("AnnounceMuted", victim->SourceName(), from ? from->SourceName() : "server", i18n.Format("Muted")));

	victim->Mute(from->SourceName());
}

void World::Kick(Command_Source *from, Character *victim, bool announce)
{
	if (announce)
		this->ServerMsg(i18n.Format("AnnounceRemoved", victim->SourceName(), from ? from->SourceName() : "server", i18n.Format("Kicked")));

	victim->player->client->Close();
}

void World::Wall(Command_Source *from, Character *victim, bool announce)
{
	if (announce)
		this->ServerMsg(i18n.Format("AnnounceRemoved", victim->SourceName(), from ? from->SourceName() : "server", i18n.Format("Walled")));

	victim->Warp(static_cast<int>(this->config["WallMap"]), static_cast<int>(this->config["WallX"]), static_cast<int>(this->config["WallY"]), this->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
}

void World::Jail(Command_Source *from, Character *victim, bool announce)
{
	if (announce)
		this->ServerMsg(i18n.Format("AnnounceRemoved", victim->SourceName(), from ? from->SourceName() : "server", i18n.Format("Jailed")));

	victim->Warp(static_cast<int>(this->config["JailMap"]), static_cast<int>(this->config["JailX"]), static_cast<int>(this->config["JailY"]), this->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
}

void World::Unjail(Command_Source *from, Character *victim)
{
    bool bubbles = this->config["WarpBubbles"] && !victim->hidden;

    Character* charfrom = dynamic_cast<Character*>(from);

    if (charfrom && charfrom->hidden)
        bubbles = false;

    if (victim->mapid != static_cast<int>(this->config["JailMap"]))
        return;

    victim->Warp(static_cast<int>(this->config["JailMap"]), static_cast<int>(this->config["UnJailX"]), static_cast<int>(this->config["UnJailY"]), bubbles ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
}

void World::Unban(Command_Source* from, std::string name, bool announce)
{
    if (this->CharacterExists(name))
    {
        Database_Result res = this->db.Query("SELECT `account` FROM `characters` WHERE `name` = '$'", name.c_str());
        std::unordered_map<std::string, util::variant> row = res.front();
        std::string account = static_cast<std::string>(row["account"]);

        if (this->CheckBan(account))
        {
            if (announce)
                this->ServerMsg(i18n.Format("AnnounceUnbanned", util::ucfirst(name), from ? from->SourceName() : "server", i18n.Format("Unbanned")));

            this->db.Query("DELETE FROM `bans` WHERE username = '$'", account.c_str());
        }
        else
        {
            from->ServerMsg(util::ucfirst(name) + " is not banned.");
        }
    }
    else
    {
        from->ServerMsg(util::ucfirst(name) + " does not exist.");
    }
}

void World::Ban(Command_Source *from, Character *victim, int duration, bool announce)
{
    std::string from_str = from ? from->SourceName() : "server";

	if (announce)
		this->ServerMsg(i18n.Format("AnnounceRemoved", victim->SourceName(), from_str, i18n.Format("banned")));

	std::string query("INSERT INTO bans (username, ip, hdid, expires, setter) VALUES ");

	query += "('" + db.Escape(victim->player->username) + "', ";
	query += util::to_string(static_cast<int>(victim->player->client->GetRemoteAddr())) + ", ";
	query += util::to_string(victim->player->client->hdid) + ", ";

	if (duration == -1)
	{
		query += "0";
	}
	else
	{
		query += util::to_string(int(std::time(0) + duration));
	}

	query += ", '" + db.Escape(from_str) + "')";

	try
	{
		this->db.Query(query.c_str());
	}
	catch (Database_Exception& e)
	{
		Console::Err("Could not save ban to database.");
		Console::Err("%s", e.error());
	}

	victim->player->client->Close();
}

int World::CheckBan(const std::string *username, const IPAddress *address, const int *hdid)
{
	std::string query("SELECT COALESCE(MAX(expires),-1) AS expires FROM bans WHERE (");

	if (!username && !address && !hdid)
		return -1;

	if (username)
	{
		query += "username = '";
		query += db.Escape(*username);
		query += "' OR ";
	}

	if (address)
	{
		query += "ip = ";
		query += util::to_string(static_cast<int>(*const_cast<IPAddress *>(address)));
		query += " OR ";
	}

	if (hdid)
	{
		query += "hdid = ";
		query += util::to_string(*hdid);
		query += " OR ";
	}

	Database_Result res = db.Query((query.substr(0, query.length()-4) + ") AND (expires > # OR expires = 0)").c_str(), int(std::time(0)));

	return static_cast<int>(res[0]["expires"]);
}

bool World::CheckBan(std::string username)
{
    Database_Result res = this->db.Query("SELECT 1 FROM `bans` WHERE `username` = '$'", username.c_str());

    return !res.empty();
}

static std::list<int> PKExceptUnserialize(std::string serialized)
{
	std::list<int> list;
	std::size_t p = 0;
	std::size_t lastp = std::numeric_limits<std::size_t>::max();

	if (!serialized.empty() && *(serialized.end()-1) != ',')
	{
		serialized.push_back(',');
	}

	while ((p = serialized.find_first_of(',', p+1)) != std::string::npos)
	{
		list.push_back(util::to_int(serialized.substr(lastp+1, p-lastp-1)));
		lastp = p;
	}

	return list;
}

bool World::PKExcept(const Map *map)
{
	return this->PKExcept(map->id);
}

bool World::PKExcept(int mapid)
{
	if (mapid == static_cast<int>(this->config["JailMap"]))
	{
		return true;
	}

	if (this->GetMap(mapid)->arena)
	{
		return true;
	}

	std::list<int> except_list = PKExceptUnserialize(this->config["PKExcept"]);

	return std::find(except_list.begin(), except_list.end(), mapid) != except_list.end();
}

bool World::LoadWlist()
{
    std::ifstream wlist;
    wlist.open(this->config["WhitelistFile"]);

    if (wlist.is_open())
    {
        this->wlist.clear();
        std::string sbuf;

        while(wlist.good())
        {
            getline(wlist, sbuf);

            for (int position = int(sbuf.find(' ')) ;int(sbuf.find(' ')) != -1; position = int(sbuf.find(' ')))
                sbuf.replace(position, 1, "");

            if ((util::lowercase(sbuf).find("ip:localhost")) != -1)
                sbuf = "ip:127.0.0.1";

            if (sbuf[0] != '#')
                this->wlist.push_back(sbuf);
        }

        wlist.close();
        return true;
    }

    Console::Wrn("Unable to open whitelist file: %s", std::string(this->config["WhitelistFile"]).c_str());
    return false;
}

bool World::CheckWlist(std::string checkstr, std::string checktag)
{
    if (checktag != "")
        checkstr = checktag + ":" + checkstr;

    UTIL_FOREACH(this->wlist, whitstr)
    {
        if (whitstr == checkstr)
            return true;

        if (whitstr.substr(0,6) == "admin:" && checkstr.substr(0,6) == "admin:")
        {
            int w_admlvl = util::to_int(whitstr.substr(6));
            int c_admlvl = util::to_int(checkstr.substr(6));

            if (c_admlvl >= w_admlvl)
                return true;
        }
    }

    return false;
}

bool World::IsInstrument(int graphic_id)
{
	return std::find(UTIL_RANGE(this->instrument_ids), graphic_id) != this->instrument_ids.end();
}

World::~World()
{
	std::list<Character *> todelete;

	UTIL_FOREACH(this->characters, character)
	{
		todelete.push_back(character);
	}

	UTIL_FOREACH(todelete, character)
	{
		character->player->client->Close(true);
		delete character;
	}

	delete this->guildmanager;

	if (this->config["TimedSave"])
	{
		this->db.Commit();
	}
}

void World::Restart()
{
    TimeEvent *event = new TimeEvent(world_restart, this, 5.0, 1);
	this->timer.Register(event);
}

void World::DevilGateEnd()
{
    TimeEvent *event = new TimeEvent(world_endgate, this, static_cast<double>(this->devilgate_config["EndingTimer"]), 1);
	this->timer.Register(event);
}

void World::PartyMaps()
{
    if (double(this->partymap_config["KickTimer"]) > 0)
    {
        TimeEvent *event = new TimeEvent(world_partymaps, this, static_cast<double>(this->partymap_config["KickTimer"]), 1);
        this->timer.Register(event);
    }
}

void World::DevilTimer()
{
    if (double(this->devilgate_config["StartTimer"]) > 0)
    {
        TimeEvent *event = new TimeEvent(world_deviltimer, this, double(this->devilgate_config["StartTimer"]), 1);
        this->timer.Register(event);
    }
}

void World::CTFTimer()
{
    if (double(this->ctf_config["StartTimer"]) > 0)
    {
        TimeEvent *event = new TimeEvent(world_ctftimer, this, double(this->ctf_config["StartTimer"]), 1);
        this->timer.Register(event);
    }
}

void World::CTFDelay()
{
    UTIL_FOREACH(this->characters, character)
    {
        int mapid = int(this->ctf_config["CTFMap"]);

        int bx = util::to_int(util::explode(',', this->ctf_config["CTFBlueSpawnXY"])[0]);
        int by = util::to_int(util::explode(',', this->ctf_config["CTFBlueSpawnXY"])[1]);

        int rx = util::to_int(util::explode(',', this->ctf_config["CTFRedSpawnXY"])[0]);
        int ry = util::to_int(util::explode(',', this->ctf_config["CTFRedSpawnXY"])[1]);

        if (bx <= 0 || by <= 0 || rx <= 0 || ry <= 0)
            return;

        if (character->mapid == mapid)
        {
            if (character->blueteam == true)
                character->Warp(mapid, bx, by, this->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
            else if (character->redteam == true)
                character->Warp(mapid, rx, ry, this->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
        }
    }

    if (this->ctf == true)
    {
        TimeEvent *event = new TimeEvent(world_ctfdelay, this, 1.0, 1);
        this->timer.Register(event);
    }
}

void World::PVPTimer()
{
    if (double(this->pvp_config["StartTimer"]) > 0)
    {
        TimeEvent *event = new TimeEvent(world_pvptimer, this, double(this->pvp_config["StartTimer"]), 1);
        this->timer.Register(event);
    }
}
