#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../drivers/debug.h"
#include "../net/net.h"
#include "../stdlibs/string.h"
#include "../kernel/thread.h"
#include "../kernel/time.h"

void netcat_cmd(const char *arg) {
    if (!arg || !arg[0]) {
        printf("Usage: netcat <ip:port>\n");
        return;
    }

    const char *colon = arg;
    while (*colon && *colon != ' ' && *colon != ':') colon++;
    if (*colon != ':') {
        printf("Usage: netcat <ip:port>\n");
        return;
    }

    char ip_str[32];
    size_t ip_len = (size_t)(colon - arg);
    if (ip_len == 0 || ip_len >= sizeof(ip_str)) {
        printf("[netcat] Ungueltige IP-Adresse.\n");
        return;
    }
    memcpy(ip_str, arg, ip_len);
    ip_str[ip_len] = '\0';

    uint32_t ip = 0;
    if (net_parse_ipv4(ip_str, &ip) != 0) {
        printf("[netcat] Konnte IP nicht parsen.\n");
        return;
    }

    int port = atoi(colon + 1);
    if (port <= 0 || port > 65535) {
        printf("[netcat] Ungueltiger Port.\n");
        return;
    }

    printf("[netcat] Verbinde zu %s:%d ...\n", ip_str, port);
    if (net_tcp_connect(ip, (uint16_t)port, 5000) != 0) {
        printf("[netcat] Verbindung fehlgeschlagen.\n");
        return;
    }

    printf("[netcat] Verbunden. ESC beendet, ENTER sendet aktuelle Zeile.\n");

    char input[256];
    uint16_t input_len = 0;
    uint8_t rx_buffer[256];

    int running = 1;
    while (running) {
        int received = net_tcp_recv(rx_buffer, sizeof(rx_buffer), 50);
        if (received > 0) {
            for (int i = 0; i < received; ++i) {
                char c = (char)rx_buffer[i];
                if (c == '\r') {
                    continue;
                }
                printf("%c", c);
            }
        } else if (received == 0 && !net_tcp_connected()) {
            running = 0;
        }

        unsigned int key = getkey_async();
        if (!key) {
            thread_yield();
            continue;
        }

        if (key & 0x80) {
            continue; // ignore key releases
        }

        if (key == SC_ESC) {
            printf("\n[netcat] Abbruch durch Benutzer.\n");
            break;
        }

        if (key == SC_BACKSPACE) {
            if (input_len > 0) {
                input_len--;
                printf("\b \b");
            }
            continue;
        }

        if (key == SC_ENTER) {
            if (input_len > 0) {
                input[input_len++] = '\n';
                net_tcp_send((const uint8_t*)input, input_len);
                input_len = 0;
            } else {
                uint8_t newline = '\n';
                net_tcp_send(&newline, 1);
            }
            printf("\n");
            continue;
        }

        char chr = char_from_key(key);
        if (chr) {
            if (input_len < sizeof(input) - 2) {
                input[input_len++] = chr;
                printf("%c", chr);
            }
        }
    }

    net_tcp_close();
    printf("[netcat] Verbindung geschlossen.\n");
}

void net_ping_cmd(const char *arg) {
    if (!arg || !arg[0]) {
        printf("Usage: ping <ip> [count]\n");
        return;
    }

    char tmp[64];
    strncpy(tmp, arg, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *ip_str = strtok(tmp, " ");
    char *count_str = strtok(NULL, " ");
    if (!ip_str) {
        printf("Usage: ping <ip> [count]\n");
        return;
    }

    uint32_t ip = 0;
    if (net_parse_ipv4(ip_str, &ip) != 0) {
        printf("[ping] Konnte IP nicht parsen.\n");
        return;
    }

    int count = 4;
    if (count_str) {
        int parsed = atoi(count_str);
        if (parsed > 0) {
            count = parsed;
        }
    }

    if (!count_str) {
        printf("[ping] Sende bis zu %d Echo-Anfragen an %s (Standard)\n", count, ip_str);
    } else {
        printf("[ping] Sende %d Echo-Anfragen an %s\n", count, ip_str);
    }

    for (int i = 0; i < count; ++i) {
        printf("[ping] Versuch %d start\n", i+1);
        int rtt = net_ping(ip, 1500);
        if (rtt >= 0) {
            printf("Antwort von %s: Zeit=%d ms\n", ip_str, rtt);
        } else {
            printf("Zeitueberschreitung fuer %s\n", ip_str);
        }
        printf("[ping] Versuch %d ende\n", i+1);
        if (i + 1 < count) {
            sleep(500);
        }
    }
}

