#ifndef PLAYERCOMMANDS_HPP_INCLUDED
#define PLAYERCOMMANDS_HPP_INCLUDED

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../character.hpp"
#include "../../command_source.hpp"

#define PLAYER_COMMAND_HANDLER_PASTE_AUX2(base, id) base##id
#define PLAYER_COMMAND_HANDLER_PASTE_AUX(base, id) PLAYER_COMMAND_HANDLER_PASTE_AUX2(base, id)

#define PLAYER_COMMAND_HANDLER_REGISTER() \
namespace { struct PLAYER_COMMAND_HANDLER_PASTE_AUX(player_command_handler_register_helper_, __LINE__) : public player_command_handler_register_helper \
{ \
    PLAYER_COMMAND_HANDLER_PASTE_AUX(player_command_handler_register_helper_, __LINE__)() \
    { \
        player_command_handler_register_init _init; \

#define PLAYER_COMMAND_HANDLER_REGISTER_END() ; \
    } \
} PLAYER_COMMAND_HANDLER_PASTE_AUX(player_command_handler_register_helper_instance_, __LINE__); }

namespace PlayerCommands
{
    struct player_command_info
    {
        std::string name;
        std::vector<std::string> arguments;
        std::vector<std::string> optional_arguments;
        std::size_t partial_min_chars;
        bool require_character;

        player_command_info(std::string name, std::vector<std::string> arguments = std::vector<std::string>(), std::vector<std::string> optional_arguments = std::vector<std::string>(), std::size_t partial_min_chars = 0)
            : name(name)
            , arguments(arguments)
            , optional_arguments(optional_arguments)
            , partial_min_chars(partial_min_chars)
            , require_character(false)
        { }
    };

    struct player_command_handler
    {
        player_command_info info;
        std::function<void(const std::vector<std::string>&, Command_Source*)> f;

        player_command_handler(player_command_info info, std::function<void(const std::vector<std::string>&, Command_Source*)> f)
            : info(info)
            , f(f)
        { }

        player_command_handler(player_command_info info, std::function<void(const std::vector<std::string>&, Character*)> f)
            : info(info)
            , f([f](const std::vector<std::string>& arguments, Command_Source* from) { f(arguments, static_cast<Character*>(from)); })
        {
            this->info.require_character = true;
        }

        bool operator <(const player_command_handler& rhs) const
        {
            return (info.partial_min_chars < rhs.info.partial_min_chars) || (info.name.length() < rhs.info.name.length()) || (info.name < rhs.info.name);
        }
    };

    class player_command_handler_register
    {
        private:
            std::map<std::string, player_command_handler> handlers;
            std::unordered_map<std::string, std::string> aliases;

        public:
            void Register(player_command_handler handler);
            void RegisterAlias(const std::string& alias, const std::string& command);

            bool Handle(std::string command, const std::vector<std::string>& arguments, Command_Source* from, int alias_depth = 0) const;
    };

    extern player_command_handler_register *player_command_handler_register_instance;

    class player_command_handler_register_init
    {
        private:
            bool master;

        public:
            player_command_handler_register_init(bool master = false)
                : master(master)
            {
                static bool initialized;

                if (!initialized)
                {
                    initialized = true;
                    init();
                }
            }

            void init() const;

            ~player_command_handler_register_init();
    };

    class player_command_handler_register_helper
    {
        public:
            template <class F> void Register(player_command_info info, const F& f)
            {
                player_command_handler_register_instance->Register(player_command_handler(info, std::function<void(const std::vector<std::string>&, Command_Source*)>(f)));
            }

            template <class F>  void RegisterCharacter(player_command_info info, const F& f)
            {
                player_command_handler_register_instance->Register(player_command_handler(info, std::function<void(const std::vector<std::string>&, Character*)>(f)));
            }

            void RegisterAlias(const std::string& alias, const std::string& command)
            {
                player_command_handler_register_instance->RegisterAlias(alias, command);
            }
    };

    inline bool Handle(std::string command, const std::vector<std::string>& arguments, Command_Source* from)
    {
        return player_command_handler_register_instance->Handle(command, arguments, from);
    }
}

#endif
