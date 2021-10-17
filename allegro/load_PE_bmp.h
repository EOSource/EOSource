#ifndef __LOAD_PE_BMP_HPP_INCLUDED
#define __LOAD_PE_BMP_HPP_INCLUDED

/**
# Requires - gdi32
*/

#include <allegro5/allegro.h>

#include <algorithm>
#include <string>
#include <cstring>

ALLEGRO_BITMAP* load_PE_bmp(std::string filename, int resid);

#endif
