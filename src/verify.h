#ifndef VERIFY_H
#define VERIFY_H
#include "metainfo.h"
/* Verify torrent files here */

/*
 * Verify files inside a torrent file
 * If append folder is 1, and torrent is a multifile one,
 * the torrent's name will be appended to data_dir
 * Returns 0 if success, -num if error
 */
int verify(metainfo_t* metai, const char* data_dir, int append_folder);

#endif
