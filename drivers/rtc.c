#include "rtc.h"
#include "pic.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS 0x04
#define RTC_DAY 0x07
#define RTC_MONTH 0x08
#define RTC_YEAR 0x09

#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B

static uint8_t cmos_read(uint8_t reg) 
{
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t val)
{
    return (val & 0x0F) + ((val >> 4) * 10);
}

void rtc_read_time(rtc_time_t* t)
{
    // Wait for any in progress update to finish, then read twice
    // until 2 consecitive snapshots match
    rtc_time_t prev = {0};
    do
    {
        while (cmos_read(RTC_STATUS_A) & 0x80);
        prev.seconds = cmos_read(RTC_SECONDS);
        prev.minutes = cmos_read(RTC_MINUTES);
        prev.hours = cmos_read(RTC_HOURS);
        prev.day = cmos_read(RTC_DAY);
        prev.month = cmos_read(RTC_MONTH);
        prev.year = cmos_read(RTC_YEAR);

        while (cmos_read(RTC_STATUS_A) & 0x80);
        t->seconds = cmos_read(RTC_SECONDS);
        t->minutes = cmos_read(RTC_MINUTES);
        t->hours = cmos_read(RTC_HOURS);
        t->day = cmos_read(RTC_DAY);
        t->month = cmos_read(RTC_MONTH);
        t->year = cmos_read(RTC_YEAR);
    } while (prev.seconds != t->seconds || prev.minutes != t->minutes || prev.hours != t->hours ||
             prev.day != t->day || prev.month != t->month || prev.year != t->year);
    
    uint8_t sb = cmos_read(RTC_STATUS_B);

    // Convert BCD to binary if bit 2 of status register B is clear
    if (!(sb & 0x04))
    {
        uint8_t pm = t->hours & 0x80;
        t->seconds = bcd_to_bin(t->seconds);
        t->minutes = bcd_to_bin(t->minutes);
        t->hours = bcd_to_bin(t->hours & 0x7F) | pm;
        t->day = bcd_to_bin(t->day);
        t->month = bcd_to_bin(t->month);
        t->year = bcd_to_bin(t->year);
    }
    
// Convert 12 hour format to 24 hour format if bit 1 of status B is clear
    if (!(sb & 0x02))
    {
        uint8_t pm = t->hours & 0x80;
        t->hours &= 0x7F;
        if (pm && t->hours < 12)
            t->hours += 12;
        else if (!pm && t->hours == 12)
            t->hours = 0;
    }

    t->year += 2000; // Assume RTC year is 2000-2099

}