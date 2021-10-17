#ifdef GUI
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>
#include <load_PE_bmp.h>

#include "graphics.hpp"

GFXStruct gfx;

bool GFX::Initialize()
{
    gfx.gui[1] = GFX::LoadFromResource("pthreadGC2.dll", 101, true);
    gfx.gui[2] = GFX::LoadFromResource("pthreadGC2.dll", 102, true);
    gfx.gui[3] = GFX::LoadFromResource("pthreadGC2.dll", 103, true);
    gfx.gui[4] = GFX::LoadFromResource("pthreadGC2.dll", 104, true);

    return true;
}

ALLEGRO_BITMAP *GFX::LoadFromResource(const char *filename, int resID, bool transparant)
{
    ALLEGRO_BITMAP *bitmap;
    bitmap = load_PE_bmp(filename,resID);

    al_set_new_bitmap_flags(ALLEGRO_VIDEO_BITMAP);
    bitmap = al_clone_bitmap(bitmap);

    if (transparant)
    {
        al_convert_mask_to_alpha(bitmap, al_map_rgb(0,0,0));
    }

    return bitmap;
}

void GFX::Destroy(ALLEGRO_BITMAP *bitmap)
{
    al_destroy_bitmap(bitmap);
}
#endif
