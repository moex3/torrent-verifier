#ifndef OPTS_H
#define OPTS_H

enum OPT_SCRIPTFORMAT {
    OPT_SCRIPTFORMAT_INVALID = -1,
    OPT_SCRIPTFORMAT_NONE,
    OPT_SCRIPTFORMAT_INFOHASH,
};

typedef struct {
    char info_char;
    enum OPT_SCRIPTFORMAT info;
} opt_scriptformat_mapping_t;

const static opt_scriptformat_mapping_t OPT_SCRIPTFORMAT_MAPPING[] = {
    { .info_char = 'i', .info = OPT_SCRIPTFORMAT_INFOHASH },
};
#define OPT_SCRIPTFORMAT_MAPPING_LEN sizeof(OPT_SCRIPTFORMAT_MAPPING)/sizeof(OPT_SCRIPTFORMAT_MAPPING[0])


extern int opt_silent;
extern int opt_showinfo;
extern int opt_help;
extern int opt_no_use_dir;
extern int opt_pretty_progress;
extern int opt_scriptformat_info;
extern char* opt_data_path;

/* Parse the given arguments. Return -1 if error */
int opts_parse(int argc, char** argv);

#endif
