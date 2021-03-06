#include "handlers.hpp"

#include "../character.hpp"
#include "../eodata.hpp"
#include "../map.hpp"
#include "../player.hpp"

std::string get_timestr(std::string format = "%c");

namespace Handlers
{
    void Trade_Request(Character *character, PacketReader &reader)
    {
        if (character->trading) return;
        if (!character->CanInteractItems()) return;

        int something = reader.GetChar();
        int victimid = reader.GetShort();

        Character *victim = character->map->GetCharacterPID(victimid);

        if (!victim)
        {
            return;
        }

        if (victim == character)
        {
            return;
        }

        if (std::string(character->world->config["AdminTradeProtection"]) == "yes")
        {
            if (victim->admin > ADMIN_PLAYER && victim->admin < ADMIN_HGM)
            {
                character->StatusMsg("Your victim cannot trade any items - [Admin Protection]");
                return;
            }

            if (character->admin > ADMIN_PLAYER && character->admin < ADMIN_HGM)
            {
                character->StatusMsg("You cannot trade any items - [Admin Protection]");
                return;
            }
        }

        if (victim && !victim->trading && victim->CanInteractItems())
        {
            PacketBuilder builder(PACKET_TRADE, PACKET_REQUEST, 3 + character->SourceName().length());
            builder.AddChar(something);
            builder.AddShort(character->player->id);
            builder.AddString(character->SourceName());
            victim->Send(builder);

            character->trade_partner = victim;
        }
    }

    void Trade_Accept(Character *character, PacketReader &reader)
    {
        if (character->trading) return;
        if (!character->CanInteractItems()) return;

        reader.GetChar();
        int victimid = reader.GetShort();

        Character *victim(character->map->GetCharacterPID(victimid));

        if (victim == character)
        {
            return;
        }

        if (victim && victim->mapid == character->mapid && victim->trade_partner == character && !victim->trading && victim->CanInteractItems())
        {
            PacketBuilder builder(PACKET_TRADE, PACKET_OPEN, 30);
            builder.AddShort(victim->player->id);
            builder.AddBreakString(victim->SourceName());
            builder.AddShort(character->player->id);
            builder.AddBreakString(character->SourceName());
            character->Send(builder);

            builder.Reset(30);
            builder.AddShort(character->player->id);
            builder.AddBreakString(character->SourceName());
            builder.AddShort(victim->player->id);
            builder.AddBreakString(victim->SourceName());
            victim->Send(builder);

            character->trade_partner = victim;
            character->trading = true;
            character->trade_agree = false;
            victim->trading = true;
            victim->trade_agree = false;
        }
    }

    void Trade_Remove(Character *character, PacketReader &reader)
    {
        if (!character->trading) return;

        int itemid = reader.GetShort();

        if (character->trade_inventory.size() > 5)
        {
            return;
        }

        if (character->DelTradeItem(itemid))
        {
            character->trade_agree = false;
            character->trade_partner->trade_agree = false;

            PacketBuilder builder(PACKET_TRADE, PACKET_REPLY,
                6 + character->trade_inventory.size() * 6 + character->trade_partner->trade_inventory.size() * 6);

            builder.AddShort(character->player->id);
            UTIL_FOREACH(character->trade_inventory, item)
            {
                builder.AddShort(item.id);
                builder.AddInt(item.amount);
            }
            builder.AddByte(255);

            builder.AddShort(character->trade_partner->player->id);
            UTIL_FOREACH(character->trade_partner->trade_inventory, item)
            {
                builder.AddShort(item.id);
                builder.AddInt(item.amount);
            }
            builder.AddByte(255);

            character->Send(builder);
            character->trade_partner->Send(builder);
        }

    }

