#include "fat32.h"
#include "vfs.h"
#include "ata.h"
#include "kmalloc.h"
#include "vga.h"

// Raw sector buffer for BPB
static uint8_t boot_sector[512];

// Fields extracted from BPB
static uint16_t bytes_per_sector;
static uint8_t  sectors_per_cluster;
static uint16_t reserved_sectors;
static uint8_t  fat_count;
static uint32_t total_sectors;
static uint32_t fat_size;
static uint32_t root_cluster;

// Calculated
static uint32_t fat_start;
static uint32_t data_start;

// Current working directory
static uint32_t cwd_cluster;
static char cwd_path[256];

// Helper to read a uint16 from a byte array at offset
static uint16_t read16(uint8_t* buf, int offset)
{
    return (uint16_t)(buf[offset] | (buf[offset + 1] << 8));
}

// Helper to read a uint32 from a byte array at offset
static uint32_t read32(uint8_t* buf, int offset)
{
    return (uint32_t)(buf[offset]       |
           (buf[offset + 1] << 8)  |
           (buf[offset + 2] << 16) |
           (buf[offset + 3] << 24));
}

// Convert a character to uppercase
static char to_upper(char c)
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    return c;
}

// Forward declarations
static int fat32_create_entry(const char* name, uint32_t dir_cluster,
                              uint32_t cluster, uint32_t size, uint8_t attr);
static int read_cluster(uint32_t cluster, uint8_t* buffer);

// Convert cluster number to LBA address
static uint32_t cluster_to_lba(uint32_t cluster)
{
    if (cluster < 2)
        return 0xFFFFFFFF;
    return data_start + (cluster - 2) * sectors_per_cluster;
}

// Read the next cluster in the chain from the FAT
static uint32_t fat_next_cluster(uint32_t cluster)
{
    uint32_t fat_offset   = cluster * 4;
    uint32_t fat_sector   = fat_start + (fat_offset / bytes_per_sector);
    uint32_t entry_offset = fat_offset % bytes_per_sector;

    uint8_t sector[512];
    if (ata_read(fat_sector, 1, sector) != 0)
        return 0;

    return read32(sector, entry_offset) & 0x0FFFFFFF;
}

// Write a value into the FAT at a given cluster entry
static int fat_set_cluster(uint32_t cluster, uint32_t value)
{
    uint32_t fat_offset   = cluster * 4;
    uint32_t fat_sector   = fat_start + (fat_offset / bytes_per_sector);
    uint32_t entry_offset = fat_offset % bytes_per_sector;

    uint8_t sector[512];
    if (ata_read(fat_sector, 1, sector) != 0)
        return -1;

    uint32_t existing = read32(sector, entry_offset);
    uint32_t newval   = (existing & 0xF0000000) | (value & 0x0FFFFFFF);

    sector[entry_offset + 0] = (uint8_t)(newval & 0xFF);
    sector[entry_offset + 1] = (uint8_t)((newval >> 8) & 0xFF);
    sector[entry_offset + 2] = (uint8_t)((newval >> 16) & 0xFF);
    sector[entry_offset + 3] = (uint8_t)((newval >> 24) & 0xFF);

    if (ata_write(fat_sector, 1, sector) != 0)
        return -1;

    return 0;
}

// Free an entire cluster chain in the FAT
static void fat_free_chain(uint32_t cluster)
{
    while (cluster >= 2 && cluster < FAT32_EOC)
    {
        uint32_t next = fat_next_cluster(cluster);
        fat_set_cluster(cluster, FAT32_FREE);
        cluster = next;
    }
}

// Find a free cluster and allocate it
static uint32_t fat_alloc_cluster(void)
{
    uint32_t total_clusters = (total_sectors - data_start) / sectors_per_cluster;

    for (uint32_t cluster = 2; cluster < total_clusters + 2; cluster++)
    {
        uint32_t val = fat_next_cluster(cluster);
        if (val == FAT32_FREE)
        {
            if (fat_set_cluster(cluster, 0x0FFFFFFF) != 0)
                return 0;
            return cluster;
        }
    }

    return 0;
}

static int read_cluster(uint32_t cluster, uint8_t* buffer)
{
    uint32_t lba = cluster_to_lba(cluster);
    return ata_read(lba, sectors_per_cluster, buffer);
}

static int write_cluster(uint32_t cluster, uint8_t* buffer)
{
    if (cluster < 2)
        return -1;
    uint32_t lba = cluster_to_lba(cluster);
    return ata_write(lba, sectors_per_cluster, buffer);
}

// +--------------------------+
// | LFN helper functions     |
// +--------------------------+

// Byte offsets within a 32-byte LFN directory entry for the 13 name chars
static const int LFN_CHAR_OFFSETS[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};

// Compute the 8.3 SFN checksum for LFN entry verification
static uint8_t sfn_checksum(const uint8_t* sfn)
{
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++)
        sum = (uint8_t)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + sfn[i]);
    return sum;
}

// Extract the display name from an SFN entry into out[] (null-terminated, <= 12+nul)
static void sfn_to_name(const uint8_t* e, char* out)
{
    int n = 0, j;
    for (j = 0; j < 8 && e[j] != ' '; j++)
        out[n++] = (char)e[j];
    if (e[8] != ' ')
    {
        out[n++] = '.';
        for (j = 8; j < 11 && e[j] != ' '; j++)
            out[n++] = (char)e[j];
    }
    out[n] = '\0';
}

// Accumulate one LFN entry into lfn_buf[256] and update the remaining count.
// The 0x40 flag on the first-seen entry (highest seq) resets the buffer and
// sets *checksum.  The caller should decrement *remain after each call and
// treat the LFN as complete when *remain reaches 0.
static void lfn_accumulate(const uint8_t* e, char* lfn_buf, uint8_t* checksum, int* remain)
{
    if (e[0] & 0x40)
    {
        for (int i = 0; i < 256; i++) lfn_buf[i] = '\0';
        *checksum = e[13];
        *remain   = e[0] & 0x3F;
    }
    int seq  = e[0] & 0x3F;
    int base = (seq - 1) * 13;
    for (int k = 0; k < 13 && base + k < 255; k++)
    {
        uint16_t ch = (uint16_t)e[LFN_CHAR_OFFSETS[k]] |
                      ((uint16_t)e[LFN_CHAR_OFFSETS[k] + 1] << 8);
        if (ch == 0x0000 || ch == 0xFFFF)
            break;
        lfn_buf[base + k] = (char)(ch < 0x80 ? ch : '?');
    }
    if (*remain > 0)
        (*remain)--;
}

