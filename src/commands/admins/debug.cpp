#include "commands.hpp"

#include "../../util.hpp"
#include "../../map.hpp"
#include "../../npc.hpp"
#include "../../eoplus.hpp"
#include "../../packet.hpp"
#include "../../player.hpp"
#include "../../quest.hpp"
#include "../../world.hpp"

namespace Commands
{
    void SpawnItem(const std::vector<std::string>& arguments, Character* from)
    {
        int id = util::to_int(arguments[0]);
        int amount = (arguments.size() >= 2) ? util::to_int(arguments[1]) : 1;

        if (from->AddItem(id, amount))
        {
            PacketBuilder reply(PACKET_ITEM, PACKET_GET, 9);
            reply.AddShort(0);
            reply.AddShort(id);
            reply.AddThree(amount);
            reply.AddChar(from->weight);
            reply.AddChar(from->maxweight);
            from->Send(reply);
        }
    }

    void DropItem(const std::vector<std::string>& arguments, Character* from)
    {
        if (from->trading) return;

        int id = util::to_int(arguments[0]);
        int amount = (arguments.size() >= 2) ? util::to_int(arguments[1]) : 1;
        int x = (arguments.size() >= 3) ? util::to_int(arguments[2]) : from->x;
        int y = (arguments.size() >= 4) ? util::to_int(arguments[3]) : from->y;

        if (from->mapid == int(from->SourceWorld()->ctf_config["CTFMap"]))
            return;

        if (amount > 0 && from->HasItem(id) >= amount)
        {
            std::shared_ptr<Map_Item> item = from->map->AddItem(id, amount, x, y, from);

            if (item)
            {
                item->owner = from->player->id;
                item->unprotecttime = Timer::GetTime() + double(from->world->config["ProtectPlayerDrop"]);
                from->DelItem(id, amount);

                PacketBuilder reply(PACKET_ITEM, PACKET_DROP, 15);
                reply.AddShort(id);
                reply.AddThree(amount);
                reply.AddInt(from->HasItem(id));
                reply.AddShort(item->uid);
                reply.AddChar(x);
                reply.AddChar(y);
                reply.AddChar(from->weight);
                reply.AddChar(from->maxweight);
                from->Send(reply);
            }
        }
    }

    void SpawnNPC(const std::vector<std::string>& arguments, Character* from)
    {
        int id = util::to_int(arguments[0]);
        int amount = (arguments.size() >= 2) ? util::to_int(arguments[1]) : 1;
        unsigned char speed = (arguments.size() >= 3) ? util::to_int(arguments[2]) : int(from->world->config["SpawnNPCSpeed"]);

        short direction = DIRECTION_DOWN;

        if (speed == 7 && arguments.size() >= 4)
        {
            direction = util::to_int(arguments[3]);
        }

        if (id <= 0 || std::size_t(id) > from->world->enf->data.size())
            return;

        for (int i = 0; i < amount; i++)
        {
            unsigned char index = from->map->GenerateNPCIndex();

            if (index > 250)
                break;

            NPC *npc = new NPC(from->map, id, from->x, from->y, speed, direction, index, true);

            if (npc->pet)
            {
                from->AddPet(from, id);
                    return;
            }

            from->map->npcs.push_back(npc);
                npc->Spawn();
        }
    }

    void DespawnNPC(const std::vector<std::string>& arguments, Character* from)
    {
        std::vector<NPC*> despawn_npcs;

        for (NPC* npc : from->map->npcs)
        {
            if (npc->temporary)
                despawn_npcs.push_back(npc);
        }

        for (NPC* npc : despawn_npcs)
        {
            if (!npc->pet)
                npc->Die();
        }
    }

    void Learn(const std::vector<std::string>& arguments, Character* from)
    {
        Character* victim = from->SourceWorld()->GetCharacter(arguments[0]);

        short skill_id = util::to_int(arguments[1]);
        short level = -1;

        if (!victim || victim->nowhere)
        {
            from->ServerMsg(from->SourceWorld()->i18n.Format("Command-CharacterNotFound"));
        }
        else
        {
            if (arguments.size() >= 3)
                level = std::max(0, std::min<int>(from->world->config["MaxSkillLevel"], util::to_int(arguments[2])));

            if (victim->AddSpell(skill_id))
            {
                PacketBuilder builder(PACKET_STATSKILL, PACKET_TAKE, 6);
                builder.AddShort(skill_id);
                builder.AddInt(victim->HasItem(1));
                victim->Send(builder);
            }

            if (level >= 0)
            {
                auto it = std::find_if(UTIL_RANGE(victim->spells), [&](Character_Spell spell) { return spell.id == skill_id; });

                if (it != victim->spells.end())
                {
                    it->level = level;

                    PacketBuilder builder(PACKET_STATSKILL, PACKET_ACCEPT, 6);
                    builder.AddShort(victim->skillpoints);
                    builder.AddShort(skill_id);
                    builder.AddShort(it->level);
                    victim->Send(builder);
                }
            }
        }
    }

