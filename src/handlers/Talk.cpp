#include "handlers.hpp"

#include <csignal>
#include <ctime>

#include "../character.hpp"
#include "../eoclient.hpp"
#include "../eodata.hpp"
#include "../player.hpp"
#include "../world.hpp"
#include "../party.hpp"
#include "../text.hpp"

extern volatile std::sig_atomic_t exiting_eoserv;

static void limit_message(std::string &message, std::size_t chatlength)
{
	if (message.length() > chatlength)
	{
		message = message.substr(0, chatlength - 6) + " [...]";
	}
}

namespace Handlers
{
    void Talk_Request(Character *character, PacketReader &reader)
    {
        if (!character->guild) return;
        if (character->muted_until > time(0)) return;

        std::string message = reader.GetEndString();
        limit_message(message, int(character->world->config["ChatLength"]));

        if (character->world->chatlogs_config["LogGuild"])
        {
            UTIL_FOREACH(character->world->characters, from)
            {
                if (from->admin >= int(character->world->chatlogs_config["SeeGuild"]))
                    from->StatusMsg(character->SourceName() + ": " + message + " [Guild Message]");
            }

            FILE *fh = fopen("./data/chatlogs/guild.txt", "a");
            fprintf(fh, "[Guild Message] %s: %s\n", util::ucfirst(character->SourceName()).c_str(), message.c_str());
            fclose(fh);

            character->world->AdminMsg(character, message + " [Guild Message]", int(character->world->chatlogs_config["SeeGuild"]), false);
        }

        character->guild->Msg(character, message, false);

        #ifdef GUI
        Text::UpdateChat(util::ucfirst(character->SourceName()) + ": " + util::ucfirst(message.c_str()), 0,0,0,19);
        #endif
    }

    void Talk_Open(Character *character, PacketReader &reader)
    {
        if (!character->party) return;
        if (character->muted_until > time(0)) return;

        std::string message = reader.GetEndString();
        limit_message(message, int(character->world->config["ChatLength"]));

        if (character->world->chatlogs_config["LogParty"])
        {
            UTIL_FOREACH(character->world->characters, from)
            {
                if (from->admin >= int(character->world->chatlogs_config["SeeParty"]))
                    from->StatusMsg(character->SourceName() + ": " + message + " [Party Message]");
            }

            FILE *fh = fopen("./data/chatlogs/party.txt", "a");
            fprintf(fh, "[Party Message] %s: %s\n", util::ucfirst(character->SourceName()).c_str(), message.c_str());
            fclose(fh);

            character->world->AdminMsg(character, message + " [Party Message]", int(character->world->chatlogs_config["SeeParty"]), false);
        }

        character->party->Msg(character, message, false);

        #ifdef GUI
        Text::UpdateChat(util::ucfirst(character->SourceName()) + ": " + util::ucfirst(message.c_str()), 0,0,0,10);
        #endif
    }

    void Talk_Msg(Character *character, PacketReader &reader)
    {
        if (character->muted_until > time(0)) return;

        if (character->mapid == int(character->world->config["JailMap"])
        || character->mapid == int(character->world->config["WallMap"]))
        {
            return;
        }

        std::string message = reader.GetEndString();
        limit_message(message, int(character->world->config["ChatLength"]));

        if (character->world->global == true)
        {
            if (character->world->chatlogs_config["LogGlobal"])
            {
                UTIL_FOREACH(character->world->characters, from)
                {
                    if (from->admin >= int(character->world->chatlogs_config["SeeGlobal"]))
                        from->StatusMsg(character->SourceName() + ": " + message + " [Global Message]");
                }

                FILE *fh = fopen("./data/chatlogs/global.txt", "a");
                fprintf(fh, "[Global Message] %s: %s\n", util::ucfirst(character->SourceName()).c_str(), message.c_str());
                fclose(fh);

                character->world->AdminMsg(character, message + " [Global Message]", int(character->world->chatlogs_config["SeeGlobal"]), false);
            }

            character->world->Msg(character, message, false);
        }
        else
        {
            character->ServerMsg(character->world->i18n.Format("GlobalOffline"));
        }

        #ifdef GUI
        Text::UpdateChat(util::ucfirst(character->SourceName()) + ": " + util::ucfirst(message.c_str()), 0,0,0,4);
        #endif
    }

