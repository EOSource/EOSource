#include "handlers.hpp"

#include "../character.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../util.hpp"

namespace Handlers
{
    void Law_Request(Character *character, PacketReader &reader)
    {
        if (character->npc_type == ENF::Law)
        {
            LawAction action = static_cast<LawAction>(reader.GetChar());

            reader.GetInt();
            reader.GetByte();

            std::string partner = reader.GetEndString();

            switch (action)
            {
                case LAW_ACTION_MARRIAGE:
                {
                    if (!character->partner.empty())
                    {
                        PacketBuilder reply;
                        reply.SetID(PACKET_MARRIAGE, PACKET_REPLY);
                        reply.AddChar(LAW_ALREADY_MARRIED);
                        character->Send(reply);
                    }
                    else
                    {
                        if (character->HasItem(1) >= int(character->world->config["LawMarriageCost"]))
                        {
                            character->DelItem(1, int(character->world->config["LawMarriageCost"]));

                            PacketBuilder reply;
                            reply.SetID(PACKET_MARRIAGE, PACKET_REPLY);
                            reply.AddChar(LAW_FIANCE);
                            character->Send(reply);

                            reply.Reset();
                            reply.SetID(PACKET_ITEM, PACKET_OBTAIN);
                            reply.AddShort(1);
                            reply.AddThree(character->HasItem(1));
                            character->Send(reply);

                            character->fiance = util::ucfirst(util::lowercase(partner));
                        }
                        else
                        {
                            PacketBuilder reply;
                            reply.SetID(PACKET_MARRIAGE, PACKET_REPLY);
                            reply.AddChar(LAW_NO_GOLD);
                            character->Send(reply);
                        }
                    }
                }
                break;

                case LAW_ACTION_DIVORCE:
                {
                    if (character->partner.empty())
                    {
                        PacketBuilder reply;
                        reply.SetID(PACKET_MARRIAGE, PACKET_REPLY);
                        reply.AddChar(LAW_NOT_MARRIED);
                        character->Send(reply);
                    }
                    else if (character->partner != partner)
                    {
                        PacketBuilder reply;
                        reply.SetID(PACKET_MARRIAGE, PACKET_REPLY);
                        reply.AddChar(LAW_INVALID_FIANCE);
                        character->Send(reply);
                    }
                    else if (character->HasItem(1) < int(character->world->config["LawDivorceCost"]))
                    {
                        PacketBuilder reply;
                        reply.SetID(PACKET_MARRIAGE, PACKET_REPLY);
                        reply.AddChar(LAW_NO_GOLD);
                        character->Send(reply);
                    }
                    else
                    {
                        if (character->world->CharacterExists(partner))
                        {
                            character->DelItem(1, int(character->world->config["LawDivorceCost"]));
                            Character *divorced = character->world->GetCharacter(partner);

                            character->Save();

                            if (divorced)
                            {
                                PacketBuilder builder(PACKET_MARRIAGE, PACKET_REPLY);
                                builder.AddChar(LAW_DIVORCED);
                                divorced->Send(builder);

                                divorced->partner = "";
                            }
                            else
                            {
                                character->world->db.Query("UPDATE `characters` SET `partner` = ''  WHERE `name` = '$'", partner.c_str());
                            }

                            PacketBuilder reply;
                            reply.SetID(PACKET_MARRIAGE, PACKET_REPLY);
                            reply.AddChar(LAW_FIANCE);
                            character->Send(reply);

                            reply.Reset();
                            reply.SetID(PACKET_ITEM, PACKET_OBTAIN);
                            reply.AddShort(1);
                            reply.AddThree(character->HasItem(1));
                            character->Send(reply);

                            character->partner = "";
                        }
                    }
                }
                break;
            }
        }
    }

    void Law_Open(Character *character, PacketReader &reader)
    {
        short id = reader.GetShort();

        UTIL_FOREACH(character->map->npcs, npc)
        {
            if (npc->index == id && npc->Data().type == ENF::Law)
            {
                character->npc = npc;
                character->npc_type = ENF::Law;

                PacketBuilder reply;
                reply.SetID(PACKET_MARRIAGE, PACKET_OPEN);
                reply.AddInt(0);
                character->Send(reply);

                break;
            }
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_MARRIAGE)
        Register(PACKET_REQUEST, Law_Request, Playing);
        Register(PACKET_OPEN, Law_Open, Playing);
    PACKET_HANDLER_REGISTER_END()
}
