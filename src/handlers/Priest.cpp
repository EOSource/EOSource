#include "handlers.hpp"

#include "../character.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../chat.hpp"

namespace Handlers
{
    void Priest_Accept(Character *character, PacketReader &reader)
    {
        (void)character;
        (void)reader;

        if (character->npc_type == ENF::Priest && character->npc->marriage && character->npc->marriage->partner[1] == character)
        {
            character->npc->marriage->request_accepted = true;
            character->npc->marriage->last_execution = Timer::GetTime() + util::to_int(character->world->config["WeddingStartDelay"]);
            character->npc->ShowDialog(character->world->i18n.Format("WeddingWaiting", util::to_string(static_cast<int>(character->world->config["WeddingStartDelay"]))));
            character->npc->marriage->state = 1;

            PacketBuilder reply;
            reply.SetID(PACKET_MUSIC, PACKET_PLAYER);
            reply.AddShort(util::to_int(character->world->config["WeddingMusic"]));

            UTIL_FOREACH(character->map->characters, from)
            {
                from->Send(reply);
            }
        }
    }

    void Priest_Use(Character *character, PacketReader &reader)
    {
        (void)character;
        (void)reader;

        if (character->npc_type == ENF::Priest && character->npc->marriage)
        {
            if (character->npc->marriage->partner[0] == character)
            {
                character->npc->marriage->partner_accepted[0] = true;
                character->npc->marriage->partner[0]->map->Msg(character, character->world->i18n.Format("WeddingAccept"));
            }

            if (character->npc->marriage->partner[1] == character)
            {
                character->npc->marriage->partner_accepted[1] = true;
                character->npc->marriage->partner[1]->map->Msg(character, character->world->i18n.Format("WeddingAccept"));
            }
        }
    }

    void Priest_Request(Character *character, PacketReader &reader)
    {
        if (character->npc_type == ENF::Priest)
        {
            reader.GetInt();
            reader.GetByte();

            Character *partner = character->world->GetCharacter(reader.GetEndString());

            if (!partner || partner == character || partner->map != character->map)
            {
                PacketBuilder reply;
                reply.SetID(PACKET_PRIEST, PACKET_REPLY);
                reply.AddChar(PRIEST_PARTNER_MAP);
                character->Send(reply);
            }
            else if (!partner->partner.empty())
            {
                PacketBuilder reply;
                reply.SetID(PACKET_PRIEST, PACKET_REPLY);
                reply.AddChar(PRIEST_PARTNER_ALREADY_MARRIED);
                character->Send(reply);
            }
            else if (partner->paperdoll[Character::Armor] != util::to_int(character->world->config[(partner->gender == GENDER_FEMALE ? "WeddingArmorFemale" : "WeddingArmorMale")]))
            {
                PacketBuilder reply;
                reply.SetID(PACKET_PRIEST, PACKET_REPLY);
                reply.AddChar(PRIEST_PARTNER_CLOTHES);
                character->Send(reply);
            }
            else if (character->fiance != util::ucfirst(util::lowercase(partner->SourceName())) || partner->fiance != util::ucfirst(util::lowercase(character->SourceName())))
            {
                PacketBuilder reply;
                reply.SetID(PACKET_PRIEST, PACKET_REPLY);
                reply.AddChar(PRIEST_NO_PREMISSION);
                character->Send(reply);
            }
            else
            {
                partner->npc = character->npc;
                partner->npc_type = ENF::Priest;

                partner->npc->marriage = new NPC_Marriage();
                partner->npc->marriage->partner[0] = character;
                partner->npc->marriage->partner[1] = partner;

                PacketBuilder builder(PACKET_PRIEST, PACKET_REQUEST);
                builder.AddShort(1);
                builder.AddString(character->SourceName());
                partner->Send(builder);
            }
        }
    }

    void Priest_Open(Character *character, PacketReader &reader)
    {
        short id = reader.GetShort();

        UTIL_FOREACH(character->map->npcs, npc)
        {
            if (npc->index == id && npc->Data().type == ENF::Priest)
            {
                character->npc = npc;
                character->npc_type = ENF::Priest;

                if (character->fiance.empty())
                {
                    npc->ShowDialog(character->world->i18n.Format("WeddingNoPartner"));
                    break;
                }

                if (character->paperdoll[Character::Armor] != util::to_int(character->world->config[(character->gender == GENDER_FEMALE ? "WeddingArmorFemale" : "WeddingArmorMale")])
                || util::to_int(character->world->config[(character->gender == GENDER_FEMALE ? "WeddingArmorFemale" : "WeddingArmorMale")]) == 0)
                {
                    PacketBuilder reply;
                    reply.SetID(PACKET_PRIEST, PACKET_REPLY);
                    reply.AddChar(PRIEST_CLOTHES);
                    character->Send(reply);
                }
                else if (character->level < util::to_int(character->world->config["WeddingLevelNeeded"]))
                {
                    PacketBuilder reply;
                    reply.SetID(PACKET_PRIEST, PACKET_REPLY);
                    reply.AddChar(PRIEST_UNEXPERIENCED);
                    character->Send(reply);
                }
                else
                {
                    PacketBuilder reply;
                    reply.SetID(PACKET_PRIEST, PACKET_OPEN);
                    reply.AddInt(0);
                    character->Send(reply);
                }

                break;
            }
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_PRIEST)
        Register(PACKET_ACCEPT, Priest_Accept, Playing);
        Register(PACKET_USE, Priest_Use, Playing);
        Register(PACKET_REQUEST, Priest_Request, Playing);
        Register(PACKET_OPEN, Priest_Open, Playing);
    PACKET_HANDLER_REGISTER_END()
}
