#include "commands.hpp"

#include "../../util.hpp"
#include "../../packet.hpp"
#include "../../world.hpp"
#include "../../eoplus.hpp"
#include "../../player.hpp"
#include "../../quest.hpp"

namespace Commands
{
    void Paperdoll(const std::vector<std::string>& arguments, Character* from)
    {
    	Character *victim = from->world->GetCharacter(arguments[0]);

    	if (!victim || victim->nowhere)
    	{
    		from->ServerMsg(from->world->i18n.Format("Command-CharacterNotFound"));
    	}
    	else
    	{
    		std::string home_str = victim->world->GetHome(victim)->name;
    		std::string guild_str = victim->guild ? victim->guild->name : "";
    		std::string rank_str = victim->guild ? victim->guild->GetRank(victim->guild_rank) : "";

    		PacketBuilder reply(PACKET_PAPERDOLL, PACKET_REPLY,
            12 + victim->SourceName().length() + home_str.length() + victim->partner.length() + victim->title.length()
            + guild_str.length() + rank_str.length() + victim->paperdoll.size() * 2);

    		reply.AddBreakString(victim->SourceName());
    		reply.AddBreakString(home_str);
    		reply.AddBreakString(victim->partner);
    		reply.AddBreakString(victim->title);
    		reply.AddBreakString(guild_str);
    		reply.AddBreakString(rank_str);
    		reply.AddShort(victim->player->id);
    		reply.AddChar(victim->clas);
    		reply.AddChar(victim->gender);
    		reply.AddChar(0);

    		UTIL_FOREACH(victim->paperdoll, item)
    		{
    			reply.AddShort(item);
    		}

    		if (victim->SourceDutyAccess() >= ADMIN_HGM)
    		{
    			if (victim->party)
    			{
    				reply.AddChar(ICON_HGM_PARTY);
    			}
    			else
    			{
    				reply.AddChar(ICON_HGM);
    			}
    		}
    		else if (victim->SourceDutyAccess() >= ADMIN_GUIDE)
    		{
    			if (victim->party)
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
    			if (victim->party)
    			{
    				reply.AddChar(ICON_PARTY);
    			}
    			else
    			{
    				reply.AddChar(ICON_NORMAL);
    			}
    		}

    		from->Send(reply);
    	}
    }

    void Book(const std::vector<std::string>& arguments, Character* from)
    {
    	Character *victim = from->world->GetCharacter(arguments[0]);

    	if (!victim || victim->nowhere)
    	{
    		from->ServerMsg(from->world->i18n.Format("Command-CharacterNotFound"));
    	}
    	else
    	{
    		std::string home_str = victim->world->GetHome(victim)->name;
    		std::string guild_str = victim->guild ? victim->guild->name : "";
    		std::string rank_str = victim->guild ? victim->guild->GetRank(victim->guild_rank) : "";

    		PacketBuilder reply(PACKET_BOOK, PACKET_REPLY,
            13 + victim->SourceName().length() + home_str.length() + victim->partner.length() + victim->title.length()
            + guild_str.length() + rank_str.length());

    		reply.AddBreakString(victim->SourceName());
    		reply.AddBreakString(home_str);
    		reply.AddBreakString(victim->partner);
    		reply.AddBreakString(victim->title);
    		reply.AddBreakString(guild_str);
    		reply.AddBreakString(rank_str);
    		reply.AddShort(victim->player->id);
    		reply.AddChar(victim->clas);
    		reply.AddChar(victim->gender);
    		reply.AddChar(0);

    		if (victim->SourceDutyAccess() >= ADMIN_HGM)
    		{
    			if (victim->party)
    			{
    				reply.AddChar(ICON_HGM_PARTY);
    			}
    			else
    			{
    				reply.AddChar(ICON_HGM);
    			}
    		}
    		else if (victim->SourceDutyAccess() >= ADMIN_GUIDE)
    		{
    			if (victim->party)
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
    			if (victim->party)
    			{
    				reply.AddChar(ICON_PARTY);
    			}
    			else
    			{
    				reply.AddChar(ICON_NORMAL);
    			}
    		}

    		reply.AddByte(255);

    		std::size_t reserve = 0;

    		UTIL_FOREACH(victim->quests, quest)
    		{
    			if (quest.second && quest.second->Finished() && !quest.second->IsHidden())
    				reserve += quest.second->GetQuest()->Name().length() + 1;
    		}

    		reply.ReserveMore(reserve);

    		UTIL_FOREACH(victim->quests, quest)
    		{
    			if (quest.second && quest.second->Finished() && !quest.second->IsHidden())
    				reply.AddBreakString(quest.second->GetQuest()->Name());
    		}

    		UTIL_FOREACH(victim->achievements, achievement)
            {
                reply.AddBreakString(achievement.name);
            }

    		from->Send(reply);
    	}
    }

