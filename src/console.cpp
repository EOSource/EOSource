#include "console.hpp"

#include <cstdio>
#include <cstdarg>

#include "platform.h"

#ifdef WIN32
#include <windows.h>
#endif

namespace Console
{
    bool Styled[2] = {true, true};

    #ifdef WIN32
    static HANDLE Handles[2];

    inline void Init(Stream i)
    {
        if (!Handles[i])
        {
            Handles[i] = GetStdHandle((i == STREAM_OUT) ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
        }
    }

    void SetTextColor(Stream stream, Color color, bool bold)
    {
        Init(stream);
        SetConsoleTextAttribute(Handles[stream], color + (bold ? 8 : 0));
    }

    void ResetTextColor(Stream stream)
    {
        Init(stream);
        SetConsoleTextAttribute(Handles[stream], COLOR_GREY);
    }

    #else

    void SetTextColor(Stream stream, Color color, bool bold)
    {
        char command[8] = {27, '[', '1', ';', '3', '0', 'm', 0};

        if (bold)
        {
            command[5] += (color - 30);
        }
        else
        {
            for (int i = 2; i < 6; ++i)
                command[i] = command[i + 2];

            command[3] += (color - 30);
        }

        std::fputs(command, (stream == STREAM_OUT) ? stdout : stderr);
    }

    void ResetTextColor(Stream stream)
    {
        char command[5] = {27, '[', '0', 'm', 0};
        std::fputs(command, (stream == STREAM_OUT) ? stdout : stderr);
    }

    #endif

    #define CONSOLE_GENERIC_OUT(prefix, stream, color, bold) \
    if (Styled[stream]) SetTextColor(stream, color, bold); \
    va_list args; \
    va_start(args, f); \
    std::vfprintf((stream == STREAM_OUT) ? stdout : stderr, (std::string(" > ") + f + "\n").c_str(), args); \
    va_end(args); \
    if (Styled[stream]) ResetTextColor(stream);

    void Out(std::string f, ...)
    {
        CONSOLE_GENERIC_OUT(" ", STREAM_OUT, COLOR_GREY, true);
    }

    void RedOut(std::string f, ...)
    {
        CONSOLE_GENERIC_OUT(" ", STREAM_OUT, COLOR_RED, true);
    }

    void PurpleOut(std::string f, ...)
    {
        CONSOLE_GENERIC_OUT(" ", STREAM_OUT, COLOR_MAGENTA, true);
    }

    void YellowOut(std::string f, ...)
    {
        CONSOLE_GENERIC_OUT(" ", STREAM_OUT, COLOR_YELLOW, true);
    }

    void GreenOut(std::string f, ...)
    {
        CONSOLE_GENERIC_OUT(" ", STREAM_OUT, COLOR_GREEN, true);
    }

    void Wrn(std::string f, ...)
    {
        CONSOLE_GENERIC_OUT("WRN ", STREAM_ERR, COLOR_YELLOW, true);
    }

    void Err(std::string f, ...)
    {
        CONSOLE_GENERIC_OUT("ERR ", STREAM_ERR, COLOR_RED, true);
    }

    void Dbg(std::string f, ...)
    {
        CONSOLE_GENERIC_OUT("DBG ", STREAM_OUT, COLOR_GREY, false);
    }
}
