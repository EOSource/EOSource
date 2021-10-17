#include "commands.hpp"

#include "../../util.hpp"
#include "../../map.hpp"
#include "../../npc.hpp"
#include "../../eoplus.hpp"
#include "../../packet.hpp"
#include "../../player.hpp"
#include "../../quest.hpp"
#include "../../world.hpp"

namespace PlayerCommands
{
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

    void Update(const std::vector<std::string>& arguments, Character* from)
    {
        short *stat = 0;

        int amount = util::to_int(arguments[1]);

        if (amount <= 0 || amount > from->statpoints)
            return;

        if (arguments[0] == "str") stat = &from->str;
        if (arguments[0] == "int") stat = &from->intl;
        if (arguments[0] == "wis") stat = &from->wis;
        if (arguments[0] == "agi") stat = &from->agi;
        if (arguments[0] == "con") stat = &from->con;
        if (arguments[0] == "cha") stat = &from->cha;

        if (stat == 0)
            return;

        (*stat) += amount;

        from->statpoints -= amount;

        from->CalculateStats();
        from->UpdateStats();
    }

    void Reset(const std::vector<std::string>& arguments, Character* from)
    {
        for (std::size_t i = 0; i < from->paperdoll.size(); ++i)
        {
            if (from->paperdoll[i] != 0)
            {
                PacketBuilder builder;
                builder.SetID(PACKET_STATSKILL, PACKET_REPLY);
                builder.AddShort(1);
                from->Send(builder);

                return;
            }
        }

        from->Reset();
    }

