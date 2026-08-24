#ifndef FAT16_H
#define FAT16_H
#include "../fs/fat16.h"

typedef struct {
    char name[9]; // trimmed, null-terminated (max 8 chars + '\0')
    char ext[4];  // trimmed, null-terminated (max 3 chars + '\0')
    unsigned char attr; // 0x10 = directory, 0x20 = regular file (archive bit)
    unsigned int cluster;
    unsigned int size;
} fat16_dirent_t;

int fat16_format(unsigned int partition_lba, unsigned int sector_count);
int fat16_mount(unsigned int partition_lba);

int fat16_mkdir_root(const char* name, const char* ext, unsigned int* out_cluster);
int fat16_create_file_root(const char* name, const char* ext, const unsigned char* data, unsigned int size);
int fat16_create_file_in_dir(unsigned int dir_cluster, const char* name, const char* ext, const unsigned char* data, unsigned int size);

// lists up to max_entries into out[]. returns the TOTAL entry count
// found (which may exceed max_entries - out[] is only filled up to
// max_entries, extra entries are still counted but not stored).
// returns -1 if not mounted.
int fat16_list_root(fat16_dirent_t* out, int max_entries);
int fat16_list_dir(unsigned int dir_cluster, fat16_dirent_t* out, int max_entries);

int fat16_find_in_root(const char* name, const char* ext, fat16_dirent_t* out);
int fat16_find_in_dir(unsigned int dir_cluster, const char* name, const char* ext, fat16_dirent_t* out);

// reads up to buffer_size bytes of a file's contents (following its
// cluster chain) into buffer. returns 1 on success, 0 on read error.
int fat16_read_file(unsigned int first_cluster, unsigned int size, unsigned char* buffer, unsigned int buffer_size);

#endif