// Case-insensitive string compare (returns 1 if equal, 0 otherwise)
static int name_match(const char* a, const char* b)
{
    int i;
    for (i = 0; a[i] || b[i]; i++)
    {
        if (to_upper(a[i]) != to_upper(b[i]))
            return 0;
    }
    return 1;
}

// Return 1 if name fits cleanly in 8.3 (no LFN entries needed).
// Lowercase letters are not valid in SFN and must force LFN.
static int is_valid_83(const char* name)
{
    int base = 0, dot = 0, ext = 0;
    for (int i = 0; name[i]; i++)
    {
        char c = name[i];
        if (c >= 'a' && c <= 'z') return 0;
        if (c == '.')
        {
            if (dot) return 0;
            if (base == 0) return 0;
            dot = 1;
        }
        else if (dot)
        {
            ext++;
            if (ext > 3) return 0;
        }
        else
        {
            base++;
            if (base > 8) return 0;
        }
    }
    return base > 0;
}

// Generate an 11-byte space-padded uppercase SFN from a long name.
// Produces BASEN~1.EXT style for names that don't fit in 8.3.
static void make_sfn(const char* name, uint8_t sfn[11])
{
    int i;
    for (i = 0; i < 11; i++) sfn[i] = ' ';

    // Find the last dot to split base and extension
    int dot = -1, len = 0;
    for (i = 0; name[i]; i++) { if (name[i] == '.') dot = i; len++; }

    int base_end = (dot >= 0) ? dot : len;

    // Copy up to 6 base chars (valid FAT chars only), then append ~1
    int j = 0;
    for (i = 0; i < base_end && j < 6; i++)
    {
        char c = to_upper(name[i]);
        if (c == ' ' || c == '+' || c == ',' || c == ';' ||
            c == '=' || c == '[' || c == ']' || c == '.' ||
            c == '"' || c == '*' || c == '/' || c == ':' ||
            c == '<' || c == '>' || c == '?' || c == '\\' || c == '|')
            continue;
        sfn[j++] = (uint8_t)c;
    }
    sfn[j++] = '~';
    sfn[j]   = '1';

    // Copy extension (up to 3 chars)
    if (dot >= 0)
    {
        j = 8;
        for (i = dot + 1; name[i] && j < 11; i++)
            sfn[j++] = (uint8_t)to_upper(name[i]);
    }
}

// Number of LFN directory entries needed for a given name.
// Returns 0 if name fits in 8.3 exactly.
static int lfn_entries_needed(const char* name)
{
    if (is_valid_83(name)) return 0;
    int len = 0;
    while (name[len]) len++;
    return (len + 12) / 13;
}

// Write `lfn_count` LFN directory entries for `name` into entries_buf[]
// starting at slot `start` (each slot is 32 bytes).
// sfn_chk is the checksum of the SFN entry that follows.
static void write_lfn_entries(uint8_t* entries_buf, int start,
                               const char* name, uint8_t sfn_chk, int lfn_count)
{
    int name_len = 0;
    while (name[name_len]) name_len++;

    for (int n = lfn_count; n >= 1; n--)
    {
        uint8_t* e   = entries_buf + (start + (lfn_count - n)) * 32;
        int base     = (n - 1) * 13;

        for (int b = 0; b < 32; b++) e[b] = 0;

        e[0]  = (uint8_t)n;
        if (n == lfn_count) e[0] |= 0x40;
        e[11] = 0x0F;
        e[12] = 0x00;
        e[13] = sfn_chk;

        for (int k = 0; k < 13; k++)
        {
            int off     = LFN_CHAR_OFFSETS[k];
            int char_pos = base + k;
            uint16_t ch;
            if (char_pos < name_len)
                ch = (uint16_t)(unsigned char)name[char_pos];
            else if (char_pos == name_len)
                ch = 0x0000;
            else
                ch = 0xFFFF;
            e[off]     = (uint8_t)(ch & 0xFF);
            e[off + 1] = (uint8_t)(ch >> 8);
        }
    }
}

// Write `count` consecutive 32-byte directory entries (in entries_buf[])
// starting at the first 0x00 slot in the dir_cluster chain.
// Allocates new clusters as needed if the directory runs out of space.
static int dir_write_entries(uint32_t dir_cluster, const uint8_t* entries_buf, int count)
{
    if (count <= 0) return 0;

    int entries_per_cluster = (sectors_per_cluster * 512) / 32;
    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf) return -1;

    // Find the first 0x00 entry (end of written directory entries)
    uint32_t cluster = dir_cluster;
    uint32_t prev_cluster = 0;
    int start_idx = -1;

    while (cluster >= 2 && cluster < FAT32_EOC)
    {
        if (read_cluster(cluster, buf) != 0) break;
        for (int i = 0; i < entries_per_cluster; i++)
        {
            if (buf[i * 32] == 0x00)
            {
                start_idx = i;
                goto found_start;
            }
        }
        prev_cluster = cluster;
        cluster = fat_next_cluster(cluster);
    }

    // Directory is completely full — allocate a new cluster
    {
        uint32_t new_cl = fat_alloc_cluster();
        if (new_cl == 0) { kfree(buf); return -1; }
        for (int b = 0; b < sectors_per_cluster * 512; b++) buf[b] = 0;
        write_cluster(new_cl, buf);
        if (prev_cluster != 0)
            fat_set_cluster(prev_cluster, new_cl);
        cluster = new_cl;
        start_idx = 0;
    }

found_start:
    if (read_cluster(cluster, buf) != 0) { kfree(buf); return -1; }

    int written = 0;
    int idx = start_idx;
    while (written < count)
    {
        if (idx >= entries_per_cluster)
        {
            // Flush current cluster and move to next
            if (write_cluster(cluster, buf) != 0) { kfree(buf); return -1; }

            uint32_t next = fat_next_cluster(cluster);
            if (next < 2 || next >= FAT32_EOC)
            {
                // Extend the directory
                next = fat_alloc_cluster();
                if (next == 0) { kfree(buf); return -1; }
                fat_set_cluster(cluster, next);
                for (int b = 0; b < sectors_per_cluster * 512; b++) buf[b] = 0;
                write_cluster(next, buf);
            }
            else
            {
                if (read_cluster(next, buf) != 0) { kfree(buf); return -1; }
            }
            cluster = next;
            idx = 0;
        }

        uint8_t* dst = buf + idx * 32;
        const uint8_t* src = entries_buf + written * 32;
        for (int b = 0; b < 32; b++) dst[b] = src[b];
        written++;
        idx++;
    }

    if (write_cluster(cluster, buf) != 0) { kfree(buf); return -1; }
    kfree(buf);
    return 0;
}

