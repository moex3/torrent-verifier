#ifndef SHOWINFO_H
#define SHOWINFO_H
#include "metainfo.h"

/*
 * Print the contents of a metainfo file
 */
void showinfo(metainfo_t* m);

/*
 * Print the selected info from the metainfo file 
 * to be readable by a script
 */
void showinfo_script(metainfo_t* m);

#endif
