#include "opts.h"
#include <unistd.h>
#include <string.h>

int opt_silent = 0;
int opt_showinfo = 0;
int opt_help = 0;
int opt_no_use_dir = 0;
int opt_pretty_progress = 0;
int opt_scriptformat_info = OPT_SCRIPTFORMAT_NONE;
char* opt_data_path = NULL;

int opts_parse(int argc, char** argv) {
    int opt;

    while ((opt = getopt(argc, argv, "pnihsv:f:")) != -1) {
        switch (opt) {
            case 'i':
                opt_showinfo = 1;
                break;
            case 'h':
                opt_help = 1;
                break;
            case 's':
                opt_silent = 1;
                break;
            case 'n':
                opt_no_use_dir = 1;
                break;
            case 'p':
                opt_pretty_progress = 1;
                break;
            case 'v':
                opt_data_path = optarg;
                break;
            case 'f':
                if (strlen(optarg) != 1)
                    return -1;
                char info_char = optarg[0];
                opt_scriptformat_info = OPT_SCRIPTFORMAT_INVALID;
                for (int i = 0; i < OPT_SCRIPTFORMAT_MAPPING_LEN; i++) {
                    const opt_scriptformat_mapping_t* curr = &OPT_SCRIPTFORMAT_MAPPING[i];
                    if (info_char == curr->info_char) {
                        opt_scriptformat_info = curr->info;
                        break;
                    }
                }
                if (opt_scriptformat_info == OPT_SCRIPTFORMAT_INVALID)
                    return -1;
                break;
            default:
                return -1;
        }
    }
    return 0;
}
