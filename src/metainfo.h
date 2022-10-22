#ifndef METAFILE_H
#define METAFILE_H
#include <bencode.h>

/* 128 MiB */
#define MAX_TORRENT_SIZE 128*1024*1024

/* This file will parse the .torrent file and make accessor functions */

typedef struct {
    const char* str;
    int len;
} lenstr_t;

typedef struct {
    bencode_t path;
    long int size;
} fileinfo_t;

typedef struct {
    bencode_t filelist;
} fileiter_t;

typedef unsigned char sha1sum_t[20];
/*
typedef struct __attribute__((packed)) sha1sum_t {
    unsigned char hash[20];
} sha1sum_t;
*/

typedef struct {
    char* bytes;
    int bytes_size;
    
    sha1sum_t info_hash;
    const sha1sum_t* pieces;
    int piece_count;

    long int piece_length, creation_date;
    bencode_t announce, created_by, name, source, comment;

    long int is_private;
    int is_multi_file;
    union {
        long int file_size;
        bencode_t files;
    };
} metainfo_t;

/*
 * Returns 0 on success, -1 on error
 */
int metainfo_create(metainfo_t* metai, const char* path);
void metainfo_destroy(metainfo_t* metai);

/*
 * Get the info_hash of the torrent as a pointer
 */
const sha1sum_t* metainfo_infohash(metainfo_t* metai);

/*
 * Get the announce url
 */
int metainfo_announce(metainfo_t* metai, const char** str, int* len);

/*
 * Get the created by string
 */
int metainfo_created_by(metainfo_t* metai, const char** str, int* len);

/*
 * Get the creation date, as a unix timestamp
 */
int metainfo_creation_date(metainfo_t* metai);

/*
 * Get the source
 */
int metainfo_source(metainfo_t* metai, const char** str, int* len);

/*
 * Get the name of the torrent 
 */
int metainfo_name(metainfo_t* metai, const char** str, int* len);

/*
 * Get the comment of the torrent 
 */
int metainfo_comment(metainfo_t* metai, const char** str, int* len);

/*
 * Is this torrent private?
 */
int metainfo_is_private(metainfo_t* metai);

/*
 * Get the array of pieces 
 */
int metainfo_pieces(metainfo_t* metai, const sha1sum_t** piece_hash);

/*
 * Get the number of pieces, this will return the number 
 */
long int metainfo_piece_count(metainfo_t* metai);

/*
 * Get the index'th piece hash, returns -1 if invalid 
 */
int metainfo_piece_index(metainfo_t* metai, int index, \
                         const sha1sum_t** piece_hash);

/*
 * Get the size of 1 piece
 */
int metainfo_piece_size(metainfo_t* metai);

/*
 * Return 1 if the torrent has multiple files, or 0 if has only 1 
 */
int metainfo_is_multi_file(metainfo_t* metai);

/*
 * Return the number of files, if it's a multi file. This is slow 
 */
long int metainfo_file_count(metainfo_t* metai);

/*
 * Get the index'th file information, if it's a multi file
 * This is slow and shouldn't be used
 */
int metainfo_file_index(metainfo_t* metai, int index, fileinfo_t* finfo);

/*
 * Create a file iterator.
 * This interface should be used to get the file infos.
 * This doesn't need to be freed.
 */
int metainfo_fileiter_create(const metainfo_t* metai, fileiter_t* fileiter);

/*
 * Get the next fileinfo in a multi file torrent
 * Return -1 if there is no more files.
 */
int metainfo_file_next(fileiter_t* iter, fileinfo_t* finfo);

/*
 * Get the file information, if torrent has only 1 file
 */
int metainfo_fileinfo(metainfo_t* metai, fileinfo_t* finfo);

/*
 * Copy the file path from a fileinto_t struct into the buffer at 'out_str',
 * adding separators according to the current platform.
 * if 'out_str' is NULL, return the required bytes to store the name,
 * otherwise the number of bytes copied, or -1 if error.
 * The count does NOT include the null terminator
 */
int metainfo_fileinfo_path(fileinfo_t* len, char* out_str);

/*
 * Return the size of the file in bytes, ofc
 */
long int metainfo_fileinfo_size(fileinfo_t* finfo);

#endif
