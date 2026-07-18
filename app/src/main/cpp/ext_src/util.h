#ifndef UTIL_H
#define UTIL_H

#include <ext2fs/ext2fs.h>

#ifdef __cplusplus
extern "C" {
#endif

extern char *get_progname(char *argv0);
extern void proceed_question(int delay);
extern void check_mount(const char *device, int force, const char *progname);
extern void parse_journal_opts(const char *opts);
struct ext2fs_journal_params;
extern void figure_journal_size(struct ext2fs_journal_params *jparams,
                                int size, int fc_size, ext2_filsys fs);
extern void print_check_message(int, unsigned int);
struct mmp_struct;
extern void dump_mmp_msg(struct mmp_struct *mmp, const char *msg);

#ifdef __cplusplus
}
#endif

#endif
