#include "handlers.hpp"

#include "../character.hpp"
#include "../map.hpp"
#include "../npc.hpp"

namespace Handlers
{
    void Citizen_Request(Character *character, PacketReader &reader)
    {
        int hpgain = reader.GetInt();
        int tpgain = reader.GetInt();

        hpgain = character->maxhp - character->hp;
        tpgain = character->maxtp - character->tp;

        if (character->npc_type == ENF::Inn && character->npc->citizenship)
        {
            std::string loc = character->npc->citizenship->home;

            if (hpgain > 0 || tpgain > 0)
            {
                PacketBuilder builder;
                builder.SetID(PACKET_CITIZEN, PACKET_REQUEST);
                builder.AddShort(int(character->world->home_config[(loc) + ".innsleepcost"]));
                character->Send(builder);
            }
        }
    }

    void Citizen_Accept(Character *character, PacketReader &reader)
    {
        (void)reader;

        if (character->npc_type == ENF::Inn && character->npc->citizenship)
        {
            std::string loc = character->npc->citizenship->home;

            int cost = (character->world->home_config[(loc) + ".innsleepcost"]);

            if (character->HasItem(1) < cost)
            {
                character->StatusMsg(character->world->i18n.Format("SleepDenied", character->world->eif->Get(1).name));
            }
            else
            {
                if (character->world->home_config[loc + ".innsleep"])
                {
                    int M = util::to_int(util::explode(',', character->world->home_config[loc + ".innsleep"])[0]);
                    int X = util::to_int(util::explode(',', character->world->home_config[loc + ".innsleep"])[1]);
                    int Y = util::to_int(util::explode(',', character->world->home_config[loc + ".innsleep"])[2]);

                    if (M > 0 && X > 0 && Y > 0)
                        character->Warp(M, X, Y, WARP_ANIMATION_NONE);
                }

                character->DelItem(1, cost);

                character->hp = character->maxhp;
                character->tp = character->maxtp;

                character->PacketRecover();

                PacketBuilder reply;
                reply.SetID(PACKET_CITIZEN, PACKET_ACCEPT);
                reply.AddInt(character->HasItem(1));
                character->Send(reply);
            }
        }
    }

    void Citizen_Reply(Character *character, PacketReader &reader)
    {
        std::string answers[3];

        reader.GetShort();
        reader.GetByte();
        reader.GetShort();
        reader.GetByte();

        answers[0] = reader.GetBreakString();
        answers[1] = reader.GetBreakString();
        answers[2] = reader.GetEndString();

        if ((character->home.size() == 0 || character->world->config["CitizenSubscribeAnytime"]) && character->npc_type == ENF::Inn)
        {
            int questions_wrong = 0;

            for (int i = 0; i < 3; ++i)
            {
                if (util::lowercase(answers[i]) != util::lowercase(character->npc->citizenship->answers[i]))
                {
                    ++questions_wrong;
                }
            }

            if (questions_wrong == 0)
            {
                character->home = character->npc->citizenship->home;
            }

            PacketBuilder reply(PACKET_CITIZEN, PACKET_REPLY, 1);
            reply.AddChar(questions_wrong);

            character->Send(reply);
        }
    }

    void Citizen_Remove(Character *character, PacketReader &reader)
    {
        (void)reader;

        if (character->npc_type == ENF::Inn)
        {
            PacketBuilder reply(PACKET_CITIZEN, PACKET_REMOVE, 1);

            if (character->home == character->npc->citizenship->home || character->world->config["CitizenUnsubscribeAnywhere"])
            {
                character->home = "";
                reply.AddChar(UNSUBSCRIBE_UNSUBSCRIBED);
            }
            else
            {
                reply.AddChar(UNSUBSCRIBE_NOT_CITIZEN);
            }

            character->Send(reply);
        }
    }

    void Citizen_Open(Character *character, PacketReader &reader)
    {
        short id = reader.GetShort();

        UTIL_FOREACH(character->map->npcs, npc)
        {
            if (npc->index == id && npc->Data().type == ENF::Inn && npc->citizenship)
            {
                character->npc = npc;
                character->npc_type = ENF::Inn;

                PacketBuilder reply(PACKET_CITIZEN, PACKET_OPEN, 9 + npc->citizenship->questions[0].length() + npc->citizenship->questions[1].length() + npc->citizenship->questions[2].length());

                Home* home = character->world->GetHome(character);

                int innkeeper_vend = 0;

                if (home)
                {
                    innkeeper_vend = home->innkeeper_vend;
                }

                reply.AddThree(npc->Data().vendor_id);

                if (character->world->config["CitizenSubscribeAnytime"] && innkeeper_vend != npc->Data().vendor_id)
                {
                    reply.AddChar(0);
                }
                else
                {
                    reply.AddChar(innkeeper_vend);
                }

                reply.AddShort(0);
                reply.AddByte(255);
                reply.AddBreakString(npc->citizenship->questions[0]);
                reply.AddBreakString(npc->citizenship->questions[1]);
                reply.AddString(npc->citizenship->questions[2]);

                character->Send(reply);

                break;
            }
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_CITIZEN)
        Register(PACKET_REQUEST, Citizen_Request, Playing);
        Register(PACKET_ACCEPT, Citizen_Accept, Playing);
        Register(PACKET_REPLY, Citizen_Reply, Playing);
        Register(PACKET_REMOVE, Citizen_Remove, Playing);
        Register(PACKET_OPEN, Citizen_Open, Playing);
    PACKET_HANDLER_REGISTER_END()
}
