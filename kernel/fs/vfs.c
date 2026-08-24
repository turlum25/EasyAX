#include "vfs.h"
#include "../headers/print.h"
#include "../mm/heap.h"

// Static node pool - this is an in-RAM filesystem with no backing
// storage and no allocator, so every node lives here for the life
// of the kernel.

static vfs_node_t* vfs_root = (vfs_node_t*)0;
static vfs_node_t* vfs_cwd  = (vfs_node_t*)0;

static void name_copy(char* dst, const char* src, unsigned int max)
{
    unsigned int i = 0;
    while (src[i] != '\0' && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

// deliberately not sharing shell's strcmp - fs shouldn't depend on shell
static int str_eq(const char* a, const char* b)
{
    unsigned int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == b[i];
}

static vfs_node_t* vfs_alloc_node(const char* name, vfs_type_t type, vfs_node_t* parent)
{
    vfs_node_t* node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (node == (vfs_node_t*)0) {
        return (vfs_node_t*)0; // heap exhausted
    }

    name_copy(node->name, name, VFS_MAX_NAME);
    node->type = type;
    node->parent = parent;
    node->child_count = 0;
    node->content[0] = '\0';

    if (parent != (vfs_node_t*)0 && parent->child_count < VFS_MAX_CHILDREN) {
        parent->children[parent->child_count++] = node;
    }

    return node;
}

vfs_node_t* vfs_mkdir(vfs_node_t* parent, const char* name)
{
    return vfs_alloc_node(name, VFS_DIR, parent);
}

vfs_node_t* vfs_create_file(vfs_node_t* parent, const char* name, const char* content)
{
    vfs_node_t* node = vfs_alloc_node(name, VFS_FILE, parent);

    if (node != (vfs_node_t*)0 && content != (const char*)0) {
        name_copy(node->content, content, VFS_MAX_FILE_SIZE);
    }

    return node;
}

vfs_node_t* vfs_find_child(vfs_node_t* parent, const char* name)
{
    for (unsigned int i = 0; i < parent->child_count; i++) {
        if (str_eq(parent->children[i]->name, name)) {
            return parent->children[i];
        }
    }

    return (vfs_node_t*)0;
}

void vfs_init(void)
{

    vfs_root = vfs_alloc_node("/", VFS_DIR, (vfs_node_t*)0);

    vfs_node_t* kernel_dir = vfs_mkdir(vfs_root, "kernel");
    vfs_node_t* shell_dir  = vfs_mkdir(vfs_root, "shell");

    vfs_create_file(kernel_dir, "README",
        "AlderKernel in-RAM VFS.\nThis directory represents kernel space.");

    vfs_create_file(shell_dir, "README",
        "zSlash shell space.\nAdd shell-related files here as they show up.");

    vfs_cwd = vfs_root;
}

vfs_node_t* vfs_get_root(void) { return vfs_root; }
vfs_node_t* vfs_get_cwd(void)  { return vfs_cwd; }
void vfs_set_cwd(vfs_node_t* node) { vfs_cwd = node; }

static void print_path_recursive(vfs_node_t* node)
{
    if (node->parent == (vfs_node_t*)0) {
        return; // reached root - it contributes nothing but the leading "/"
    }

    print_path_recursive(node->parent);
    print_text("/");
    print_text(node->name);
}

void vfs_print_cwd_path(void)
{
    if (vfs_cwd == vfs_root) {
        print_text("/");
        return;
    }

    print_path_recursive(vfs_cwd);
}
