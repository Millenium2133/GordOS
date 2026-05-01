#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// Primary ATA ports
#define ATA_DATA		0x1F0
#define ATA_ERROR		0x1F1
#define ATA_SECTOR_COUNT	0x1F2
#define ATA_LBA_LOW		0x1F3
#define ATA_LBA_MID		0x1F4
#define ATA_LBA_HIGH		0x1F5
#define ATA_DRIVE_SELECT	0x1F6
#define ATA_COMMAND		0x1F7
#define ATA_STATUS		0x1F7

// Status register bits
#define ATA_STATUS_BSY		0x80 //Busy
#define ATA_STATUS_DRQ		0x08 // Data Request
#define ATA_STATUS_ERR		0x01 // Self explanitory (error)

// Commands
#define ATA_CMD_READ		0x20
#define ATA_CMD_WRITE		0x30

// Read sectors from disk
// lba = which sector to start at
// count = how many sectors to read
// buffer = where to put the data
int ata_read(uint32_t lba, uint8_t count, void* buffer);

// Write sectors to disk
int ata_write(uint32_t lba, uint8_t count, const void* buffer);

// Initialoize and detect ATA Drive
int ata_init(void);

#endif






