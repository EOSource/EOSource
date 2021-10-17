#include "handlers.hpp"

#include "../character.hpp"
#include "../eodata.hpp"
#include "../map.hpp"
#include "../player.hpp"

namespace Handlers
{
    void Paperdoll_Request(Character *character, PacketReader &reader)
    {
        unsigned int id = reader.GetShort();

        Character *target = character->map->GetCharacterPID(id);

        if (!target)
        {
            target = character;
        }

        std::string home_str = target->world->GetHome(target)->name;
        std::string guild_str = target->guild ? target->guild->name : "";
        std::string rank_str = target->guild ? target->guild->GetRank(target->guild_rank) : "";

        PacketBuilder reply(PACKET_PAPERDOLL, PACKET_REPLY,
        12 + target->SourceName().length() + home_str.length() + target->partner.length() + target->title.length()
        + guild_str.length() + rank_str.length() + target->paperdoll.size() * 2);

        reply.AddBreakString(target->SourceName());
        reply.AddBreakString(home_str);
        reply.AddBreakString(target->partner);
        reply.AddBreakString(target->title);
        reply.AddBreakString(guild_str);

        if (std::string(character->world->config["GlobalGuildUpdate"]) == "yes")
        {
            reply.AddBreakString(rank_str);
        }
        else
        {
            reply.AddBreakString(character->guild ? character->guild_rankname : "");
        }

        reply.AddShort(target->player->id);
        reply.AddChar(target->clas);
        reply.AddChar(target->gender);
        reply.AddChar(0);

        UTIL_FOREACH(target->paperdoll, item)
        {
            reply.AddShort(item);
        }

        if (target->admin >= ADMIN_HGM)
        {
            if (target->party)
            {
                reply.AddChar(ICON_HGM_PARTY);
            }
            else
            {
                reply.AddChar(ICON_HGM);
            }
        }
        else if (target->admin >= ADMIN_GUIDE)
        {
            if (target->party)
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
            if (target->party)
            {
                reply.AddChar(ICON_PARTY);
            }
            else
            {
                reply.AddChar(ICON_NORMAL);
            }
        }

        character->Send(reply);
    }

    void Paperdoll_Remove(Character *character, PacketReader &reader)
    {
        int itemid = reader.GetShort();
        int subloc = reader.GetChar();

        if (character->world->eif->Get(itemid).special == EIF::Cursed)
            return;

        if (character->hairstyle == 0 && character->world->eif->Get(itemid).subtype == EIF::HiddenHair)
        {
            if (character->oldhairstyle > 0)
            {
                character->hairstyle = character->oldhairstyle;
                character->oldhairstyle = 0;
                character->Warp(character->mapid, character->x, character->y, WARP_ANIMATION_NONE);
            }
        }

        if (character->Unequip(itemid, subloc))
        {
            PacketBuilder reply(PACKET_PAPERDOLL, PACKET_REMOVE, 43);
            reply.AddShort(character->player->id);
            reply.AddChar(SLOT_CLOTHES);
            reply.AddChar(0);

            character->AddPaperdollData(reply, "BAHWS");

            reply.AddShort(itemid);
            reply.AddChar(subloc);
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
            character->Send(reply);
        }

        PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 14);
        builder.AddShort(character->player->id);
        builder.AddChar(SLOT_CLOTHES);
        builder.AddChar(0);
        character->AddPaperdollData(builder, "BAHWS");

        if (itemid == int(character->world->equipment_config["BagID"]))
        {
            character->CalculateStats();
            character->UpdateStats();
        }

        UTIL_FOREACH(character->map->characters, updatecharacter)
        {
            if (updatecharacter == character || !character->InRange(updatecharacter))
                continue;

            updatecharacter->Send(builder);
        }
    }

    void Paperdoll_Add(Character *character, PacketReader &reader)
    {
        if (character->trading) return;

        int itemid = reader.GetShort();
        int subloc = reader.GetChar();

        for (int i = 0; i < int(character->world->equipment_config["Amount"]); i++)
        {
            int item = int(character->world->equipment_config[util::to_string(i+1) + ".ItemID"]);
            int level = int(character->world->equipment_config[util::to_string(i+1) + ".RebirthLevel"]);

            if (itemid == item && character->rebirth < level)
            {
                character->StatusMsg("This item requires Rebirth LVL " + util::to_string(level));
                return;
            }
        }

        for (int i = 0; i < int(character->world->members_config["Amount"]); i++)
        {
            int item = int(character->world->members_config[util::to_string(i+1) + ".MemberItem"]);
            int level = int(character->world->members_config[util::to_string(i+1) + ".MemberLevel"]);

            if (itemid == item && character->member < level)
            {
                character->StatusMsg("This item requires Membership LVL " + util::to_string(level));
                return;
            }
        }

        if (character->Equip(itemid, subloc))
        {
            PacketBuilder reply(PACKET_PAPERDOLL, PACKET_AGREE, 46);
            reply.AddShort(character->player->id);
            reply.AddChar(SLOT_CLOTHES);
            reply.AddChar(0);

            character->AddPaperdollData(reply, "BAHWS");

            reply.AddShort(itemid);
            reply.AddThree(character->HasItem(itemid));
            reply.AddChar(subloc);
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
            character->Send(reply);
        }

        PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 14);
        builder.AddShort(character->player->id);
        builder.AddChar(SLOT_CLOTHES);
        builder.AddChar(0);

        character->AddPaperdollData(builder, "BAHWS");

        if (itemid == int(character->world->equipment_config["BagID"]))
        {
            character->CalculateStats();
            character->UpdateStats();
        }

        UTIL_FOREACH(character->map->characters, updatecharacter)
        {
            if (updatecharacter == character || !character->InRange(updatecharacter))
                continue;

            updatecharacter->Send(builder);
        }

        for (std::size_t i = 0; i < character->paperdoll.size(); ++i)
        {
            if (character->world->eif->Get(character->paperdoll[i]).subtype == EIF::HiddenHair)
            {
                if (character->hairstyle == 0)
                    return;

                character->oldhairstyle = character->hairstyle;
                character->hairstyle = 0;
                character->Warp(character->mapid, character->x, character->y, WARP_ANIMATION_NONE);
            }
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_PAPERDOLL)
        Register(PACKET_REQUEST, Paperdoll_Request, Playing);
        Register(PACKET_REMOVE, Paperdoll_Remove, Playing);
        Register(PACKET_ADD, Paperdoll_Add, Playing);
    PACKET_HANDLER_REGISTER_END()
}
