#include "handlers.hpp"

#include <string>

#include "../character.hpp"
#include "../player.hpp"
#include "../world.hpp"

namespace Handlers
{
    void AdminInteract_Tell(Character *character, PacketReader &reader)
    {
        std::string message = reader.GetEndString();

        if (character->world->config["OldReports"])
        {
            message = character->world->i18n.Format("AdminRequest", message);
            character->world->AdminMsg(character, message, static_cast<int>(character->world->admin_config["reports"]));
        }
        else
        {
            character->world->AdminRequest(character, message);
        }
    }

    void AdminInteract_Report(Character *character, PacketReader &reader)
    {
        std::string reportee = reader.GetBreakString();
        std::string message = reader.GetEndString();

        if (character->world->config["OldReports"])
        {
            message = character->world->i18n.Format("AdminReport", reportee, message);
            character->world->AdminMsg(character, message, static_cast<int>(character->world->admin_config["reports"]));
        }
        else
        {
            character->world->AdminReport(character, reportee, message);
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_ADMININTERACT)
        Register(PACKET_TELL, AdminInteract_Tell, Playing | OutOfBand);
        Register(PACKET_REPORT, AdminInteract_Report, Playing | OutOfBand);
    PACKET_HANDLER_REGISTER_END()
}
