#include "handlers.hpp"

#include <functional>

#include "../character.hpp"
#include "../npc.hpp"
#include "../player.hpp"
#include "../party.hpp"

namespace Handlers
{
    static void walk_common(Character *character, PacketReader &reader, bool (Character::*f)(Direction))
    {
        Direction direction = static_cast<Direction>(reader.GetChar());

        Timestamp timestamp = reader.GetThree();

        unsigned char x = reader.GetChar();
        unsigned char y = reader.GetChar();

        if (character->world->config["EnforceTimestamps"])
    	{
    		if (timestamp - character->timestamp < 36)
    		{
    			return;
    		}
    	}

    	character->timestamp = timestamp;

        if (character->sitting != SIT_STAND)
            return;

        if (direction <= 3)
        {
            character->npc = 0;
            character->npc_type = ENF::NPC;
            character->board = 0;
            character->jukebox_open = false;

            if (character->trading)
            {
                PacketBuilder builder(PACKET_TRADE, PACKET_CLOSE, 2);
                builder.AddShort(character->player->id);
                character->trade_partner->Send(builder);

                character->trading = false;
                character->trade_inventory.clear();
                character->trade_agree = false;

                character->trade_partner->trading = false;
                character->trade_partner->trade_inventory.clear();
                character->trade_agree = false;

                character->CheckQuestRules();
                character->trade_partner->CheckQuestRules();

                character->trade_partner->trade_partner = 0;
                character->trade_partner = 0;
            }

            if (!(character->*f)(direction))
            {
                return;
            }
        }

        if (character->x != x || character->y != y)
            character->Refresh();

        if (character->map->GetSpec(character->x, character->y) == Map_Tile::Spikes2 || character->map->GetSpec(character->x, character->y) == Map_Tile::Spikes3)
        {
            int amount = 0;

            if (character->map->GetSpec(character->x, character->y) == Map_Tile::Spikes2)
                amount = util::rand(int(character->world->config["DefaultSpikeMinDamage"]), int(character->world->config["DefaultSpikeMaxDamage"]));
            if (character->map->GetSpec(character->x, character->y) == Map_Tile::Spikes3)
                amount = util::rand(int(character->world->config["InvisibleSpikeMinDamage"]), int(character->world->config["InvisibleSpikeMaxDamage"]));

            int limitamount = std::min(amount, int(character->hp));

            if (character->world->config["LimitDamage"])
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

            UTIL_FOREACH(character->map->characters, mapchar)
            {
                if (!mapchar->InRange(character) || mapchar == character)
                    continue;

                if (!character->hidden)
                    mapchar->Send(builder);
                else if ((character->hidden && mapchar->admin >= static_cast<int>(character->world->admin_config["seehide"])) || character == mapchar)
                    mapchar->Send(builder);
            }

            character->PacketRecover();

            if (character->hp == 0)
                character->DeathRespawn();

            if (character->party)
                character->party->UpdateHP(character);
        }

        for (int ii = 0; ii < int(character->world->tilemessage_config["Amount"]); ii++)
        {
            int clas = character->world->tilemessage_config[util::to_string(ii+1) + ".class"];
            int reborn = character->world->tilemessage_config[util::to_string(ii+1) + ".reborn"];
            int minlevel = character->world->tilemessage_config[util::to_string(ii+1) + ".minlevel"];
            int maxlevel = character->world->tilemessage_config[util::to_string(ii+1) + ".maxlevel"];

            int M = util::to_int(util::explode(',', character->world->tilemessage_config[util::to_string(ii+1) + ".location"])[0]);
            int X = util::to_int(util::explode(',', character->world->tilemessage_config[util::to_string(ii+1) + ".location"])[1]);
            int Y = util::to_int(util::explode(',', character->world->tilemessage_config[util::to_string(ii+1) + ".location"])[2]);

            if (M < 0 || X < 0 || Y < 0)
                return;

            if (int(character->world->tilemessage_config[util::to_string(ii+1) + ".class"]) == 0 || character->clas == clas)
            {
                if (character->level >= minlevel && (character->level <= maxlevel || maxlevel == 0))
                {
                    if (character->mapid == M && character->x == X && character->y == Y && character->rebirth >= reborn)
                    {
                        for (int i = 1; std::string(character->world->tilemessage_config[util::to_string(ii+1) + ".message." + util::to_string(i)]) != "0"; ++i)
                        {
                            if (i != 0)
                            {
                                PacketBuilder builder;
                                builder.SetID(PACKET_QUEST, PACKET_DIALOG);
                                builder.AddChar(1);
                                builder.AddShort(446);
                                builder.AddShort(446);
                                builder.AddInt(0);
                                builder.AddByte(255);
                                builder.AddShort(446);
                                builder.AddBreakString(std::string(character->world->tilemessage_config[util::to_string(ii+1) + ".title"]));

                                for (int i = 1; std::string(character->world->tilemessage_config[util::to_string(ii+1) + ".message." + util::to_string(i)]) != "0"; ++i)
                                {
                                    builder.AddShort(1);
                                    builder.AddBreakString(character->world->tilemessage_config[util::to_string(ii+1) + ".message." + util::to_string(i)]);
                                }

                                character->Send(builder);
                            }
                        }
                    }
                }
            }
        }

        if (character->world->ctf == true)
        {
            Map *maps = character->world->GetMap(int(character->world->ctf_config["CTFMap"]));

            if (character->mapid == int(character->world->ctf_config["CTFMap"]))
            {
                int blueflag = int(character->world->ctf_config["BlueFlag"]);
                int redflag = int(character->world->ctf_config["RedFlag"]);

                int bx = util::to_int(util::explode(',', character->world->ctf_config["BlueXY"])[0]);
                int by = util::to_int(util::explode(',', character->world->ctf_config["BlueXY"])[1]);

                int rx = util::to_int(util::explode(',', character->world->ctf_config["RedXY"])[0]);
                int ry = util::to_int(util::explode(',', character->world->ctf_config["RedXY"])[1]);

                int fbarmor = character->world->ctf_config["FemaleBlueTeamArmor"];
                int frarmor = character->world->ctf_config["FemaleRedTeamArmor"];

                int mbarmor = character->world->ctf_config["MaleBlueTeamArmor"];
                int mrarmor = character->world->ctf_config["MaleRedTeamArmor"];

                const EIF_Data &fblue = character->world->eif->Get(fbarmor);
                const EIF_Data &fred = character->world->eif->Get(frarmor);

                const EIF_Data &mblue = character->world->eif->Get(mbarmor);
                const EIF_Data &mred = character->world->eif->Get(mrarmor);

                int rounds = int(character->world->ctf_config["RoundsToWin"]);

                if (bx <= 0 || by <= 0 || rx <= 0 || ry <= 0)
                    return;

                bool DeletedFlags = false;

                if (character->blueteam == true)
                {
                    if (character->x == rx && character->y == ry)
                    {
                        character->Warp(character->mapid, rx -1, ry -1, WARP_ANIMATION_NONE);

                        if (character->gender == 0)
                            character->Dress(Character::Armor, fblue.dollgraphic);
                        else if (character->gender == 1)
                            character->Dress(Character::Armor, mblue.dollgraphic);
                    }
                    else if (character->x == bx && character->y == by)
                    {
                        if (!character->HasItem(redflag))
                        {
                            character->Warp(character->mapid, bx -1, by -1, WARP_ANIMATION_NONE);

                            if (character->gender == 0)
                                character->Dress(Character::Armor, fblue.dollgraphic);
                            else if (character->gender == 1)
                                character->Dress(Character::Armor, mblue.dollgraphic);
                        }

                        if (character->HasItem(redflag) >= 1)
                        {
                            if (character->world->atbluebase == true)
                            {
                                DeletedFlags = false;

                                UTIL_FOREACH(character->map->characters, checkchar)
                                {
                                    checkchar->RemoveItem(blueflag, 1);
                                    checkchar->RemoveItem(redflag, 1);
                                }

                                character->world->bluecounter ++;

                                if (DeletedFlags == false)
                                {
                                    restart_loop1:
                                    UTIL_FOREACH(maps->items, item)
                                    {
                                        maps->DelItem(item->uid, 0);
                                        DeletedFlags = true;
                                        goto restart_loop1;
                                    }
                                }

                                if (character->world->bluecounter < rounds || character->world->redcounter < rounds)
                                {
                                    character->map->AddItem(blueflag, 1, bx, by, character);
                                    character->map->AddItem(redflag, 1, rx, ry, character);

                                    UTIL_FOREACH(maps->characters, checkchar)
                                    {
                                        checkchar->Refresh();
                                    }

                                    character->world->atbluebase = true;
                                    character->world->atredbase = true;
                                }

                                character->world->CTFDelay();
                            }
                            else
                            {
                                character->ServerMsg("Recover your team's flag first!");
                            }
                        }
                    }
                }
                else if (character->redteam == true)
                {
                    if (character->x == bx && character->y == by)
                    {
                        character->Warp(character->mapid, bx -1, by -1, WARP_ANIMATION_NONE);

                        if (character->gender == 0)
                            character->Dress(Character::Armor, fred.dollgraphic);
                        else if (character->gender == 1)
                            character->Dress(Character::Armor, mred.dollgraphic);
                    }
                    else if (character->x == rx && character->y == ry)
                    {
                        if (character->HasItem(blueflag) == 1)
                        {
                            if (character->world->atredbase == true)
                            {
                                DeletedFlags = false;

                                UTIL_FOREACH(character->map->characters, checkchar)
                                {
                                    checkchar->RemoveItem(blueflag, 1);
                                    checkchar->RemoveItem(redflag, 1);
                                }

                                character->world->redcounter ++;

                                if (DeletedFlags == false)
                                {
                                    restart_loop2:
                                    UTIL_FOREACH(maps->items, item)
                                    {
                                        maps->DelItem(item->uid, 0);
                                        DeletedFlags = true;
                                        goto restart_loop2;
                                    }
                                }

                                if (character->world->bluecounter < rounds || character->world->redcounter < rounds)
                                {
                                    character->map->AddItem(blueflag, 1, bx, by, character);
                                    character->map->AddItem(redflag, 1, rx, ry, character);

                                    UTIL_FOREACH(maps->characters, checkchar)
                                    {
                                        checkchar->Refresh();
                                    }

                                    character->world->atbluebase = true;
                                    character->world->atredbase = true;
                                }

                                character->world->CTFDelay();
                            }
                            else
                            {
                                character->ServerMsg("Recover your team's flag first!");
                            }
                        }
                    }
                }
            }
        }

        for (int i = 0; i < int(character->world->shrines_config["Amount"]); i++)
        {
            int M = int(character->world->shrines_config[util::to_string(i+1) + ".ShrineMap"]);
            int X = int(character->world->shrines_config[util::to_string(i+1) + ".ShrineX"]);
            int Y = int(character->world->shrines_config[util::to_string(i+1) + ".ShrineY"]);

            if (character->mapid == M && character->x == X && character->y == Y)
            {
                PacketBuilder builder;
                builder.SetID(PACKET_QUEST, PACKET_DIALOG);
                builder.AddChar(1);
                builder.AddShort(400);
                builder.AddShort(400);
                builder.AddInt(0);
                builder.AddByte(255);
                builder.AddShort(400);
                builder.AddBreakString("Warp List");
                builder.AddShort(1);
                builder.AddBreakString("Select a warp location.");

                for (int i = 0; i < int(character->world->shrines_config["Amount"]); i++)
                {
                    builder.AddShort(2);
                    builder.AddShort(i+1);

                    builder.AddBreakString(std::string(character->world->shrines_config[util::to_string(i+1) + ".WarpName"]) + (int(character->world->shrines_config[util::to_string(i+1) + ".WarpCost"]) == 0 ? "" : (" - " + util::to_string(int(character->world->shrines_config[util::to_string(i+1) + ".WarpCost"])) + " " + character->world->eif->Get(1).name)));
                }

                character->Send(builder);
            }
        }
    }

    void Walk_Admin(Character *character, PacketReader &reader)
    {
        if (character->SourceDutyAccess() < int(character->world->admin_config["nowall"]))
            return;

        walk_common(character, reader, &Character::AdminWalk);
    }

    void Walk_Player(Character *character, PacketReader &reader)
    {
        walk_common(character, reader, &Character::Walk);
    }

    PACKET_HANDLER_REGISTER(PACKET_WALK)
        Register(PACKET_ADMIN, Walk_Admin, Playing, 0.4);
        Register(PACKET_PLAYER, Walk_Player, Playing, 0.4);
        Register(PACKET_SPEC, Walk_Player, Playing, 0.4);
    PACKET_HANDLER_REGISTER_END()
}
