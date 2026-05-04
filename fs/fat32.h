#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

#define FAT32_EOC       0x0FFFFFF8
#define FAT32_FREE      0x00000000

#define FAT_ATTR_READONLY   0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20

int fat32_init(void);
int fat32_read_file(const char* path, void* buffer, uint32_t* size);
int fat32_list_dir(const char* path);
int fat32_write_file(const char* path, const void* buffer, uint32_t size);
int fat32_delete_file(const char* path);
int fat32_rename_file(const char* oldpath, const char* newpath);

#endif
