#ifndef OPTS_H
#define OPTS_H

extern int opt_silent;
extern int opt_showinfo;
extern int opt_help;
extern int opt_no_use_dir;
extern int opt_pretty_progress;
extern char* opt_data_path;

/* Parse the given arguments. Return -1 if error */
int opts_parse(int argc, char** argv);

#endif
