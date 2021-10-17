#include "handlers.hpp"

#include <csignal>
#include <ctime>

#include "../character.hpp"
#include "../eoclient.hpp"
#include "../eodata.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../player.hpp"
#include "../world.hpp"
#include "../commands.hpp"
#include "../text.hpp"

#include "../commands/admins/commands.hpp"
#include "../commands/players/commands.hpp"

extern volatile std::sig_atomic_t exiting_eoserv;

static void limit_message(std::string &message, std::size_t chatlength)
{
	if (message.length() > chatlength)
	{
		message = message.substr(0, chatlength - 6) + " [...]";
	}
}

namespace Handlers
{
    void Talk_Report(Character *character, PacketReader &reader)
    {
        if (character->muted_until > time(0))
            return;

        std::string message = reader.GetEndString();

        limit_message(message, static_cast<int>(character->world->config["ChatLength"]));

        if (message.empty())
            return;

        for (int i = 0; i < int(character->world->cursefilter_config["Amount"]); i++)
        {
            std::string curseword = std::string(character->world->cursefilter_config[util::to_string(i+1) + ".Message"]);
            std::string replacement = std::string(character->world->cursefilter_config[util::to_string(i+1) + ".Replacement"]);

            size_t cursefilter = message.find(curseword);

            if (cursefilter != string::npos)
                message.replace(message.find(curseword), curseword.length(), replacement);
        }

        if (character->world->chatlogs_config["LogPublic"])
        {
            UTIL_FOREACH(character->world->characters, checkchar)
            {
                if (checkchar->admin >= int(character->world->chatlogs_config["SeePublic"]))
                    checkchar->StatusMsg(character->SourceName() + ": " + message + " [Public Message]");
            }

            FILE *fh = fopen("./data/chatlogs/public.txt", "a");
            fprintf(fh, "[Public Message] %s: %s\n", util::ucfirst(character->SourceName()).c_str(), message.c_str());
            fclose(fh);

            #ifdef GUI
            Text::UpdateChat(util::ucfirst(character->SourceName()) + ": " + util::ucfirst(message.c_str()), 0,0,0,0);
            #endif

            character->world->AdminMsg(character, message + " [Public Message]", int(character->world->chatlogs_config["SeePublic"]), false);
        }

        if (message.find_first_of(std::string(character->world->config["PlayerPrefix"])) == 0 || message.find_first_of(std::string(character->world->config["AdminPrefix"])) == 0)
        {
            std::string command;
            std::vector<std::string> arguments = util::explode(' ', message);
            command = arguments.front().substr(1);
            arguments.erase(arguments.begin());

            UTIL_FOREACH(character->world->commands, commands)
            {
                if ((commands->type.compare(0,6, "player") == 0 && message.find_first_of(std::string(character->world->config["PlayerPrefix"])) == 0) || (commands->type.compare(0,5, "admin") == 0 && message.find_first_of(std::string(character->world->config["AdminPrefix"])) == 0 && character->SourceAccess() > ADMIN_PLAYER))
                {
                    if (command == "") return;

                    if (commands->name.size() -1 == command.size())
                    {
                        if (commands->name.compare(0, command.size(), command) == 0)
                        {
                            UTIL_FOREACH(commands->checks, check)
                            {
                                if (check.name.compare("item") == 0)
                                {
                                    short item = int(check.args[0]);
                                    short amount = int(check.args[1]);

                                    if (character->HasItem(item) >= amount)
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[2]) == "true" || std::string(check.args[2]) == "")
                                            character->StatusMsg("You do not have " + util::to_string(amount) + " " + character->world->eif->Get(item).name);

                                        return;
                                    }
                                }
                                else if (check.name.compare("stripped") == 0)
                                {
                                    for (std::size_t i = 0; i < character->paperdoll.size(); ++i)
                                    {
                                        if (character->paperdoll[i] == 0)
                                        {
                                            continue;
                                        }
                                        else
                                        {
                                            PacketBuilder builder;
                                            builder.SetID(PACKET_STATSKILL, PACKET_REPLY);
                                            builder.AddShort(1);
                                            character->Send(builder);

                                            if (std::string(check.args[0]) == "true" || std::string(check.args[0]) == "")
                                                character->StatusMsg("You haven't unequipped all items in your paperdoll.");

                                            return;
                                        }
                                    }
                                }
                                else if (check.name.compare("location") == 0)
                                {
                                    if (character->mapid == int(check.args[0]) && character->x == int(check.args[1]) && character->y == int(check.args[2]))
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        return;
                                    }
                                }
                                else if (check.name.compare("gender") == 0)
                                {
                                    short gender = int(check.args[0]);

                                    if (character->gender == gender)
                                    {
                                       continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                        {
                                            if (gender == 0)
                                                character->StatusMsg("You aren't using a female character.");
                                            else if (gender == 1)
                                                character->StatusMsg("You aren't using a male character.");
                                        }

                                        return;
                                    }
                                }
                                else if (check.name.compare("map") == 0)
                                {
                                    if (character->mapid == int(check.args[0]))
                                    {
                                       continue;
                                    }
                                    else
                                    {
                                        return;
                                    }
                                }
                                else if (check.name.compare("member") == 0)
                                {
                                    short member = int(check.args[0]);

                                    if (character->member >= member)
                                    {
                                       continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You aren't a level " + util::to_string(member) + " member.");

                                        return;
                                    }
                                }
                                else if (check.name.compare("cookinglevel") == 0)
                                {
                                    short level = int(check.args[0]);

                                    if (character->clevel >= level)
                                    {
                                       continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You aren't cooking level " + util::to_string(level));

                                        return;
                                    }
                                }
                                else if (check.name.compare("fishinglevel") == 0)
                                {
                                    short level = int(check.args[0]);

                                    if (character->flevel >= level)
                                    {
                                       continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You aren't fishing level " + util::to_string(level));

                                        return;
                                    }
                                }
                                else if (check.name.compare("mininglevel") == 0)
                                {
                                    short level = int(check.args[0]);

                                    if (character->mlevel >= level)
                                    {
                                       continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You aren't mining level " + util::to_string(level));

                                        return;
                                    }
                                }
                                else if (check.name.compare("woodcuttinglevel") == 0)
                                {
                                    short level = int(check.args[0]);

                                    if (character->wlevel >= level)
                                    {
                                       continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You aren't woodcutting level " + util::to_string(level));

                                        return;
                                    }
                                }
                                else if (check.name.compare("level") == 0)
                                {
                                    short level = int(check.args[0]);

                                    if (character->level >= level || character->rebirth > 0)
                                    {
                                       continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You aren't level " + util::to_string(level));

                                        return;
                                    }
                                }
                                else if (check.name.compare("rebirth") == 0)
                                {
                                    short rebirth = int(check.args[0]);

                                    if (character->rebirth >= rebirth)
                                    {
                                       continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You aren't rebirth level " + util::to_string(rebirth));

                                        return;
                                    }
                                }
                                else if (check.name.compare("admin") == 0)
                                {
                                    if (character->admin >= static_cast<AdminLevel>(int(check.args[0])))
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        return;
                                    }
                                }
                                else if (check.name.compare("class") == 0)
                                {
                                    short clas = int(check.args[0]);

                                    if (character->clas == clas)
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You aren't " + character->world->ecf->Get(clas).name);

                                        return;
                                    }
                                }
                                else if (check.name.compare("home") == 0)
                                {
                                    std::string home = check.args[0];

                                    if (character->home == home)
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You don't live in the right town. You have to live at: " + home);

                                        return;
                                    }
                                }
                                else if (check.name.compare("partner") == 0)
                                {
                                    std::string partner = std::string(check.args[0]);

                                    if (character->partner != "")
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You are not married.");

                                        return;
                                    }
                                }
                                else if (check.name.compare("title") == 0)
                                {
                                    std::string title = std::string(check.args[0]);

                                    if (character->title == title)
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You do not have the title: " + title);

                                        return;
                                    }
                                }
                                else if (check.name.compare("spell") == 0)
                                {
                                    short spell = int(check.args[0]);

                                    if (character->HasSpell(spell))
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You do not have the spell " + character->world->esf->Get(spell).name);

                                        return;
                                    }
                                }
                                else if (check.name.compare("race") == 0)
                                {
                                    short race = int(check.args[0]);

                                    if (character->race == static_cast<Skin>(race))
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        if (std::string(check.args[1]) == "true" || std::string(check.args[1]) == "")
                                            character->StatusMsg("You have to be race " + util::to_string(race));

                                        return;
                                    }
                                }
                            }

                            UTIL_FOREACH(commands->calls, call)
                            {
                                if (call.name.compare("statusmsg") == 0)
                                {
                                    std::string message = call.args[0];
                                    message = character->ReplaceStrings(character, message);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                message = std::string(util::variant(arguments[0]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            victim->StatusMsg(message);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            message = std::string(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->StatusMsg(message);
                                    }
                                }
                                else if (call.name.compare("servermsg") == 0)
                                {
                                    std::string message = call.args[0];
                                    message = character->ReplaceStrings(character, message);

                                    if (std::string(call.args[2]) == "world")
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            message = std::string(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->world->ServerMsg(message);
                                    }
                                    else
                                    {
                                        if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                        {
                                            Character *victim = character->world->GetCharacter(arguments[0]);

                                            if (victim && !victim->nowhere)
                                            {
                                                if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                    message = std::string(util::variant(arguments[0]));

                                                if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                    return;

                                                victim->ServerMsg(message);
                                            }
                                        }
                                        else
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                                message = std::string(util::variant(arguments[0]));

                                            if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                                return;

                                            character->ServerMsg(message);
                                        }
                                    }
                                }
                                else if (call.name.compare("givestats") == 0)
                                {
                                    std::string stat = std::string(call.args[0]);
                                    int amount = int(call.args[1]);

                                    if (call.args.size() == 3 && std::string(call.args[2]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                stat = std::string(util::variant(arguments[1]));

                                            if (std::string(call.args[1]) == "arguments[2]" && arguments.size() >= 3)
                                                amount = int(util::variant(arguments[2]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            if (std::string(call.args[1]) == "arguments[2]" && arguments.size() <= 2)
                                                return;

                                            if (stat == "str")
                                                victim->str += amount;
                                            else if (stat == "int")
                                                victim->intl += amount;
                                            else if (stat == "wis")
                                                victim->wis += amount;
                                            else if (stat == "agi")
                                                victim->agi += amount;
                                            else if (stat == "con")
                                                victim->con += amount;
                                            else if (stat == "cha")
                                                victim->cha += amount;

                                            victim->CalculateStats();
                                            victim->UpdateStats();
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            stat = std::string(util::variant(arguments[0]));

                                        if (std::string(call.args[1]) == "arguments[2]" && arguments.size() >= 2)
                                            amount = int(util::variant(arguments[1]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        if (std::string(call.args[1]) == "arguments[2]" && arguments.size() <= 1)
                                            return;

                                        if (stat == "str")
                                            character->str += amount;
                                        else if (stat == "int")
                                            character->intl += amount;
                                        else if (stat == "wis")
                                            character->wis += amount;
                                        else if (stat == "agi")
                                            character->agi += amount;
                                        else if (stat == "con")
                                            character->con += amount;
                                        else if (stat == "cha")
                                            character->cha += amount;

                                        character->CalculateStats();
                                        character->UpdateStats();
                                    }
                                }
                                else if (call.name.compare("giveitem") == 0)
                                {
                                    short item = int(call.args[0]);
                                    int amount = int(call.args[1]);

                                    if (call.args.size() == 3 && std::string(call.args[2]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                item = int(util::variant(arguments[1]));

                                            if (std::string(call.args[1]) == "arguments[2]" && arguments.size() >= 3)
                                                amount = int(util::variant(arguments[2]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            if (std::string(call.args[1]) == "arguments[2]" && arguments.size() <= 2)
                                                return;

                                            victim->GiveItem(item,amount);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            item = int(util::variant(arguments[0]));

                                        if (std::string(call.args[1]) == "arguments[2]" && arguments.size() >= 2)
                                            amount = int(util::variant(arguments[1]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        if (std::string(call.args[1]) == "arguments[2]" && arguments.size() <= 1)
                                            return;

                                        character->GiveItem(item,amount);
                                    }
                                }
                                else if (call.name.compare("removeitem") == 0)
                                {
                                    short item = int(call.args[0]);
                                    int amount = int(call.args[1]);

                                    if (call.args.size() == 3 && std::string(call.args[2]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                item = int(util::variant(arguments[1]));

                                            if (std::string(call.args[1]) == "arguments[2]" && arguments.size() >= 3)
                                                amount = int(util::variant(arguments[2]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            if (std::string(call.args[1]) == "arguments[2]" && arguments.size() <= 2)
                                                return;

                                            victim->RemoveItem(item, amount);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            item = int(util::variant(arguments[0]));

                                        if (std::string(call.args[1]) == "arguments[2]" && arguments.size() >= 2)
                                            amount = int(util::variant(arguments[1]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        if (std::string(call.args[1]) == "arguments[2]" && arguments.size() <= 1)
                                            return;

                                        character->RemoveItem(item, amount);
                                    }
                                }
                                else if (call.name.compare("giveexp") == 0)
                                {
                                    int exp = int(call.args[0]);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                exp = int(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            victim->GiveEXP(std::min(std::max(exp, 0), int(victim->world->config["MaxExp"])));
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            exp = int(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->GiveEXP(std::min(std::max(exp, 0), int(character->world->config["MaxExp"])));
                                    }
                                }
                                else if (call.name.compare("giverebirth") == 0)
                                {
                                    short level = int(call.args[0]);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                level = int(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            victim->GiveRebirth(level);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            level = int(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->GiveRebirth(level);
                                    }
                                }
                                else if (call.name.compare("givespell") == 0)
                                {
                                    short spell = int(call.args[0]);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                spell = int(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            victim->AddSpell(spell);

                                            PacketBuilder reply;
                                            reply.SetID(PACKET_STATSKILL, PACKET_TAKE);
                                            reply.AddShort(spell);
                                            reply.AddInt(victim->HasItem(1));
                                            victim->Send(reply);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            spell = int(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->AddSpell(spell);

                                        PacketBuilder reply;
                                        reply.SetID(PACKET_STATSKILL, PACKET_TAKE);
                                        reply.AddShort(spell);
                                        reply.AddInt(character->HasItem(1));
                                        character->Send(reply);
                                    }
                                }
                                else if (call.name.compare("removespell") == 0)
                                {
                                    short spell = int(call.args[0]);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                spell = int(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            if (victim->HasSpell(spell))
                                            {
                                                victim->DelSpell(spell);

                                                PacketBuilder reply(PACKET_STATSKILL, PACKET_REMOVE, 2);
                                                reply.AddShort(spell);
                                                victim->Send(reply);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            spell = int(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        if (character->HasSpell(spell))
                                        {
                                            character->DelSpell(spell);

                                            PacketBuilder reply(PACKET_STATSKILL, PACKET_REMOVE, 2);
                                            reply.AddShort(spell);
                                            character->Send(reply);
                                        }
                                    }
                                }
                                else if (call.name.compare("playsound") == 0)
                                {
                                    short sound = int(call.args[0]);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                sound = int(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            victim->PlaySFX(sound);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            sound = int(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->PlaySFX(sound);
                                    }
                                }
                                else if (call.name.compare("playeffect") == 0)
                                {
                                    short effect = int(call.args[0]);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                effect = int(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            victim->Effect(effect);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            effect = int(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->Effect(effect);
                                    }
                                }
                                else if (call.name.compare("warp") == 0)
                                {
                                    short M = int(call.args[0]);

                                    unsigned char X = int(call.args[1]);
                                    unsigned char Y = int(call.args[2]);

                                    if (call.args.size() == 4 && std::string(call.args[3]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                M = int(util::variant(arguments[1]));

                                            if (std::string(call.args[1]) == "arguments[2]" && arguments.size() >= 3)
                                                X = int(util::variant(arguments[2]));

                                            if (std::string(call.args[2]) == "arguments[3]" && arguments.size() >= 4)
                                                Y = int(util::variant(arguments[3]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            if (std::string(call.args[1]) == "arguments[2]" && arguments.size() <= 2)
                                                return;

                                            if (std::string(call.args[2]) == "arguments[3]" && arguments.size() <= 3)
                                                return;

                                            if (victim->mapid == int(character->world->config["JailMap"]) || victim->mapid == int(character->world->config["WallMap"]))
                                                return;

                                            if (M > 0 && X > 0 && Y > 0)
                                                victim->Warp(M, X, Y, character->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            M = int(util::variant(arguments[0]));

                                        if (std::string(call.args[1]) == "arguments[2]" && arguments.size() >= 2)
                                            X = int(util::variant(arguments[1]));

                                        if (std::string(call.args[2]) == "arguments[3]" && arguments.size() >= 3)
                                            Y = int(util::variant(arguments[2]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        if (character->mapid == int(character->world->config["JailMap"]) || character->mapid == int(character->world->config["WallMap"]))
                                            return;

                                        if (M > 0 && X > 0 && Y > 0)
                                            character->Warp(M, X, Y, character->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                                    }
                                }
                                else if (call.name.compare("setclass") == 0)
                                {
                                    short clas = int(call.args[0]);

                                    if (clas <= 0 || static_cast<std::size_t>(clas) >= character->world->ecf->data.size())
                                        return;

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                clas = int(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            victim->SetClass(clas);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            clas = int(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->SetClass(clas);
                                    }
                                }
                                else if (call.name.compare("setgender") == 0)
                                {
                                    short gender = int(call.args[0]);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                gender = int(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            if (gender < 0 || gender > 1)
                                                return;

                                            victim->gender = static_cast<Gender>(gender);
                                            victim->Warp(victim->mapid, victim->x, victim->y, WARP_ANIMATION_NONE);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            gender = int(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        if (gender < 0 || gender > 1)
                                            return;

                                        character->gender = static_cast<Gender>(gender);
                                        character->Warp(character->mapid, character->x, character->y, WARP_ANIMATION_NONE);
                                    }
                                }
                                else if (call.name.compare("sethairstyle") == 0)
                                {
                                    short hairstyle = int(call.args[0]);

                                    if (hairstyle < 0 || hairstyle > int(character->world->config["MaxHairStyle"]))
                                        return;

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                hairstyle = int(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            victim->hairstyle = hairstyle;
                                            victim->Warp(victim->mapid, victim->x, victim->y, WARP_ANIMATION_NONE);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            hairstyle = int(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->hairstyle = hairstyle;
                                        character->Warp(character->mapid, character->x, character->y, WARP_ANIMATION_NONE);
                                    }
                                }
                                else if (call.name.compare("sethaircolor") == 0)
                                {
                                    short haircolor = int(call.args[0]);

                                    if (haircolor <= 0 || haircolor > int(character->world->config["MaxHairColor"]))
                                        return;

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                haircolor = int(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            victim->haircolor = haircolor;
                                            victim->Warp(victim->mapid, victim->x, victim->y, WARP_ANIMATION_NONE);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            haircolor = int(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->haircolor = haircolor;
                                        character->Warp(character->mapid, character->x, character->y, WARP_ANIMATION_NONE);
                                    }
                                }
                                else if (call.name.compare("setadmin") == 0)
                                {
                                    short admin = int(call.args[0]);

                                    if (admin < 0 || admin > 5)
                                        return;

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                admin = int(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            if (victim->admin < character->admin)
                                                victim->admin = static_cast<AdminLevel>(admin);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            admin = int(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->admin = static_cast<AdminLevel>(admin);
                                    }
                                }
                                else if (call.name.compare("setpartner") == 0)
                                {
                                    std::string partner = std::string(call.args[0]);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                partner = static_cast<std::string>(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            victim->partner = partner;
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            partner = static_cast<std::string>(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->partner = partner;
                                    }
                                }
                                else if (call.name.compare("sethome") == 0)
                                {
                                    std::string home = std::string(call.args[0]);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                home = static_cast<std::string>(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            victim->home = home;
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            home = static_cast<std::string>(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        character->home = home;
                                    }
                                }
                                else if (call.name.compare("settitle") == 0)
                                {
                                    std::string title = std::string(call.args[0]);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                title = static_cast<std::string>(util::variant(arguments[1]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            for (unsigned short i = 1; i < arguments.size(); i++)
                                            {
                                                if (title == "")
                                                    title = arguments[i];
                                                else
                                                    title += " " + arguments[i];
                                            }

                                            victim->title = title;
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            title = static_cast<std::string>(util::variant(arguments[0]));

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;

                                        for (unsigned short i = 1; i < arguments.size(); i++)
                                        {
                                            if (title == "")
                                                title = arguments[i];
                                            else
                                                title += " " + arguments[i];
                                        }

                                        character->title = title;
                                    }
                                }
                                else if (call.name.compare("setrace") == 0)
                                {
                                    short race = int(call.args[0]);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2 && int(util::variant(arguments[1])) <= int(character->world->config["MaxSkin"]))
                                            {
                                                race = int(util::variant(arguments[0]));

                                                victim->race = static_cast<Skin>(race);
                                                victim->Warp(victim->mapid, victim->x, victim->y, WARP_ANIMATION_NONE);
                                            }

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1 && int(util::variant(arguments[0])) <= int(character->world->config["MaxSkin"]))
                                        {
                                            race = int(util::variant(arguments[0]));

                                            character->race = static_cast<Skin>(race);
                                            character->Warp(character->mapid, character->x, character->y, WARP_ANIMATION_NONE);
                                        }

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;
                                    }
                                }
                                else if (call.name.compare("setkarma") == 0)
                                {
                                    short karma = int(call.args[0]);

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                            {
                                                karma = int(util::variant(arguments[0]));

                                                victim->karma = std::min(std::max(int(karma), 0), 2000);

                                                PacketBuilder builder(PACKET_RECOVER, PACKET_REPLY, 7);
                                                builder.AddInt(victim->exp);
                                                builder.AddShort(victim->karma);
                                                builder.AddChar(0);
                                                victim->Send(builder);
                                            }

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                        {
                                            karma = int(util::variant(arguments[0]));

                                            character->karma = std::min(std::max(int(karma), 0), 2000);

                                            PacketBuilder builder(PACKET_RECOVER, PACKET_REPLY, 7);
                                            builder.AddInt(character->exp);
                                            builder.AddShort(character->karma);
                                            builder.AddChar(0);
                                            character->Send(builder);
                                        }

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;
                                    }
                                }
                                else if (call.name.compare("setname") == 0)
                                {
                                    std::string name = call.args[0];

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2 && (!victim->world->CharacterExists(name) && Character::ValidName(name)))
                                            {
                                                name = std::string(util::variant(arguments[0]));

                                                if (victim->world->CharacterExists(name) && !Character::ValidName(name))
                                                {
                                                    character->ServerMsg("This name is invalid or already exists.");
                                                    return;
                                                }

                                                victim->world->db.Query("UPDATE `characters` SET `name` = '$' WHERE `name` = '$'", name.c_str(), victim->SourceName().c_str());
                                                victim->SourceName() = name;

                                                UTIL_FOREACH(victim->map->characters, character)
                                                {
                                                    if (character->InRange(victim))
                                                        character->Refresh();
                                                }
                                            }

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1 && (!character->world->CharacterExists(name) && Character::ValidName(name)))
                                        {
                                            name = std::string(util::variant(arguments[0]));

                                            if (character->world->CharacterExists(name) && !Character::ValidName(name))
                                            {
                                                character->ServerMsg("This name is invalid or already exists.");
                                                return;
                                            }

                                            character->world->db.Query("UPDATE `characters` SET `name` = '$' WHERE `name` = '$'", name.c_str(), character->SourceName().c_str());
                                            character->SourceName() = name;

                                            UTIL_FOREACH(character->map->characters, from)
                                            {
                                                if (from->InRange(character))
                                                    from->Refresh();
                                            }
                                        }

                                        if (std::string(call.args[0]) == "arguments[1]" && !arguments.size())
                                            return;
                                    }
                                }
                                else if (call.name.compare("openlocker") == 0)
                                {
                                    if (call.args.size() == 1 && std::string(call.args[0]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[1]) == "pet")
                                            {
                                                if (victim->HasPet)
                                                    victim->pet->OpenInventory();
                                            }
                                            else
                                            {
                                                PacketBuilder reply(PACKET_LOCKER, PACKET_OPEN, 2 + victim->bank.size() * 5);
                                                reply.AddChar(victim->x);
                                                reply.AddChar(victim->y);

                                                UTIL_FOREACH(victim->bank, item)
                                                {
                                                    reply.AddShort(item.id);
                                                    reply.AddThree(item.amount);
                                                }

                                                victim->Send(reply);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[1]) == "pet")
                                        {
                                            if (character->HasPet)
                                                character->pet->OpenInventory();
                                        }
                                        else
                                        {
                                            PacketBuilder reply(PACKET_LOCKER, PACKET_OPEN, 2 + character->bank.size() * 5);
                                            reply.AddChar(character->x);
                                            reply.AddChar(character->y);

                                            UTIL_FOREACH(character->bank, item)
                                            {
                                                reply.AddShort(item.id);
                                                reply.AddThree(item.amount);
                                            }

                                            character->Send(reply);
                                        }
                                    }
                                }
                                else if (call.name.compare("reset") == 0)
                                {
                                    if (call.args.size() == 1 && std::string(call.args[0]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                            victim->Reset();
                                    }
                                    else
                                    {
                                        character->Reset();
                                    }
                                }
                                else if (call.name.compare("kick") == 0)
                                {
                                    if (call.args.size() == 1 && std::string(call.args[0]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                            victim->player->client->Close();
                                    }
                                    else
                                    {
                                        character->player->client->Close();
                                    }
                                }
                                else if (call.name.compare("questdialog") == 0)
                                {
                                    std::string title = call.args[0];
                                    std::string message = call.args[1];

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                title = static_cast<std::string>(util::variant(arguments[1]));

                                            if (std::string(call.args[1]) == "arguments[2]" && arguments.size() >= 3)
                                                message = static_cast<std::string>(util::variant(arguments[2]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            if (std::string(call.args[1]) == "arguments[2]" && arguments.size() <= 2)
                                                return;

                                            victim->QuestMsg(title, message);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            title = static_cast<std::string>(util::variant(arguments[0]));

                                        if (std::string(call.args[1]) == "arguments[2]" && arguments.size() >= 2)
                                            message = static_cast<std::string>(util::variant(arguments[1]));

                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 0)
                                            return;

                                        if (std::string(call.args[1]) == "arguments[2]" && arguments.size() <= 1)
                                            return;

                                        character->QuestMsg(title, message);
                                    }
                                }
                                else if (call.name.compare("infodialog") == 0)
                                {
                                    std::string title = call.args[0];
                                    std::string message = call.args[1];

                                    if (call.args.size() == 2 && std::string(call.args[1]) == "victim")
                                    {
                                        Character *victim = character->world->GetCharacter(arguments[0]);

                                        if (victim && !victim->nowhere)
                                        {
                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 2)
                                                title = static_cast<std::string>(util::variant(arguments[1]));

                                            if (std::string(call.args[1]) == "arguments[2]" && arguments.size() >= 3)
                                                message = static_cast<std::string>(util::variant(arguments[2]));

                                            if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 1)
                                                return;

                                            if (std::string(call.args[1]) == "arguments[2]" && arguments.size() <= 2)
                                                return;

                                            victim->DialogMsg(title, message);
                                        }
                                    }
                                    else
                                    {
                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() >= 1)
                                            title = static_cast<std::string>(util::variant(arguments[0]));

                                        if (std::string(call.args[1]) == "arguments[2]" && arguments.size() >= 2)
                                            message = static_cast<std::string>(util::variant(arguments[1]));

                                        if (std::string(call.args[0]) == "arguments[1]" && arguments.size() <= 0)
                                            return;

                                        if (std::string(call.args[1]) == "arguments[2]" && arguments.size() <= 1)
                                            return;

                                        character->DialogMsg(title, message);
                                    }
                                }
                                else if (call.name.compare("quake") == 0)
                                {
                                    if (call.args.size() == 1 || std::string(call.args[1]) == "map")
                                    {
                                        character->map->Effect(MAP_EFFECT_QUAKE, int(call.args[0]));
                                    }
                                    else if (std::string(call.args[1]) == "world")
                                    {
                                        UTIL_FOREACH(character->map->world->maps, map)
                                        {
                                            map->Effect(MAP_EFFECT_QUAKE, int(call.args[0]));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (message.find_first_of(std::string(character->world->config["PlayerPrefix"])) == 0)
            {
                std::vector<std::string> discoms = util::explode(',', std::string(character->world->config["DisabledCommands"]));

                if (std::find(discoms.begin(), discoms.end(), command) == discoms.end())
                    character->world->PlayerCommands(command, arguments, character);
            }
            else if (message.find_first_of(std::string(character->world->config["AdminPrefix"])) == 0)
                character->world->AdminCommands(command, arguments, character);
        }
        else
        {
            character->map->Msg(character, message, false);
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_TALK)
        Register(PACKET_REPORT, Talk_Report, Playing);
    PACKET_HANDLER_REGISTER_END()
}