// Resolve a path to a (dir_cluster, filename) pair.
// Absolute paths start from root_cluster; relative from cwd_cluster.
// Returns 0 on success, -1 if any intermediate directory doesn't exist.
static int fat32_resolve_path(const char* path, uint32_t* dir_cluster, const char** filename)
{
    if (!path || !dir_cluster || !filename)
        return -1;

    uint32_t cluster = (path[0] == '/') ? root_cluster : cwd_cluster;
    if (path[0] == '/')
        path++;

    *dir_cluster = cluster;
    *filename = path;

    while (1)
    {
        int i = 0;
        while (path[i] && path[i] != '/')
            i++;

        if (path[i] == '\0')
        {
            *dir_cluster = cluster;
            *filename = path;
            return 0;
        }

        // Extract this directory component (up to 255 chars)
        char component[256];
        int k;
        for (k = 0; k < i && k < 255; k++)
            component[k] = path[k];
        component[k] = '\0';

        // Search for this component in the current cluster chain
        uint8_t* buf = kmalloc(sectors_per_cluster * 512);
        if (!buf) return -1;

        uint32_t scan  = cluster;
        uint32_t found = 0;
        int matched    = 0;

        char lfn_buf[256];
        uint8_t lfn_chk = 0;
        int lfn_remain  = 0;
        for (int b = 0; b < 256; b++) lfn_buf[b] = '\0';

        while (scan >= 2 && scan < FAT32_EOC)
        {
            if (read_cluster(scan, buf) != 0) break;

            uint32_t entries = (sectors_per_cluster * 512) / 32;
            uint32_t ei;

            for (ei = 0; ei < entries; ei++)
            {
                uint8_t* e = buf + (ei * 32);

                if (e[0] == 0x00) goto not_found;

                if ((uint8_t)e[0] == 0xE5) { lfn_remain = 0; lfn_buf[0] = '\0'; continue; }

                uint8_t attr = e[11];

                if (attr == 0x0F)
                {
                    lfn_accumulate(e, lfn_buf, &lfn_chk, &lfn_remain);
                    continue;
                }
                if (attr == 0x08) { lfn_remain = 0; lfn_buf[0] = '\0'; continue; }
                if (!(attr & FAT_ATTR_DIRECTORY)) { lfn_remain = 0; lfn_buf[0] = '\0'; continue; }

                // Check LFN name first, then 8.3 SFN
                int match = 0;
                if (lfn_buf[0] != '\0' && lfn_remain == 0 && sfn_checksum(e) == lfn_chk)
                    match = name_match(lfn_buf, component);
                if (!match)
                {
                    char sfn[13];
                    sfn_to_name(e, sfn);
                    match = name_match(sfn, component);
                }

                lfn_remain = 0;
                lfn_buf[0] = '\0';

                if (match)
                {
                    found = ((uint32_t)read16(e, 20) << 16) | read16(e, 26);
                    if (found == 0) found = root_cluster;
                    matched = 1;
                    goto found_component;
                }
            }

            scan = fat_next_cluster(scan);
        }

not_found:
        kfree(buf);
        return -1;

found_component:
        kfree(buf);
        if (!matched) return -1;
        cluster = found;
        path   += i + 1;
    }
}

int fat32_init(void)
{
    if (ata_read(0, 1, boot_sector) != 0)
        return -1;

    bytes_per_sector    = read16(boot_sector, 11);
    sectors_per_cluster = boot_sector[13];
    reserved_sectors    = read16(boot_sector, 14);
    fat_count           = boot_sector[16];
    total_sectors       = read32(boot_sector, 32);
    fat_size            = read32(boot_sector, 36);
    root_cluster        = read32(boot_sector, 44);

    if (bytes_per_sector != 512) return -1;
    if (sectors_per_cluster == 0) return -1;
    if (fat_count == 0) return -1;

    fat_start  = reserved_sectors;
    data_start = fat_start + (fat_count * fat_size);

    cwd_cluster = root_cluster;
    cwd_path[0] = '/';
    cwd_path[1] = '\0';

    return 0;
}

int fat32_list_dir(const char* path)
{
    uint32_t cluster;

    if (!path || path[0] == '\0' || (path[0] == '/' && path[1] == '\0'))
    {
        cluster = cwd_cluster;
    }
    else
    {
        const char* dirname;
        uint32_t parent;
        if (fat32_resolve_path(path, &parent, &dirname) != 0)
            return -1;

        uint8_t* buf = kmalloc(sectors_per_cluster * 512);
        if (!buf) return -1;

        uint32_t scan = parent;
        cluster = 0;

        char lfn_buf[256];
        uint8_t lfn_chk = 0;
        int lfn_remain  = 0;
        for (int b = 0; b < 256; b++) lfn_buf[b] = '\0';

        while (scan >= 2 && scan < FAT32_EOC)
        {
            if (read_cluster(scan, buf) != 0) break;

            uint32_t entries = (sectors_per_cluster * 512) / 32;
            for (uint32_t i = 0; i < entries; i++)
            {
                uint8_t* e = buf + (i * 32);
                if (e[0] == 0x00) { kfree(buf); return -1; }
                if ((uint8_t)e[0] == 0xE5) { lfn_remain = 0; lfn_buf[0] = '\0'; continue; }
                uint8_t attr = e[11];
                if (attr == 0x0F) { lfn_accumulate(e, lfn_buf, &lfn_chk, &lfn_remain); continue; }
                if (attr == 0x08) { lfn_remain = 0; lfn_buf[0] = '\0'; continue; }
                if (!(attr & FAT_ATTR_DIRECTORY)) { lfn_remain = 0; lfn_buf[0] = '\0'; continue; }

                int match = 0;
                if (lfn_buf[0] != '\0' && lfn_remain == 0 && sfn_checksum(e) == lfn_chk)
                    match = name_match(lfn_buf, dirname);
                if (!match)
                {
                    char sfn[13]; sfn_to_name(e, sfn);
                    match = name_match(sfn, dirname);
                }
                lfn_remain = 0; lfn_buf[0] = '\0';

                if (match)
                {
                    cluster = ((uint32_t)read16(e, 20) << 16) | read16(e, 26);
                    if (cluster == 0) cluster = root_cluster;
                    goto ls_found;
                }
            }
            scan = fat_next_cluster(scan);
        }
        kfree(buf);
        return -1;

ls_found:
        kfree(buf);
    }

    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf) return -1;

    char lfn_buf[256];
    uint8_t lfn_chk = 0;
    int lfn_remain  = 0;
    for (int b = 0; b < 256; b++) lfn_buf[b] = '\0';

    while (cluster >= 2 && cluster < FAT32_EOC)
    {
        if (read_cluster(cluster, buf) != 0) break;

        uint32_t entries = (sectors_per_cluster * 512) / 32;
        for (uint32_t i = 0; i < entries; i++)
        {
            uint8_t* e = buf + (i * 32);

            if (e[0] == 0x00) goto ls_done;

            if ((uint8_t)e[0] == 0xE5) { lfn_remain = 0; lfn_buf[0] = '\0'; continue; }

            uint8_t attr = e[11];
            if (attr == 0x0F) { lfn_accumulate(e, lfn_buf, &lfn_chk, &lfn_remain); continue; }
            if (attr == 0x08) { lfn_remain = 0; lfn_buf[0] = '\0'; continue; }

            // Choose display name: prefer LFN if valid
            char display[256];
            if (lfn_buf[0] != '\0' && lfn_remain == 0 && sfn_checksum(e) == lfn_chk)
            {
                int di = 0;
                while (lfn_buf[di] && di < 254) { display[di] = lfn_buf[di]; di++; }
                display[di] = '\0';
            }
            else
            {
                sfn_to_name(e, display);
            }
            lfn_remain = 0; lfn_buf[0] = '\0';

            terminal_writestring(display);
            if (attr & FAT_ATTR_DIRECTORY)
                terminal_writestring("/");
            terminal_putchar('\n');
        }

        cluster = fat_next_cluster(cluster);
    }

