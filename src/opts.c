#include "opts.h"
#include <unistd.h>

int opt_silent = 0;
int opt_showinfo = 0;
int opt_help = 0;
int opt_no_use_dir = 0;
int opt_pretty_progress = 0;
char* opt_data_path = NULL;

int opts_parse(int argc, char** argv) {
    int opt;

    while ((opt = getopt(argc, argv, "pnihsv:")) != -1) {
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
            default:
                return -1;
        }
    }
    return 0;
}
