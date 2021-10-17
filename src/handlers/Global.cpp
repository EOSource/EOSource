#include "handlers.hpp"

#include "../character.hpp"

namespace Handlers
{
    void Global_Remove(Character *character, PacketReader &reader)
    {
        reader.GetChar();

        character->whispers = true;
    }

    void Global_Player(Character *character, PacketReader &reader)
    {
        reader.GetChar();

        character->whispers = false;
    }

    void Global_Open(Character *character, PacketReader &reader)
    {
        reader.GetChar();

        (void)character;
    }

    void Global_Close(Character *character, PacketReader &reader)
    {
        reader.GetChar();

        (void)character;
    }

    PACKET_HANDLER_REGISTER(PACKET_GLOBAL)
        Register(PACKET_REMOVE, Global_Remove, Playing);
        Register(PACKET_PLAYER, Global_Player, Playing);
        Register(PACKET_OPEN, Global_Open, Playing);
        Register(PACKET_CLOSE, Global_Close, Playing);
    PACKET_HANDLER_REGISTER_END()
}