    void Inventory(const std::vector<std::string>& arguments, Character* from)
    {
        Character *victim = from->world->GetCharacter(arguments[0]);

        if (!victim || victim->nowhere)
        {
            from->ServerMsg(from->SourceWorld()->i18n.Format("Command-CharacterNotFound"));
        }
        else
        {
            if (victim->SourceAccess() < int(from->world->admin_config["cmdprotect"]) || victim->SourceAccess() <= from->SourceAccess())
            {
                std::string name = util::ucfirst(victim->SourceName());

                PacketBuilder reply(PACKET_ADMININTERACT, PACKET_LIST, 32 + name.length() + victim->inventory.size() * 6 + victim->bank.size() * 7);

                switch (victim->admin)
                {
                    case ADMIN_HGM: reply.AddString(from->world->i18n.Format("HighGameMaster", name)); break;
                    case ADMIN_GM: reply.AddString(from->world->i18n.Format("GameMaster", name)); break;
                    case ADMIN_GUARDIAN: reply.AddString(from->world->i18n.Format("Guardian", name)); break;
                    case ADMIN_GUIDE: reply.AddString(from->world->i18n.Format("LightGuide", name)); break;

                    default: reply.AddString(name); break;
                }

                reply.AddString(" ");
                reply.AddBreakString(util::trim(victim->PaddedGuildTag()));
                reply.AddInt(victim->Usage());
                reply.AddByte(255);
                reply.AddInt(victim->goldbank);
                reply.AddByte(255);

                UTIL_FOREACH(victim->inventory, item)
                {
                    reply.AddShort(item.id);
                    reply.AddInt(item.amount);
                }

                reply.AddByte(255);

                UTIL_FOREACH(victim->bank, item)
                {
                    reply.AddShort(item.id);
                    reply.AddThree(item.amount);
                }

                from->Send(reply);
            }
            else
            {
                from->ServerMsg(from->SourceWorld()->i18n.Format("Command-AccessDenied"));
            }
        }
    }

    void Info(const std::vector<std::string>& arguments, Character* from)
    {
        Character *victim = from->world->GetCharacter(arguments[0]);

        if (!victim || victim->nowhere)
        {
            from->ServerMsg(from->SourceWorld()->i18n.Format("Command-CharacterNotFound"));
        }
        else
        {
            if (victim->SourceAccess() < int(from->world->admin_config["cmdprotect"]) || victim->SourceAccess() <= from->SourceAccess())
            {
                std::string name = util::ucfirst(victim->SourceName());

                PacketBuilder reply(PACKET_ADMININTERACT, PACKET_TELL, 85 + name.length());

                switch (victim->admin)
                {
                    case ADMIN_HGM: reply.AddString(from->world->i18n.Format("HighGameMaster", name)); break;
                    case ADMIN_GM: reply.AddString(from->world->i18n.Format("GameMaster", name)); break;
                    case ADMIN_GUARDIAN: reply.AddString(from->world->i18n.Format("Guardian", name)); break;
                    case ADMIN_GUIDE: reply.AddString(from->world->i18n.Format("LightGuide", name)); break;

                    default: reply.AddString(name); break;
                }

                reply.AddString(" ");
                reply.AddBreakString(util::trim(victim->PaddedGuildTag()));
                reply.AddInt(victim->Usage());
                reply.AddByte(255);
                reply.AddByte(255);

                if (victim->rebirth >= 1)
                {
                    reply.AddInt(victim->rebirth * 200 + victim->level);
                }
                else
                {
                    reply.AddInt(victim->exp);
                }

                reply.AddChar(victim->level);
                reply.AddShort(victim->mapid);
                reply.AddShort(victim->x);
                reply.AddShort(victim->y);
                reply.AddShort(victim->hp);
                reply.AddShort(victim->maxhp);
                reply.AddShort(victim->tp);
                reply.AddShort(victim->maxtp);
                reply.AddShort(victim->display_str);
                reply.AddShort(victim->display_intl);
                reply.AddShort(victim->display_wis);
                reply.AddShort(victim->display_agi);
                reply.AddShort(victim->display_con);
                reply.AddShort(victim->display_cha);
                reply.AddShort(victim->maxdam);
                reply.AddShort(victim->mindam);
                reply.AddShort(victim->accuracy);
                reply.AddShort(victim->evade);
                reply.AddShort(victim->armor);
                reply.AddShort(0);
                reply.AddShort(0);
                reply.AddShort(0);
                reply.AddShort(0);
                reply.AddShort(0);
                reply.AddShort(0);
                reply.AddChar(victim->weight);
                reply.AddChar(victim->maxweight);

                from->Send(reply);
            }
            else
            {
                from->ServerMsg(from->SourceWorld()->i18n.Format("Command-AccessDenied"));
            }
        }
    }

