#ifndef VFS_H
#define VFS_H

#include <stdint.h>

// Sentinel returned by vfs_next_cluster() at end-of-chain.
// Matches the FAT32 EOC range; a future driver would map its own EOC here.
#define VFS_CLUSTER_EOC 0x0FFFFFF8

// Abstract filesystem operations.  A driver fills one of these and passes
// it to vfs_register(); all kernel code then calls through vfs_*.
typedef struct
{
    int         (*read_file)   (const char* path, void* buf,
                                uint32_t bufsize, uint32_t* out_size);
    int         (*write_file)  (const char* path, const void* buf, uint32_t size);
    int         (*delete_file) (const char* path);
    int         (*rename)      (const char* oldpath, const char* newpath);
    int         (*mkdir)       (const char* path);
    int         (*list_dir)    (const char* path);
    int         (*chdir)       (const char* path);
    const char* (*get_cwd)     (void);
    int         (*lookup)      (const char* path,
                                uint32_t* first_cluster, uint32_t* size);
    int         (*find_prefix) (const char* prefix,
                                char matches[][256], int max_matches);
    uint32_t    (*cluster_size)(void);
    uint32_t    (*next_cluster)(uint32_t cluster);
    int         (*read_cluster)(uint32_t cluster, void* buf);
} vfs_ops_t;

// Register the active filesystem driver.  Only one driver is supported at
// a time; a second call replaces the previous one.
void vfs_register(const vfs_ops_t* ops);

// Returns 1 if a filesystem has been registered, 0 otherwise.
int vfs_mounted(void);

// Forwarding calls — each returns -1 / 0 / NULL if no fs is mounted.
int         vfs_read_file   (const char* path, void* buf,
                             uint32_t bufsize, uint32_t* out_size);
int         vfs_write_file  (const char* path, const void* buf, uint32_t size);
int         vfs_delete_file (const char* path);
int         vfs_rename      (const char* oldpath, const char* newpath);
int         vfs_mkdir       (const char* path);
int         vfs_list_dir    (const char* path);
int         vfs_chdir       (const char* path);
const char* vfs_get_cwd     (void);
int         vfs_lookup      (const char* path,
                             uint32_t* first_cluster, uint32_t* size);
int         vfs_find_prefix (const char* prefix,
                             char matches[][256], int max_matches);
uint32_t    vfs_cluster_size(void);
uint32_t    vfs_next_cluster(uint32_t cluster);
int         vfs_read_cluster(uint32_t cluster, void* buf);

#endif
