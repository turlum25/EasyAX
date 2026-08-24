#ifndef COMMANDS_H
#define COMMANDS_H

void command_help(void);
void command_clear(void);
void command_about(void);
void command_echo(char* args);
void command_halt(void);
void command_uname(char* option);
void command_req_syscallop(char* arg);
void command_memtest(void);
void command_meminfo(void);
void command_ls(char* arg);
void command_cd(char* arg);
void command_pwd(void);
void command_cat(char* arg);
void command_loop(char* arg);
void command_diskinfo(void);
void command_partinfo(void);
void command_mkpart(void);
void command_mkfs(void);
void command_install(void);
void command_lsdisk(char* arg);
void command_catdisk(char* arg);
void command_runelf(char* arg);
#endif