    void Devil(const std::vector<std::string>& arguments, Character* from)
    {
        std::list<int> except_list = ExceptUnserialize(from->world->devilgate_config["ExceptID"]);

        if (from->mapid != int(from->world->config["JailMap"]) || from->mapid != int(from->world->config["WallMap"]))
        {
            if (std::find(except_list.begin(), except_list.end(), from->mapid) != except_list.end())
            {
                return;
            }
            else
            {
                int levelReq = int(from->world->devilgate_config["DevilLevelReq"]);
                int RebirthReq = int(from->world->devilgate_config["DevilRebirthReq"]);

                if ((from->level < levelReq && from->rebirth == 0) || (from->rebirth < RebirthReq))
                {
                    if (from->level < levelReq && from->rebirth == 0)
                        from->ServerMsg("This command requires level " + util::to_string(levelReq));
                    else if (from->rebirth < RebirthReq)
                        from->ServerMsg("This command requires rebirth level " + util::to_string(RebirthReq));

                    return;
                }

                int mapid = from->world->devilgate_config["DevilMap"];
                int x = from->world->devilgate_config["DevilX"];
                int y = from->world->devilgate_config["DevilY"];

                if (from->world->devil == true)
                {
                    from->oldmap = from->mapid;
                    from->oldx = from->x;
                    from->oldy = from->y;

                    if (int(from->world->devilgate_config["DevilMap"]) > 0)
                        from->Warp(mapid, x, y, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                }
                else
                {
                    from->ServerMsg("You can't join right now, because the devil gate is closed.");
                }
            }
        }
    }

    void Warp(const std::vector<std::string>& arguments, Character* from)
    {
        std::list<int> except_list = ExceptUnserialize(from->world->warps_config["ExceptID"]);

        std::string loc = arguments[0];

        if (from->mapid == static_cast<int>(from->world->config["JailMap"]) || from->mapid == static_cast<int>(from->world->config["WallMap"]))
            return;

        if (static_cast<int>(from->world->warps_config[loc + ".location"]) > 0)
        {
            if (std::find(except_list.begin(), except_list.end(), from->mapid) != except_list.end())
            {
                return;
            }
            else
            {
                int cost = from->world->warps_config[loc + ".cost"];
                int clas = from->world->warps_config[loc + ".class"];
                int level = from->world->warps_config[loc + ".level"];
                int reborn = from->world->warps_config[loc + ".reborn"];
                int member = from->world->warps_config[loc + ".member"];

                int M = util::to_int(util::explode(',', from->world->warps_config[loc + ".location"])[0]);
                int X = util::to_int(util::explode(',', from->world->warps_config[loc + ".location"])[1]);
                int Y = util::to_int(util::explode(',', from->world->warps_config[loc + ".location"])[2]);

                if (from->level < level && from->rebirth == 0)
                {
                    from->StatusMsg("You need to be at least level " + util::to_string(level) + " to warp to the location.");
                }
                else if (from->rebirth < reborn)
                {
                    from->StatusMsg("You need " + util::to_string(reborn) + " rebirth levels to warp to this location.");
                }
                else if (int(from->world->warps_config[loc + ".class"]) != 0 && from->clas != clas)
                {
                    from->StatusMsg("You need class: " + from->world->ecf->Get(clas).name + " to warp to the location.");
                }
                else if (from->HasItem(1) < cost)
                {
                    from->StatusMsg("You need " + util::to_string(cost) + " " + from->world->eif->Get(1).name + " to warp to the location.");
                }
                else if (from->member < member)
                {
                    from->StatusMsg("Only level " + util::to_string(member) + " members can warp to this location.");
                }
                else
                {
                    if (cost > 0)
                        from->RemoveItem(1, cost);

                    from->Warp(M, X, Y, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                }
            }
        }
    }

    void Bounty(const std::vector<std::string>& arguments, Character* from)
    {
        Character *victim = from->world->GetCharacter(arguments[0]);

        if (!victim || victim->nowhere)
        {
            from->ServerMsg(from->SourceWorld()->i18n.Format("Command-CharacterNotFound"));
        }
        else
        {
            if (victim->SourceAccess() < int(from->world->admin_config["cmdprotect"]) || victim->SourceAccess() <= from->SourceAccess())
            {
                if (victim->SourceName() != from->SourceName())
                {
                    int amount = util::to_int(arguments[1]);

                    if (amount <= 0 || from->HasItem(1) < amount)
                    {
                        from->StatusMsg("The amount of bounty is invalid.");
                    }
                    else
                    {
                        victim->bounty += amount;

                        from->RemoveItem(1, amount);
                        from->world->ServerMsg(util::ucfirst(from->SourceName()) + " has placed " + util::to_string(amount) + " bounty on " + util::ucfirst(victim->SourceName()) + "'s head.");
                    }
                }
                else
                {
                    from->StatusMsg("You cannot place a bounty on yourself.");
                }
            }
            else
            {
                from->ServerMsg(from->SourceWorld()->i18n.Format("Command-AccessDenied"));
            }
        }
    }

    void Reborn(const std::vector<std::string>& arguments, Character* from)
    {
        if (from->level >= int(from->world->commands_config["RebirthLevel"]))
        {
            if (from->rebirth >= int(from->world->config["MaxRebirth"]))
            {
                from->ServerMsg("You reached the highest rebirth level!");
            }
            else
            {
                from->GiveRebirth(1);

                PacketBuilder builder(PACKET_RECOVER, PACKET_REPLY, 7);
                builder.AddInt(from->exp);
                builder.AddShort(from->karma);
                builder.AddChar(from->level);
                from->Send(builder);

                from->world->ServerMsg("Attention!! " + from->SourceName() + " has been reborn! Rebirth Level: " + util::to_string(from->rebirth));
            }
        }
        else
        {
            from->StatusMsg("[Rebirth level " + util::to_string(static_cast<int>(from->world->commands_config["RebirthLevel"])) + " needed]");
        }
    }

    void Insert(const std::vector<std::string>& arguments, Character* from)
    {
        int amount = int(util::variant(arguments[0]));

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

        int Random = util::rand(1,100);

        int WinAnimation = int(from->world->commands_config["WinAnimation"]);
        int LoseAnimation = int(from->world->commands_config["LoseAnimation"]);

        int Chance = int(from->world->commands_config["GambleChance"]);

        if (from->map->GetSpec(target_x, target_y) == Map_Tile::Gambling || (from->x != target_x && from->y != target_y))
        {
            if (from->HasItem(1) >= amount)
            {
                if (amount > 0 && amount <= int(from->world->commands_config["GambleLimit"]))
                {
                    if (Random > 100 - Chance)
                    {
                        from->GiveItem(1,amount);

                        if (WinAnimation > 0)
                            from->Effect(WinAnimation);

                        from->StatusMsg("You have won " + util::to_string(amount) + " " + from->world->eif->Get(1).name);
                    }
                    else
                    {
                        from->RemoveItem(1, amount);

                        if (LoseAnimation > 0)
                            from->Effect(LoseAnimation);

                        from->StatusMsg("You lost " + util::to_string(amount) + " " + from->world->eif->Get(1).name + ", maybe next time..");
                    }
                }
                else
                {
                    from->StatusMsg("You must insert a value between 1 and " + util::to_string(int(from->world->commands_config["GambleLimit"])) + " " + from->world->eif->Get(1).name);
                }
            }
            else
            {
                from->StatusMsg("You do not have " + arguments[0] + " " + from->world->eif->Get(1).name);
            }
        }
    }

    void OpenLocker(const std::vector<std::string>& arguments, Character* from)
    {
        int pin = util::to_int(arguments[0]);

        if (!from->lockerpin == 0000)
        {
            if (from->lockerpin == pin)
            {
                from->ServerMsg("Correct pin entered!");
                from->lockeraccess = true;
            }
            else
            {
                from->ServerMsg("Incorrect pin entered!");
                from->lockeraccess = false;
            }
        }
        else
        {
            from->ServerMsg("You don't have a pin. Typ " + std::string(from->world->config["PlayerPrefix"]) + "setlockerpin <pin>, to set a pin on your locker.");
        }
    }

    void SetLockerPin(const std::vector<std::string>& arguments, Character* from)
    {
        int pin = util::to_int(arguments[0]);

        if (!from->lockerpin == 0000)
            from->ServerMsg("You already have a pin set to your locker.");
        else
        {
            if (pin > 999 || pin < 10000)
            {
                from->ServerMsg("Your locker's pin is now: " + arguments[0]);
                from->lockerpin = pin;
            }
            else
            {
                from->ServerMsg("Your pin must contain 4 digits.");
            }
        }
    }

    void RemoveLockerPin(const std::vector<std::string>& arguments, Character* from)
    {
        int pin = util::to_int(arguments[0]);

        if (from->lockerpin == pin)
        {
            from->lockerpin = 0000;
            from->ServerMsg("Your pin has been successfuly removed.");
        }
        else
        {
            from->ServerMsg("Invalid pin entered.");
        }
    }

    void CTF(const std::vector<std::string>& arguments, Character* from)
    {
        std::list<int> except_list = ExceptUnserialize(from->world->ctf_config["ExceptID"]);

        if (from->mapid != int(from->world->config["JailMap"])
        || from->mapid != int(from->world->config["WallMap"]))
        {
            if (std::find(except_list.begin(), except_list.end(), from->mapid) != except_list.end())
            {
                return;
            }
            else
            {
                int mapid = from->world->ctf_config["CTFMap"];

                int bx = util::to_int(util::explode(',', from->world->ctf_config["CTFBlueSpawnXY"])[0]);
                int by = util::to_int(util::explode(',', from->world->ctf_config["CTFBlueSpawnXY"])[1]);

                int rx = util::to_int(util::explode(',', from->world->ctf_config["CTFRedSpawnXY"])[0]);
                int ry = util::to_int(util::explode(',', from->world->ctf_config["CTFRedSpawnXY"])[1]);

                int fbarmor = from->world->ctf_config["FemaleBlueTeamArmor"];
                int frarmor = from->world->ctf_config["FemaleRedTeamArmor"];

                int mbarmor = from->world->ctf_config["MaleBlueTeamArmor"];
                int mrarmor = from->world->ctf_config["MaleRedTeamArmor"];

                int blueflag = int(from->world->ctf_config["BlueFlag"]);
                int redflag = int(from->world->ctf_config["RedFlag"]);

                const EIF_Data &fblue = from->world->eif->Get(fbarmor);
                const EIF_Data &fred = from->world->eif->Get(frarmor);

                const EIF_Data &mblue = from->world->eif->Get(mbarmor);
                const EIF_Data &mred = from->world->eif->Get(mrarmor);

                if (bx <= 0 || by <= 0 || rx <= 0 || ry <= 0)
                    return;

                if (from->world->ctfenabled == true)
                {
                    if (from->HasLockerItem(blueflag) || from->HasLockerItem(redflag))
                    {
                        from->ServerMsg("You either have a blue or red flag in your locker! Please junk it to get access to the CTF event.");
                        return;
                    }

                    if (from->HasItem(blueflag))
                        from->RemoveItem(blueflag, 1);

                    if (from->HasItem(redflag))
                        from->RemoveItem(redflag, 1);

                    from->oldmap = from->mapid;
                    from->oldx = from->x;
                    from->oldy = from->y;

                    if (from->blueteam == false && from->redteam == false)
                    {
                        if (from->world->bluemembers > from->world->redmembers)
                        {
                            from->redteam = true;
                            from->world->redmembers ++;

                            from->world->ServerMsg(util::ucfirst(from->SourceName()) + " has been added to the red Capture The Flag team.");

                            if (from->gender == 0 && frarmor > 0)
                                from->Dress(Character::Armor, fred.dollgraphic);
                            else if (from->gender == 1 && mrarmor > 0)
                                from->Dress(Character::Armor, mred.dollgraphic);
                        }
                        else
                        {
                            from->blueteam = true;
                            from->world->bluemembers ++;

                            from->world->ServerMsg(util::ucfirst(from->SourceName()) + " has been added to the blue Capture The Flag team.");

                            if (from->gender == 0 && fbarmor > 0)
                                from->Dress(Character::Armor, fblue.dollgraphic);
                            else if (from->gender == 1 && mbarmor > 0)
                                from->Dress(Character::Armor, mblue.dollgraphic);
                        }
                    }
                    else
                    {
                        from->ServerMsg("You already joined a team!");
                        return;
                    }

                    if (int(from->world->ctf_config["CTFMap"]) > 0 && from->blueteam == true)
                        from->Warp(mapid, bx, by, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                    else if (int(from->world->ctf_config["CTFMap"]) > 0 && from->redteam == true)
                        from->Warp(mapid, rx, ry, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                }
                else
                {
                    from->ServerMsg("You cannot join right now, because Capture The Flag has either already started or is currently disabled.");
                }
            }
        }
    }

    void PVP(const std::vector<std::string>& arguments, Character* from)
    {
        std::list<int> except_list = ExceptUnserialize(from->world->ctf_config["ExceptID"]);

        if (from->mapid != int(from->world->config["JailMap"]) || from->mapid != int(from->world->config["WallMap"]))
        {
            if (std::find(except_list.begin(), except_list.end(), from->mapid) != except_list.end())
            {
                return;
            }
            else
            {
                int mapid = from->world->pvp_config["PVPMap"];

                int rx = util::to_int(util::explode(',', from->world->pvp_config["RedTeam.Location"])[0]);
                int ry = util::to_int(util::explode(',', from->world->pvp_config["RedTeam.Location"])[1]);

                int bx = util::to_int(util::explode(',', from->world->pvp_config["BlueTeam.Location"])[0]);
                int by = util::to_int(util::explode(',', from->world->pvp_config["BlueTeam.Location"])[1]);

                int frarmor = from->world->pvp_config["RedTeam.FemaleArmor"];
                int mrarmor = from->world->pvp_config["RedTeam.MaleArmor"];

                int fbarmor = from->world->pvp_config["BlueTeam.FemaleArmor"];
                int mbarmor = from->world->pvp_config["BlueTeam.MaleArmor"];

                const EIF_Data &fblue = from->world->eif->Get(fbarmor);
                const EIF_Data &fred = from->world->eif->Get(frarmor);

                const EIF_Data &mblue = from->world->eif->Get(mbarmor);
                const EIF_Data &mred = from->world->eif->Get(mrarmor);

                if (bx <= 0 || by <= 0 || rx <= 0 || ry <= 0)
                    return;

                if (from->world->pvpenabled == true)
                {
                    from->oldmap = from->mapid;
                    from->oldx = from->x;
                    from->oldy = from->y;

                    if (from->pvp_blueteam == false && from->pvp_redteam == false)
                    {
                        if (from->world->pvp_bluemembers > from->world->pvp_redmembers)
                        {
                            from->pvp_redteam = true;
                            from->world->pvp_redmembers ++;

                            from->world->ServerMsg(util::ucfirst(from->SourceName()) + " has been added to the red PVP team.");

                            if (from->gender == 0 && frarmor > 0)
                                from->Dress(Character::Armor, fred.dollgraphic);
                            else if (from->gender == 1 && mrarmor > 0)
                                from->Dress(Character::Armor, mred.dollgraphic);
                        }
                        else
                        {
                            from->pvp_blueteam = true;
                            from->world->pvp_bluemembers ++;

                            from->world->ServerMsg(util::ucfirst(from->SourceName()) + " has been added to the blue PVP team.");

                            if (from->gender == 0 && fbarmor > 0)
                                from->Dress(Character::Armor, fblue.dollgraphic);
                            else if (from->gender == 1 && mbarmor > 0)
                                from->Dress(Character::Armor, mblue.dollgraphic);
                        }
                    }
                    else
                    {
                        from->ServerMsg("You already joined a team!");
                        return;
                    }

                    if (int(from->world->pvp_config["PVPMap"]) > 0 && from->pvp_blueteam == true)
                        from->Warp(mapid, bx, by, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                    else if (int(from->world->pvp_config["PVPMap"]) > 0 && from->pvp_redteam == true)
                        from->Warp(mapid, rx, ry, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                }
                else
                {
                    from->ServerMsg("You cannot join right now, because PVP has either already started or is currently disabled.");
                }
            }
        }
    }

    PLAYER_COMMAND_HANDLER_REGISTER()
        using namespace std::placeholders;
        RegisterCharacter({"update", {"stat"}, {"amount"}, 3}, Update);
        RegisterCharacter({"reset"}, Reset);
        RegisterCharacter({"devil"}, Devil);
        RegisterCharacter({"warp"}, Warp);
        RegisterCharacter({"bounty"}, Bounty);
        RegisterCharacter({"reborn"}, Reborn);
        RegisterCharacter({"insert"}, Insert);
        RegisterCharacter({"openlocker"}, OpenLocker);
        RegisterCharacter({"setlockerpin"}, SetLockerPin);
        RegisterCharacter({"removelockerpin"}, RemoveLockerPin);
        RegisterCharacter({"ctf"}, CTF);
        RegisterCharacter({"pvp"}, PVP);
    PLAYER_COMMAND_HANDLER_REGISTER_END()
}
