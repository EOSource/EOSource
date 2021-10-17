#include "handlers.hpp"

#include <string>

#include "../character.hpp"
#include "../eoclient.hpp"
#include "../player.hpp"
#include "../world.hpp"

namespace Handlers
{
    void Players_Accept(Character *character, PacketReader &reader)
    {
        std::string name = reader.GetEndString();
        Character *victim = character->world->GetCharacter(name);

        PacketBuilder reply(PACKET_PLAYERS, PACKET_PING, name.length());

        if (victim && !victim->hidden)
        {
            if (victim->mapid == character->mapid && !victim->nowhere)
            {
                reply.SetID(PACKET_PLAYERS, PACKET_PONG);
            }
            else
            {
                reply.SetID(PACKET_PLAYERS, PACKET_NET3);
            }
        }
        else
        {
            reply.SetID(PACKET_PLAYERS, PACKET_PING);
        }

        reply.AddString(name);
        character->Send(reply);
    }

    void Players_List(EOClient *client, PacketReader &reader)
    {
        if (!client->server()->world->config["SLN"] && !client->server()->world->config["AllowStats"]
        && (!client->player || (client->player && !client->player->character)))
        {
            return;
        }

        int online = client->server()->world->characters.size();

        UTIL_FOREACH(client->server()->world->characters, character)
        {
            if (character->hidden)
            {
                --online;
            }
        }

        PacketBuilder reply(PACKET_F_INIT, PACKET_A_INIT, 4 + client->server()->world->characters.size() * 35);
        reply.AddChar((reader.Action() == PACKET_LIST) ? INIT_FRIEND_LIST_PLAYERS : INIT_PLAYERS);

        if (static_cast<std::string>(client->server()->world->eosbot_config["bot.enabled"]) != "no")
        {
            reply.AddShort(online + 1);
        }
        else
        {
            reply.AddShort(online);
        }

        reply.AddByte(255);

        UTIL_FOREACH(client->server()->world->characters, character)
        {
            if (character->hidden)
            {
                continue;
            }

            reply.AddBreakString(character->SourceName());

            if (reader.Action() != PACKET_LIST)
            {
                reply.AddBreakString(character->title);
                reply.AddChar(0);

                if (character->bot && !client->player)
                {
                    reply.AddChar(ICON_SLN_BOT);
                }
                else if (character->admin >= ADMIN_HIDDEN)
                {
                    if (character->party)
                    {
                        reply.AddChar(ICON_PARTY);
                    }
                    else
                    {
                        reply.AddChar(ICON_NORMAL);
                    }
                }
                else if (character->admin >= ADMIN_HGM)
                {
                    if (character->party)
                    {
                        reply.AddChar(ICON_HGM_PARTY);
                    }
                    else
                    {
                        reply.AddChar(ICON_HGM);
                    }
                }
                else if (character->admin >= ADMIN_GUIDE)
                {
                    if (character->party)
                    {
                        reply.AddChar(ICON_GM_PARTY);
                    }
                    else
                    {
                        reply.AddChar(ICON_GM);
                    }
                }
                else
                {
                    if (character->party)
                    {
                        reply.AddChar(ICON_PARTY);
                    }
                    else
                    {
                        reply.AddChar(ICON_NORMAL);
                    }
                }

                reply.AddChar(character->clas);
                reply.AddString(character->PaddedGuildTag());
                reply.AddByte(255);
            }
        }

        if (static_cast<std::string>(client->server()->world->eosbot_config["bot.enabled"]) != "no")
        {
            if (static_cast<std::string>(client->server()->world->eosbot_config["bot.name"]) == "0")
            {
                reply.AddBreakString("EOBot");
            }
            else
            {
                reply.AddBreakString(static_cast<std::string>(client->server()->world->eosbot_config["bot.name"]));
            }

            if (static_cast<std::string>(client->server()->world->eosbot_config["bot.title"]) == "0")
            {
                reply.AddBreakString("EOSource");
            }
            else
            {
                reply.AddBreakString(static_cast<std::string>(client->server()->world->eosbot_config["bot.title"]));
            }

            reply.AddChar(0);

            if (static_cast<int>(client->server()->world->eosbot_config["bot.access"]) == 0)
            {
                reply.AddChar(ADMIN_BOT);
            }
            else
            {
                reply.AddChar(static_cast<int>(client->server()->world->eosbot_config["bot.access"]));
            }

            if (static_cast<int>(client->server()->world->eosbot_config["bot.class"]) == 0)
            {
                reply.AddChar(1);
            }
            else
            {
                reply.AddChar(static_cast<int>(client->server()->world->eosbot_config["bot.class"]));
            }

            if (static_cast<std::string>(client->server()->world->eosbot_config["bot.guildtag"]) == "0")
            {
                reply.AddString("BOT");
            }
            else
            {
                reply.AddString(static_cast<std::string>(client->server()->world->eosbot_config["bot.guildtag"]));
            }

            reply.AddByte(255);
        }

        client->Send(reply);
    }

    PACKET_HANDLER_REGISTER(PACKET_PLAYERS)
        Register(PACKET_ACCEPT, Players_Accept, Playing);
        Register(PACKET_LIST, Players_List, Any);
        Register(PACKET_REQUEST, Players_List, Any);
    PACKET_HANDLER_REGISTER_END()
}
