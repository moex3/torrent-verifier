#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "metainfo.h"
#include "verify.h"
#include "showinfo.h"
#include "opts.h"

#ifndef PROGRAM_NAME
#define PROGRAM_NAME "torrent-verify"
#endif

static_assert((sizeof(long long) >= 8), "Size of long long is less than 8, cannot compile");

void usage() {
    fprintf(stderr, "Usage: " PROGRAM_NAME " [-h | -i | -s] [-n] [-v data_path] [--] .torrent_file...\n");
    exit(EXIT_FAILURE);
}

void help() {
    printf(
"Usage:\n"
"   " PROGRAM_NAME " [options] <.torrent file>\n"
"\n"
"OPTIONS:\n"
"   -h        print this help text\n"
"   -i        show info about the torrent file\n"
"   -v PATH   verify the torrent file, pass in the path of the files\n"
"   -s        don't write any output\n"
"   -n        Don't use torrent name as a folder when verifying\n"
"\n"
"EXIT CODE\n"
"   If no error, exit code is 0. In verify mode exit code is 0 if it's\n"
"   verified correctly, otherwise non-zero\n"
#ifdef BUILD_INFO
"\n"
BUILD_HASH " (" BUILD_DATE ")\n"
#ifdef MT
"MultiThread support\n"
#endif
#endif
);
    exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {
    if (opts_parse(argc, argv) == -1)
        usage();

    if (opt_help)
        help();

    if (optind >= argc) {
        fprintf(stderr, "Provide at least one torrent file\n");
        usage();
    }
    /*
    if (data_path && optind != argc - 1) {
        fprintf(stderr, "If used with -v, only input 1 bittorrent file\n");
        usage();
    }
    */
    
    int exit_code = EXIT_SUCCESS;
    for (int i = optind; i < argc; i++) {
        metainfo_t m;
        if (metainfo_create(&m, argv[i]) == -1) {
            return EXIT_FAILURE;
        }

        if (opt_showinfo && !opt_silent) {
            showinfo(&m);
        }

        if (opt_data_path) { /* Verify */
            int verify_result = verify(&m, opt_data_path, !opt_no_use_dir);
            if (verify_result != 0) {
                if (!opt_silent)
                    printf("Torrent verify failed: %s\n", strerror(verify_result));
                exit_code = EXIT_FAILURE;
            } else {
                if (!opt_silent)
                    printf("Torrent verified successfully\n");
            }
        }

        metainfo_destroy(&m);
    }

    return exit_code;
}
