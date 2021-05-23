#include <string.h>
#include <stdio.h>
#include <time.h>
#include "showinfo.h"
#include "util.h"
#include "opts.h"

void showinfo(metainfo_t* m) {
    const char* s;
    int slen;
    long int lint;
    char str_buff[100] = {0};

    if (metainfo_name(m, &s, &slen) != 0) {
        s = "[UNKNOWN (>_<)]";
        slen = strlen("[UNKNOWN (>_<)]");
    }
    printf("Name: %.*s\n", slen, s);

    util_byte2hex((const unsigned char*)metainfo_infohash(m), sizeof(sha1sum_t), 0, str_buff);
    printf("Info hash: %s\n", str_buff);

    lint = metainfo_piece_count(m);
    printf("Piece count: %ld\n", lint);

    lint = metainfo_piece_size(m);
    if (util_byte2human(lint, 1, 0, str_buff, sizeof(str_buff)) == -1)
        strncpy(str_buff, "err", sizeof(str_buff));
    printf("Piece size: %ld (%s)\n", lint, str_buff);

    printf("Is multi file: %s\n", metainfo_is_multi_file(m) ? "Yes" : "No");
    if (metainfo_is_multi_file(m)) {
        printf("File count is: %ld\n", metainfo_file_count(m));
    }

    printf("Is private: %s\n", metainfo_is_private(m) ? "Yes" : "No");

    if (metainfo_announce(m, &s, &slen) != -1) {
        printf("Tracker: %.*s\n", slen, s);
    }

    lint = metainfo_creation_date(m);
    if (lint != -1) {
        struct tm* time = localtime(&lint);
        if (strftime(str_buff, sizeof(str_buff), "%F %T", time) == 0)
            strncpy(str_buff, "overflow", sizeof(str_buff));
        printf("Creation date: %s\n", str_buff);
    }

    if (metainfo_created_by(m, &s, &slen) != -1) {
        printf("Created by: %.*s\n", slen, s);
    }

    if (metainfo_source(m, &s, &slen) != -1) {
        printf("Source: %.*s\n", slen, s);
    }

    printf("Files:\n");

    unsigned long total_size = 0;
    fileinfo_t f;
    if (metainfo_is_multi_file(m)) {
        fileiter_t fi;
        if (metainfo_fileiter_create(m, &fi) == 0) {
            while (metainfo_file_next(&fi, &f) == 0) {
                int pathlen = metainfo_fileinfo_path(&f, NULL);
                char pathbuff[pathlen];
                metainfo_fileinfo_path(&f, pathbuff);

		        if (util_byte2human(f.size, 1, -1, str_buff, sizeof(str_buff)) == -1) {
                    strncpy(str_buff, "err", sizeof(str_buff));
                }

                printf("\t%8s %.*s\n", str_buff, pathlen, pathbuff);

                total_size += metainfo_fileinfo_size(&f);
            }
        }
    } else {
        metainfo_fileinfo(m, &f);
        int pathlen = metainfo_fileinfo_path(&f, NULL);
        char pathbuff[pathlen];
        metainfo_fileinfo_path(&f, pathbuff);


        if (util_byte2human(f.size, 1, -1, str_buff, sizeof(str_buff)) == -1) {
            strncpy(str_buff, "err", sizeof(str_buff));
        }

        printf("\t%8s %.*s\n", str_buff, pathlen, pathbuff);

        total_size = metainfo_fileinfo_size(&f);
    }

    if (util_byte2human(total_size, 1, -1, str_buff, sizeof(str_buff)) == -1)
        strncpy(str_buff, "err", sizeof(str_buff));
    printf("Total size: %s\n", str_buff);
}

void showinfo_script(metainfo_t* m) {
    switch (opt_scriptformat_info) {
    case OPT_SCRIPTFORMAT_INFOHASH:
    {
        char hex_str[sizeof(sha1sum_t)*2+1];
        const sha1sum_t* infohash = metainfo_infohash(m);
        util_byte2hex((const unsigned char*)infohash, sizeof(sha1sum_t), 0, hex_str);
        printf("%s\n", hex_str);
        break;
    }
    default:
        printf("Unknown?? [>.<]\n");
        break;
    }
}

