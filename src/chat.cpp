#ifdef GUI
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>

#include "chat.hpp"
#include "text.hpp"

ChatStruct chat;

int line;
int scroller;

void Chat::Info(std::string text, int r, int g, int b)
{
    chat.chat[chat.p_line] = text;

    chat.r[chat.p_line] = r;
    chat.g[chat.p_line] = g;
    chat.b[chat.p_line] = b;

    chat.p_line += 1;
}

void Chat::ChatBox()
{
    int width = 15;
    int height = 15;

    if (chat.x >= 517 && chat.x <= 517 + width && chat.y >= 55 && chat.y <= 55 + height)
    {
        al_draw_filled_triangle(517,55 + height,517 + width / 2,55,517 + width, 55 + height, al_map_rgb(237,225,223));

        if (chat.cp_line - 15 - scroller > 0 && chat.p == 1 && chat.cp_line > 15)
        {
            scroller ++;
        }
    }
    else
    {
        al_draw_filled_triangle(517,55 + height, 517 + width / 2,55,517 + width, 55 + height, al_map_rgb(200,170,135));
    }

    if (chat.x >= 517 && chat.x <= 517 + width && chat.y >= 195 && chat.y <= 195 + height)
    {
        al_draw_filled_triangle(517,195,517 + width / 2,195 + height, 517 + width, 195, al_map_rgb(237,225,223));

        if (scroller > 0 && chat.p == 1)
        {
            scroller --;
        }
    }
    else
    {
        al_draw_filled_triangle(517,195,517 + width / 2,195 + height, 517 + width, 195, al_map_rgb(200,170,135));
    }

    for (int i = 0; i <= 12; i++)
    {
        if (chat.cp_line >= 12)
        {
            line = chat.cp_line - 12 - scroller + i;
        }
        else
        {
            line = i;
        }

        if (chat.cchat[line].length() > 0)
        {
            int bmp_height = al_get_bitmap_height(chat.png) / 24;
            int bmp_width = al_get_bitmap_width(chat.png);

            Text::Draw(chat.cchat[line].c_str(), chat.cr[line], chat.cg[line], chat.cb[line], 110,55 + i * 13, NULL);
            if(chat.begin[line]) al_draw_bitmap_region(chat.png, 0, bmp_height * chat.cp_id[line], bmp_width, bmp_height, 93, 55 + i * 13, 0);
        }
    }
}

void Chat::InfoBox()
{
    int width = 15;
    int height = 15;

    if (chat.x >= 540 && chat.x <= 540 + width && chat.y >= 55 && chat.y <= 55 + height)
    {
        al_draw_filled_triangle(540,55 + height,540 + width / 2,55,540 + width,55 + height,al_map_rgb(237,225,213));

        if (chat.p_line - 15 - scroller > 0 && chat.p == 1 && chat.p_line > 15)
        {
            scroller ++;
        }
    }
    else
    {
        al_draw_filled_triangle(540,55 + height,540 + width / 2,55,540 + width,55 + height,al_map_rgb(220,197,173));
    }

    if (chat.x >= 540 && chat.x <= 540 + width && chat.y >= 228 && chat.y <= 228 + height)
    {
        al_draw_filled_triangle(540,228,540 + width / 2,228 + height,540 + width,228,al_map_rgb(237,225,213));

        if (scroller > 0 && chat.p == 1)
        {
            scroller --;
        }
    }
    else
    {
        al_draw_filled_triangle(540,228,540 + width / 2,228 + height,540 + width,228,al_map_rgb(220,197,173));
    }

    for (int i = 0; i <= 15; i++)
    {
        if (chat.p_line >= 15)
        {
            line = chat.p_line - 15 - scroller + i;
        }
        else
        {
            line = i;
        }

        if (chat.chat[line].length() > 0)
        {
            Text::Draw(chat.chat[line].c_str(),chat.r[line],chat.g[line],chat.b[line],110,60 + i * 12,NULL);
        }
    }
}
#endif
