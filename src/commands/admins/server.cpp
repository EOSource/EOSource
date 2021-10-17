#include "commands.hpp"

#include "../../util.hpp"
#include "../../console.hpp"
#include "../../eoclient.hpp"
#include "../../eoserver.hpp"
#include "../../map.hpp"
#include "../../player.hpp"
#include "../../timer.hpp"
#include "../../world.hpp"
#include "../../chat.hpp"

namespace Commands
{
    void ReloadMap(const std::vector<std::string>& arguments, Character* from)
    {
        World* world = from->SourceWorld();
        Map* map = from->map;
        bool isnew = false;

        UTIL_FOREACH(from->map->characters, character)
        {
            if (character->HasPet)
            {
                character->PetTransfer();
                character->pettransfer = false;
            }
        }

        if (arguments.size() >= 1)
        {
            int mapid = util::to_int(arguments[0]);

            if (mapid < 1)
                mapid = 1;

            if (world->maps.size() > mapid - 1)
            {
                map = world->maps[mapid - 1];
            }
            else if (mapid <= static_cast<int>(world->config["Maps"]))
            {
                isnew = true;

                while (world->maps.size() < mapid)
                {
                    int newmapid = world->maps.size() + 1;
                    world->maps.push_back(new Map(newmapid, world));
                }
            }
        }

        if (map && !isnew)
        {
            map->Reload();
        }
    }

    void ReloadPub(const std::vector<std::string>& arguments, Command_Source* from)
    {
        (void)arguments;

        #ifdef GUI
        Chat::Info("Pub files reloaded by " + util::ucfirst(from->SourceName()), 255,255,255);
        #else
        Console::Out("Pub files reloaded by %s", util::ucfirst(from->SourceName()).c_str());
        #endif

        bool quiet = true;

        if (arguments.size() >= 1)
            quiet = (arguments[0] != "announce");

        from->SourceWorld()->ReloadPub(quiet);
    }

    void ReloadConfig(const std::vector<std::string>& arguments, Command_Source* from)
    {
        (void)arguments;

        #ifdef GUI
        Chat::Info("Config reloaded by " + util::ucfirst(from->SourceName()),255,255,255);
        #else
        Console::Out("Config reloaded by %s", util::ucfirst(from->SourceName()).c_str());
        #endif

        from->SourceWorld()->Rehash();
        from->ServerMsg("Config reloaded.");
    }

    void ReloadQuest(const std::vector<std::string>& arguments, Command_Source* from)
    {
        (void)arguments;

        #ifdef GUI
        Chat::Info("Quests reloaded by " + util::ucfirst(from->SourceName()), 255,255,255);
        #else
        Console::Out("Quests reloaded by %s", util::ucfirst(from->SourceName()).c_str());
        #endif

        from->SourceWorld()->ReloadQuests();
        from->ServerMsg("Quests reloaded.");
    }

    void Shutdown(const std::vector<std::string>& arguments, Command_Source* from)
    {
        (void)arguments;

        UTIL_FOREACH(from->SourceWorld()->characters, character)
        {
            character->Save();

            if (character->HasPet)
                character->SavePet();

            PacketBuilder builder;
            builder.SetID(PACKET_MESSAGE, PACKET_CLOSE);
            character->Send(builder);
        }

        from->SourceWorld()->guildmanager->SaveAll();
        from->SourceWorld()->Restart();

        Console::Wrn("Server shut down by %s", util::ucfirst(from->SourceName()).c_str());
    }

    void Uptime(const std::vector<std::string>& arguments, Command_Source* from)
    {
        (void)arguments;

        from->ServerMsg("Server started " + util::timeago(from->SourceWorld()->server->start, Timer::GetTime()));
    }

    void SetConfig(const std::vector<std::string>& arguments, Command_Source* from)
    {
        (void)arguments;

        if (arguments.size() > 0)
        {
            UTIL_FOREACH(from->SourceWorld()->config, configval)
            {
                if (util::lowercase(configval.first) == util::lowercase(arguments[0]))
                {
                    from->SourceWorld()->config[configval.first] = arguments.size() > 1 ? arguments[1] : "0";
                    from->ServerMsg(configval.first + " has been changed to " + std::string(from->SourceWorld()->config[configval.first]));

                    return;
                }
            }

            from->SourceWorld()->config[arguments[0]] = arguments.size() > 1 ? arguments[1] : "0";
            from->ServerMsg("A new config by the name of \"" + arguments[0] + "\" was made and given the value of \"" + std::string(from->SourceWorld()->config[arguments[0]]) + "\".");
        }
    }

    COMMAND_HANDLER_REGISTER()
        using namespace std::placeholders;
        RegisterCharacter({"remap", {}, {}, 3}, ReloadMap);
        Register({"repub", {}, {"announce"}, 3}, ReloadPub);
        Register({"rehash", {}, {}, 3}, ReloadConfig);
        Register({"request", {}, {}, 3}, ReloadQuest);
        Register({"shutdown", {}, {}, 8}, Shutdown);
        Register({"uptime"}, Uptime);
        Register({"configset", {"name"}, {}, 3}, SetConfig);
    COMMAND_HANDLER_REGISTER_END()
}
