#include "headers/commands.h"
#include "headers/util.h"
#include "../headers/print.h"
#include "../fs/vfs.h"

// command: ls [path]
// with no argument, lists the current directory. Only supports a
// single path component (a child name, "/", or "..") for now - no
// multi-segment paths like "kernel/sub" yet.
void command_ls(char* arg)
{
    vfs_node_t* target = vfs_get_cwd();

    if (arg != (char*)0) {
        if (strcmp(arg, "/") == 0) {
            target = vfs_get_root();
        } else if (strcmp(arg, "..") == 0) {
            target = target->parent != (vfs_node_t*)0 ? target->parent : target;
        } else {
            vfs_node_t* found = vfs_find_child(target, arg);
            if (found == (vfs_node_t*)0 || found->type != VFS_DIR) {
                print_text("ls: not a directory: ");
                print_text(arg);
                print_text("\n");
                return;
            }
            target = found;
        }
    }

    if (target->child_count == 0) {
        print_text("(empty)\n");
        return;
    }

    for (unsigned int i = 0; i < target->child_count; i++) {
        vfs_node_t* child = target->children[i];
        print_text(child->name);
        if (child->type == VFS_DIR) {
            print_text("/");
        }
        print_text("\n");
    }
}



// command: cd [path]
// no argument -> root. supports a single child name, "/", or ".."
void command_cd(char* arg)
{
    if (arg == (char*)0 || strcmp(arg, "/") == 0) {
        vfs_set_cwd(vfs_get_root());
        return;
    }

    if (strcmp(arg, "..") == 0) {
        vfs_node_t* cwd = vfs_get_cwd();
        if (cwd->parent != (vfs_node_t*)0) {
            vfs_set_cwd(cwd->parent);
        }
        return;
    }

    vfs_node_t* found = vfs_find_child(vfs_get_cwd(), arg);

    if (found == (vfs_node_t*)0 || found->type != VFS_DIR) {
        print_text("cd: no such directory: ");
        print_text(arg);
        print_text("\n");
        return;
    }

    vfs_set_cwd(found);
}



// command: pwd
void command_pwd(void)
{
    vfs_print_cwd_path();
    print_text("\n");
}



// command: cat <file>
void command_cat(char* arg)
{
    if (arg == (char*)0) {
        print_text("Usage: cat <file>\n");
        return;
    }

    vfs_node_t* found = vfs_find_child(vfs_get_cwd(), arg);

    if (found == (vfs_node_t*)0 || found->type != VFS_FILE) {
        print_text("cat: no such file: ");
        print_text(arg);
        print_text("\n");
        return;
    }

    print_text(found->content);
    print_text("\n");
}
