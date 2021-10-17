#ifdef GUI
#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>
#include "text.hpp"
#include "chat.hpp"

TextStruct text;

ALLEGRO_FONT *arial;

void Text::Initialize()
{
    std::string sWINDIR;
    char *cWINDIR = getenv("WINDIR");
    sWINDIR = cWINDIR;

    arial = al_load_ttf_font((sWINDIR + "\\Fonts\\Arial.ttf").c_str(), 11, ALLEGRO_TTF_NO_KERNING);
}

void Text::Destroy()
{
    al_destroy_font(arial);
}

int Text::Width(string message)
{
  return al_get_text_width(arial, message.c_str());
}

void Text::Draw(const char *text,int r,int g,int b, int x, int y, ...)
{
    al_draw_textf(arial, al_map_rgb(r,g,b), x, y, ALLEGRO_ALIGN_LEFT, text);
}

void Text::DrawMiddle(const char *text,int r,int g,int b, int x, int y, ...)
{
    al_draw_textf(arial, al_map_rgb(r,g,b), x, y, ALLEGRO_ALIGN_CENTRE, text);
}

void Text::UpdateChat(std::string text, int r, int g ,int b, int id)
{
    int width = 400;
    char stext[1024];
    char *pch;
    char word[255];
    char Lines[40][1024];
    char TempLine[1024];
    int CurrentLine = 0;

    strcpy(stext,text.c_str());
    strcpy(TempLine,"");;

    for (int i = 0; i < 40; i++)
    {
        sprintf(Lines[i], "");
    }

    pch = strtok (stext," ");

    do
    {
        if (al_get_text_width(arial, pch) > (width - 5))
        {
            string cal = pch;
            pch = (char*)("'" + cal.substr(0,20) + "..'").c_str();
        }


        strcpy(word,"");
        sprintf(word,"%s ",pch);
        sprintf(TempLine,"%s%s",TempLine,word);

        if (al_get_text_width(arial,TempLine) >= (width))
        {
            strcpy(TempLine,word);
            CurrentLine ++;
        }

        if (CurrentLine < 40)
        {
            strcat(Lines[CurrentLine],word);
        }

        pch = strtok (NULL, " ");
    }

    while (pch != NULL);

    for (int i = 0; i <= CurrentLine; i++)
    {
        if (i == 0)
        {
            chat.begin[chat.cp_line] = true;
        }

        chat.cchat[chat.cp_line] = Lines[i];
        chat.cr[chat.cp_line] = r;
        chat.cg[chat.cp_line] = g;
        chat.cb[chat.cp_line] = b;
        chat.cp_id[chat.cp_line] = id;
        chat.cp_line ++;
    }
}
#endif
