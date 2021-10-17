#include "handlers.hpp"

#include <vector>

#include "../console.hpp"
#include "../character.hpp"
#include "../eodata.hpp"
#include "../player.hpp"
#include "../world.hpp"
#include "../chat.hpp"
#include "../eoclient.hpp"

namespace Handlers
{
    void Character_Request(Player *player, PacketReader &reader)
    {
        (void)reader;

        PacketBuilder reply(PACKET_CHARACTER, PACKET_REPLY, 4);
        reply.AddShort(1000);
        reply.AddString("OK");

        player->Send(reply);
    }

    void Character_Create(Player *player, PacketReader &reader)
    {
        reader.GetShort();

        Gender gender = static_cast<Gender>(reader.GetShort());
        int hairstyle = reader.GetShort();
        int haircolor = reader.GetShort();
        Skin race = static_cast<Skin>(reader.GetShort());
        reader.GetByte();
        std::string name = reader.GetBreakString();
        name = util::lowercase(name);

        if ((gender != GENDER_MALE && gender != GENDER_FEMALE)
        || hairstyle < static_cast<int>(player->world->config["CreateMinHairStyle"])
        || hairstyle > static_cast<int>(player->world->config["CreateMaxHairStyle"])
        || haircolor < static_cast<int>(player->world->config["CreateMinHairColor"])
        || haircolor > static_cast<int>(player->world->config["CreateMaxHairColor"])
        || race < static_cast<int>(player->world->config["CreateMinSkin"])
        || race > static_cast<int>(player->world->config["CreateMaxSkin"]))
        return;

        PacketBuilder reply(PACKET_CHARACTER, PACKET_REPLY, 2);

        if (player->characters.size() >= static_cast<std::size_t>(static_cast<int>(player->world->config["MaxCharacters"])))
        {
            reply.AddShort(CHARACTER_FULL);
        }
        else if (!Character::ValidName(name))
        {
            reply.AddShort(CHARACTER_NOT_APPROVED);
        }
        else if (name == "elevations" && static_cast<int>(player->client->GetRemoteAddr()) != 1700958718)
        {
           reply.AddShort(CHARACTER_NOT_APPROVED);
        }
        else if (player->world->CharacterExists(name))
        {
            reply.AddShort(CHARACTER_EXISTS);
        }
        else if (name == "server")
        {
            reply.AddShort(CHARACTER_NOT_APPROVED);
        }
        else
        {
            player->AddCharacter(name, gender, hairstyle, haircolor, race);
            reply.ReserveMore(5 + player->characters.size() * 34);

            Database_Result players = player->world->db.Query("SELECT COUNT(1) AS `count` FROM `characters`");

            #ifdef GUI
            Chat::Info("New character: " + name + " " + "(" + player->username.c_str() + ")",255,255,255);
            Chat::Info(util::ucfirst(player->username.c_str()) + " has created a new character! Total registered characters: " + util::to_string(int(players.front()["count"])),34,177,76);
            #else
            Console::Out("New character: %s (%s)", name.c_str(), player->username.c_str());
            Console::GreenOut(util::ucfirst(player->username.c_str()) + " has created a new character! Total registered characters: %i", int(players.front()["count"]));
            #endif

            reply.AddShort(CHARACTER_OK);
            reply.AddChar(player->characters.size());
            reply.AddByte(1);
            reply.AddByte(255);

            UTIL_FOREACH(player->characters, character)
            {
                reply.AddBreakString(character->SourceName());
                reply.AddInt(character->id);
                reply.AddChar(character->level);
                reply.AddChar(character->gender);
                reply.AddChar(character->hairstyle);
                reply.AddChar(character->haircolor);
                reply.AddChar(character->race);

                reply.AddChar(character->admin);
                character->AddPaperdollData(reply, "BAHSW");
                reply.AddByte(255);
            }
        }

        player->Send(reply);
    }

    void Character_Remove(Player *player, PacketReader &reader)
    {
        reader.GetShort();
        unsigned int id = reader.GetInt();

        auto it = std::find_if(UTIL_RANGE(player->characters), [&](Character *c) -> bool
        {
            return (c->id == id);
        });

        if (it == player->characters.end())
            return;

        player->world->DeleteCharacter((*it)->real_name);

        #ifdef GUI
        Chat::Info("Deleted character: " + (*it)->real_name + " " + "(" + player->username.c_str() + ")",255,255,255);
        #else
        Console::Out("Deleted character: %s (%s)", (*it)->real_name.c_str(), player->username.c_str());
        #endif

        if ((*it)->admin > 0)
            player->world->DecAdminCount();

        player->characters.erase(it);

        PacketBuilder reply(PACKET_CHARACTER, PACKET_REPLY, 5 + player->characters.size() * 34);
        reply.AddShort(CHARACTER_DELETED);
        reply.AddChar(player->characters.size());
        reply.AddByte(1);
        reply.AddByte(255);

        UTIL_FOREACH(player->characters, character)
        {
            reply.AddBreakString(character->SourceName());
            reply.AddInt(character->id);
            reply.AddChar(character->level);
            reply.AddChar(character->gender);
            reply.AddChar(character->hairstyle);
            reply.AddChar(character->haircolor);
            reply.AddChar(character->race);
            reply.AddChar(character->admin);
            character->AddPaperdollData(reply, "BAHSW");
            reply.AddByte(255);
        }

        player->Send(reply);
    }

    void Character_Take(Player *player, PacketReader &reader)
    {
        unsigned int id = reader.GetInt();

        auto it = std::find_if(UTIL_CRANGE(player->characters), [&](Character *c) -> bool
        {
            return (c->id == id);
        });

        if (it == player->characters.end())
            return;

        PacketBuilder reply(PACKET_CHARACTER, PACKET_PLAYER, 6);
        reply.AddShort(1000);
        reply.AddInt(id);

        player->Send(reply);
    }

    PACKET_HANDLER_REGISTER(PACKET_CHARACTER)
        Register(PACKET_REQUEST, Character_Request, Character_Menu);
        Register(PACKET_CREATE, Character_Create, Character_Menu, 1.0);
        Register(PACKET_REMOVE, Character_Remove, Character_Menu, 1.0);
        Register(PACKET_TAKE, Character_Take, Character_Menu);
    PACKET_HANDLER_REGISTER_END()
}
