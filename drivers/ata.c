#include "ata.h"
#include "pic.h"
#include "vga.h"

//16 bit version of inb/outb
static inline uint16_t inw(uint16_t port)
{
	uint16_t ret;
	asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

static inline void outw(uint16_t port, uint16_t value)
{
	asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

// Delay wait (for debugging on real hardware)
static void ata_400ns_delay(void)
{
	inb(0x3F6);
	inb(0x3F6);
	inb(0x3F6);
	inb(0x3F6);
}

// Pring hex (for more real hardware debugging)
static void print_hex8(uint8_t val)
{
	const char* hex = "0123456789ABCDEF";
	terminal_putchar(hex[(val >> 4) & 0xF]);
	terminal_putchar(hex[val & 0xF]);
}

// Wait till drive is no longer busy
static int ata_wait(void)
{
	uint8_t status;

	// Wait for BSY to clear
	while ((status = inb(ATA_STATUS)) & ATA_STATUS_BSY);

	// error checking
	if (status & ATA_STATUS_ERR)
		return -1;
	return 0;
}

// wait until drive is ready
static int ata_wait_drq(void)
{
	uint8_t status;
	while (1)
	{
		status = inb(ATA_STATUS);
	
		if (status & ATA_STATUS_ERR)
			return -1;

		if (status & ATA_STATUS_DRQ)
			return 0;
	}
}

// Sent the LBA and sector count to the drive
static void ata_setup(uint32_t lba, uint8_t count)
{
	// select master drive, LBA mode, and top 4 bits of LBA
	outb(ATA_DRIVE_SELECT, 0xE0 | ((lba >> 24) & 0x0F));

	// send sector count
	outb(ATA_SECTOR_COUNT, count);

	// Send LBA address in 3 bytes
	outb(ATA_LBA_LOW, (uint8_t)(lba & 0xFF));
	outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
	outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
}

int ata_init(void)
{
	// Debug
	uint8_t status = inb(ATA_STATUS);
	terminal_writestring("ATA status at entry: 0x");
	print_hex8(status);
	terminal_putchar('\n');

	//Select master drive
	outb(ATA_DRIVE_SELECT, 0xA0);
	ata_400ns_delay();

	status = inb(ATA_STATUS);
	terminal_writestring("ATA Status after select: 0x");
	print_hex8(status);
	terminal_putchar('\n');

	if (status == 0x00 || status == 0xFF)
		return -1;

	// Clear LBA and sector count registers
	outb(ATA_SECTOR_COUNT, 0);
	outb(ATA_LBA_LOW, 0);
	outb(ATA_LBA_MID, 0);
	outb(ATA_LBA_HIGH, 0);

	// send IDENTIFY command
	outb(ATA_COMMAND, 0xEC);

	// Read status
	status = inb(ATA_STATUS);
	if (status == 0) // 0 = no drive present
		return -1;

	// Wait for BSY to clear
	if (ata_wait() !=0)
		return -1;

	//check it is indeed ATA and not ATAPI
	if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HIGH) != 0)
		return -1;

	// wait for DRQ
	if (ata_wait_drq() != 0)
		return -1;

	// Read and discard the 256 words of identify data
	for (int i = 0; i < 256; i++)
		inw(ATA_DATA);

	return 0;
}

int ata_read(uint32_t lba, uint8_t count, void* buffer)
{
	if (!buffer || count == 0)
		return -1;

	// wait for drive to be ready
	if (ata_wait() != 0)
		return -1;

	ata_setup(lba, count);

	// send read command
	outb(ATA_COMMAND, ATA_CMD_READ);

	uint16_t* buf = (uint16_t*)buffer;

	for (int sector = 0; sector < count; sector++)
	{
		// wait for data to be ready
		if (ata_wait_drq() != 0)
			return -1;

		// read 256 words n shit
		for (int i = 0; i < 256; i++)
			buf[i] = inw(ATA_DATA);

		// Advance buffer by one sector
		buf += 256;
	}
	return 0;
}

int ata_write(uint32_t lba, uint8_t count, const void* buffer)
{
	if (!buffer || count == 0)
		return -1;

	if (ata_wait() != 0)
		return -1;

	ata_setup(lba, count);

	//send write command
	outb(ATA_COMMAND, ATA_CMD_WRITE);

	const uint16_t* buf = (const uint16_t*)buffer;

	for (int sector = 0; sector < count; sector++)
	{
		// Wait for drive to be ready for data
		if (ata_wait_drq() != 0)
			return -1;

		// write 256 words
		for (int i = 0; i < 256; i++)
			outw(ATA_DATA, buf[i]);

		// Advance buffer by one sector
		buf += 256;
	}
	// flush write cache
	outb(ATA_COMMAND, 0xE7);
	ata_wait();

	return 0;
}
