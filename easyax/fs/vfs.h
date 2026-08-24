#ifndef VFS_H
#define VFS_H

typedef enum {
    VFS_DIR,
    VFS_FILE
} vfs_type_t;

#define VFS_MAX_NAME      32
#define VFS_MAX_CHILDREN  16
#define VFS_MAX_FILE_SIZE 256

typedef struct vfs_node {
    char name[VFS_MAX_NAME];
    vfs_type_t type;
    struct vfs_node* parent;
    struct vfs_node* children[VFS_MAX_CHILDREN];
    unsigned int child_count;
    char content[VFS_MAX_FILE_SIZE]; // only used when type == VFS_FILE
} vfs_node_t;

// builds the initial tree: / , /kernel , /shell (+ a README file in each)
void vfs_init(void);

vfs_node_t* vfs_get_root(void);
vfs_node_t* vfs_get_cwd(void);
void vfs_set_cwd(vfs_node_t* node);

vfs_node_t* vfs_mkdir(vfs_node_t* parent, const char* name);
vfs_node_t* vfs_create_file(vfs_node_t* parent, const char* name, const char* content);
vfs_node_t* vfs_find_child(vfs_node_t* parent, const char* name);

// prints the current working directory's absolute path (no newline)
void vfs_print_cwd_path(void);

#endif