ls_done:
    kfree(buf);
    return 0;
}

int fat32_find_prefix(const char* prefix, char matches[][256], int max_matches)
{
    if (!prefix || !matches)
        return 0;

    const char* name_prefix = prefix;
    int last_slash = -1;
    int pi = 0;
    while (prefix[pi]) { if (prefix[pi] == '/') last_slash = pi; pi++; }

    uint32_t scan_cluster;
    char dir_part[256];
    int dir_part_len = 0;

    if (last_slash >= 0)
    {
        int k;
        for (k = 0; k <= last_slash && k < 255; k++) dir_part[k] = prefix[k];
        dir_part[k] = '\0';
        dir_part_len = k;
        name_prefix  = prefix + last_slash + 1;

        char resolve_path[256];
        int rp = 0;
        while (dir_part[rp] && rp < 254) { resolve_path[rp] = dir_part[rp]; rp++; }
        if (rp > 0 && resolve_path[rp - 1] == '/') rp--;
        resolve_path[rp] = '\0';

        uint32_t parent;
        const char* dirname;
        if (fat32_resolve_path(resolve_path, &parent, &dirname) != 0) return 0;

        if (dirname[0] == '\0')
        {
            scan_cluster = parent;
            goto prefix_dir_found;
        }

        uint8_t* buf = kmalloc(sectors_per_cluster * 512);
        if (!buf) return 0;

        uint32_t scan = parent;
        scan_cluster  = 0;

        char lfn_buf[256]; uint8_t lfn_chk = 0; int lfn_remain = 0;
        for (int b = 0; b < 256; b++) lfn_buf[b] = '\0';

        while (scan >= 2 && scan < FAT32_EOC)
        {
            if (read_cluster(scan, buf) != 0) break;
            uint32_t entries = (sectors_per_cluster * 512) / 32;
            for (uint32_t i = 0; i < entries; i++)
            {
                uint8_t* e = buf + (i * 32);
                if (e[0] == 0x00) goto prefix_dir_done;
                if ((uint8_t)e[0] == 0xE5) { lfn_remain=0; lfn_buf[0]='\0'; continue; }
                uint8_t attr = e[11];
                if (attr == 0x0F) { lfn_accumulate(e,lfn_buf,&lfn_chk,&lfn_remain); continue; }
                if (attr == 0x08) { lfn_remain=0; lfn_buf[0]='\0'; continue; }
                if (!(attr & FAT_ATTR_DIRECTORY)) { lfn_remain=0; lfn_buf[0]='\0'; continue; }

                int match = 0;
                if (lfn_buf[0] != '\0' && lfn_remain==0 && sfn_checksum(e)==lfn_chk)
                    match = name_match(lfn_buf, dirname);
                if (!match) { char sfn[13]; sfn_to_name(e,sfn); match=name_match(sfn,dirname); }
                lfn_remain=0; lfn_buf[0]='\0';

                if (match)
                {
                    scan_cluster = ((uint32_t)read16(e,20)<<16)|read16(e,26);
                    if (scan_cluster == 0) scan_cluster = root_cluster;
                    kfree(buf);
                    goto prefix_dir_found;
                }
            }
            scan = fat_next_cluster(scan);
        }

prefix_dir_done:
        kfree(buf);
        return 0;

prefix_dir_found:;
    }
    else
    {
        scan_cluster = cwd_cluster;
        dir_part[0]  = '\0';
        dir_part_len = 0;
    }

    int name_prefix_len = 0;
    while (name_prefix[name_prefix_len]) name_prefix_len++;

    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf) return 0;

    int count = 0;
    uint32_t cluster = scan_cluster;

    char lfn_buf[256]; uint8_t lfn_chk = 0; int lfn_remain = 0;
    for (int b = 0; b < 256; b++) lfn_buf[b] = '\0';

    while (cluster >= 2 && cluster < FAT32_EOC)
    {
        if (read_cluster(cluster, buf) != 0) break;

        uint32_t entries = (sectors_per_cluster * 512) / 32;
        for (uint32_t i = 0; i < entries; i++)
        {
            uint8_t* e = buf + (i * 32);

            if (e[0] == 0x00) goto done;

            if ((uint8_t)e[0] == 0xE5) { lfn_remain=0; lfn_buf[0]='\0'; continue; }

            uint8_t attr = e[11];
            if (attr == 0x0F) { lfn_accumulate(e,lfn_buf,&lfn_chk,&lfn_remain); continue; }
            if (attr == 0x08) { lfn_remain=0; lfn_buf[0]='\0'; continue; }

            // Choose display name
            char display[256];
            if (lfn_buf[0] != '\0' && lfn_remain==0 && sfn_checksum(e)==lfn_chk)
            {
                int di = 0;
                while (lfn_buf[di] && di < 254) { display[di] = lfn_buf[di]; di++; }
                display[di] = '\0';
            }
            else
            {
                sfn_to_name(e, display);
            }
            lfn_remain=0; lfn_buf[0]='\0';

            // Skip dot entries
            if (display[0] == '.') continue;

            // Check prefix match (case-insensitive)
            int match = 1;
            for (int j = 0; j < name_prefix_len; j++)
            {
                if (to_upper(display[j]) != to_upper(name_prefix[j]))
                {
                    match = 0;
                    break;
                }
            }

            if (match && count < max_matches)
            {
                int mi = 0, k2;
                for (k2 = 0; k2 < dir_part_len && mi < 255; k2++)
                    matches[count][mi++] = dir_part[k2];
                for (k2 = 0; display[k2] && mi < 255; k2++)
                    matches[count][mi++] = display[k2];
                matches[count][mi] = '\0';
                count++;
            }
        }

        cluster = fat_next_cluster(cluster);
    }

