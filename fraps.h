#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#ifndef _MAIN_H
#define _MAIN_H

extern bool dllLoaded;

char *GetDirectoryFile(char *filename);
void __cdecl add_log (const char * fmt, ...);

#endif
