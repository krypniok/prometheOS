#include <stdint.h>
#include "../include/io.h"
#include "../include/rtc.h"
#include "../include/drivers/timer.h"

// Fallback für fehlendes perf.h/micros()
// Wir nutzen den System-Timer (1ms Auflösung) aus drivers/timer.h
static uint64_t micros(void) {
    return (uint64_t)timer_ticks * 1000ULL;
}

static uint64_t g_epoch_at_boot = 0;
static uint64_t g_boot_micros   = 0;

static uint8_t cmos_read(uint8_t reg){ outb(0x70, reg); return inb(0x71); }

// Liest RTC stabil aus (verhindert Glitches während des Updates)
static void rtc_read_consistent(uint8_t* psec, uint8_t* pmin, uint8_t* phour,
                                uint8_t* pday, uint8_t* pmon, uint8_t* pyear, uint8_t* pcent, uint8_t* pregB){
    uint8_t sec,min,hour,day,mon,year,cent,regB;
    uint8_t sec2,min2,hour2,day2,mon2,year2,cent2,regB2;
    for(;;){
        // Warten bis Update-In-Progress (UIP) Bit gelöscht ist
        while (cmos_read(0x0A) & 0x80) { }
        sec  = cmos_read(0x00);
        min  = cmos_read(0x02);
        hour = cmos_read(0x04);
        day  = cmos_read(0x07);
        mon  = cmos_read(0x08);
        year = cmos_read(0x09);
        regB = cmos_read(0x0B);
        cent = cmos_read(0x32);
        
        // Zweite Lesung zum Vergleich
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
    if (g_epoch_at_boot == 0){
        time_init_with_rtc();
    }
    uint64_t delta_us = micros() - g_boot_micros;
    return g_epoch_at_boot + (delta_us / 1000000ULL);
}

// 64-bit Division Helper (um libgcc Abhängigkeit zu vermeiden)
uint64_t __udivdi3(uint64_t n, uint64_t d) {
    if (d == 0) return 0xFFFFFFFFFFFFFFFFULL;
    uint64_t q = 0;
    uint64_t r = 0;
    for (int i = 63; i >= 0; --i) {
        r = (r << 1) | ((n >> i) & 1ULL);
        if (r >= d) { r -= d; q |= (1ULL << i); }
    }
    return q;
}

// Hilfsfunktion für Integer zu String (mit Padding)
static void int_to_str_pad(int val, char* buf, int pad) {
    char tmp[16];
    int i = 0;
    if (val == 0) tmp[i++] = '0';
    else {
        while (val > 0) {
            tmp[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    while (i < pad) tmp[i++] = '0';
    while (i > 0) *buf++ = tmp[--i];
}

void time_now_iso(char* buf, int bufsz){
    if (!buf || bufsz < 20){ return; }
    uint64_t t = time_now_seconds();
    uint64_t days = __udivdi3(t, 86400ULL);
    uint32_t sec = (uint32_t)(t - days*86400ULL);
    
    int y=1970; while (1){ int dy = is_leap(y) ? 366 : 365; if (days >= (uint64_t)dy){ days -= dy; y++; } else break; }
    int m=1; while (1){ int md = 31; if (m==2) md = is_leap(y)?29:28; else if (m==4||m==6||m==9||m==11) md=30; if (days >= (uint64_t)md){ days -= md; m++; } else break; }
    int d = (int)days + 1;
    int hh = sec/3600; sec%=3600; int mm = sec/60; int ss = sec%60;

    // Manuelle Formatierung ohne sprintf
    char *p = buf;
    int_to_str_pad(y, p, 4); p += 4; *p++ = '-';
    int_to_str_pad(m, p, 2); p += 2; *p++ = '-';
    int_to_str_pad(d, p, 2); p += 2; *p++ = ' ';
    int_to_str_pad(hh, p, 2); p += 2; *p++ = ':';
    int_to_str_pad(mm, p, 2); p += 2; *p++ = ':';
    int_to_str_pad(ss, p, 2); p += 2; *p++ = 0;
}