done:
    kfree(buf);
    return count;
}

int fat32_lookup_file(const char* path, uint32_t* first_cluster, uint32_t* size)
{
    if (!path || !first_cluster || !size) return -1;

    uint32_t cluster;
    const char* filename;
    if (fat32_resolve_path(path, &cluster, &filename) != 0) return -1;
    path = filename;

    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf) return -1;

    int found = 0;

    char lfn_buf[256]; uint8_t lfn_chk = 0; int lfn_remain = 0;
    for (int b = 0; b < 256; b++) lfn_buf[b] = '\0';

    while (cluster >= 2 && cluster < FAT32_EOC && !found)
    {
        if (read_cluster(cluster, buf) != 0) break;

        uint32_t entries = (sectors_per_cluster * 512) / 32;
        for (uint32_t i = 0; i < entries; i++)
        {
            uint8_t* e = buf + (i * 32);

            if (e[0] == 0x00) goto done;

            if ((uint8_t)e[0] == 0xE5) { lfn_remain=0; lfn_buf[0]='\0'; continue; }

            uint8_t attr = e[11];
            if (attr == 0x0F) { lfn_accumulate(e,lfn_buf,&lfn_chk,&lfn_remain); continue; }
            if (attr == 0x08) { lfn_remain=0; lfn_buf[0]='\0'; continue; }
            if (attr & FAT_ATTR_DIRECTORY) { lfn_remain=0; lfn_buf[0]='\0'; continue; }

            int match = 0;
            if (lfn_buf[0] != '\0' && lfn_remain==0 && sfn_checksum(e)==lfn_chk)
                match = name_match(lfn_buf, path);
            if (!match) { char sfn[13]; sfn_to_name(e,sfn); match=name_match(sfn,path); }
            lfn_remain=0; lfn_buf[0]='\0';

            if (match)
            {
                *first_cluster = ((uint32_t)read16(e,20)<<16)|read16(e,26);
                *size          = read32(e, 28);
                found = 1;
                break;
            }
        }

        cluster = fat_next_cluster(cluster);
    }

done:
    kfree(buf);
    return found ? 0 : -1;
}

int fat32_read_file(const char* path, void* buffer, uint32_t bufsize, uint32_t* size)
{
    if (!path || !buffer || !size) return -1;

    uint32_t file_cluster = 0, file_size = 0;
    if (fat32_lookup_file(path, &file_cluster, &file_size) != 0) return -1;

    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf) return -1;

    if (file_size > bufsize) file_size = bufsize;

    uint32_t bytes_read = 0;
    uint8_t* out = (uint8_t*)buffer;
    uint32_t cluster = file_cluster;

    while (cluster >= 2 && cluster < FAT32_EOC && bytes_read < file_size)
    {
        if (read_cluster(cluster, buf) != 0) break;

        uint32_t cluster_bytes = sectors_per_cluster * 512;
        uint32_t remaining     = file_size - bytes_read;
        uint32_t to_copy       = remaining < cluster_bytes ? remaining : cluster_bytes;

        for (uint32_t i = 0; i < to_copy; i++)
            out[bytes_read + i] = buf[i];

        bytes_read += to_copy;
        cluster     = fat_next_cluster(cluster);
    }

    *size = bytes_read;
    kfree(buf);
    return 0;
}

uint32_t fat32_cluster_size(void) { return sectors_per_cluster * 512; }
uint32_t fat32_next_cluster(uint32_t cluster) { return fat_next_cluster(cluster); }
int fat32_read_cluster(uint32_t cluster, void* buffer) { return read_cluster(cluster, (uint8_t*)buffer); }