    void QuestState(const std::vector<std::string>& arguments, Character* from)
    {
        World* world = from->SourceWorld();

        short quest_id = util::to_int(arguments[0]);

        std::shared_ptr<Quest_Context> quest = from->GetQuest(quest_id);

        if (!quest)
        {
            auto it = world->quests.find(quest_id);

            if (it != world->quests.end())
            {
                quest = std::make_shared<Quest_Context>(from, it->second.get());
                from->quests[it->first] = quest;
                quest->SetState("begin", true);
            }
        }

        if (!quest)
        {
            from->ServerMsg(world->i18n.Format("Quest-Not-Found"));
        }
        else
        {
            try
            {
                quest->SetState(arguments[1]);
            }
            catch (EOPlus::Runtime_Error& e)
            {
                from->ServerMsg(world->i18n.Format("Quest-State-Not-Found"));
            }
        }
    }

    void Copycat(const std::vector<std::string>& arguments, Character* from)
    {
        Character *victim = from->SourceWorld()->GetCharacter(arguments[0]);

        if (!victim || victim->nowhere)
        {
            from->ServerMsg(from->SourceWorld()->i18n.Format("Command-CharacterNotFound"));
        }
        else
        {
            from->oldhairstyle = victim->hairstyle;
            from->hairstyle = victim->hairstyle;
            from->haircolor = victim->haircolor;
            from->race = victim->race;
            from->gender = victim->gender;
            from->title = victim->title;
            from->home = victim->home;
            from->partner = victim->partner;
            from->guild = victim->guild;
            from->guild_rank = victim->guild_rank;

            if (victim->guild)
                from->guild->tag = victim->guild->tag;

            from->goldbank = victim->goldbank;
            from->bank = victim->bank;
            from->paperdoll = victim->paperdoll;

            from->SetClass(victim->clas);

            UTIL_FOREACH(from->map->characters, character)
            {
                if (character->InRange(from))
                    character->Refresh();
            }

            from->Save();
        }
    }

    void Freeze(const std::vector<std::string>& arguments, Command_Source* from)
    {
        Character *victim = from->SourceWorld()->GetCharacter(arguments[0]);

        if (!victim || victim->nowhere)
        {
            from->ServerMsg(from->SourceWorld()->i18n.Format("Command-CharacterNotFound"));
        }
        else
        {
            PacketBuilder builder;
            builder.SetID(PACKET_WALK, PACKET_CLOSE);
            victim->Send(builder);
        }
    }

    void Unfreeze(const std::vector<std::string>& arguments, Command_Source* from)
    {
        Character *victim = from->SourceWorld()->GetCharacter(arguments[0]);

        if (!victim || victim->nowhere)
        {
            from->ServerMsg(from->SourceWorld()->i18n.Format("Command-CharacterNotFound"));
        }
        else
        {
            PacketBuilder builder;
            builder.SetID(PACKET_WALK, PACKET_OPEN);
            victim->Send(builder);
        }
    }

    void Server(const std::vector<std::string>& arguments, Command_Source* from)
    {
        std::string message = "";

        for (int i = 0; i < static_cast<int>(arguments.size()); ++i)
        {
            message += arguments[i];
            message += " ";
        }

        from->SourceWorld()->ServerMsg(message);
    }

    void Immune(const std::vector<std::string>& arguments, Command_Source* from)
    {
        Character *victim = from->SourceWorld()->GetCharacter(arguments[0]);

        if (!victim || victim->nowhere)
        {
            from->ServerMsg(from->SourceWorld()->i18n.Format("Command-CharacterNotFound"));
        }
        else
        {
            if (victim->immune == false)
            {
                victim->immune = true;
            }
            else
            {
                victim->immune = false;
            }
        }
    }

    void GlobalPK(const std::vector<std::string>& arguments, Command_Source* from)
    {
        UTIL_FOREACH(from->SourceWorld()->maps, server)
        {
            if (server->pk == 1)
            {
                server->pk = 0;
            }
            else
            {
                server->pk = 1;
            }
        }
    }

    void GlobalChat(const std::vector<std::string>& arguments, Command_Source* from)
    {
        if (from->SourceWorld()->global == true)
            from->SourceWorld()->global = false;
        else
            from->SourceWorld()->global = true;
    }

    COMMAND_HANDLER_REGISTER()
        RegisterCharacter({"sitem", {"item"}, {"amount"}, 2}, SpawnItem, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"ditem", {"item"}, {"amount", "x", "y"}, 2}, DropItem, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"snpc", {"npc"}, {"amount", "speed", "direction"}, 2}, SpawnNPC, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"dnpc", {}, {}, 2}, DespawnNPC, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"learn", {"skill"}, {"level"}}, Learn, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"qstate", {"quest", "state"}}, QuestState, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"copycat", {"victim"}}, Copycat, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"freeze", {"victim"}}, Freeze, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"unfreeze", {"victim"}}, Unfreeze, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"server", {"message"}}, Server, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"immune", {"victim"}}, Immune, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"pk"}, GlobalPK);
        RegisterCharacter({"global"}, GlobalChat);

        RegisterAlias("si", "sitem");
        RegisterAlias("di", "ditem");
        RegisterAlias("sn", "snpc");
        RegisterAlias("dn", "dnpc");
    COMMAND_HANDLER_REGISTER_END()
}
