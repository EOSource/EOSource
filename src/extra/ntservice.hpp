#ifndef EXTRA_NTSERVICE_HPP_INCLUDED
#define EXTRA_NTSERVICE_HPP_INCLUDED

#include "../platform.h"

#ifndef WIN32
#error Services are Windows only
#endif

void service_init(const char *name);
bool service_install(const char *name);
bool service_uninstall(const char *name);

#endif
