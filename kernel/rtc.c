#include <stdint.h>
#include "../drivers/ports.h"
#include "../stdlibs/string.h"
#include "perf.h"
#include "rtc.h"

static uint64_t g_epoch_at_boot = 0;   // seconds since 1970-01-01
static uint64_t g_boot_micros   = 0;   // micros() at sampling time

static uint8_t cmos_read(uint8_t reg){ port_byte_out(0x70, reg); return port_byte_in(0x71); }

// Read RTC registers in a stable way: ensure no update-in-progress and
// re-read until two consecutive samples match (avoids boundary glitches).
static void rtc_read_consistent(uint8_t* psec, uint8_t* pmin, uint8_t* phour,
                                uint8_t* pday, uint8_t* pmon, uint8_t* pyear, uint8_t* pcent, uint8_t* pregB){
    uint8_t sec,min,hour,day,mon,year,cent,regB;
    uint8_t sec2,min2,hour2,day2,mon2,year2,cent2,regB2;
    for(;;){
        // wait not updating
        while (cmos_read(0x0A) & 0x80) { /* UIP */ }
        sec  = cmos_read(0x00);
        min  = cmos_read(0x02);
        hour = cmos_read(0x04);
        day  = cmos_read(0x07);
        mon  = cmos_read(0x08);
        year = cmos_read(0x09);
        regB = cmos_read(0x0B);
        cent = cmos_read(0x32);
        // sample again and compare
        while (cmos_read(0x0A) & 0x80) { }
        sec2  = cmos_read(0x00);
        min2  = cmos_read(0x02);
        hour2 = cmos_read(0x04);
        day2  = cmos_read(0x07);
        mon2  = cmos_read(0x08);
        year2 = cmos_read(0x09);
        regB2 = cmos_read(0x0B);
        cent2 = cmos_read(0x32);
        if (sec==sec2 && min==min2 && hour==hour2 && day==day2 && mon==mon2 && year==year2 && regB==regB2 && cent==cent2) break;
    }
    *psec=sec; *pmin=min; *phour=hour; *pday=day; *pmon=mon; *pyear=year; *pregB=regB; *pcent=cent;
}

static int is_leap(int y){ return ((y%4==0) && (y%100!=0)) || (y%400==0); }

static uint32_t days_before_month(int y, int m){
    static const int cum[12]={0,31,59,90,120,151,181,212,243,273,304,334};
    uint32_t d = cum[m-1];
    if (m>2 && is_leap(y)) d++;
    return d;
}

static uint64_t ymd_to_epoch(int y, int m, int d, int hh, int mm, int ss){
    // days since 1970-01-01
    int y0 = y-1;
    uint64_t days = (uint64_t)(y0-1969)*365ULL + (uint64_t)((y0-1968)/4) - (uint64_t)((y0-1900)/100) + (uint64_t)((y0-1600)/400);
    days += days_before_month(y,m) + (uint64_t)(d-1);
    return days*86400ULL + (uint64_t)hh*3600ULL + (uint64_t)mm*60ULL + (uint64_t)ss;
}

static uint8_t bcd_to_bin(uint8_t v){ return (uint8_t)((v & 0x0F) + ((v >> 4) * 10)); }

void time_init_with_rtc(void){
    uint8_t sec,min,hour,day,mon,year,cent,regB;
    rtc_read_consistent(&sec,&min,&hour,&day,&mon,&year,&cent,&regB);

    int bcd = ((regB & 0x04)==0);
    int hour12 = ((regB & 0x02)==0);

    if (bcd){ sec=bcd_to_bin(sec); min=bcd_to_bin(min); hour=bcd_to_bin(hour); day=bcd_to_bin(day); mon=bcd_to_bin(mon); year=bcd_to_bin(year); if (cent) cent=bcd_to_bin(cent); }
    if (hour12){ if (hour & 0x80){ hour = (hour & 0x7F); if (hour != 12) hour = (uint8_t)(hour + 12); } else { if (hour == 12) hour = 0; } }

    int full_year = year + (cent? (cent*100) : (year < 70 ? 2000 : 1900));
    uint64_t epoch = ymd_to_epoch(full_year, mon, day, hour, min, sec);
    g_epoch_at_boot = epoch;
    g_boot_micros   = micros();
}

uint64_t time_now_seconds(void){
    if (g_epoch_at_boot == 0){ // fallback: initialize if not yet
        time_init_with_rtc();
    }
    uint64_t delta_us = micros() - g_boot_micros;
    return g_epoch_at_boot + (delta_us / 1000000ULL);
}

extern uint64_t __udivdi3(uint64_t n, uint64_t d); // provided in perf.c

void time_now_iso(char* buf, int bufsz){
    if (!buf || bufsz < 20){ return; }
    uint64_t t = time_now_seconds();
    // Convert epoch to calendar without 64-bit modulo helper
    uint64_t days = __udivdi3(t, 86400ULL);
    uint32_t sec = (uint32_t)(t - days*86400ULL);
    // Compute date from days since 1970-01-01
    int y=1970; while (1){ int dy = is_leap(y) ? 366 : 365; if (days >= (uint64_t)dy){ days -= dy; y++; } else break; }
    int m=1; while (1){ int md = 31; if (m==2) md = is_leap(y)?29:28; else if (m==4||m==6||m==9||m==11) md=30; if (days >= (uint64_t)md){ days -= md; m++; } else break; }
    int d = (int)days + 1;
    int hh = sec/3600; sec%=3600; int mm = sec/60; int ss = sec%60;
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", y, m, d, hh, mm, ss);
}
