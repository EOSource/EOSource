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
    void Fishing(const std::vector<std::string>& arguments, Character* from)
    {
        from->ServerMsg("Fishing Level: " + util::to_string(from->flevel));
        from->ServerMsg("Fishing Exp: " + util::to_string(from->fexp));
    }

    void Mining(const std::vector<std::string>& arguments, Character* from)
    {
        from->ServerMsg("Mining Level: " + util::to_string(from->mlevel));
        from->ServerMsg("Mining Exp: " + util::to_string(from->mexp));
    }

    void Woodcutting(const std::vector<std::string>& arguments, Character* from)
    {
        from->ServerMsg("Woodcutting Level: " + util::to_string(from->wlevel));
        from->ServerMsg("Woodcutting Exp: " + util::to_string(from->wexp));
    }

    void Cooking(const std::vector<std::string>& arguments, Character* from)
    {
        from->ServerMsg("Cooking Level: " + util::to_string(from->clevel));
        from->ServerMsg("Cooking Exp: " + util::to_string(from->cexp));
    }

    PLAYER_COMMAND_HANDLER_REGISTER()
        using namespace std::placeholders;
        RegisterCharacter({"fishing"}, Fishing);
        RegisterCharacter({"mining"}, Mining);
        RegisterCharacter({"woodcutting"}, Woodcutting);
        RegisterCharacter({"cooking"}, Cooking);
    PLAYER_COMMAND_HANDLER_REGISTER_END()
}