int fat32_write_file(const char* path, const void* buffer, uint32_t size)
{
    if (!path || !buffer) return -1;

    uint32_t dir_cluster;
    const char* filename;
    if (fat32_resolve_path(path, &dir_cluster, &filename) != 0) return -1;
    path = filename;

    // Delete existing entry (if any) before writing the new one
    uint8_t* scan_buf = kmalloc(sectors_per_cluster * 512);
    if (!scan_buf) return -1;

    uint32_t scan_cluster = dir_cluster;

    char lfn_buf[256]; uint8_t lfn_chk = 0; int lfn_remain = 0;
    for (int b = 0; b < 256; b++) lfn_buf[b] = '\0';

    // Track LFN positions for cleanup
#define MAX_LFN_TRACK 20
    struct { uint32_t cluster; int idx; } lfn_track[MAX_LFN_TRACK];
    int lfn_track_count = 0;

    while (scan_cluster >= 2 && scan_cluster < FAT32_EOC)
    {
        if (read_cluster(scan_cluster, scan_buf) != 0) break;

        uint32_t entries = (sectors_per_cluster * 512) / 32;
        for (uint32_t i = 0; i < entries; i++)
        {
            uint8_t* e = scan_buf + (i * 32);

            if (e[0] == 0x00) goto scan_done;

            if ((uint8_t)e[0] == 0xE5)
            {
                lfn_remain = 0; lfn_buf[0] = '\0'; lfn_track_count = 0;
                continue;
            }

            uint8_t attr = e[11];
            if (attr == 0x0F)
            {
                lfn_accumulate(e, lfn_buf, &lfn_chk, &lfn_remain);
                if (lfn_track_count < MAX_LFN_TRACK)
                {
                    lfn_track[lfn_track_count].cluster = scan_cluster;
                    lfn_track[lfn_track_count].idx     = (int)i;
                    lfn_track_count++;
                }
                continue;
            }
            if (attr == 0x08) { lfn_remain=0; lfn_buf[0]='\0'; lfn_track_count=0; continue; }
            if (attr & FAT_ATTR_DIRECTORY) { lfn_remain=0; lfn_buf[0]='\0'; lfn_track_count=0; continue; }

            int match = 0;
            if (lfn_buf[0] != '\0' && lfn_remain==0 && sfn_checksum(e)==lfn_chk)
                match = name_match(lfn_buf, path);
            if (!match) { char sfn[13]; sfn_to_name(e,sfn); match=name_match(sfn,path); }

            if (match)
            {
                uint32_t old_cluster = ((uint32_t)read16(e,20)<<16)|read16(e,26);
                if (old_cluster >= 2) fat_free_chain(old_cluster);

                // Mark SFN deleted
                e[0] = 0xE5;
                // Mark LFN entries in same cluster
                for (int k = 0; k < lfn_track_count; k++)
                {
                    if (lfn_track[k].cluster == scan_cluster)
                        scan_buf[lfn_track[k].idx * 32] = 0xE5;
                }
                write_cluster(scan_cluster, scan_buf);

                // Mark LFN entries in other clusters
                for (int k = 0; k < lfn_track_count; k++)
                {
                    uint32_t lc = lfn_track[k].cluster;
                    if (lc == scan_cluster || lc == 0) continue;
                    uint8_t* lbuf = kmalloc(sectors_per_cluster * 512);
                    if (!lbuf) continue;
                    if (read_cluster(lc, lbuf) == 0)
                    {
                        for (int m = 0; m < lfn_track_count; m++)
                            if (lfn_track[m].cluster == lc) lbuf[lfn_track[m].idx*32] = 0xE5;
                        write_cluster(lc, lbuf);
                    }
                    kfree(lbuf);
                    // Prevent re-processing this cluster
                    for (int m = k; m < lfn_track_count; m++)
                        if (lfn_track[m].cluster == lc) lfn_track[m].cluster = 0;
                }
                goto scan_done;
            }

            lfn_remain=0; lfn_buf[0]='\0'; lfn_track_count=0;
        }

        scan_cluster = fat_next_cluster(scan_cluster);
    }

scan_done:
    kfree(scan_buf);

    if (size == 0)
        return fat32_create_entry(path, dir_cluster, 0, 0, FAT_ATTR_ARCHIVE);

    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf) return -1;

    const uint8_t* data    = (const uint8_t*)buffer;
    uint32_t bytes_written = 0;
    uint32_t first_cluster = 0;
    uint32_t prev_cluster  = 0;

    while (bytes_written < size)
    {
        uint32_t cluster = fat_alloc_cluster();
        if (cluster == 0) { kfree(buf); return -1; }

        if (prev_cluster != 0) fat_set_cluster(prev_cluster, cluster);
        else first_cluster = cluster;

        uint32_t cluster_bytes = sectors_per_cluster * 512;
        uint32_t remaining     = size - bytes_written;
        uint32_t to_write      = remaining < cluster_bytes ? remaining : cluster_bytes;

        for (uint32_t i = 0; i < cluster_bytes; i++) buf[i] = 0;
        for (uint32_t i = 0; i < to_write; i++) buf[i] = data[bytes_written + i];

        if (write_cluster(cluster, buf) != 0) { kfree(buf); return -1; }

        bytes_written += to_write;
        prev_cluster   = cluster;
    }

    kfree(buf);
    return fat32_create_entry(path, dir_cluster, first_cluster, size, FAT_ATTR_ARCHIVE);
}

int fat32_delete_file(const char* path)
{
    if (!path) return -1;

    uint32_t dir_cluster;
    const char* filename;
    if (fat32_resolve_path(path, &dir_cluster, &filename) != 0) return -1;
    path = filename;

    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf) return -1;

    char lfn_buf[256]; uint8_t lfn_chk = 0; int lfn_remain = 0;
    for (int b = 0; b < 256; b++) lfn_buf[b] = '\0';

    struct { uint32_t cluster; int idx; } lfn_track[MAX_LFN_TRACK];
    int lfn_track_count = 0;

    uint32_t cur_cluster = dir_cluster;

    while (cur_cluster >= 2 && cur_cluster < FAT32_EOC)
    {
        if (read_cluster(cur_cluster, buf) != 0) break;

        uint32_t entries = (sectors_per_cluster * 512) / 32;
        for (uint32_t i = 0; i < entries; i++)
        {
            uint8_t* e = buf + (i * 32);

            if (e[0] == 0x00) goto not_found;

            if ((uint8_t)e[0] == 0xE5)
            {
                lfn_remain=0; lfn_buf[0]='\0'; lfn_track_count=0;
                continue;
            }

            uint8_t attr = e[11];
            if (attr == 0x0F)
            {
                lfn_accumulate(e, lfn_buf, &lfn_chk, &lfn_remain);
                if (lfn_track_count < MAX_LFN_TRACK)
                {
                    lfn_track[lfn_track_count].cluster = cur_cluster;
                    lfn_track[lfn_track_count].idx     = (int)i;
                    lfn_track_count++;
                }
                continue;
            }
            if (attr == 0x08) { lfn_remain=0; lfn_buf[0]='\0'; lfn_track_count=0; continue; }
            if (attr & FAT_ATTR_DIRECTORY) { lfn_remain=0; lfn_buf[0]='\0'; lfn_track_count=0; continue; }

            int match = 0;
            if (lfn_buf[0] != '\0' && lfn_remain==0 && sfn_checksum(e)==lfn_chk)
                match = name_match(lfn_buf, path);
            if (!match) { char sfn[13]; sfn_to_name(e,sfn); match=name_match(sfn,path); }

            if (match)
            {
                uint32_t fc = ((uint32_t)read16(e,20)<<16)|read16(e,26);
                if (fc >= 2) fat_free_chain(fc);

                e[0] = 0xE5;
                for (int k = 0; k < lfn_track_count; k++)
                    if (lfn_track[k].cluster == cur_cluster)
                        buf[lfn_track[k].idx * 32] = 0xE5;
                write_cluster(cur_cluster, buf);

                for (int k = 0; k < lfn_track_count; k++)
                {
                    uint32_t lc = lfn_track[k].cluster;
                    if (lc == cur_cluster || lc == 0) continue;
                    uint8_t* lbuf = kmalloc(sectors_per_cluster * 512);
                    if (!lbuf) continue;
                    if (read_cluster(lc, lbuf) == 0)
                    {
                        for (int m = 0; m < lfn_track_count; m++)
                            if (lfn_track[m].cluster == lc) lbuf[lfn_track[m].idx*32] = 0xE5;
                        write_cluster(lc, lbuf);
                    }
                    kfree(lbuf);
                    for (int m = k; m < lfn_track_count; m++)
                        if (lfn_track[m].cluster == lc) lfn_track[m].cluster = 0;
                }

                kfree(buf);
                return 0;
            }

            lfn_remain=0; lfn_buf[0]='\0'; lfn_track_count=0;
        }

        cur_cluster = fat_next_cluster(cur_cluster);
    }

