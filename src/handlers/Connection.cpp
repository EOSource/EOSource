#include "handlers.hpp"

#include "../eoclient.hpp"

namespace Handlers
{
    void Connection_Accept(EOClient *client, PacketReader &reader)
    {
        (void)client;
        (void)reader;
    }

    void Connection_Ping(EOClient *client, PacketReader &reader)
    {
        (void)reader;

        if (client->needpong)
        {
            client->needpong = false;
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_CONNECTION)
        Register(PACKET_ACCEPT, Connection_Accept, Menu);
        Register(PACKET_PING, Connection_Ping, Any | OutOfBand);
    PACKET_HANDLER_REGISTER_END()
}
