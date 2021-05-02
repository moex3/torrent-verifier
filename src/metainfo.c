#include "metainfo.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sha1.h"

/* 128 MiB */
#define MAX_TORRENT_SIZE 128*1024*1024

/* 
 * Read the file in memory, and return the pointer to it (which needs to be
 * freed) in out_contents and the size in out_size. If the file is too big,
 * fail. Returns 0 on success and an errno on fail.
 */
static int metainfo_read(const char* path, char** out_contents, int* out_size) {
    int ret = 0;
    FILE* f = NULL;
    long size;
    char* contents = NULL, *curr_contents = NULL;
    size_t read;

    f = fopen(path, "rb");
    if (!f) {
        ret = errno;
        goto end;
    }
    
    /* Get the file size, and bail if it's too large */
    if (fseek(f, 0, SEEK_END) == -1) {
        ret = errno;
        goto end;
    }

    size = ftell(f);
    if (size > MAX_TORRENT_SIZE) {
        ret = EFBIG;
        goto end;
    }
    rewind(f);

    contents = curr_contents = malloc(size);
    if (!contents) {
        ret = ENOMEM;
        goto end;
    }

    /* Read it in */
    while ((read = fread(curr_contents, 1, \
                    size - (curr_contents - contents), f)) > 0) {
        curr_contents += read;
    }
    if (ferror(f)) {
        ret = errno;
        free(curr_contents);
        curr_contents = NULL;
        goto end;
    }

end:
    if (f)
        fclose(f);

    *out_size = size;
    *out_contents = contents;
    return ret;
}

static int len_strcmp(const char* s1, int s1_len, const char* s2, int s2_len) {
    return (s1_len == s2_len) && (strncmp(s1, s2, s1_len) == 0);
}

static int read_benc_int_into(bencode_t* benc, long int* val, int def) {
    if (!bencode_int_value(benc, val)) {
        *val = def;
        return -1;
    }
    return 0;
}

#define tkey(s) len_strcmp(key, klen, s, strlen(s))
#define ttype(t) bencode_is_##t(&item)


static int metainfo_hash_info(metainfo_t* metai, bencode_t* info) {
    const char* info_start;
    int info_len;
    /* Almost as if this function was made for this, lol */
    bencode_dict_get_start_and_len(info, &info_start, &info_len);
    
    SHA1_CTX ctx;
    SHA1Init(&ctx);
    SHA1Update(&ctx, (const unsigned char*)info_start, info_len);
    SHA1Final((unsigned char*)&metai->info_hash, &ctx);
    return 0;
}

static int metainfo_parse_info(metainfo_t* metai, bencode_t* benc) {
    
    metainfo_hash_info(metai, benc);
    while (bencode_dict_has_next(benc)) {
        const char* key;
        int klen;
        bencode_t item;
        bencode_dict_get_next(benc, &item, &key, &klen);
        
        if (tkey("name") && ttype(string)) {
            metai->name = item;
        } else if (tkey("piece length") && ttype(int)) {
            read_benc_int_into(&item, &metai->piece_length, -1);
        } else if (tkey("pieces") && ttype(string)) {
            int plen = -1;
            bencode_string_value(&item, (const char**)&metai->pieces, &plen);
            /* A piece hash is always 20 bytes */
            if (plen % sizeof(sha1sum_t) != 0) {
                fprintf(stderr, "A piece hash is not 20 bytes somewhere\n" \
                        "\tHash string length is: %d\n", plen);
                /* This should never happen tho */
                plen -= plen % sizeof(sha1sum_t);
            }
            metai->piece_count = plen / sizeof(sha1sum_t);
        } else if (tkey("length") && ttype(int)) { /* If single file */
            metai->is_multi_file = 0;
            read_benc_int_into(&item, &metai->file_size, -1);
        } else if (tkey("files") && ttype(list)) { /* If multiple files */
            metai->is_multi_file = 1;
            metai->files = item;
        } else if (tkey("private") && ttype(int)) {
            read_benc_int_into(&item, &metai->is_private, 0);
        } else if (tkey("source") && ttype(string)) {
            metai->source = item;
        } else {
            fprintf(stderr, "Unknown key in info dict: %.*s\n", klen, key);
        }
    }

    return 0;
}