not_found:
    kfree(buf);
    return -1;
}

int fat32_rename_file(const char* oldpath, const char* newname)
{
    if (!oldpath || !newname) return -1;

    uint32_t dir_cluster;
    const char* old_filename;
    if (fat32_resolve_path(oldpath, &dir_cluster, &old_filename) != 0) return -1;
    oldpath = old_filename;

    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf) return -1;

    char lfn_buf[256]; uint8_t lfn_chk = 0; int lfn_remain = 0;
    for (int b = 0; b < 256; b++) lfn_buf[b] = '\0';

    struct { uint32_t cluster; int idx; } lfn_track[MAX_LFN_TRACK];
    int lfn_track_count = 0;

    uint32_t cur_cluster = dir_cluster;

    while (cur_cluster >= 2 && cur_cluster < FAT32_EOC)
    {
        if (read_cluster(cur_cluster, buf) != 0) break;

        uint32_t entries = (sectors_per_cluster * 512) / 32;
        for (uint32_t i = 0; i < entries; i++)
        {
            uint8_t* e = buf + (i * 32);

            if (e[0] == 0x00) goto not_found;

            if ((uint8_t)e[0] == 0xE5)
            {
                lfn_remain=0; lfn_buf[0]='\0'; lfn_track_count=0;
                continue;
            }

            uint8_t attr = e[11];
            if (attr == 0x0F)
            {
                lfn_accumulate(e, lfn_buf, &lfn_chk, &lfn_remain);
                if (lfn_track_count < MAX_LFN_TRACK)
                {
                    lfn_track[lfn_track_count].cluster = cur_cluster;
                    lfn_track[lfn_track_count].idx     = (int)i;
                    lfn_track_count++;
                }
                continue;
            }
            if (attr == 0x08) { lfn_remain=0; lfn_buf[0]='\0'; lfn_track_count=0; continue; }

            int match = 0;
            if (lfn_buf[0] != '\0' && lfn_remain==0 && sfn_checksum(e)==lfn_chk)
                match = name_match(lfn_buf, oldpath);
            if (!match) { char sfn[13]; sfn_to_name(e,sfn); match=name_match(sfn,oldpath); }

            if (match)
            {
                // Extract file info from old entry
                uint32_t fc   = ((uint32_t)read16(e,20)<<16)|read16(e,26);
                uint32_t fsz  = read32(e, 28);
                uint8_t  fat  = e[11];

                // Delete old SFN and its LFN entries
                e[0] = 0xE5;
                for (int k = 0; k < lfn_track_count; k++)
                    if (lfn_track[k].cluster == cur_cluster)
                        buf[lfn_track[k].idx * 32] = 0xE5;
                write_cluster(cur_cluster, buf);

                for (int k = 0; k < lfn_track_count; k++)
                {
                    uint32_t lc = lfn_track[k].cluster;
                    if (lc == cur_cluster || lc == 0) continue;
                    uint8_t* lbuf = kmalloc(sectors_per_cluster * 512);
                    if (!lbuf) continue;
                    if (read_cluster(lc, lbuf) == 0)
                    {
                        for (int m = 0; m < lfn_track_count; m++)
                            if (lfn_track[m].cluster == lc) lbuf[lfn_track[m].idx*32] = 0xE5;
                        write_cluster(lc, lbuf);
                    }
                    kfree(lbuf);
                    for (int m = k; m < lfn_track_count; m++)
                        if (lfn_track[m].cluster == lc) lfn_track[m].cluster = 0;
                }

                kfree(buf);
                // Create new entry with new name (LFN if needed)
                return fat32_create_entry(newname, dir_cluster, fc, fsz, fat);
            }

            lfn_remain=0; lfn_buf[0]='\0'; lfn_track_count=0;
        }

        cur_cluster = fat_next_cluster(cur_cluster);
    }

not_found:
    kfree(buf);
    return -1;
}

int fat32_mkdir(const char* name)
{
    if (!name) return -1;

    uint32_t parent_cluster;
    const char* dirname;
    if (fat32_resolve_path(name, &parent_cluster, &dirname) != 0) return -1;
    name = dirname;

    uint32_t new_cluster = fat_alloc_cluster();
    if (new_cluster == 0) return -1;

    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf) return -1;

    for (uint32_t i = 0; i < sectors_per_cluster * 512; i++) buf[i] = 0;

    // '.' entry
    uint8_t* dot = buf;
    dot[0] = '.';
    for (int i = 1; i < 11; i++) dot[i] = ' ';
    dot[11] = FAT_ATTR_DIRECTORY;
    dot[20] = (uint8_t)((new_cluster >> 16) & 0xFF);
    dot[21] = (uint8_t)((new_cluster >> 24) & 0xFF);
    dot[26] = (uint8_t)(new_cluster & 0xFF);
    dot[27] = (uint8_t)((new_cluster >> 8) & 0xFF);

    // '..' entry
    uint8_t* dotdot = buf + 32;
    dotdot[0] = '.'; dotdot[1] = '.';
    for (int i = 2; i < 11; i++) dotdot[i] = ' ';
    dotdot[11] = FAT_ATTR_DIRECTORY;
    dotdot[20] = (uint8_t)((parent_cluster >> 16) & 0xFF);
    dotdot[21] = (uint8_t)((parent_cluster >> 24) & 0xFF);
    dotdot[26] = (uint8_t)(parent_cluster & 0xFF);
    dotdot[27] = (uint8_t)((parent_cluster >> 8) & 0xFF);

    int ret = write_cluster(new_cluster, buf);
    kfree(buf);

    if (ret != 0)
    {
        fat_set_cluster(new_cluster, FAT32_FREE);
        return -1;
    }

    return fat32_create_entry(name, parent_cluster, new_cluster, 0, FAT_ATTR_DIRECTORY);
}

