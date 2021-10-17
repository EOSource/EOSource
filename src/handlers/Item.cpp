#include "handlers.hpp"

#include "../character.hpp"
#include "../eodata.hpp"
#include "../map.hpp"
#include "../party.hpp"
#include "../player.hpp"
#include "../quest.hpp"
#include "../world.hpp"
#include "../npc.hpp"

std::string get_timestr(std::string format = "%c");

static std::list<int> ExceptUnserialize(std::string serialized)
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
        list.push_back(util::to_int(serialized.substr(lastp + 1, p-lastp - 1)));
        lastp = p;
    }

    return list;
}

namespace Handlers
{
    void Item_Use(Character *character, PacketReader &reader)
    {
        if (character->trading) return;

        int id = reader.GetShort();

        if (character->HasItem(id))
        {
            const EIF_Data& item = character->world->eif->Get(id);

            PacketBuilder reply(PACKET_ITEM, PACKET_REPLY, 3);
            reply.AddChar(item.type);
            reply.AddShort(id);

            auto QuestUsedItems = [](Character* character, int id)
            {
                UTIL_FOREACH(character->quests, q)
                {
                    if (!q.second || q.second->GetQuest()->Disabled())
                        continue;

                    q.second->UsedItem(id);
                }
            };

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

            for (int i = 0 ; i < int(character->world->pets_config["PotionAmount"]); i++)
            {
                if (id == int(character->world->pets_config[util::to_string(i+1) + ".PotionID"]))
                {
                    if (character->HasPet)
                    {
                        character->pet->hp += int(character->world->pets_config[util::to_string(i+1) + ".HPAmount"]);
                        character->pet->tp += int(character->world->pets_config[util::to_string(i+1) + ".TPAmount"]);

                        character->pet->hp = std::min(character->pet->hp, character->pet->Data().hp);
                        character->pet->tp = std::min(character->pet->tp, character->pet->maxtp);
                    }
                }
            }

            for (int i = 0 ; i < int(character->world->pets_config["PetAmount"]); i++)
            {
                int item = character->world->pets_config[util::to_string(i+1) + ".ItemID"];
                int npcid = character->world->pets_config[util::to_string(i+1) + ".NpcID"];

                std::list<int> except_list = ExceptUnserialize(character->world->pets_config["AntiSpawnIDs"]);

                if (id == item && !character->nowhere)
                {
                    if (std::find(except_list.begin(), except_list.end(), character->mapid) != except_list.end())
                    {
                        character->StatusMsg("You cannot spawn any pets on this map!");
                    }
                    else
                    {
                        character->HatchPet(character, npcid);

                        if (!character->HasPet)
                        {
                            character->AddPet(character, npcid);
                        }
                        else
                        {
                            character->PetTransfer();
                            character->pettransfer = false;
                        }
                    }

                    return;
                }
            }

            for (int i = 0 ; i < int(character->world->buffitems_config["Amount"]); i++)
            {
                if (id == int(character->world->buffitems_config[util::to_string(i+1) + ".BuffItem"]))
                {
                    int effect = character->world->buffitems_config[util::to_string(i+1) + ".BuffEffect"];
                    int str = character->world->buffitems_config[util::to_string(i+1) + ".BuffSTR"];
                    int lnt = character->world->buffitems_config[util::to_string(i+1) + ".BuffINT"];
                    int wis = character->world->buffitems_config[util::to_string(i+1) + ".BuffWIS"];
                    int agi = character->world->buffitems_config[util::to_string(i+1) + ".BuffAGI"];
                    int con = character->world->buffitems_config[util::to_string(i+1) + ".BuffCON"];
                    int cha = character->world->buffitems_config[util::to_string(i+1) + ".BuffCHA"];
                    int time = character->world->buffitems_config[util::to_string(i+1) + ".BuffTime"];
                    double exp = character->world->buffitems_config[util::to_string(i+1) + ".EXPRate"];
                    double drop = character->world->buffitems_config[util::to_string(i+1) + ".DropRate"];

                    if (character->boosttimer < Timer::GetTime())
                    {
                        character->boosttimer = Timer::GetTime() + int(time);

                        if (effect > 0) character->boosteffect = effect;
                        if (str > 0) character->booststr += str;
                        if (lnt > 0) character->boostint += lnt;
                        if (wis > 0) character->boostwis += wis;
                        if (agi > 0) character->boostagi += agi;
                        if (con > 0) character->boostcon += con;
                        if (cha > 0) character->boostcha += cha;
                        if (exp > 0) character->boostexp = exp;
                        if (drop > 0) character->boostdrop = drop;

                        character->CalculateStats();
                        character->PacketRecover();
                        character->UpdateStats();

                        character->ServerMsg("You have been boosted for " + util::to_string(time) + " seconds.");
                    }
                    else
                    {
                        character->ServerMsg("you are already boosted!");
                    }
                }
            }

            if (id == int(character->world->buffitems_config["UndoBuffItem"]))
                character->UndoBuff();

            for (int i = 0 ; i < static_cast<int>(character->world->giftbox1_config["Amount"]); i++)
            {
                int GiftID = static_cast<int>(character->world->giftbox1_config[util::to_string(i+1) + ".GiftID"]);

                if (id == GiftID)
                {
                    int Random = util::rand(1,100);
                    int Chance = static_cast<int>(character->world->giftbox1_config[util::to_string(i+1) + ".Chance"]);

                    if (Random > 100 - Chance)
                    {
                        restart_loop:

                        int EffectID = static_cast<int>(character->world->giftbox1_config[util::to_string(i+1) + ".Effect"]);

                        int Reward = util::rand(static_cast<int>(character->world->giftbox1_config[util::to_string(i+1) + ".MinRewardID"]), static_cast<int>(character->world->giftbox1_config[util::to_string(i+1) + ".MaxRewardID"]));
                        int Amount = util::rand(static_cast<int>(character->world->giftbox1_config[util::to_string(i+1) + ".MinAmount"]), static_cast<int>(character->world->giftbox1_config[util::to_string(i+1) + ".MaxAmount"]));

                        std::list<int> except_list = ExceptUnserialize(character->world->giftbox1_config["ExceptID"]);

                        if (std::find(except_list.begin(), except_list.end(), Reward) != except_list.end())
                        {
                            goto restart_loop;
                        }
                        else
                        {
                            character->GiveItem(Reward, Amount);

                            if (EffectID != 0)
                                character->Effect(EffectID);

                            character->ServerMsg("You have won " + util::to_string(Amount) + " " + character->world->eif->Get(Reward).name);
                        }
                    }
                    else
                    {
                        character->ServerMsg("You didn't win anything!");
                    }
                }
            }

            for (int i = 0 ; i < static_cast<int>(character->world->giftbox2_config["Amount"]); i++)
            {
                int GiftID = static_cast<int>(character->world->giftbox2_config[util::to_string(i+1) + ".GiftID"]);

                if (id == GiftID)
                {
                    std::vector<int> rewards;

                    int EffectID = static_cast<int>(character->world->giftbox2_config[util::to_string(i+1) + ".Effect"]);

                    int MinAmountID = static_cast<int>(character->world->giftbox2_config[util::to_string(i+1) + ".MinAmount"]);
                    int MaxAmountID = static_cast<int>(character->world->giftbox2_config[util::to_string(i+1) + ".MaxAmount"]);

                    int Amount = util::rand(MinAmountID, MaxAmountID);

                    for (int ii = 1; std::string(character->world->giftbox2_config[util::to_string(i+1) + ".RewardID" + util::to_string(ii)]) != "0"; ++ii)
                    {
                        rewards.push_back(character->world->giftbox2_config[util::to_string(i+1) + ".RewardID" + util::to_string(ii)]);
                    }

                    int RandomID = (rewards.at(util::rand(0, rewards.size() - 1)));

                    if (rewards.size() > 0)
                    {
                        character->GiveItem(RandomID, Amount);

                        if (EffectID != 0)
                            character->Effect(EffectID);

                        character->ServerMsg("You have won " + util::to_string(Amount) + " " + character->world->eif->Get(RandomID).name);
                    }
                }
            }

            if (character->world->eif->Get(id).unkf > 0 && character->world->config["RegenerationItems"])
            {
                if (character->regeneratetimer < Timer::GetTime())
                {
                    character->regenerateid = id;
                    character->regeneratetimer = Timer::GetTime() + character->world->eif->Get(id).unkf;
                    character->ServerMsg("Your stats are regenerating for " + util::to_string(character->world->eif->Get(id).unkf) + " seconds.");
                }
                else
                {
                    character->ServerMsg("Your stats are still regenerating.");
                    return;
                }
            }

            if (character->map->GetSpec(target_x, target_y) == Map_Tile::Cooking)
            {
                for (int i = 0; i < int(character->world->cooking_config["Amount"]); i++)
                {
                    int itemid = character->world->cooking_config[util::to_string(i+1) + ".CookItemID"];
                    int level = character->world->cooking_config[util::to_string(i+1) + ".LevelRequirement"];

                    if (id == itemid)
                    {
                        if (character->cooking < Timer::GetTime())
                        {
                            if (character->clevel >= level)
                            {
                                character->cookid = (i+1);
                                character->cooking = Timer::GetTime() + 5;

                                character->StatusMsg(character->world->i18n.Format("Cooking-Start"));
                            }
                            else
                            {
                                character->StatusMsg(character->world->i18n.Format("Cooking-Error", util::to_string(level)));
                                return;
                            }
                        }
                        else
                        {
                            character->StatusMsg(character->world->i18n.Format("Cooking-Busy"));
                            return;
                        }
                    }
                }
            }

            if (id == int(character->world->pets_config["PetInventoryItemID"]))
            {
                if (character->HasPet)
                    character->pet->OpenInventory();

                return;
            }

            if (id == int(character->world->pets_config["RevivalPotion"]))
            {
                if (!character->RevivePets())
                    return;
            }

            if (id == int(character->world->pets_config[util::to_string(id) + ".HatchPet"]))
            {
                if (!character->HatchPet(character, util::to_int(character->world->pets_config[util::to_string(id) + ".HatchPet"])))
                {
                    character->StatusMsg("This pet can't be hatched. You already have this type of pet.");
                    return;
                }
            }

            switch (item.type)
            {
                case EIF::Teleport:
                {
                    if (!character->map->scroll)
                    {
                        break;
                    }

                    character->DelItem(id, 1);

                    reply.ReserveMore(6);
                    reply.AddInt(character->HasItem(id));
                    reply.AddChar(character->weight);
                    reply.AddChar(character->maxweight);

                    if (item.scrollmap == 0)
                    {
                        character->Warp(character->SpawnMap(), character->SpawnX(), character->SpawnY(), WARP_ANIMATION_SCROLL);
                    }
                    else
                    {
                        character->Warp(item.scrollmap, item.scrollx, item.scrolly, WARP_ANIMATION_SCROLL);
                    }

                    character->Send(reply);

                    QuestUsedItems(character, id);
                }
                break;

                case EIF::Heal:
                {
                    int hpgain = item.hp;
                    int tpgain = item.tp;

                    if (character->world->config["LimitDamage"])
                    {
                        hpgain = std::min(hpgain, character->maxhp - character->hp);
                        tpgain = std::min(tpgain, character->maxtp - character->tp);
                    }

                    hpgain = std::max(hpgain, 0);
                    tpgain = std::max(tpgain, 0);

                    character->hp += hpgain;
                    character->tp += tpgain;

                    if (!character->world->config["LimitDamage"])
                    {
                        character->hp = std::min(character->hp, character->maxhp);
                        character->tp = std::min(character->tp, character->maxtp);
                    }

                    character->DelItem(id, 1);

                    reply.ReserveMore(14);
                    reply.AddInt(character->HasItem(id));
                    reply.AddChar(character->weight);
                    reply.AddChar(character->maxweight);

                    reply.AddInt(hpgain);
                    reply.AddShort(character->hp);
                    reply.AddShort(character->tp);

                    PacketBuilder builder(PACKET_RECOVER, PACKET_AGREE, 7);
                    builder.AddShort(character->player->id);
                    builder.AddInt(hpgain);
                    builder.AddChar(util::clamp<int>(double(character->hp) / double(character->maxhp) * 100.0, 0, 100));

                    UTIL_FOREACH(character->map->characters, updatecharacter)
                    {
                        if (updatecharacter != character && character->InRange(updatecharacter))
                        {
                            updatecharacter->Send(builder);
                        }
                    }

                    if (character->party)
                    {
                        character->party->UpdateHP(character);
                    }

                    character->Send(reply);

                    QuestUsedItems(character, id);
                }
                break;

                case EIF::HairDye:
                {
                    if (character->haircolor == item.haircolor)
                    {
                        break;
                    }

                    character->haircolor = item.haircolor;

                    character->DelItem(id, 1);

                    reply.ReserveMore(7);
                    reply.AddInt(character->HasItem(id));
                    reply.AddChar(character->weight);
                    reply.AddChar(character->maxweight);

                    reply.AddChar(item.haircolor);

                    PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 5);
                    builder.AddShort(character->player->id);
                    builder.AddChar(SLOT_HAIRCOLOR);
                    builder.AddChar(0);
                    builder.AddChar(item.haircolor);

                    UTIL_FOREACH(character->map->characters, updatecharacter)
                    {
                        if (updatecharacter != character && character->InRange(updatecharacter))
                        {
                            updatecharacter->Send(builder);
                        }
                    }

                    character->Send(reply);

                    QuestUsedItems(character, id);
                }
                break;

                case EIF::Beer:
                {
                    character->DelItem(id, 1);

                    reply.ReserveMore(6);
                    reply.AddInt(character->HasItem(id));
                    reply.AddChar(character->weight);
                    reply.AddChar(character->maxweight);

                    character->Send(reply);

                    QuestUsedItems(character, id);
                }
                break;

                case EIF::EffectPotion:
                {
                    character->DelItem(id, 1);

                    reply.ReserveMore(8);
                    reply.AddInt(character->HasItem(id));
                    reply.AddChar(character->weight);
                    reply.AddChar(character->maxweight);
                    reply.AddShort(item.effect);

                    character->Effect(item.effect, false);

                    character->Send(reply);

                    QuestUsedItems(character, id);
                }
                break;

                case EIF::CureCurse:
                {
                    bool found = false;

                    for (std::size_t i = 0; i < character->paperdoll.size(); ++i)
                    {
                        if (character->world->eif->Get(character->paperdoll[i]).special == EIF::Cursed)
                        {
                            character->paperdoll[i] = 0;
                            found = true;
                        }
                    }

                    if (!found)
                    {
                        break;
                    }

                    character->CalculateStats();
                    character->DelItem(id, 1);

                    reply.ReserveMore(32);
                    reply.AddInt(character->HasItem(id));
                    reply.AddChar(character->weight);
                    reply.AddChar(character->maxweight);

                    reply.AddShort(character->maxhp);
                    reply.AddShort(character->maxtp);

                    reply.AddShort(character->display_str);
                    reply.AddShort(character->display_intl);
                    reply.AddShort(character->display_wis);
                    reply.AddShort(character->display_agi);
                    reply.AddShort(character->display_con);
                    reply.AddShort(character->display_cha);

                    reply.AddShort(character->mindam);
                    reply.AddShort(character->maxdam);
                    reply.AddShort(character->accuracy);
                    reply.AddShort(character->evade);
                    reply.AddShort(character->armor);

                    PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 14);
                    builder.AddShort(character->player->id);
                    builder.AddChar(SLOT_CLOTHES);
                    builder.AddChar(0);
                    character->AddPaperdollData(builder, "BAHWS");

                    UTIL_FOREACH(character->map->characters, updatecharacter)
                    {
                        if (updatecharacter != character && character->InRange(updatecharacter))
                        {
                            updatecharacter->Send(builder);
                        }
                    }

                    character->Send(reply);

                    QuestUsedItems(character, id);
                }
                break;

                case EIF::EXPReward:
                {
                    bool level_up = false;

                    character->GiveEXP(std::min(std::max(item.expreward, 0), int(character->world->config["MaxExp"])));
                    character->exp = std::min(character->exp, int(character->map->world->config["MaxExp"]));

                    while (character->level < int(character->map->world->config["MaxLevel"]) && character->exp >= character->map->world->exp_table[character->level+1])
                    {
                        level_up = true;
                        ++character->level;
                        character->statpoints += int(character->map->world->config["StatPerLevel"]);
                        character->skillpoints += int(character->map->world->config["SkillPerLevel"]);
                        character->CalculateStats();
                    }

                    character->DelItem(id, 1);

                    reply.ReserveMore(21);
                    reply.AddInt(character->HasItem(id));
                    reply.AddChar(character->weight);
                    reply.AddChar(character->maxweight);

                    reply.AddInt(character->exp);

                    reply.AddChar(level_up ? character->level : 0);
                    reply.AddShort(character->statpoints);
                    reply.AddShort(character->skillpoints);
                    reply.AddShort(character->maxhp);
                    reply.AddShort(character->maxtp);
                    reply.AddShort(character->maxsp);

                    if (level_up)
                    {
                        if (character->party)
                        {
                            UTIL_FOREACH(character->party->members, member)
                            {
                                character->party->RefreshMembers(member->player->character);
                            }
                        }

                        PacketBuilder builder(PACKET_RECOVER, PACKET_REPLY, 9);
                        builder.AddInt(character->exp);
                        builder.AddShort(character->karma);
                        builder.AddChar(character->level);
                        builder.AddShort(character->statpoints);
                        builder.AddShort(character->skillpoints);

                        UTIL_FOREACH(character->map->characters, check)
    					{
    						if (character != check && character->InRange(check))
    						{
    							PacketBuilder builder(PACKET_ITEM, PACKET_ACCEPT, 2);
    							builder.AddShort(character->player->id);
    							character->Send(builder);
    						}
    					}
                    }

                    character->Send(reply);

                    QuestUsedItems(character, id);
                }
                break;

                default:
                return;
            }
        }
    }

    void Item_Drop(Character *character, PacketReader &reader)
    {
        if (character->trading) return;
        if (!character->CanInteractItems()) return;

        int id = reader.GetShort();
        int amount;

        if (character->world->eif->Get(id).special == EIF::Lore)
        {
            return;
        }

        if (reader.Remaining() == 5)
        {
            amount = reader.GetThree();
        }
        else
        {
            amount = reader.GetInt();
        }

        unsigned char x = reader.GetByte();
        unsigned char y = reader.GetByte();

        amount = std::min<int>(amount, character->world->config["MaxDrop"]);

        if (amount <= 0)
        {
            return;
        }

        int blueflag = int(character->world->ctf_config["BlueFlag"]);
        int redflag = int(character->world->ctf_config["RedFlag"]);

        if (id == redflag || id == blueflag)
        {
            character->ServerMsg("You aren't allowed to drop the flag!");
            return;
        }

        if (std::string(character->world->config["AdminTradeProtection"]) == "yes")
        {
            if (character->admin > ADMIN_PLAYER && character->admin < ADMIN_HGM)
            {
                character->StatusMsg("You cannot drop any items - [Admin Protection]");
                return;
            }
        }

        if (x == 255 && y == 255)
        {
            x = character->x;
            y = character->y;
        }
        else
        {
            x = PacketProcessor::Number(x);
            y = PacketProcessor::Number(y);
        }

        int distance = util::path_length(x, y, character->x, character->y);

        if (distance > static_cast<int>(character->world->config["DropDistance"]))
            return;

        if (!character->map->Walkable(x, y))
            return;

        if (character->HasItem(id) >= amount && character->mapid != static_cast<int>(character->world->config["JailMap"]))
        {
            std::shared_ptr<Map_Item> item = character->map->AddItem(id, amount, x, y, character);

            if (item)
            {
                item->owner = character->player->id;
                item->unprotecttime = Timer::GetTime() + static_cast<double>(character->world->config["ProtectPlayerDrop"]);

                character->DelItem(id, amount);

                PacketBuilder reply(PACKET_ITEM, PACKET_DROP, 15);
                reply.AddShort(id);
                reply.AddThree(amount);
                reply.AddInt(character->HasItem(id));
                reply.AddShort(item->uid);
                reply.AddChar(x);
                reply.AddChar(y);
                reply.AddChar(character->weight);
                reply.AddChar(character->maxweight);
                character->Send(reply);

                if (character->world->chatlogs_config["LogDrops"])
                {
                    FILE *fh = fopen("./data/logs/drops.txt", "a");
                    fprintf(fh, "%s %s dropped %i %s (ID: %i)\n", get_timestr().c_str(), util::ucfirst(character->SourceName()).c_str(), amount, character->world->eif->Get(id).name.c_str(), id);
                    fclose(fh);
                }
            }
        }
    }

    void Item_Junk(Character *character, PacketReader &reader)
    {
        if (character->trading) return;

        int id = reader.GetShort();
        int amount = reader.GetInt();

        if (amount <= 0)
            return;

        int blueflag = int(character->world->ctf_config["BlueFlag"]);
        int redflag = int(character->world->ctf_config["RedFlag"]);

        if (id == redflag || id == blueflag)
        {
            character->ServerMsg("You aren't allowed to junk the flag!");
            return;
        }

        if (character->HasItem(id) >= amount)
        {
            character->DelItem(id, amount);

            PacketBuilder reply(PACKET_ITEM, PACKET_JUNK, 11);
            reply.AddShort(id);
            reply.AddThree(amount);
            reply.AddInt(character->HasItem(id));
            reply.AddChar(character->weight);
            reply.AddChar(character->maxweight);
            character->Send(reply);

            if (character->world->chatlogs_config["LogJunk"])
            {
                FILE *fh = fopen("./data/logs/junk.txt", "a");
                fprintf(fh, "%s %s junked %i %s (ID: %i)\n", get_timestr().c_str(), util::ucfirst(character->SourceName()).c_str(), amount, character->world->eif->Get(id).name.c_str(), id);
                fclose(fh);
            }
        }
    }

    void Item_Get(Character *character, PacketReader &reader)
    {
        int uid = reader.GetShort();

        std::shared_ptr<Map_Item> item = character->map->GetItem(uid);

        if (item)
        {
            int distance = util::path_length(item->x, item->y, character->x, character->y);

            if (distance > static_cast<int>(character->world->config["DropDistance"]))
                return;

            int blueflag = int(character->world->ctf_config["BlueFlag"]);
            int redflag = int(character->world->ctf_config["RedFlag"]);

            if (character->redteam == false && character->blueteam == false)
            {
                if (item->id == blueflag || item->id == redflag)
                {
                    character->ServerMsg("You cannot pick up this item because you aren't participating in the CTF event!");
                    return;
                }
            }

            if (character->blueteam == true)
            {
                if (item->id == redflag)
                {
                    UTIL_FOREACH(character->map->characters, checkchar)
                    {
                        checkchar->ServerMsg(util::ucfirst(character->SourceName()) + " has taken the red team's flag!");
                    }

                    character->world->atredbase = false;
                    character->redhost = true;
                }

                if (item->id == blueflag)
                {
                    character->ServerMsg("You cannot pick up your own flag!");
                    return;
                }
            }

            if (character->redteam == true)
            {
                if (item->id == blueflag)
                {
                    UTIL_FOREACH(character->map->characters, checkchar)
                    {
                        checkchar->ServerMsg(util::ucfirst(character->SourceName()) + " has taken the blue team's flag!");
                    }

                    character->world->atbluebase = false;
                    character->bluehost = true;
                }

                if (item->id == redflag)
                {
                    character->ServerMsg("You cannot pick up your own flag!");
                    return;
                }
            }

            if (item->owner != character->player->id && item->unprotecttime > Timer::GetTime())
                return;

            int taken = character->CanHoldItem(item->id, item->amount);

            if (taken > 0)
            {
                character->AddItem(item->id, taken);

                PacketBuilder reply(PACKET_ITEM, PACKET_GET, 9);
                reply.AddShort(uid);
                reply.AddShort(item->id);
                reply.AddThree(taken);
                reply.AddChar(character->weight);
                reply.AddChar(character->maxweight);
                character->Send(reply);

                character->map->DelSomeItem(item->uid, taken, character);
            }

            if (character->world->chatlogs_config["LogPickup"])
            {
                FILE *fh = fopen("./data/logs/pickup.txt", "a");
                fprintf(fh, "%s %s picked up %i %s (ID: %i)\n", get_timestr().c_str(), util::ucfirst(character->SourceName()).c_str(), taken, character->world->eif->Get(item->id).name.c_str(), item->id);
                fclose(fh);
            }
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_ITEM)
        Register(PACKET_USE, Item_Use, Playing);
        Register(PACKET_DROP, Item_Drop, Playing);
        Register(PACKET_JUNK, Item_Junk, Playing);
        Register(PACKET_GET, Item_Get, Playing);
    PACKET_HANDLER_REGISTER_END()
}