    void Talk_Tell(Character *character, PacketReader &reader)
    {
        if (character->muted_until > time(0)) return;

        std::string name = reader.GetBreakString();
        std::string message = reader.GetEndString();

        limit_message(message, int(character->world->config["ChatLength"]));
        Character *to = character->world->GetCharacter(name);

        if (to && !to->hidden)
        {
            if (to->whispers)
            {
                to->Msg(character, message);
            }
            else
            {
                character->Msg(to, character->world->i18n.Format("Whisper-Blocked", to->SourceName()));
            }

            if (character->world->chatlogs_config["LogPrivate"])
            {
                UTIL_FOREACH(character->world->characters, from)
                {
                    if (from->admin >= int(character->world->chatlogs_config["SeePrivate"]))
                        from->StatusMsg(character->SourceName() + ": " + message + " [Private Message]");
                }

                FILE *fh = fopen("./data/chatlogs/private.txt", "a");
                fprintf(fh, "[Private Message] %s: %s\n", util::ucfirst(character->SourceName()).c_str(), message.c_str());
                fclose(fh);

                character->world->AdminMsg(character, message + " [Private Message]", int(character->world->chatlogs_config["SeePrivate"]), false);
            }
        }
        else
        {
            PacketBuilder reply(PACKET_TALK, PACKET_REPLY, 2 + name.length());
            reply.AddShort(TALK_NOTFOUND);
            reply.AddString(name);
            character->Send(reply);
        }

        #ifdef GUI
        Text::UpdateChat(util::ucfirst(character->SourceName()) + ": " + util::ucfirst(message.c_str()), 0,0,0,1);
        #endif
    }

    void Talk_Admin(Character *character, PacketReader &reader)
    {
        if (character->SourceAccess() < ADMIN_GUARDIAN) return;
        if (character->muted_until > time(0)) return;

        std::string message = reader.GetEndString();
        limit_message(message, int(character->world->config["ChatLength"]));

        if (character->world->chatlogs_config["LogAdmin"])
        {
            UTIL_FOREACH(character->world->characters, from)
            {
                if (from->admin >= int(character->world->chatlogs_config["SeeAdmin"]))
                    from->StatusMsg(character->SourceName() + ": " + message + " [Admin Message]");
            }

            FILE *fh = fopen("./data/chatlogs/admin.txt", "a");
            fprintf(fh, "[Admin Message] %s: %s\n", util::ucfirst(character->SourceName()).c_str(), message.c_str());
            fclose(fh);
        }

        #ifdef GUI
        Text::UpdateChat(util::ucfirst(character->SourceName()) + ": " + util::ucfirst(message.c_str()), 0,0,0,12);
        #endif

        character->world->AdminMsg(character, message + " [Admin Message]", int(character->world->chatlogs_config["SeeAdmin"]), false);
    }

    void Talk_Announce(Character *character, PacketReader &reader)
    {
        if (character->SourceAccess() < ADMIN_GUARDIAN) return;
        if (character->muted_until > time(0)) return;

        std::string message = reader.GetEndString();
        limit_message(message, int(character->world->config["ChatLength"]));

        if (character->world->chatlogs_config["LogAnnounce"])
        {
            UTIL_FOREACH(character->world->characters, from)
            {
                if (from->admin >= int(character->world->chatlogs_config["SeeAnnounce"]))
                    from->StatusMsg(character->SourceName() + ": " + message + " [Announce Message]");
            }

            FILE *fh = fopen("./data/chatlogs/announce.txt", "a");
            fprintf(fh, "[Announce Message] %s: %s\n", util::ucfirst(character->SourceName()).c_str(), message.c_str());
            fclose(fh);

            character->world->AdminMsg(character, message + " [Announce Message]", int(character->world->chatlogs_config["SeeAnnounce"]), false);
        }

        #ifdef GUI
        Text::UpdateChat(util::ucfirst(character->SourceName()) + ": " + util::ucfirst(message.c_str()), 0,0,0,4);
        #endif

        character->world->AnnounceMsg(character, message, false);
    }

    PACKET_HANDLER_REGISTER(PACKET_TALK)
        Register(PACKET_REQUEST, Talk_Request, Playing);
        Register(PACKET_OPEN, Talk_Open, Playing);
        Register(PACKET_MSG, Talk_Msg, Playing);
        Register(PACKET_TELL, Talk_Tell, Playing);
        Register(PACKET_ADMIN, Talk_Admin, Playing);
        Register(PACKET_ANNOUNCE, Talk_Announce, Playing);
    PACKET_HANDLER_REGISTER_END()
}
