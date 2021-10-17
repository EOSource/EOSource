#ifndef CONSOLE_HPP_INCLUDED
#define CONSOLE_HPP_INCLUDED

#include "fwd/console.hpp"

#include <string>
#include "platform.h"

namespace Console
{
    extern bool Styled[2];

    #ifdef WIN32

    enum Color
    {
        COLOR_BLUE = 1,
        COLOR_GREEN = 2,
        COLOR_CYAN = 3,
        COLOR_RED = 4,
        COLOR_MAGENTA = 5,
        COLOR_YELLOW = 6,
        COLOR_GREY = 7,
        COLOR_BLACK = 8
    };

    #else

    enum Color
    {
        COLOR_BLUE = 34,
        COLOR_GREEN = 32,
        COLOR_CYAN = 36,
        COLOR_RED = 31,
        COLOR_MAGENTA = 35,
        COLOR_YELLOW = 33,
        COLOR_GREY = 37,
        COLOR_BLACK = 30
    };

    #endif

    enum Stream
    {
        STREAM_OUT,
        STREAM_ERR
    };

    void Out(std::string f, ...);
    void RedOut(std::string f, ...);
    void PurpleOut(std::string f, ...);
    void YellowOut(std::string f, ...);
    void GreenOut(std::string f, ...);

    void Wrn(std::string f, ...);
    void Err(std::string f, ...);
    void Dbg(std::string f, ...);
}

#endif
