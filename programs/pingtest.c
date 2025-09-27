#include "../drivers/display.h"
#include "../drivers/debug.h"
#include "../net/net.h"
#include "../stdlibs/string.h"
#include "../kernel/time.h"
#include "../kernel/thread.h"

void pingtest_cmd(void) {
    uint32_t host = net_ipv4(10,0,2,2);
    printf("[pingtest] Test start\n");
    net_dump_regs();
    printf("[pingtest] seq 0\n");
    int rtt = net_ping(host, 1500);
    printf("[pingtest] result 0 -> %d\n", rtt);
    net_dump_regs();
}
