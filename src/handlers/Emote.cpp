#include "handlers.hpp"

#include "../character.hpp"

namespace Handlers
{
    void Emote_Report(Character *character, PacketReader &reader)
    {
        Emote emote = static_cast<Emote>(reader.GetChar());

        if ((emote >= 1 && emote <= 10) || emote == EMOTE_DRUNK || emote == EMOTE_PLAYFUL)
        {
            character->Emote(emote, false);
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_EMOTE)
        Register(PACKET_REPORT, Emote_Report, Playing);
    PACKET_HANDLER_REGISTER_END()
}