static int metainfo_parse(metainfo_t* metai, bencode_t* benc) {
    int ret = 0;

    /* All .torrent files are dictionaries, so this is an error */
    if (!bencode_is_dict(benc)) {
        metainfo_destroy(metai);
        fprintf(stderr, "File is not a valid .torrent file\n");
        return -1;
    }

    while (bencode_dict_has_next(benc)) {
        const char* key;
        int klen;
        bencode_t item;
        bencode_dict_get_next(benc, &item, &key, &klen);
        
        if (tkey("announce")) {
            metai->announce = item;
        } else if (tkey("created by") && ttype(string)) {
            metai->created_by = item;
        } else if (tkey("creation date") && ttype(int)) {
            read_benc_int_into(&item, &metai->creation_date, -1);
        } else if (tkey("info") && ttype(dict)) {
            metainfo_parse_info(metai, &item);
        } else if (tkey("comment") && ttype(string)) {
            metai->comment = item;
        } else {
            fprintf(stderr, "Unknown dict key: %.*s\n", klen, key);
        }
    }

    return ret;
}

int metainfo_create(metainfo_t* metai, const char* path) {
    char* bytes;
    int size;
    int ret = metainfo_read(path, &bytes, &size);
    if (ret) {
        fprintf(stderr, "Metafile reading failed: %s\n", \
                strerror(ret));
        return -1;
    }

    memset(metai, 0, sizeof(metainfo_t));
    metai->bytes = bytes;
    metai->bytes_size = size;

    bencode_t benc;
    bencode_init(&benc, bytes, size);
    
    if (metainfo_parse(metai, &benc) == -1) {
        metainfo_destroy(metai);
        fprintf(stderr, "Can't parse metainfo file\n");
        return -1;
    }

    return 0;
}

void metainfo_destroy(metainfo_t* metai) {
    if (metai->bytes) {
        free(metai->bytes);
        metai->bytes = NULL;
    }
}

const sha1sum_t* metainfo_infohash(metainfo_t* metai) {
    return &metai->info_hash;
}

static int metainfo_get_string(bencode_t* benc, const char** str, int* len) {
    if (!benc->start)
        return -1;
    return bencode_string_value(benc, str, len) - 1;
}

int metainfo_announce(metainfo_t* metai, const char** str, int* len) {
    return metainfo_get_string(&metai->announce, str, len);
}

int metainfo_created_by(metainfo_t* metai, const char** str, int* len) {
    return metainfo_get_string(&metai->created_by, str, len);
}

int metainfo_creation_date(metainfo_t* metai) {
    return metai->creation_date;
}

int metainfo_source(metainfo_t* metai, const char** str, int* len) {
    return metainfo_get_string(&metai->source, str, len);
}

int metainfo_is_private(metainfo_t* metai) {
    return metai->is_private;
}

int metainfo_name(metainfo_t* metai, const char** str, int* len) {
    return metainfo_get_string(&metai->name, str, len);
}

int metainfo_comment(metainfo_t* metai, const char** str, int* len) {
    return metainfo_get_string(&metai->comment, str, len);
}

int metainfo_pieces(metainfo_t* metai, const sha1sum_t** piece_hash) {
    if (!metai->pieces)
        return -1;
    *piece_hash = metai->pieces;
    return 0;
}

int metainfo_piece_index(metainfo_t* metai, int index, \
                         const sha1sum_t** piece_hash) {
    if (!metai->pieces || index >= metai->piece_count)
        return -1;
    *piece_hash = metai->pieces + index;
    return 0;
}

