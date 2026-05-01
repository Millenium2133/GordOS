#include "fat32.h"
#include "ata.h"
#include "kmalloc.h"
#include "vga.h"

// Cached BPB so we don't re-read sector 0 every time
static fat32_bpb_t bpb;

// Calculated values we use constantly
static uint32_t fat_start;
static uint32_t data_start;
static uint32_t sectors_per_cluster;

// Convert cluster number to LBA address
static uint32_t cluster_to_lba(uint32_t cluster)
{
    return data_start + (cluster - 2) * sectors_per_cluster;
}

// Read the next cluster in the chain from the FAT
static uint32_t fat_next_cluster(uint32_t cluster)
{
    uint32_t fat_offset   = cluster * 4;
    uint32_t fat_sector   = fat_start + (fat_offset / bpb.bytes_per_sector);
    uint32_t entry_offset = fat_offset % bpb.bytes_per_sector;

    uint8_t sector[512];
    if (ata_read(fat_sector, 1, sector) != 0)
        return 0;

    uint32_t next = *(uint32_t*)(sector + entry_offset) & 0x0FFFFFFF;
    return next;
}

int fat32_init(void)
{
    uint8_t sector[512];
    if (ata_read(0, 1, sector) != 0)
        return -1;

    bpb = *(fat32_bpb_t*)sector;

    // Verify it's FAT32
    if (bpb.fs_type[0] != 'F' || bpb.fs_type[1] != 'A' ||
        bpb.fs_type[2] != 'T' || bpb.fs_type[3] != '3' ||
        bpb.fs_type[4] != '2')
        return -1;

    fat_start          = bpb.hidden_sectors + bpb.reserved_sectors;
    sectors_per_cluster = bpb.sectors_per_cluster;

    uint32_t fat_size  = bpb.fat_size_32;
    data_start         = fat_start + (bpb.fat_count * fat_size);

    return 0;
}

static int read_cluster(uint32_t cluster, uint8_t* buffer)
{
    uint32_t lba = cluster_to_lba(cluster);
    return ata_read(lba, sectors_per_cluster, buffer);
}

int fat32_list_dir(const char* path)
{
    (void)path;

    uint32_t cluster = bpb.root_cluster;
    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf)
        return -1;

    while (cluster < FAT32_EOC)
    {
        if (read_cluster(cluster, buf) != 0)
            break;

        uint32_t entries = (sectors_per_cluster * 512) / sizeof(fat32_entry_t);
        fat32_entry_t* entries_ptr = (fat32_entry_t*)buf;

        for (uint32_t i = 0; i < entries; i++)
        {
            fat32_entry_t* e = &entries_ptr[i];

            if (e->name[0] == 0x00)
                goto done;

            if ((uint8_t)e->name[0] == 0xE5)
                continue;

            if (e->attributes == FAT_ATTR_VOLUME_ID)
                continue;

            if (e->attributes == 0x0F)
                continue;

            // Build 8.3 filename
            char name[13];
            int n = 0;
            for (int j = 0; j < 8 && e->name[j] != ' '; j++)
                name[n++] = e->name[j];
            if (e->name[8] != ' ')
            {
                name[n++] = '.';
                for (int j = 8; j < 11 && e->name[j] != ' '; j++)
                    name[n++] = e->name[j];
            }
            name[n] = '\0';

            terminal_writestring(name);
            if (e->attributes & FAT_ATTR_DIRECTORY)
                terminal_writestring("/");
            terminal_putchar('\n');
        }

        cluster = fat_next_cluster(cluster);
    }

done:
    kfree(buf);
    return 0;
}

int fat32_read_file(const char* path, void* buffer, uint32_t* size)
{
    if (!path || !buffer || !size)
        return -1;

    // Start at root directory
    uint32_t cluster = bpb.root_cluster;
    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf)
        return -1;

    // Find the file in the root directory
    fat32_entry_t target;
    int found = 0;

    while (cluster < FAT32_EOC && !found)
    {
        if (read_cluster(cluster, buf) != 0)
            break;

        uint32_t entries = (sectors_per_cluster * 512) / sizeof(fat32_entry_t);
        fat32_entry_t* entries_ptr = (fat32_entry_t*)buf;

        for (uint32_t i = 0; i < entries; i++)
        {
            fat32_entry_t* e = &entries_ptr[i];

            if (e->name[0] == 0x00)
                goto not_found;

            if ((uint8_t)e->name[0] == 0xE5)
                continue;

            if (e->attributes == 0x0F)
                continue;

            if (e->attributes & FAT_ATTR_DIRECTORY)
                continue;

            // Build 8.3 filename to compare
            char name[13];
            int n = 0;
            for (int j = 0; j < 8 && e->name[j] != ' '; j++)
                name[n++] = e->name[j];
            if (e->name[8] != ' ')
            {
                name[n++] = '.';
                for (int j = 8; j < 11 && e->name[j] != ' '; j++)
                    name[n++] = e->name[j];
            }
            name[n] = '\0';

            // Compare with requested path
            // Simple comparison, path should be uppercase 8.3 format
            int match = 1;
            for (int j = 0; name[j] || path[j]; j++)
            {
                if (name[j] != path[j])
                {
                    match = 0;
                    break;
                }
            }

            if (match)
            {
                target = *e;
                found = 1;
                break;
            }
        }

        cluster = fat_next_cluster(cluster);
    }

not_found:
    if (!found)
    {
        kfree(buf);
        return -1;
    }

    // Read the file contents by following the cluster chain
    uint32_t file_cluster = ((uint32_t)target.cluster_high << 16) | target.cluster_low;
    uint32_t bytes_read = 0;
    uint32_t file_size  = target.file_size;
    uint8_t* out = (uint8_t*)buffer;

    while (file_cluster < FAT32_EOC && bytes_read < file_size)
    {
        if (read_cluster(file_cluster, buf) != 0)
            break;

        // How many bytes to copy from this cluster
        uint32_t cluster_bytes = sectors_per_cluster * 512;
        uint32_t remaining     = file_size - bytes_read;
        uint32_t to_copy       = remaining < cluster_bytes ? remaining : cluster_bytes;

        for (uint32_t i = 0; i < to_copy; i++)
            out[bytes_read + i] = buf[i];

        bytes_read    += to_copy;
        file_cluster   = fat_next_cluster(file_cluster);
    }

    *size = bytes_read;
    kfree(buf);
    return 0;
}