    void Item(const std::vector<std::string>& arguments, Character* from)
    {
        EIF_Data& eif = from->world->eif->Get(1);

        std::string idstring = "";

        if (arguments.size() >= 1)
        {
            for (std::size_t i = 0; i < arguments.size(); ++i)
            {
                idstring += arguments[i];

                if (i < arguments.size() - 1)
                    idstring += " ";
            }

            for (unsigned short i = 1; i <= from->world->eif->data.size(); i++)
            {
                eif = from->world->eif->Get(i);

                if (int(util::lowercase(eif.name).find(util::lowercase(idstring))) != -1 || idstring == util::to_string(eif.id))
                    from->ServerMsg("[Item] " + eif.name + " (" + util::to_string(eif.id) + ")");
            }
        }
    }

    void Class(const std::vector<std::string>& arguments, Character* from)
    {
        ECF_Data& ecf = from->world->ecf->Get(1);

        std::string idstring = "";

        if (arguments.size() >= 1)
        {
            for (std::size_t i = 0; i < arguments.size(); ++i)
            {
                idstring += arguments[i];

                if (i < arguments.size() - 1)
                    idstring += " ";
            }

            for (unsigned short i = 1; i <= from->world->ecf->data.size(); i++)
            {
                ecf = from->world->ecf->Get(i);

                if (int(util::lowercase(ecf.name).find(util::lowercase(idstring))) != -1 || idstring == util::to_string(ecf.id))
                    from->ServerMsg("[Class] " + ecf.name + " (" + util::to_string(ecf.id) + ")");
            }
        }
    }

    void NPC(const std::vector<std::string>& arguments, Character* from)
    {
        ENF_Data& enf = from->world->enf->Get(1);

        std::string idstring = "";

        if (arguments.size() >= 1)
        {
            for (std::size_t i = 0; i < arguments.size(); ++i)
            {
                idstring += arguments[i];

                if (i < arguments.size() - 1)
                    idstring += " ";
            }

            for (unsigned short i = 1; i <= from->world->enf->data.size(); i++)
            {
                enf = from->world->enf->Get(i);

                if (int(util::lowercase(enf.name).find(util::lowercase(idstring))) != -1 || idstring == util::to_string(enf.id))
                    from->ServerMsg("[NPC] " + enf.name + " (" + util::to_string(enf.id) + ")");
            }
        }
    }

    void Spell(const std::vector<std::string>& arguments, Character* from)
    {
        ESF_Data& esf = from->world->esf->Get(1);

        std::string idstring = "";

        if (arguments.size() >= 1)
        {
            for (std::size_t i = 0; i < arguments.size(); ++i)
            {
                idstring += arguments[i];

                if (i < arguments.size() - 1)
                    idstring += " ";
            }

            for (unsigned short i = 1; i <= from->world->esf->data.size(); i++)
            {
                esf = from->world->esf->Get(i);

                if (int(util::lowercase(esf.name).find(util::lowercase(idstring))) != -1 || idstring == util::to_string(esf.id))
                    from->ServerMsg("[Spell] " + esf.name + " (" + util::to_string(esf.id) + ")");
            }
        }
    }

    COMMAND_HANDLER_REGISTER()
        RegisterCharacter({"paperdoll", {"victim"}, {}}, Paperdoll);
	    RegisterCharacter({"book", {"victim"}, {}, 2}, Book);
        RegisterCharacter({"inventory", {}, {}, 3}, Inventory);
        RegisterCharacter({"info", {}, {}, 2}, Info);
        RegisterCharacter({"item", {}, {}, 2}, Item);
        RegisterCharacter({"class", {}, {}, 2}, Class);
        RegisterCharacter({"npc", {}, {}, 2}, NPC);
        RegisterCharacter({"spell", {}, {}, 2}, Spell);

        RegisterAlias("p", "paperdoll");
        RegisterAlias("i", "info");
    COMMAND_HANDLER_REGISTER_END()
}
