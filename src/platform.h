#ifndef PLATFORM_H_INCLUDED
#define PLATFORM_H_INCLUDED

#if defined(_WIN32) || defined(EOSERV_MINGW)
#ifndef WIN32
#define WIN32
#endif
#endif

#ifdef __GNUC__
#if __GNUC__ == 4 && __GNUC_MINOR__ <= 5
#define noexcept throw()
#endif
#endif

#endif
