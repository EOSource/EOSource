#include "handlers.hpp"

#include "../character.hpp"

namespace Handlers
{
    void Refresh_Request(Character *character, PacketReader &reader)
    {
        (void)reader;

        character->Refresh();
    }

    PACKET_HANDLER_REGISTER(PACKET_REFRESH)
        Register(PACKET_REQUEST, Refresh_Request, Playing);
    PACKET_HANDLER_REGISTER_END()
}
