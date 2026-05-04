#include "fat32.h"
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

// Helper to read a uint16 from a byte array at offset
static uint16_t read16(uint8_t* buf, int offset)
{
	return (uint16_t)(buf[offset] | (buf[offset + 1] << 8));
}

// Current working directory
static uint32_t cwd_cluster;
static char cwd_path[256];

// Helper to read a uint32 from a byte array at offset
static uint32_t read32(uint8_t* buf, int offset)
{
    return (uint32_t)(buf[offset]        |
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
static int fat32_create_entry(const char* name, uint32_t cluster, uint32_t size);
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

int fat32_init(void)
{
    if (ata_read(0, 1, boot_sector) != 0)
        return -1;

    // Read BPB fields directly by byte offset
    bytes_per_sector    = read16(boot_sector, 11);
    sectors_per_cluster = boot_sector[13];
    reserved_sectors    = read16(boot_sector, 14);
    fat_count           = boot_sector[16];
    total_sectors       = read32(boot_sector, 32);
    fat_size            = read32(boot_sector, 36);
    root_cluster        = read32(boot_sector, 44);

    // Validate
    if (bytes_per_sector != 512)
        return -1;
    if (sectors_per_cluster == 0)
        return -1;
    if (fat_count == 0)
        return -1;

    // Calculate region locations
    fat_start  = reserved_sectors;
    data_start = fat_start + (fat_count * fat_size);

	// Start in root directory
	cwd_cluster = root_cluster;
	cwd_path[0] = '/';
	cwd_path[1] = '\0';

    return 0;
}

int fat32_list_dir(const char* path)
{
    (void)path;

    uint32_t cluster = cwd_cluster;
    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf)
        return -1;

    while (cluster >= 2 && cluster < FAT32_EOC)
    {
        if (read_cluster(cluster, buf) != 0)
            break;

        uint32_t entries = (sectors_per_cluster * 512) / 32;
        uint32_t i;

        for (i = 0; i < entries; i++)
        {
            uint8_t* e = buf + (i * 32);

            if (e[0] == 0x00)
                goto done;

            if ((uint8_t)e[0] == 0xE5)
                continue;

            uint8_t attr = e[11];
            if (attr == 0x0F || attr == 0x08)
                continue;

            // Build 8.3 filename
            char name[13];
            int n = 0;
            int j;

            for (j = 0; j < 8 && e[j] != ' '; j++)
                name[n++] = e[j];

            if (e[8] != ' ')
            {
                name[n++] = '.';
                for (j = 8; j < 11 && e[j] != ' '; j++)
                    name[n++] = e[j];
            }
            name[n] = '\0';

            terminal_writestring(name);
            if (attr & FAT_ATTR_DIRECTORY)
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

    uint32_t cluster = cwd_cluster;
    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf)
        return -1;

    uint32_t file_cluster = 0;
    uint32_t file_size    = 0;
    int found = 0;

    while (cluster >= 2 && cluster < FAT32_EOC && !found)
    {
        if (read_cluster(cluster, buf) != 0)
            break;

        uint32_t entries = (sectors_per_cluster * 512) / 32;
        uint32_t i;

        for (i = 0; i < entries; i++)
        {
            uint8_t* e = buf + (i * 32);

            if (e[0] == 0x00)
                goto not_found;

            if ((uint8_t)e[0] == 0xE5)
                continue;

            uint8_t attr = e[11];
            if (attr == 0x0F || attr == 0x08)
                continue;

            if (attr & FAT_ATTR_DIRECTORY)
                continue;

            // Build 8.3 filename
            char name[13];
            int n = 0;
            int j;

            for (j = 0; j < 8 && e[j] != ' '; j++)
                name[n++] = e[j];
            if (e[8] != ' ')
            {
                name[n++] = '.';
                for (j = 8; j < 11 && e[j] != ' '; j++)
                    name[n++] = e[j];
            }
            name[n] = '\0';

            // Case insensitive compare
            int match = 1;
            for (j = 0; name[j] || path[j]; j++)
            {
                if (to_upper(name[j]) != to_upper(path[j]))
                {
                    match = 0;
                    break;
                }
            }

            if (match)
            {
                file_cluster = ((uint32_t)read16(e, 20) << 16) | read16(e, 26);
                file_size    = read32(e, 28);
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

    uint32_t bytes_read = 0;
    uint8_t* out = (uint8_t*)buffer;
    cluster = file_cluster;

    while (cluster >= 2 && cluster < FAT32_EOC && bytes_read < file_size)
    {
        if (read_cluster(cluster, buf) != 0)
            break;

        uint32_t cluster_bytes = sectors_per_cluster * 512;
        uint32_t remaining     = file_size - bytes_read;
        uint32_t to_copy       = remaining < cluster_bytes ? remaining : cluster_bytes;
        uint32_t i;

        for (i = 0; i < to_copy; i++)
            out[bytes_read + i] = buf[i];

        bytes_read += to_copy;
        cluster     = fat_next_cluster(cluster);
    }

    *size = bytes_read;
    kfree(buf);
    return 0;
}

int fat32_write_file(const char* path, const void* buffer, uint32_t size)
{
    if (!path || !buffer)
        return -1;

    // Check if file already exists and remove it first
    uint32_t dir_cluster = cwd_cluster;
    uint8_t* scan_buf = kmalloc(sectors_per_cluster * 512);
    if (!scan_buf)
        return -1;

    while (dir_cluster >= 2 && dir_cluster < FAT32_EOC)
    {
        if (read_cluster(dir_cluster, scan_buf) != 0)
            break;

        uint32_t entries = (sectors_per_cluster * 512) / 32;
        uint32_t i;

        for (i = 0; i < entries; i++)
        {
            uint8_t* e = scan_buf + (i * 32);

            if (e[0] == 0x00)
                goto scan_done;

            if ((uint8_t)e[0] == 0xE5)
                continue;

            uint8_t attr = e[11];
            if (attr == 0x0F || attr == 0x08)
                continue;

            if (attr & FAT_ATTR_DIRECTORY)
                continue;

            // Build filename to compare
            char name[13];
            int n = 0;
            int j;
            for (j = 0; j < 8 && e[j] != ' '; j++)
                name[n++] = e[j];
            if (e[8] != ' ')
            {
                name[n++] = '.';
                for (j = 8; j < 11 && e[j] != ' '; j++)
                    name[n++] = e[j];
            }
            name[n] = '\0';

            // Case insensitive compare
            int match = 1;
            for (j = 0; name[j] || path[j]; j++)
            {
                if (to_upper(name[j]) != to_upper(path[j])) { match = 0; break; }
            }

            if (match)
            {
                // Free existing cluster chain
                uint32_t old_cluster = ((uint32_t)read16(e, 20) << 16) | read16(e, 26);
                if (old_cluster >= 2)
                    fat_free_chain(old_cluster);

                // Mark entry as deleted
                e[0] = 0xE5;
                write_cluster(dir_cluster, scan_buf);
                goto scan_done;
            }
        }

        dir_cluster = fat_next_cluster(dir_cluster);
    }

scan_done:
    kfree(scan_buf);

    // Handle empty file
    if (size == 0)
        return fat32_create_entry(path, 0, 0);

    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf)
        return -1;

    const uint8_t* data    = (const uint8_t*)buffer;
    uint32_t bytes_written = 0;
    uint32_t first_cluster = 0;
    uint32_t prev_cluster  = 0;

    while (bytes_written < size)
    {
        uint32_t cluster = fat_alloc_cluster();
        if (cluster == 0)
        {
            kfree(buf);
            return -1;
        }

        if (prev_cluster != 0)
            fat_set_cluster(prev_cluster, cluster);
        else
            first_cluster = cluster;

        uint32_t cluster_bytes = sectors_per_cluster * 512;
        uint32_t remaining     = size - bytes_written;
        uint32_t to_write      = remaining < cluster_bytes ? remaining : cluster_bytes;
        uint32_t i;

        for (i = 0; i < cluster_bytes; i++)
            buf[i] = 0;
        for (i = 0; i < to_write; i++)
            buf[i] = data[bytes_written + i];

        if (write_cluster(cluster, buf) != 0)
        {
            kfree(buf);
            return -1;
        }

        bytes_written += to_write;
        prev_cluster   = cluster;
    }

    kfree(buf);
    return fat32_create_entry(path, first_cluster, size);
}

int fat32_delete_file(const char* path)
{
	if (!path)
		return -1;

	uint32_t dir_cluster = cwd_cluster;
	uint8_t* buf = kmalloc(sectors_per_cluster * 512);
	if (!buf)
		return -1;

	while (dir_cluster >= 2 && dir_cluster < FAT32_EOC)
	{
		if (read_cluster(dir_cluster, buf) != 0)
			break;

		uint32_t entries = (sectors_per_cluster * 512) / 32;
		uint32_t i;

		for (i = 0; i < entries; i++)
		{
			uint8_t* e = buf + (i * 32);

			if (e[0] == 0x00)
				goto not_found;

			if ((uint8_t)e[0] == 0xE5)
				continue;

			uint8_t attr = e[11];
			if (attr == 0x0F || attr == 0x08)
				continue;

			if (attr & FAT_ATTR_DIRECTORY)
				continue;

			// Build filename
			char name[13];
			int n = 0;
			int j;
			for (j = 0; j < 8 && e[j] != ' '; j++)
				name[n++] = e[j];
			if (e[8] != ' ')
			{
				name[n++] = '.';
				for (j = 8; j < 11 && e[j] != ' '; j++)
					name[n++] =e[j];
			}
			name[n] = '\0';

			// case insensitive compare
			int match = 1;
			for (j = 0; name[j] || path[j]; j++)
			{
				if (to_upper(name[j]) != to_upper(path[j])) { match = 0; break; }
			}

			if (match)
			{
				// Free cluster chain
				uint32_t file_cluster = ((uint32_t)read16(e, 20) << 16) | read16(e, 26);
				if (file_cluster >= 2)
					fat_free_chain(file_cluster);

				// Mark as deleted
				e[0] = 0xE5;
				write_cluster(dir_cluster, buf);
				kfree(buf);
				return 0;
			}
		}

		dir_cluster = fat_next_cluster(dir_cluster);

	}

not_found:
	kfree(buf);
	return -1;

}

int fat32_rename_file(const char* oldpath, const char* newpath)
{
	if (!oldpath || !newpath)
		return -1;

	uint32_t dir_cluster = cwd_cluster;
	uint8_t* buf = kmalloc(sectors_per_cluster * 512);
	if (!buf)
		return -1;

	while (dir_cluster >= 2 && dir_cluster < FAT32_EOC)
	{
		if (read_cluster(dir_cluster, buf) != 0)
			break;
		uint32_t entries = (sectors_per_cluster * 512) / 32;
		uint32_t i;

		for (i = 0; i < entries; i++)
		{
			uint8_t* e = buf + (i * 32);

			if (e[0] == 0x00)
				goto not_found;

			if ((uint8_t)e[0] == 0xE5)
				continue;

			uint8_t attr = e[11];
			if (attr == 0x0F || attr == 0x08)
				continue;

			// Build the filename
			char name[13];
			int n = 0;
			int j;
			for (j = 0; j < 8 &&  e[j] != ' '; j++)
				name[n++] = e[j];
			if (e[8] != ' ')
			{
				name[n++] = '.';
				for (j = 8; j < 11 && e[j] != ' '; j++)
					name[n++] = e[j];
			}
			name[n] = '\0';

			// Case insensitive compare
			int match = 1;
			for (j = 0; name[j] || oldpath[j]; j++)
			{
				if (to_upper(name[j]) != to_upper(oldpath[j])) { match = 0; break; }
			}

			if (match)
			{
				int ni = 0;

				// Space pad name feild
				for (j = 0; j < 11; j++)
					e[j] = ' ';

				// Write new base name in uppercase
				ni = 0;
				j = 0;
				while (newpath[ni] && newpath[ni] != '.' && j < 8)
					e[j++] = to_upper(newpath[ni++]);

				if (newpath[ni] == '.')
				{
					ni++;
					j = 8;
					while (newpath[ni] && j < 11)
						e[j++] = to_upper(newpath[ni++]);
				}

				write_cluster(dir_cluster, buf);
				kfree(buf);
				return 0;

			}

		}

		dir_cluster = fat_next_cluster(dir_cluster);

	}
not_found:
	kfree(buf);
	return -1;
}

int fat32_mkdir(const char* name)
{
	if (!name)
		return -1;

	// Allocate a cluster for the new directory
	uint32_t new_cluster = fat_alloc_cluster();
	if (new_cluster == 0)
		return -1;

	// Zero out the cluster
	uint8_t* buf = kmalloc(sectors_per_cluster * 512);
	if (!buf)
		return -1;

	uint32_t i;
	for (i = 0; i < sectors_per_cluster * 512; i++)
		buf[i] = 0;

	//Write '.' entry (points to itself)
	uint8_t* dot = buf;
	dot[0] = '.';
	for (i = 1; i < 11; i++) dot[i] = ' ';
	dot[11] = FAT_ATTR_DIRECTORY;
	dot[20] = (uint8_t)((new_cluster >> 16) & 0xFF);
	dot[21] = (uint8_t)((new_cluster >> 24) & 0xFF);
	dot[26] = (uint8_t)(new_cluster & 0xFF);
	dot[27] = (uint8_t)((new_cluster >> 8) & 0xFF);

	// Wrie '..' entry (points to parent)
	uint8_t* dotdot = buf + 32;
	dotdot[0] = '.';
	dotdot[1] = '.';
	for (i = 2; i < 11; i++) dotdot[i] = ' ';
	dotdot[11] = FAT_ATTR_DIRECTORY;
	dotdot[20] = (uint8_t)((cwd_cluster >> 16) & 0xFF);
	dotdot[21] = (uint8_t)((cwd_cluster >> 24) & 0xFF);
	dotdot[26] = (uint8_t)(cwd_cluster & 0xFF);
	dotdot[27] = (uint8_t)((cwd_cluster >> 8) & 0xFF);

	if (write_cluster(new_cluster, buf) != 0)
	{
		kfree(buf);
		return -1;
	}

	kfree(buf);

	// Create directory entry in current directory
	// Reuse create_entery but with directory attribute
	uint32_t dir_cluster = cwd_cluster;
	buf = kmalloc(sectors_per_cluster * 512);
	if (!buf)
		return -1;

	while (dir_cluster >= 2 && dir_cluster < FAT32_EOC)
	{
		if (read_cluster(dir_cluster, buf) != 0)
			break;

		uint32_t entries = (sectors_per_cluster * 512) / 32;

		for (i = 0; i < entries; i++)
		{
			uint8_t* e = buf + (i * 32);

			if (e[0] == 0x00 || (uint8_t)e[0] == 0XE5)
			{
				int ni, j;

				for (j = 0; j < 32; j++)
					e[j] = 0;
				for (j = 0; j < 11; j++)
					e[j] = ' ';

				ni = 0;
				j = 0;
				while (name[ni] && name[ni] != '.' && j < 8)
					e[j++] = to_upper(name[ni++]);

				if (name[ni] == '.')
				{
					ni++;
					j = 8;
					while (name[ni] && j < 11)
						e[j++] = to_upper(name[ni++]);
				}

				e[11] = FAT_ATTR_DIRECTORY;
				e[20] = (uint8_t)((new_cluster >> 16) & 0xFF);
				e[21] = (uint8_t)((new_cluster >> 24) & 0xFF);
				e[26] = (uint8_t)(new_cluster & 0xFF);
				e[27] = (uint8_t)((new_cluster >> 8) & 0xFF);

				if (write_cluster(dir_cluster, buf) != 0)
				{
					kfree(buf);
					return -1;
				}

				kfree(buf);
				return 0;

			}
		}

		dir_cluster = fat_next_cluster(dir_cluster);

	}

	kfree(buf);
	return -1;
}

int fat32_cd(const char* name)
{
	if (!name)
		return -1;

	// Handle cd
	if (name[0] == '.' && name[1] == '.' && name[2] == '\0')
	{
		// Read the current directory to find '..' entry
		uint8_t* buf = kmalloc(sectors_per_cluster * 512);
		if (!buf)
			return -1;

		if (read_cluster(cwd_cluster, buf) != 0)
		{
			kfree(buf);
			return -1;
		}

		// '..' is ALWAYS the second entry
		uint8_t* dotdot = buf + 32;
		uint32_t parent = ((uint32_t)read16(dotdot, 20) << 16) | read16(dotdot, 26);

		kfree(buf);

		// if parent is 0, we are already in root
		if (parent == 0)
			parent = root_cluster;

		cwd_cluster = parent;

		// Update path. Remove last component
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

	// Handle cd /
	if (name[0] == '/' && name[1] == '\0')
	{
		cwd_cluster = root_cluster;
		cwd_path[0] = '/';
		cwd_path[1] = '\0';
		return 0;
	}

	// Find the named directory in current directory
	uint32_t dir_cluster = cwd_cluster;
	uint8_t* buf = kmalloc(sectors_per_cluster * 512);
	if (!buf)
		return -1;

	while (dir_cluster >= 2 && dir_cluster < FAT32_EOC)
	{
		if (read_cluster(dir_cluster, buf) != 0)
			break;

		uint32_t entries = (sectors_per_cluster * 512) / 32;
		uint32_t i;

		for (i = 0; i < entries; i++)
		{
			uint8_t* e = buf + (i * 32);

			if (e[0] == 0x00)
				goto not_found;

			if ((uint8_t)e[0] == 0xE5)
				continue;

			uint8_t attr = e[11];
			if (!(attr & FAT_ATTR_DIRECTORY))
				continue;

			if (attr == 0x0F)
				continue;

			// Build name
			char dname[13];
			int n = 0;
			int j;
			for (j = 0; j < 8 && e[j] != ' ';j++)
				dname[n++] = e[j];

		dname[n] = '\0';

		// Case insensitive compare
		int match = 1;
		for (j = 0; dname[j] || name[j]; j++)
		{
			if (to_upper(dname[j]) != to_upper(name[j])) { match = 0; break; }
		}

		if (match)
		{
			uint32_t target = ((uint32_t)read16(e, 20) << 16) | read16(e, 26);
			cwd_cluster = target;

			// Update the path string
			int len = 0;
			while (cwd_path[len]) len++;
			if (len > 1)
				cwd_path[len++] = '/';
			int ni = 0;
			while (name[ni] && len < 255)
				cwd_path[len++] = to_upper(name[ni++]);
			cwd_path[len] = '\0';

			kfree(buf);
			return 0;
		}
		}

		dir_cluster = fat_next_cluster(dir_cluster);
	}
not_found:
	kfree(buf);
	return -1;
}

static int fat32_create_entry(const char* name, uint32_t cluster, uint32_t size)
{
    uint32_t dir_cluster = cwd_cluster;
    uint8_t* buf = kmalloc(sectors_per_cluster * 512);
    if (!buf)
        return -1;

    while (dir_cluster >= 2 && dir_cluster < FAT32_EOC)
    {
        if (read_cluster(dir_cluster, buf) != 0)
            break;

        uint32_t entries = (sectors_per_cluster * 512) / 32;
        uint32_t i;

        for (i = 0; i < entries; i++)
        {
            uint8_t* e = buf + (i * 32);

            if (e[0] == 0x00 || (uint8_t)e[0] == 0xE5)
            {
                int ni, j;

                // Clear the entry
                for (j = 0; j < 32; j++)
                    e[j] = 0;

                // Space pad name field
                for (j = 0; j < 11; j++)
                    e[j] = ' ';

                // Write base name in uppercase
                ni = 0;
                j  = 0;
                while (name[ni] && name[ni] != '.' && j < 8)
                    e[j++] = to_upper(name[ni++]);

                // Write extension in uppercase
                if (name[ni] == '.')
                {
                    ni++;
                    j = 8;
                    while (name[ni] && j < 11)
                        e[j++] = to_upper(name[ni++]);
                }

                e[11] = FAT_ATTR_ARCHIVE;

                // Cluster high (offset 20)
                e[20] = (uint8_t)((cluster >> 16) & 0xFF);
                e[21] = (uint8_t)((cluster >> 24) & 0xFF);

                // Cluster low (offset 26)
                e[26] = (uint8_t)(cluster & 0xFF);
                e[27] = (uint8_t)((cluster >> 8) & 0xFF);

                // File size (offset 28)
                e[28] = (uint8_t)(size & 0xFF);
                e[29] = (uint8_t)((size >> 8) & 0xFF);
                e[30] = (uint8_t)((size >> 16) & 0xFF);
                e[31] = (uint8_t)((size >> 24) & 0xFF);

                if (write_cluster(dir_cluster, buf) != 0)
                {
                    kfree(buf);
                    return -1;
                }

                kfree(buf);
                return 0;
            }
        }

        dir_cluster = fat_next_cluster(dir_cluster);
    }

    kfree(buf);
    return -1;
}

uint32_t fat32_get_cwd_cluster(void)
{
	return cwd_cluster;
}

const char* fat32_get_cwd_path(void)
{
	return cwd_path;
}
