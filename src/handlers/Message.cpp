#include "handlers.hpp"

#include "../character.hpp"

namespace Handlers
{
    void Message_Ping(Character *character, PacketReader &reader)
    {
        int something = reader.GetShort();

        PacketBuilder reply(PACKET_MESSAGE, PACKET_PONG, 2);
        reply.AddShort(something);
        character->Send(reply);
    }

    PACKET_HANDLER_REGISTER(PACKET_MESSAGE)
        Register(PACKET_PING, Message_Ping, Playing | OutOfBand)
    PACKET_HANDLER_REGISTER_END()
}
