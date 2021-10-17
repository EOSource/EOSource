#include "handlers.hpp"

#include <stdexcept>
#include <vector>

#include "../character.hpp"
#include "../console.hpp"
#include "../eoclient.hpp"
#include "../eodata.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../player.hpp"

namespace Handlers
{
    void Warp_Accept(Character *character, PacketReader &reader)
    {
        reader.GetShort();

        int anim = character->warp_anim;

        if (anim != WARP_ANIMATION_INVALID)
        {
            character->warp_anim = WARP_ANIMATION_INVALID;
        }
        else
        {
            return;
        }

        std::vector<Character *> updatecharacters;
        std::vector<NPC *> updatenpcs;
        std::vector<std::shared_ptr<Map_Item>> updateitems;

        UTIL_FOREACH(character->map->characters, checkcharacter)
        {
            if (checkcharacter->InRange(character))
            {
                if (!checkcharacter->hidden)
                    updatecharacters.push_back(checkcharacter);
                else if (checkcharacter->hidden && character->admin >= static_cast<int>(character->world->admin_config["seehide"]))
                    updatecharacters.push_back(checkcharacter);
            }
        }

        UTIL_FOREACH(character->map->npcs, npc)
        {
            if (character->InRange(npc) && npc->alive)
            {
                updatenpcs.push_back(npc);
            }
        }

        UTIL_FOREACH(character->map->items, item)
        {
            if (character->InRange(*item))
            {
                updateitems.push_back(item);
            }
        }

        PacketBuilder reply(PACKET_WARP, PACKET_AGREE, 7 + updatecharacters.size() * 60 + updatenpcs.size() * 6 + updateitems.size() * 9);
        reply.AddChar(2);
        reply.AddShort(character->mapid);
        reply.AddChar(anim);
        reply.AddChar(updatecharacters.size());
        reply.AddByte(255);

        UTIL_FOREACH(updatecharacters, character)
        {
            reply.AddBreakString(character->SourceName());
            reply.AddShort(character->player->id);
            reply.AddShort(character->mapid);
            reply.AddShort(character->x);
            reply.AddShort(character->y);
            reply.AddChar(character->direction);
            reply.AddChar(6); // ?
            reply.AddString(character->PaddedGuildTag());
            reply.AddChar(character->level);
            reply.AddChar(character->gender);
            reply.AddChar(character->hairstyle);
            reply.AddChar(character->haircolor);
            reply.AddChar(character->race);
            reply.AddShort(character->maxhp);
            reply.AddShort(character->hp);
            reply.AddShort(character->maxtp);
            reply.AddShort(character->tp);

            character->AddPaperdollData(reply, "B000A0HSW");

            reply.AddChar(character->sitting);
            reply.AddChar(character->hidden);
            reply.AddByte(255);
        }

        if (character->mapid == int(character->world->eosbot_config["bot.map"]) && std::string(character->world->eosbot_config["bot.enabled"]) != "no")
        {
            if (static_cast<std::string>(character->world->eosbot_config["bot.name"]) == "0")
            {
                reply.AddBreakString("EOSbot");
            }
            else
            {
                reply.AddBreakString(static_cast<std::string>(character->world->eosbot_config["bot.name"]));
            }

            reply.AddShort(0);
            reply.AddShort(static_cast<int>(character->world->eosbot_config["bot.map"]));
            reply.AddShort(static_cast<int>(character->world->eosbot_config["bot.locx"]));
            reply.AddShort(static_cast<int>(character->world->eosbot_config["bot.locy"]));
            reply.AddChar(static_cast<int>(character->world->eosbot_config["bot.direction"]));
            reply.AddChar(6);
            reply.AddString(static_cast<std::string>(character->world->eosbot_config["bot.guildtag"]));
            reply.AddChar(static_cast<int>(character->world->eosbot_config["bot.level"]));
            reply.AddChar(static_cast<int>(character->world->eosbot_config["bot.gender"]));
            reply.AddChar(static_cast<int>(character->world->eosbot_config["bot.hairstyle"]));
            reply.AddChar(static_cast<int>(character->world->eosbot_config["bot.haircolor"]));
            reply.AddChar(static_cast<int>(character->world->eosbot_config["bot.race"]));
            reply.AddShort(0);
            reply.AddShort(0);
            reply.AddShort(0);
            reply.AddShort(0);
            reply.AddShort(static_cast<int>(character->world->eosbot_config["bot.boots"]));
            reply.AddShort(0);
            reply.AddShort(0);
            reply.AddShort(0);
            reply.AddShort(static_cast<int>(character->world->eosbot_config["bot.armor"]));
            reply.AddShort(0);
            reply.AddShort(static_cast<int>(character->world->eosbot_config["bot.hat"]));
            reply.AddShort(static_cast<int>(character->world->eosbot_config["bot.shield"]));
            reply.AddShort(static_cast<int>(character->world->eosbot_config["bot.weapon"]));
            reply.AddChar(static_cast<int>(character->world->eosbot_config["bot.sitting"]));
            reply.AddChar(static_cast<int>(character->world->eosbot_config["bot.hidden"]));
            reply.AddByte(255);
        }

        UTIL_FOREACH(updatenpcs, npc)
        {
            reply.AddChar(npc->index);
            reply.AddShort(npc->Data().id);
            reply.AddChar(npc->x);
            reply.AddChar(npc->y);
            reply.AddChar(npc->direction);
        }

        reply.AddByte(255);

        UTIL_FOREACH(updateitems, item)
        {
            reply.AddShort(item->uid);
            reply.AddShort(item->id);
            reply.AddChar(item->x);
            reply.AddChar(item->y);
            reply.AddThree(item->amount);
        }

        character->Send(reply);
    }

    void Warp_Take(Character *character, PacketReader &reader)
    {
        (void)reader;

        if (!character->player->client->Upload(FILE_MAP, character->mapid, INIT_BANNED))
        {
            throw std::runtime_error("Failed to upload file");
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_WARP)
        Register(PACKET_ACCEPT, Warp_Accept, Playing);
        Register(PACKET_TAKE, Warp_Take, Playing, 0.15);
    PACKET_HANDLER_REGISTER_END()
}
