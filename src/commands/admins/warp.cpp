#include "commands.hpp"

#include "../../util.hpp"
#include "../../map.hpp"
#include "../../packet.hpp"
#include "../../world.hpp"

namespace Commands
{
    void Warp(const std::vector<std::string>& arguments, Character* from)
    {
        Character *victim = from->SourceWorld()->GetCharacter(arguments[0]);

        if (victim && !victim->nowhere)
        {
            int mapid = from->world->FindMap(util::implode(" ", arguments, 1));

            Map *map = from->world->GetMap(mapid);

            int x = arguments.size() >= 3 && mapid == 0 ? util::to_int(arguments[2]) : map->width / 2;
            int y = arguments.size() >= 4 && mapid == 0 ? util::to_int(arguments[3]) : map->height / 2;

            if (mapid == 0) mapid = util::to_int(arguments[1]);

            victim->oldmap = victim->mapid;
            victim->oldx = victim->x;
            victim->oldy = victim->y;

            victim->Warp(mapid, x, y, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
        }
        else
        {
            int mapid = from->world->FindMap(util::implode(" ", arguments));

            Map *map = from->world->GetMap(mapid);

            int x = arguments.size() >= 2 && mapid == 0 ? util::to_int(arguments[1]) : map->width / 2;
            int y = arguments.size() >= 3 && mapid == 0 ? util::to_int(arguments[2]) : map->height / 2;

            if (mapid == 0) mapid = util::to_int(arguments[0]);

            from->oldmap = from->mapid;
            from->oldx = from->x;
            from->oldy = from->y;

            from->Warp(mapid, x, y, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
        }
    }

    void WarpMeTo(const std::vector<std::string>& arguments, Character* from)
    {
        Character *victim = from->world->GetCharacter(arguments[0]);

        if (std::string(from->SourceWorld()->eosbot_config["bot.warpable"]) == "yes")
        {
            if (arguments[0] == util::lowercase(static_cast<std::string>(from->SourceWorld()->eosbot_config["bot.name"])) || (arguments[0] == "eosbot" && std::string(from->SourceWorld()->eosbot_config["bot.name"]) == "0"))
            {
                from->Warp(int(from->SourceWorld()->eosbot_config["bot.map"]), int(from->SourceWorld()->eosbot_config["bot.locx"]), int(from->SourceWorld()->eosbot_config["bot.locy"]), from->SourceWorld()->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                from->ServerMsg("You have successfully warped to " + std::string(from->SourceWorld()->eosbot_config["bot.name"]));

                return;
            }
        }

        if (!victim || victim->nowhere)
        {
            from->ServerMsg(from->SourceWorld()->i18n.Format("Command-CharacterNotFound"));
        }
        else
        {
            if (victim->SourceAccess() < int(from->world->admin_config["cmdprotect"]) || victim->SourceAccess() <= from->SourceAccess())
            {
                from->oldmap = from->mapid;
                from->oldx = from->x;
                from->oldy = from->y;

                from->Warp(victim->mapid, victim->x, victim->y, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
            }
            else
            {
                from->ServerMsg(from->SourceWorld()->i18n.Format("Command-AccessDenied"));
            }
        }
    }

    void WarpToMe(const std::vector<std::string>& arguments, Character* from)
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
                victim->oldmap = victim->mapid;
                victim->oldx = victim->x;
                victim->oldy = victim->y;

                victim->Warp(from->mapid, from->x, from->y, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
            }
            else
            {
                from->ServerMsg(from->SourceWorld()->i18n.Format("Command-AccessDenied"));
            }
        }
    }

    void WarpBack(const std::vector<std::string>& arguments, Character* from)
    {
        Character *victim = from->SourceWorld()->GetCharacter(arguments[0]);

        if (!victim || victim->nowhere)
        {
            from->ServerMsg(from->SourceWorld()->i18n.Format("Command-CharacterNotFound"));
        }
        else
        {
            if (!victim->oldmap <= 0 || !victim->oldmap >= int(from->SourceWorld()->maps.size()))
            {
                victim->Warp(victim->oldmap, victim->oldx, victim->oldy, from->SourceWorld()->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
            }
        }
    }

    void GlobalWarp(const std::vector<std::string>& arguments, Character* from)
    {
        UTIL_FOREACH(from->world->characters, character)
        {
            if (character->mapid != int(character->world->config["JailMap"]) || character->mapid != int(character->world->config["WallMap"]))
            {
                if (character->SourceName() != from->SourceName() && character->SourceAccess() < int(from->world->admin_config["cmdprotect"]))
                    character->Warp(from->mapid, from->x, from->y, character->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
            }
        }
    }

    COMMAND_HANDLER_REGISTER()
        RegisterCharacter({"warp", {"map"}}, Warp);
        RegisterCharacter({"warpmeto", {"victim"}}, WarpMeTo, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"warptome", {"victim"}}, WarpToMe, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"warpback", {"victim"}}, WarpBack, CMD_FLAG_DUTY_RESTRICT);
        RegisterCharacter({"gw"}, GlobalWarp, CMD_FLAG_DUTY_RESTRICT);

        RegisterAlias("w", "warp");
    COMMAND_HANDLER_REGISTER_END()
}
