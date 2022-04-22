#include "commands.hpp"

#include "../../util.hpp"
#include "../../map.hpp"
#include "../../npc.hpp"
#include "../../eoplus.hpp"
#include "../../packet.hpp"
#include "../../player.hpp"
#include "../../quest.hpp"
#include "../../world.hpp"

namespace PlayerCommands
{
    void SetRace(const std::vector<std::string>& arguments, Character* from)
    {
        from->race = Skin(std::min(std::max(util::to_int(arguments[0]), 0), int(from->world->config["MaxSkin"])));
        from->Warp(from->mapid, from->x, from->y, WARP_ANIMATION_NONE);
    }

    void SetTitle(const std::vector<std::string>& arguments, Character* from)
    {
        std::string title = "";

        for (std::size_t i = 0; i < arguments.size(); ++i)
        {
            title += arguments[i];

            if (i < arguments.size() - 1)
            {
                title += " ";
            }
        }

        if (title.length() <= 25)
           from->title = util::ucfirst(title);
    }

    void SetPartner(const std::vector<std::string>& arguments, Character* from)
    {
        from->partner = arguments[0];
    }

    void SetGender(const std::vector<std::string>& arguments, Character* from)
    {
        from->gender = Gender(std::min(std::max(util::to_int(arguments[0]), 0), 1));
        from->Warp(from->mapid, from->x, from->y, WARP_ANIMATION_NONE);
    }

    void SetHairStyle(const std::vector<std::string>& arguments, Character* from)
    {
        from->hairstyle = std::min(std::max(util::to_int(arguments[0]), 0), int(from->SourceWorld()->config["MaxHairStyle"]));
        from->oldhairstyle = util::to_int(arguments[0]);
        from->Warp(from->mapid, from->x, from->y, WARP_ANIMATION_NONE);
    }

    void SetHairColor(const std::vector<std::string>& arguments, Character* from)
    {
        from->haircolor = std::min(std::max(util::to_int(arguments[0]), 0), int(from->SourceWorld()->config["MaxHairColor"]));
        from->Warp(from->mapid, from->x, from->y, WARP_ANIMATION_NONE);
    }

    void SetGuildRank(const std::vector<std::string>& arguments, Character* from)
    {
        Character *victim = from->SourceWorld()->GetCharacter(arguments[0]);

        if (!victim || victim == from)
        {
            from->ServerMsg(from->SourceWorld()->i18n.Format("Command-CharacterNotFound"));
        }
        else
        {
            if (from->guild_rank <= int(from->SourceWorld()->config["GuildPromoteRank"]))
            {
                if (victim->guild && victim->guild->tag == from->guild->tag)
                {
                    victim->guild_rank = std::min(std::max(util::to_int(arguments[1]), 0), 9);
                }
                else
                {
                    from->ServerMsg(victim->SourceName() + " is not part of your guild.");
                }
            }
        }
    }

    void SetClass(const std::vector<std::string>& arguments, Character* from)
    {
        from->SetClass(std::min(std::max(util::to_int(arguments[0]), 0), int(from->SourceWorld()->ecf->data.size() - 1)));
    }

    PLAYER_COMMAND_HANDLER_REGISTER()
        using namespace std::placeholders;
        RegisterCharacter({"race"}, SetRace);
        RegisterCharacter({"title"}, SetTitle);
        RegisterCharacter({"partner"}, SetPartner);
        RegisterCharacter({"gender"}, SetGender);
        RegisterCharacter({"hairstyle"}, SetHairStyle);
        RegisterCharacter({"haircolor"}, SetHairColor);
        RegisterCharacter({"guildrank"}, SetGuildRank);
        RegisterCharacter({"class"}, SetClass);
    PLAYER_COMMAND_HANDLER_REGISTER_END()
}