    void Trade_Agree(Character *character, PacketReader &reader)
    {
        if (!character->trading) return;

        int agree = reader.GetChar();

        character->trade_agree = agree;

        if (agree)
        {
            if (character->trade_inventory.empty())
            {
                return;
            }

            if (character->trade_partner->trade_agree)
            {
                PacketBuilder builder(PACKET_TRADE, PACKET_USE,
                    6 + character->trade_partner->trade_inventory.size() * 6 + character->trade_inventory.size() * 6);

                builder.AddShort(character->trade_partner->player->id);
                UTIL_FOREACH(character->trade_partner->trade_inventory, item)
                {
                    builder.AddShort(item.id);
                    builder.AddInt(item.amount);
                    character->trade_partner->DelItem(item.id, item.amount);
                    character->AddItem(item.id, item.amount);
                }
                builder.AddByte(255);
                builder.AddShort(character->player->id);
                UTIL_FOREACH(character->trade_inventory, item)
                {
                    builder.AddShort(item.id);
                    builder.AddInt(item.amount);
                    character->DelItem(item.id, item.amount);
                    character->trade_partner->AddItem(item.id, item.amount);
                }
                builder.AddByte(255);
                character->Send(builder);
                character->trade_partner->Send(builder);

                character->Emote(EMOTE_TRADE);
                character->trade_partner->Emote(EMOTE_TRADE);

                if (static_cast<std::string>(character->world->chatlogs_config["LogTrade"]) == "yes")
                {
                    FILE *fh = fopen("./data/logs/trade.txt", "a");
                    fprintf(fh, "%s [%s trading with %s]\n\n", get_timestr().c_str(), util::ucfirst(character->SourceName()).c_str(), util::ucfirst(character->trade_partner->SourceName()).c_str());
                    fprintf(fh, "%s received: ", util::ucfirst(character->trade_partner->SourceName()).c_str());

                    UTIL_FOREACH(character->trade_inventory, item)
                    {
                        fprintf(fh, "%i %s (ID: %i), ", item.amount, character->world->eif->Get(item.id).name.c_str(), item.id);
                    }

                    fprintf(fh, "\n%s received: ", util::ucfirst(character->SourceName()).c_str());

                    UTIL_FOREACH(character->trade_partner->trade_inventory, item)
                    {
                        fprintf(fh, "%i %s (ID: %i), ", item.amount, character->world->eif->Get(item.id).name.c_str(), item.id);
                    }

                    fprintf(fh, "\n\n");
                    fclose(fh);
                }

                character->trading = false;
                character->trade_inventory.clear();
                character->trade_agree = false;

                character->CheckQuestRules();
                character->trade_partner->CheckQuestRules();

                character->trade_partner->trading = false;
                character->trade_partner->trade_inventory.clear();
                character->trade_agree = false;

                character->trade_partner->trade_partner = 0;
                character->trade_partner = 0;
            }
            else
            {
                PacketBuilder reply(PACKET_TRADE, PACKET_SPEC, 1);
                reply.AddChar(agree);
                character->Send(reply);

                PacketBuilder builder(PACKET_TRADE, PACKET_AGREE, 3);
                builder.AddShort(character->trade_partner->player->id);
                builder.AddChar(agree);
                character->trade_partner->Send(builder);
            }
        }
        else
        {
            PacketBuilder reply(PACKET_TRADE, PACKET_SPEC, 1);
            reply.AddChar(agree);
            character->Send(reply);

            PacketBuilder builder(PACKET_TRADE, PACKET_AGREE, 3);
            builder.AddShort(character->trade_partner->player->id);
            builder.AddChar(agree);
            character->trade_partner->Send(builder);
        }
    }

    void Trade_Add(Character *character, PacketReader &reader)
    {
        if (!character->trading) return;

        int itemid = reader.GetShort();
        int amount = reader.GetInt();

        if (character->world->eif->Get(itemid).special == EIF::Lore)
        {
            return;
        }

        bool offered = false;
        UTIL_FOREACH(character->trade_inventory, item)
        {
            if (item.id == itemid)
            {
                offered = true;
                break;
            }
        }

        if (!offered && character->trade_inventory.size() >= 10)
        {
            return;
        }

        if (character->AddTradeItem(itemid, amount))
        {
            character->trade_agree = false;
            character->trade_partner->trade_agree = false;

            PacketBuilder builder(PACKET_TRADE, PACKET_REPLY,
                6 + character->trade_inventory.size() * 6 + character->trade_partner->trade_inventory.size() * 6);

            builder.AddShort(character->player->id);
            UTIL_FOREACH(character->trade_inventory, item)
            {
                builder.AddShort(item.id);
                builder.AddInt(item.amount);
            }
            builder.AddByte(255);

            builder.AddShort(character->trade_partner->player->id);
            UTIL_FOREACH(character->trade_partner->trade_inventory, item)
            {
                builder.AddShort(item.id);
                builder.AddInt(item.amount);
            }
            builder.AddByte(255);

            character->Send(builder);
            character->trade_partner->Send(builder);
        }
    }

    void Trade_Close(Character *character, PacketReader &reader)
    {
        if (!character->trading) return;

        reader.GetChar();

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

    PACKET_HANDLER_REGISTER(PACKET_TRADE)
        Register(PACKET_REQUEST, Trade_Request, Playing, 0.5);
        Register(PACKET_ACCEPT, Trade_Accept, Playing);
        Register(PACKET_REMOVE, Trade_Remove, Playing);
        Register(PACKET_AGREE, Trade_Agree, Playing);
        Register(PACKET_ADD, Trade_Add, Playing);
        Register(PACKET_CLOSE, Trade_Close, Playing);
    PACKET_HANDLER_REGISTER_END()
}
