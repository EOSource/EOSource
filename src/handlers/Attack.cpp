#include "handlers.hpp"

#include "../character.hpp"
#include "../map.hpp"
#include "../player.hpp"
#include "../config.hpp"
#include "../npc.hpp"

namespace Handlers
{
    void Attack_Use(Character *character, PacketReader &reader)
    {
        Direction direction = static_cast<Direction>(reader.GetChar());
        Timestamp timestamp = reader.GetThree();

        int ts_diff = timestamp - character->timestamp;

        if (character->world->config["EnforceTimestamps"])
        {
            if (ts_diff < 48)
                return;
        }

        character->timestamp = timestamp;

        if (character->sitting != SIT_STAND)
            return;

        if (int(character->world->config["EnforceWeight"]) >= 1 && character->weight > character->maxweight)
            return;

        int limit_attack = character->world->config["LimitAttack"];

        if (limit_attack != 0 && character->attacks >= limit_attack)
            return;

        int target_x = character->x;
        int target_y = character->y;

        switch (character->direction)
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

        for (int i = 0 ; i < int(character->world->effects_config["TAmount"]) ; i++)
        {
            if (character->paperdoll[Character::Weapon] == int(character->world->effects_config[util::to_string(i+1) + ".WeaponID"]))
            {
                int distance = character->world->effects_config[util::to_string(i+1) + ".DistanceID"];
                int effect1 = character->world->effects_config[util::to_string(i+1) + ".EffectID1"];
                int effect2 = character->world->effects_config[util::to_string(i+1) + ".EffectID2"];
                int effect3 = character->world->effects_config[util::to_string(i+1) + ".EffectID3"];

                PacketBuilder reply(PACKET_EFFECT, PACKET_AGREE);

                for (int i = 1; i <= distance; i ++)
                {
                    int effect = 0;

                    while (effect <= 2)
                    {
                        if (character->direction == 0)
                        {
                            reply.AddChar(character->x);
                            reply.AddChar(character->y + i);
                        }
                        else if (character->direction == 1)
                        {
                            reply.AddChar(character->x - i);
                            reply.AddChar(character->y);
                        }
                        else if (character->direction == 2)
                        {
                            reply.AddChar(character->x);
                            reply.AddChar(character->y - i);
                        }
                        else if (character->direction == 3)
                        {
                            reply.AddChar(character->x + i);
                            reply.AddChar(character->y);
                        }

                        switch (effect)
                        {
                            case 0:
                            reply.AddShort(effect1);
                            break;

                            case 1:
                            reply.AddShort(effect2);
                            break;

                            case 2:
                            reply.AddShort(effect3);
                            break;

                            default:
                            break;
                        }

                        effect ++;
                    }

                    character->Send(reply);
                }
            }
        }

        for (int i = 0 ; i < int(character->world->event_config["Amount"]); i++)
        {
            if (int(character->world->event_config[util::to_string(i+1) + ".PlayerLocation"]) > 0)
            {
                int direction = int(character->world->event_config[util::to_string(i+1) + ".Direction"]);

                if (character->direction == direction)
                {
                    int M = util::to_int(util::explode(',', character->world->event_config[util::to_string(i+1) + ".PlayerLocation"])[0]);
                    int X = util::to_int(util::explode(',', character->world->event_config[util::to_string(i+1) + ".PlayerLocation"])[1]);
                    int Y = util::to_int(util::explode(',', character->world->event_config[util::to_string(i+1) + ".PlayerLocation"])[2]);

                    if (character->mapid == M && character->x == X && character->y == Y)
                    {
                        int random = util::rand(1, 100);

                        if (random > 100 - int(character->world->event_config["WinChance"]))
                        {
                            int Reward = int(character->world->event_config["RewardID"]);
                            int Amount = int(character->world->event_config["RewardAmount"]);

                            character->GiveItem(Reward, Amount);

                            character->world->ServerMsg("Attention!! " + character->SourceName() + " gained " + util::to_string(Amount) + " " + character->world->eif->Get(Reward).name + " for winning the event.");
                        }
                        else
                        {
                            character->ServerMsg("You didn't win anything!");
                        }

                        character->Warp(character->oldmap, character->oldx, character->oldy, character->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                    }
                }
            }
        }

        if (character->map->GetSpec(target_x, target_y) == Map_Tile::Harvesting)
        {
            int random = util::rand(1,100);

            int id = int(character->world->harvesting_config["HarvestingItem"]);
            int chance = int(character->world->harvesting_config["HarvestingChance"]);
            int amount = util::rand(int(character->world->harvesting_config["HarvestingMinAmount"]), int(character->world->harvesting_config["HarvestingMaxAmount"]));

            if (random > 100 - chance)
            {
                character->GiveItem(id,amount);

                if (character->world->harvesting_config["Hurt"])
                {
                    if ((double(util::rand(0,10000)) / 100.0) < int(character->world->harvesting_config["HurtChance"]))
                    {
                        int amount = util::rand(int(character->world->harvesting_config["MinHurt"]), int(character->world->harvesting_config["MaxHurt"]));
                        int limitamount = std::min(amount, int(character->hp));

                        if (character->world->config["LimitDamage"])
                            amount = limitamount;

                        character->hp -= limitamount;

                        PacketBuilder builder;
                        builder.SetID(PACKET_AVATAR, PACKET_REPLY);
                        builder.AddShort(character->player->id);
                        builder.AddShort(character->player->id);
                        builder.AddThree(amount);
                        builder.AddChar(character->direction);
                        builder.AddChar(int(double(character->hp) / double(character->maxhp) * 100.0));
                        builder.AddChar(character->hp == 0);

                        UTIL_FOREACH(character->map->characters, checkchar)
                        {
                            if (character->InRange(checkchar))
                                checkchar->Send(builder);
                        }

                        if (character->hp == 0)
                            character->DeathRespawn();

                        character->PacketRecover();
                    }
                }

                character->StatusMsg("You harvested " + util::to_string(amount) + " " + character->world->eif->Get(id).name);
            }
            else
            {
                character->StatusMsg("You failed to harvest anything.");
            }
        }

        if (character->world->ctf == true)
        {
            if (character->mapid == int(character->world->ctf_config["CTFMap"]))
            {
                Map *maps = character->world->GetMap(character->mapid);

                int blueflag = int(character->world->ctf_config["BlueFlag"]);
                int redflag = int(character->world->ctf_config["RedFlag"]);

                int bx = util::to_int(util::explode(',', character->world->ctf_config["BlueXY"])[0]);
                int by = util::to_int(util::explode(',', character->world->ctf_config["BlueXY"])[1]);

                int rx = util::to_int(util::explode(',', character->world->ctf_config["RedXY"])[0]);
                int ry = util::to_int(util::explode(',', character->world->ctf_config["RedXY"])[1]);

                if (bx <= 0 || by <= 0 || rx <= 0 || ry <= 0)
                    return;

                UTIL_FOREACH(maps->characters, checkchar)
                {
                    if (character->blueteam == true)
                    {
                        if (util::path_length(character->x, character->y, checkchar->x, checkchar->y) == 1)
                        {
                            if (checkchar->HasItem(blueflag))
                            {
                                checkchar->bluehost = false;
                                checkchar->RemoveItem(blueflag, 1);
                                character->map->AddItem(blueflag, 1, bx, by, character);
                                character->world->ServerMsg(util::ucfirst(character->SourceName()) + " has recovered the blue team's flag!");
                                character->world->atbluebase = true;

                                UTIL_FOREACH(character->map->characters, all)
                                {
                                    all->Refresh();
                                }
                            }

                            if (checkchar->redteam == true)
                                checkchar->Warp(int(character->world->ctf_config["CTFMap"]), rx, ry, character->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                        }
                    }
                    else if (character->redteam == true)
                    {
                        if (util::path_length(character->x, character->y, checkchar->x, checkchar->y) == 1)
                        {
                            if (checkchar->HasItem(redflag))
                            {
                                checkchar->redhost = false;
                                checkchar->RemoveItem(redflag, 1);
                                character->map->AddItem(redflag, 1, rx, ry, character);
                                character->world->ServerMsg(util::ucfirst(character->SourceName()) + " has recovered the red team's flag!");
                                character->world->atredbase = true;

                                UTIL_FOREACH(character->map->characters, all)
                                {
                                    all->Refresh();
                                }
                            }

                            if (checkchar->blueteam == true)
                                checkchar->Warp(int(character->world->ctf_config["CTFMap"]), bx, by, character->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                        }
                    }
                }
            }
        }

        if (!character->world->config["EnforceTimestamps"] || ts_diff >= 60)
            direction = character->direction;

        character->Attack(direction);
    }

    PACKET_HANDLER_REGISTER(PACKET_ATTACK)
        Register(PACKET_USE, Attack_Use, Playing, 0.5);
    PACKET_HANDLER_REGISTER_END()
}