int metainfo_piece_size(metainfo_t* metai) {
    return metai->piece_length;
}

long int metainfo_piece_count(metainfo_t* metai) {
    return metai->piece_count;
}

int metainfo_is_multi_file(metainfo_t* metai) {
    return metai->is_multi_file;
}

long int metainfo_file_count(metainfo_t* metai) {
    if (!(metai->is_multi_file && bencode_is_list(&metai->files)))
        return 0;
    long int count = 0;
    bencode_t iterb = metai->files;
    while (bencode_list_get_next(&iterb, NULL) != 0)
        count++;
    return count;
}

static int metainfo_file_dict2fileinfo(bencode_t* f_dict, fileinfo_t* finfo) {
    int has_path = 0, has_size = 0;
    while (bencode_dict_has_next(f_dict) && (!has_path || !has_size)) {
        const char* key;
        int klen;
        bencode_t item;
        bencode_dict_get_next(f_dict, &item, &key, &klen);

        if (tkey("length") && ttype(int)) {
            has_size = 1;
            bencode_int_value(&item, &finfo->size);
        } else if (tkey("path") && ttype(list)) {
            has_path = 1;
            finfo->path = item;
        } else {
            fprintf(stderr, "Unknown key in files dict: %*.s\n", klen, key);
        }
    }
    return (has_path && has_size) ? 0 : -1;
}

int metainfo_file_index(metainfo_t* metai, int index, fileinfo_t* finfo) {
    if (!(metai->is_multi_file && bencode_is_list(&metai->files)))
        return -1;
    bencode_t iterb = metai->files;
    while (index-- && bencode_list_get_next(&iterb, NULL) != 0);
    
    if (!bencode_is_dict(&iterb))
        return -1;
        
    return metainfo_file_dict2fileinfo(&iterb, finfo);
}

int metainfo_fileiter_create(const metainfo_t* metai, fileiter_t* fileiter) {
    if (!metai->is_multi_file || !bencode_is_list(&metai->files))
        return -1;
    fileiter->filelist = metai->files;
    return 0;
}

int metainfo_file_next(fileiter_t* iter, fileinfo_t* finfo) {
    if (!bencode_list_has_next(&iter->filelist))
        return -1;
    bencode_t f_dict;
    bencode_list_get_next(&iter->filelist, &f_dict);
    return metainfo_file_dict2fileinfo(&f_dict, finfo);
}

int metainfo_fileinfo(metainfo_t* metai, fileinfo_t* finfo) {
    if (metai->is_multi_file)
        return -1;

    finfo->size = metai->file_size;
    /* In the case of single files, the name is the filename */
    finfo->path = metai->name;
    return 0;
}

#ifdef _WIN32
    #define PATH_SEP '\\'
#else
    #define PATH_SEP '/'
#endif

int metainfo_fileinfo_path(fileinfo_t* finfo, char* out_str) {
    int count = 0;

    if (bencode_is_list(&finfo->path)) {
        bencode_t local_copy = finfo->path;
        bencode_t item;
        while (bencode_list_has_next(&local_copy)) {
            /* If not in the first iter, append separator */
            if (count > 0) {
                if (out_str)
                    *out_str++ = PATH_SEP;
                count++;
            }

            bencode_list_get_next(&local_copy, &item);
            
            int slen;
            const char* s;
            bencode_string_value(&item, &s, &slen);

            count += slen;
            if (out_str) {
                memcpy(out_str, s, slen);
                out_str += slen;
            }
        }
    } else {
        /* Single file, we shouldn't even copy here, but it's easier... */
        int slen;
        const char* s;
        bencode_string_value(&finfo->path, &s, &slen);
        if (out_str) {
            memcpy(out_str, s, slen);
        }
        count = slen;
    }

    return count;
}

long int metainfo_fileinfo_size(fileinfo_t* finfo) {
    return finfo->size;
}

#undef tkey
#undef ttype

