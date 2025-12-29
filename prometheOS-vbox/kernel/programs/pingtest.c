#include "../../include/drivers/display.h"
#include "../../include/drivers/debug.h"
#include "../../include/drivers/net.h"
#include "../../include/lib/string.h"
#include "../../include/time.h"
#include "../../include/thread.h"

// TODO: net_ipv4 and net_ping are not in the stub and will cause linker errors.
// Need to port the full network stack.
void pingtest_cmd(void) {
    uint32_t host = net_ipv4(10,0,2,2);
    printf("[pingtest] Test start\n");
    net_dump_regs();
    printf("[pingtest] seq 0\n");
    int rtt = net_ping(host, 1500);
    printf("[pingtest] result 0 -> %d\n", rtt);
    net_dump_regs();
}
