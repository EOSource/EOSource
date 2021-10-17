#include "commands.hpp"

#include "../../util.hpp"
#include "../../map.hpp"
#include "../../npc.hpp"
#include "../../eoplus.hpp"
#include "../../packet.hpp"
#include "../../player.hpp"
#include "../../quest.hpp"
#include "../../world.hpp"

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

namespace PlayerCommands
{
    void PetCommands(const std::vector<std::string>& arguments, Character* from)
    {
        std::list<int> except_list = ExceptUnserialize(from->SourceWorld()->pets_config["AntiSpawnIDs"]);

        if (arguments[0] == "call" && !from->nowhere && from->world->commands_config["PetCall"])
        {
            std::string name;

            for (unsigned short i = 1; i < arguments.size(); i++)
            {
                if (name == "")
                {
                    name = arguments[i];
                }
                else
                {
                    name += " " + arguments[i];
                }
            }

            if (std::find(except_list.begin(), except_list.end(), from->mapid) != except_list.end())
            {
                from->StatusMsg("You cannot spawn any pets on this map!");
            }
            else
            {
                from->SummonPet(from, util::lowercase(name));
            }
        }
        else if (arguments[0] == "list" && from->world->commands_config["PetList"])
        {
            Database_Result res = from->world->db.Query("SELECT `name`, `npcid`, `level`, `rebirth` FROM `pets` WHERE `owner_name` = '$'", from->SourceName().c_str());

            if (!res.empty())
            {
                UTIL_FOREACH_REF(res, row)
                {
                    std::string petname = row["name"];

                    int npcid = row["npcid"];
                    int level = row["level"];
                    int rebirths = row["rebirth"];
                    int truelevel = level + (rebirths * util::to_int(from->world->pets_config["RebirthLevel"]));

                    ENF_Data& npc = from->world->enf->Get(npcid);

                    from->ServerMsg("[Pet name]: " + util::ucfirst(petname) + " [Level]: " + util::to_string(truelevel) + " [Species]: " + npc.name);
                }
            }
            else
            {
                from->StatusMsg("You have no pets.");
            }
        }
        else if (arguments[0] == "talk" && from->world->commands_config["PetTalk"])
        {
            if (from->pettalk == true)
            {
                from->pettalk = false;
                from->StatusMsg("Your pet will no longer talk.");
            }
            else
            {
                from->pettalk = true;
                from->StatusMsg("Your pet will start talking again.");
            }
        }
        else if (arguments[0] == "attack" && from->world->commands_config["PetAttack"])
        {
            if (from->petattack == true)
            {
                from->petattack = false;
                from->StatusMsg("Your pet stopped looking for something to attack.");
            }
            else
            {
                from->petattack = true;
                from->StatusMsg("Your pet started looking for something to attack.");
            }
        }
        else if (arguments[0] == "drops" && from->world->commands_config["PetDrops"])
        {
            if (from->petdrops == true)
            {
                from->petdrops = false;
                from->StatusMsg("Your pet will no longer pick up drops.");
            }
            else
            {
                from->petdrops = true;
                from->StatusMsg("Your pet will now pick up drops.");
            }
        }
        else if (arguments[0] == "heal" && from->world->commands_config["PetHeal"])
        {
            if (from->petheal == true)
            {
                from->petheal = false;
                from->StatusMsg("Your pet will no longer try to heal you.");
            }
            else
            {
                from->petheal = true;
                from->StatusMsg("Your pet will attempt to heal you if it can.");
            }
        }
        else if (arguments[0] == "spells" && from->world->commands_config["PetSpells"])
        {
            if (from->petspells == true)
            {
                from->petspells = false;
                from->StatusMsg("Your pet will no longer try to cast spells");
            }
            else
            {
                from->petspells = true;
                from->StatusMsg("Your pet will attempt to cast spells if it can.");
            }
        }
        else if (arguments[0] == "commands" && from->world->commands_config["PetCommands"])
        {
            from->QuestMsg("Pet Commands", std::string(from->world->config["PlayerPrefix"]) + "pet call, dismiss, list, talk, attack, drops, spells, heal, say, name, reborn, inventory, transfer");
        }
        else if (from->HasPet)
        {
            if (arguments[0] == "dismiss" && from->world->commands_config["PetDismiss"])
            {
                from->PetTransfer();
                from->pettransfer = false;

                from->StatusMsg("Your pet, " + util::ucfirst(from->pet->name) + " was dismissed.");
            }
            else if (arguments[0] == "say" && from->world->commands_config["PetSay"])
            {
                std::string message;

                for (std::size_t i = 1; i < arguments.size(); ++i)
                {
                    message += arguments[i];

                    if (i < arguments.size() - 1)
                    {
                        message += " ";
                    }
                }

                from->pet->ShowDialog(message);
            }
            else if (arguments[0] == "name" && from->world->commands_config["PetName"])
            {
                if (arguments.size() >= 2)
                {
                    std::string newname = util::lowercase(arguments[1]);

                    if (from->PetNameTaken(from, newname))
                    {
                        from->ServerMsg("Your pet, " + util::ucfirst(from->pet->name) + " had it's name changed to " + util::ucfirst(newname));
                        from->world->db.Query("UPDATE `pets` SET `name` = '$' WHERE `name` = '$' AND `owner_name` = '$'", newname.c_str(), from->pet->name.c_str(), from->SourceName().c_str());
                        from->pet->name = util::lowercase(newname);
                    }
                    else
                    {
                        from->ServerMsg("You already have a pet with this name.");
                    }
                }
            }
            else if ((arguments[0] == "reborn" || arguments[0] == "rebirth") && from->world->commands_config["PetRebirth"])
            {
                int level = from->world->pets_config["RebirthLevel"];

                if (from->pet->level >= level)
                {
                    from->pet->rebirth ++;

                    from->pet->level = 0;
                    from->pet->exp = 0;

                    from->StatusMsg(from->pet->name + " has been reborn.");
                }
                else
                {
                    from->StatusMsg("Requires level " + util::to_string(level));
                }
            }
            else if (arguments[0] == "inventory" && from->world->commands_config["PetInventory"])
            {
                from->pet->OpenInventory();
            }
            else if (arguments[0] == "transfer" && from->world->commands_config["PetTransfer"])
            {
                Character *victim = from->world->GetCharacter(arguments[1]);

                if (arguments.size() < 3)
                    return;

                int amount = util::to_int(arguments[2]);

                if (amount < 0)
                    amount = 0;

                if (from->transfer_timer < Timer::GetTime())
                {
                    from->transfer_timer = Timer::GetTime() + 60.0;

                    if (victim && victim->online && victim->SourceName() != from->SourceName())
                    {
                        if (from->HasPet)
                        {
                            PacketBuilder builder;
                            builder.SetID(PACKET_QUEST, PACKET_DIALOG);
                            builder.AddChar(1);
                            builder.AddShort(447);
                            builder.AddShort(447);
                            builder.AddInt(0);
                            builder.AddByte(255);
                            builder.AddShort(447);
                            builder.AddBreakString("Pet transfer");
                            builder.AddShort(1);
                            builder.AddBreakString("Are you sure you want to transfer your pet to " + util::ucfirst(victim->SourceName()) + " for " + arguments[2] + " " + from->world->eif->Get(1).name + "?");
                            builder.AddShort(2);
                            builder.AddShort(3);
                            builder.AddBreakString("Yes, request transfer");
                            builder.AddShort(2);
                            builder.AddShort(4);
                            builder.AddBreakString("No, don't transfer");
                            from->Send(builder);

                            from->transfer_petname = from->world->enf->Get(from->pet->id).name;
                            from->transfer_petid = from->pet->id;
                            from->transfer_target = victim->SourceName();
                            from->transfer_cost = amount;

                            from->transfer_pending = true;
                        }
                        else
                        {
                            from->ServerMsg("Spawn your pet to transfer it to a new owner.");
                        }
                    }
                    else
                    {
                        from->ServerMsg(util::ucfirst(arguments[1]) + " is not online.");
                    }
                }
                else
                {
                    from->ServerMsg("You already have another transfer request pending, please wait..");
                }
            }
            else
            {
                from->StatusMsg("Unknown pet command.");
            }
        }
    }

    PLAYER_COMMAND_HANDLER_REGISTER()
        using namespace std::placeholders;
        RegisterCharacter({"pet"}, PetCommands);
    PLAYER_COMMAND_HANDLER_REGISTER_END()
}
