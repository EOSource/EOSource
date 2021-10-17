#include <algorithm>

#include "commands.hpp"

#include "../../config.hpp"
#include "../../console.hpp"
#include "../../eoclient.hpp"
#include "../../player.hpp"
#include "../../world.hpp"

namespace PlayerCommands
{
    void player_command_handler_register::Register(player_command_handler handler)
    {
        if (handler.info.partial_min_chars < 1)
            handler.info.partial_min_chars = 1;
        else if (handler.info.partial_min_chars > handler.info.name.length())
            handler.info.partial_min_chars = handler.info.name.length();

        auto result = handlers.insert(std::make_pair(handler.info.name, handler));

        if (!result.second)
            Console::Wrn("Duplicate command name: %s", handler.info.name.c_str());
    }

    void player_command_handler_register::RegisterAlias(const std::string& alias, const std::string& command)
    {
        auto result = aliases.insert(std::make_pair(alias, command));

        if (!result.second)
            Console::Wrn("Duplicate command alias name: %s", alias.c_str());
    }

    bool player_command_handler_register::Handle(std::string command, const std::vector<std::string>& arguments, Command_Source* from, int alias_depth) const
    {
        if (command.empty())
            return false;

        if (alias_depth > 100)
        {
            Console::Wrn("Command alias nested too deeply or infinitely recursive: %s", command.c_str());
            return false;
        }

        auto player_command_result = handlers.find(command);

        if (player_command_result != handlers.end() && player_command_result->second.info.arguments.size() <= arguments.size())
        {
            if (player_command_result->second.info.require_character && !from->SourceCharacter())
                return false;

            player_command_result->second.f(arguments, from);
            return true;
        }
        else
        {
            auto alias_result = aliases.find(command);

            if (alias_result != aliases.end())
            {
                return this->Handle(alias_result->second, arguments, from, alias_depth + 1);
            }

            auto match = handlers.end();

            UTIL_CIFOREACH(handlers, handler)
            {
                if (command.length() < handler->second.info.partial_min_chars
                 || command.length() >= handler->second.info.name.length())
                    continue;

                std::string prefix = handler->second.info.name.substr(0, std::max(command.length(), handler->second.info.partial_min_chars));

                if (command.substr(0, prefix.length()) == prefix)
                {
                    if (handler->second.info.require_character && !from->SourceCharacter())
                        continue;

                    if (match != handlers.end())
                        return false;

                    match = handler;
                }
            }

            if (match != handlers.end())
            {
                if (match->second.info.arguments.size() > arguments.size())
                {
                    from->ServerMsg(from->SourceWorld()->i18n.Format("Command-NotEnoughArguments"));
                    return false;
                }

                match->second.f(arguments, from);
                return true;
            }
        }

        return false;
    }

    player_command_handler_register *player_command_handler_register_instance;

    void player_command_handler_register_init::init() const
    {
        player_command_handler_register_instance = new player_command_handler_register();
    }

    player_command_handler_register_init::~player_command_handler_register_init()
    {
        if (master)
            delete player_command_handler_register_instance;
    }

    static player_command_handler_register_init player_command_handler_register_init_instance(true);
}
