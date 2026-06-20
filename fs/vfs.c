#include "vfs.h"

static const vfs_ops_t* g_fs = 0;

void vfs_register(const vfs_ops_t* ops) { g_fs = ops; }
int  vfs_mounted(void)                  { return g_fs != 0; }

int vfs_read_file(const char* path, void* buf, uint32_t bufsize, uint32_t* out)
{
    return g_fs ? g_fs->read_file(path, buf, bufsize, out) : -1;
}

int vfs_write_file(const char* path, const void* buf, uint32_t size)
{
    return g_fs ? g_fs->write_file(path, buf, size) : -1;
}

int vfs_delete_file(const char* path)
{
    return g_fs ? g_fs->delete_file(path) : -1;
}

int vfs_rename(const char* oldpath, const char* newpath)
{
    return g_fs ? g_fs->rename(oldpath, newpath) : -1;
}

int vfs_mkdir(const char* path)
{
    return g_fs ? g_fs->mkdir(path) : -1;
}

int vfs_list_dir(const char* path)
{
    return g_fs ? g_fs->list_dir(path) : -1;
}

int vfs_chdir(const char* path)
{
    return g_fs ? g_fs->chdir(path) : -1;
}

const char* vfs_get_cwd(void)
{
    return g_fs ? g_fs->get_cwd() : "/";
}

int vfs_lookup(const char* path, uint32_t* first_cluster, uint32_t* size)
{
    return g_fs ? g_fs->lookup(path, first_cluster, size) : -1;
}

int vfs_find_prefix(const char* prefix, char matches[][256], int max_matches)
{
    return g_fs ? g_fs->find_prefix(prefix, matches, max_matches) : 0;
}

uint32_t vfs_cluster_size(void)
{
    return g_fs ? g_fs->cluster_size() : 0;
}

uint32_t vfs_next_cluster(uint32_t cluster)
{
    return g_fs ? g_fs->next_cluster(cluster) : 0;
}

int vfs_read_cluster(uint32_t cluster, void* buf)
{
    return g_fs ? g_fs->read_cluster(cluster, buf) : -1;
}
