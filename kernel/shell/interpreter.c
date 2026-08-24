#include "headers/commands.h"
#include "headers/interpreter.h"
#include "headers/util.h"
#include "../headers/print.h"

// splits input into a command (input is mutated, truncated at the
// first space) and returns a pointer to the trimmed argument string,
// or NULL if there is no argument.
static char* split_args(char* input)
{
    char* p = input;
    while (*p && *p != ' ') {
        p++;
    }
    if (*p != ' ') {
        return (char*)0; // no space found - no argument
    }
    *p = '\0'; // terminate the command
    p++;
    while (*p == ' ') {
        p++;
    }
    if (*p == '\0') {
        return (char*)0; // trailing whitespace only
    }
    return p;
}

// interpret shell commands
void interpret_command(char* input)
{
    char* args = split_args(input);

    if (input[0] == '\0') {
        return; // empty line
    }

    if (strcmp(input, "help") == 0) {
        command_help();
    }
    else if (strcmp(input, "clear") == 0) {
        command_clear();
    }
    else if (strcmp(input, "about") == 0) {
        command_about();
    }
    else if (strcmp(input, "halt") == 0) {
        command_halt();
    }
    else if (strcmp(input, "echo") == 0) {
        command_echo(args ? args : "");
    }
    else if (strcmp(input, "uname") == 0) {
        command_uname(args);
    }
    else if (strcmp(input, "req-syscallop") == 0) {
        command_req_syscallop(args);
    }
    else if (strcmp(input, "memtest") == 0) {
        command_memtest();
    }
    else if (strcmp(input, "ls") == 0) {
        command_ls(args);
    }
    else if (strcmp(input, "cd") == 0) {
        command_cd(args);
    }
    else if (strcmp(input, "pwd") == 0) {
        command_pwd();
    }
    else if (strcmp(input, "cat") == 0) {
        command_cat(args);
    }
    else if (strcmp(input, "meminfo") == 0) {
        command_meminfo();
    }
    else if (strcmp(input, "loop") == 0) {
        command_loop(args);
    }
    else if (strcmp(input, "diskinfo") == 0) {
        command_diskinfo();
    }
    else if (strcmp(input, "partinfo") == 0) {
        command_partinfo();
    }
    else if (strcmp(input, "mkpart") == 0) {
        command_mkpart();
    }
    else if (strcmp(input, "mkfs") == 0) {
        command_mkfs();
    }
    else if (strcmp(input, "install") == 0) {
        command_install();
    }
    else if (strcmp(input, "lsdisk") == 0) {
        command_lsdisk(args);
    }
    else if (strcmp(input, "catdisk") == 0) {
        command_catdisk(args);
    }
    else if (strcmp(input, "runelf") == 0) {
        command_runelf(args);
    }
    else {
        print_text("Unknown command: ");
        print_text(input);
        print_text("\n");
    }
}