int fat32_cd(const char* name)
{
    if (!name) return -1;

    // cd ..
    if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
    {
        uint8_t* buf = kmalloc(sectors_per_cluster * 512);
        if (!buf) return -1;

        if (read_cluster(cwd_cluster, buf) != 0) { kfree(buf); return -1; }

        uint8_t* dotdot = buf + 32;
        uint32_t parent = ((uint32_t)read16(dotdot, 20) << 16) | read16(dotdot, 26);
        kfree(buf);

        if (parent == 0) parent = root_cluster;
        cwd_cluster = parent;

        int len = 0;
        while (cwd_path[len]) len++;
        if (len > 1)
        {
            len--;
            while (len > 0 && cwd_path[len] != '/') len--;
            if (len == 0) len = 1;
            cwd_path[len] = '\0';
        }
        return 0;
    }

    // cd /
    if (name[0] == '/' && name[1] == '\0')
    {
        cwd_cluster = root_cluster;
        cwd_path[0] = '/';
        cwd_path[1] = '\0';
        return 0;
    }

    uint32_t parent;
    const char* dirname;
    if (fat32_resolve_path(name, &parent, &dirname) != 0) return -1;

    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf) return -1;

    uint32_t scan = parent;

    char lfn_buf[256]; uint8_t lfn_chk = 0; int lfn_remain = 0;
    for (int b = 0; b < 256; b++) lfn_buf[b] = '\0';

    while (scan >= 2 && scan < FAT32_EOC)
    {
        if (read_cluster(scan, buf) != 0) break;

        uint32_t entries = (sectors_per_cluster * 512) / 32;
        for (uint32_t i = 0; i < entries; i++)
        {
            uint8_t* e = buf + (i * 32);

            if (e[0] == 0x00) goto not_found;

            if ((uint8_t)e[0] == 0xE5) { lfn_remain=0; lfn_buf[0]='\0'; continue; }

            uint8_t attr = e[11];
            if (attr == 0x0F) { lfn_accumulate(e,lfn_buf,&lfn_chk,&lfn_remain); continue; }
            if (attr == 0x08) { lfn_remain=0; lfn_buf[0]='\0'; continue; }
            if (!(attr & FAT_ATTR_DIRECTORY)) { lfn_remain=0; lfn_buf[0]='\0'; continue; }

            int match = 0;
            if (lfn_buf[0] != '\0' && lfn_remain==0 && sfn_checksum(e)==lfn_chk)
                match = name_match(lfn_buf, dirname);
            if (!match) { char sfn[13]; sfn_to_name(e,sfn); match=name_match(sfn,dirname); }
            lfn_remain=0; lfn_buf[0]='\0';

            if (match)
            {
                uint32_t target = ((uint32_t)read16(e,20)<<16)|read16(e,26);
                if (target == 0) target = root_cluster;
                cwd_cluster = target;

                if (name[0] == '/')
                {
                    int pi = 0;
                    cwd_path[pi++] = '/';
                    int ni = 1;
                    while (name[ni] && pi < 255)
                        cwd_path[pi++] = name[ni++];
                    cwd_path[pi] = '\0';
                }
                else
                {
                    int len = 0;
                    while (cwd_path[len]) len++;
                    if (len > 1) cwd_path[len++] = '/';
                    int ni = 0;
                    while (name[ni] && len < 255)
                        cwd_path[len++] = name[ni++];
                    cwd_path[len] = '\0';
                }

                kfree(buf);
                return 0;
            }
        }

        scan = fat_next_cluster(scan);
    }

not_found:
    kfree(buf);
    return -1;
}

static int fat32_create_entry(const char* name, uint32_t dir_cluster,
                              uint32_t cluster, uint32_t size, uint8_t attr)
{
    int lfn_count = lfn_entries_needed(name);
    int total     = lfn_count + 1;

    // Build all directory entries in a temp buffer
    uint8_t* entries = kmalloc((uint32_t)(total * 32));
    if (!entries) return -1;
    for (int i = 0; i < total * 32; i++) entries[i] = 0;

    // Build the 11-byte SFN
    uint8_t sfn[11];
    if (lfn_count == 0)
    {
        // Name fits in 8.3 — encode it directly
        for (int i = 0; i < 11; i++) sfn[i] = ' ';
        int ni = 0, j = 0;
        while (name[ni] && name[ni] != '.' && j < 8)
            sfn[j++] = (uint8_t)to_upper(name[ni++]);
        if (name[ni] == '.')
        {
            ni++; j = 8;
            while (name[ni] && j < 11)
                sfn[j++] = (uint8_t)to_upper(name[ni++]);
        }
    }
    else
    {
        make_sfn(name, sfn);
    }

    uint8_t chk = sfn_checksum(sfn);

    // Write LFN entries (if any) before the SFN slot
    if (lfn_count > 0)
        write_lfn_entries(entries, 0, name, chk, lfn_count);

    // Fill in the SFN entry
    uint8_t* sfn_e = entries + lfn_count * 32;
    for (int i = 0; i < 11; i++) sfn_e[i] = sfn[i];
    sfn_e[11] = attr;
    sfn_e[20] = (uint8_t)((cluster >> 16) & 0xFF);
    sfn_e[21] = (uint8_t)((cluster >> 24) & 0xFF);
    sfn_e[26] = (uint8_t)(cluster & 0xFF);
    sfn_e[27] = (uint8_t)((cluster >> 8) & 0xFF);
    sfn_e[28] = (uint8_t)(size & 0xFF);
    sfn_e[29] = (uint8_t)((size >> 8) & 0xFF);
    sfn_e[30] = (uint8_t)((size >> 16) & 0xFF);
    sfn_e[31] = (uint8_t)((size >> 24) & 0xFF);

    int ret = dir_write_entries(dir_cluster, entries, total);
    kfree(entries);
    return ret;
}

uint32_t fat32_get_cwd_cluster(void) { return cwd_cluster; }
const char* fat32_get_cwd_path(void) { return cwd_path; }

const vfs_ops_t fat32_vfs_ops = {
    .read_file    = fat32_read_file,
    .write_file   = fat32_write_file,
    .delete_file  = fat32_delete_file,
    .rename       = fat32_rename_file,
    .mkdir        = fat32_mkdir,
    .list_dir     = fat32_list_dir,
    .chdir        = fat32_cd,
    .get_cwd      = fat32_get_cwd_path,
    .lookup       = fat32_lookup_file,
    .find_prefix  = fat32_find_prefix,
    .cluster_size = fat32_cluster_size,
    .next_cluster = fat32_next_cluster,
    .read_cluster = fat32_read_cluster,
};
