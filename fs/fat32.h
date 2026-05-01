#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

typedef struct
{
	uint8_t jump[3];		//Jump instruction to boot code
	uint8_t oem[8];			// OEM name
	uint16_t bytes_per_sector;	// almost always 512
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors;	// Sectors before FAT
	uint8_t fat_count;		// number of FAT's
	uint16_t root_entry_count;	//0 for FAT32
	uint16_t total_sectors_16;	// 0 for FAT32
	uint8_t media_type;
	uint16_t fat_size_16;		// 0 For FAT32
	uint16_t sectors_per_track;
	uint16_t head_count;
	uint32_t hidden_sectors;
	uint32_t total_sectors_32;

	// FAT32 Specific feilds
	uint32_t fat_size_32;		// Sectors per FAT
	uint16_t ext_flags;
	uint16_t fs_version;
	uint32_t root_cluster;		// cluster number of root directory
	uint16_t fs_info;
	uint16_t backup_boot_sector;
	uint8_t reserved[12];
	uint8_t drive_number;
	uint8_t reserved1;
	uint8_t boot_signature;
	uint32_t volume_id;
	uint8_t volume_label[11];
	uint8_t fs_type[8];
} __attribute__((packed)) fat32_bpb_t;

// A directory entry
typedef struct
{
	uint8_t name[11];		// 8.3 filename
	uint8_t attributes;		// File attributes
	uint8_t reserved;
	uint8_t creation_time_ms;
	uint16_t creation_time;
	uint16_t creation_date;
	uint16_t access_date;
	uint16_t cluster_high;		// High 16 bits of starting cluster
	uint16_t modified_time;
	uint16_t modified_date;
	uint16_t cluster_low;		// Low 16 bits of starting cluster
	uint32_t file_size;		// Size in bytes
} __attribute__((packed)) fat32_entry_t;

// File attributes
#define FAT_ATTR_READONLY 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE 0x20

// Special cluster values
#define FAT32_EOC 0x0FFFFFF8  // End of chain
#define FAT32_FREE 0x00000000  // Free cluster

// Functions
int fat32_init(void);
int fat32_read_file(const char* path, void* buffer, uint32_t* size);
int fat32_list_dir(const char* path);

#endif
