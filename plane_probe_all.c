/*
 * plane_probe_all.c — probe ALL display planes + cursors + pipe state (read-only)
 * Build:  cc -std=c11 -framework IOKit -framework CoreFoundation plane_probe_all.c -o plane_probe_all
 * Usage:  ./plane_probe_all   (prints all registers, no writes)
 *
 * Goal: find which plane WindowServer actually uses to scan out the desktop.
 * If plane 1A is disabled (CTL bit31 clear) pre-test, the desktop must be on
 * another plane — restoring only plane 1A would be restoring the WRONG plane.
 */
#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define KSELECTOR_READMMIO 6

static io_connect_t conn;
static uint32_t rd(uint32_t reg) {
    uint64_t in = reg, out = 0;
    uint32_t outCnt = 1;
    IOReturn kr = IOConnectCallMethod(conn, KSELECTOR_READMMIO,
        &in, 1, NULL, 0, &out, &outCnt, NULL, NULL);
    if (kr != kIOReturnSuccess) { fprintf(stderr, "ReadMMIO(0x%04X) fail 0x%x\n", reg, kr); return 0xDEADBEEF; }
    return (uint32_t)out;
}

/* PLANE_CTL bit 31 = enable (SKL+) */
static const char* en(uint32_t ctl) { return (ctl & 0x80000000u) ? "EN " : "-- "; }

static void plane(const char* name, uint32_t base) {
    uint32_t ctl   = rd(base + 0x80);
    uint32_t offs  = rd(base + 0x84);
    uint32_t strd  = rd(base + 0x88);
    uint32_t pos   = rd(base + 0x8C);
    uint32_t size  = rd(base + 0x90);
    uint32_t surf  = rd(base + 0x9C);
    uint32_t live  = rd(base + 0xAC);
    printf("%-8s %s CTL=0x%08X OFF=0x%08X STR=0x%08X POS=0x%08X SZ=0x%08X SURF=0x%08X LIVE=0x%08X\n",
           name, en(ctl), ctl, offs, strd, pos, size, surf, live);
}

static void cursor(const char* name, uint32_t reg) {
    uint32_t ctl = rd(reg);
    uint32_t pos = rd(reg + 4);
    uint32_t surf = rd(reg + 8);
    printf("%-8s %s CTL=0x%08X POS=0x%08X SURF=0x%08X\n", name, en(ctl), ctl, pos, surf);
}

int main(void) {
    io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault,
                          IOServiceMatching("MyIntelGPU"));
    if (!svc) { fprintf(stderr, "kext not found\n"); return 1; }
    kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
    if (kr != kIOReturnSuccess) { fprintf(stderr, "open fail 0x%x\n", kr); return 1; }
    IOObjectRelease(svc);

    printf("=== Primary planes (SKL+ layout) ===\n");
    plane("P1A", 0x70100);
    plane("P1B", 0x71100);
    plane("P1C", 0x72100);
    plane("P2A", 0x70200);
    plane("P2B", 0x71200);
    plane("P2C", 0x72200);
    plane("P3A", 0x70300);
    plane("P3B", 0x71300);
    plane("P3C", 0x72300);
    plane("P4A", 0x70400);
    plane("P4B", 0x71400);
    plane("P4C", 0x72400);

    printf("=== Cursors ===\n");
    cursor("CUR_A", 0x70080);
    cursor("CUR_B", 0x71080);
    cursor("CUR_C", 0x72080);
    cursor("CUR_D", 0x73080);

    printf("=== Pipe state ===\n");
    printf("PIPECONF_A  = 0x%08X (0x70008)\n", rd(0x70008));
    printf("PIPECONF_B  = 0x%08X (0x71008)\n", rd(0x71008));
    printf("PIPECONF_C  = 0x%08X (0x72008)\n", rd(0x72008));
    printf("PIPESRC_A   = 0x%08X (0x7000C)\n", rd(0x7000C));
    printf("PIPESRC_B   = 0x%08X (0x7100C)\n", rd(0x7100C));
    printf("PIPESRC_C   = 0x%08X (0x7200C)\n", rd(0x7200C));
    printf("PIPEA_CTL   = 0x%08X (0x70080 trans conf?)\n", rd(0x70080));
    printf("TRANS_DDI_A = 0x%08X (0x64100)\n", rd(0x64100));
    printf("TRANS_DDI_B = 0x%08X (0x64110)\n", rd(0x64110));
    printf("TRANS_DDI_C = 0x%08X (0x64120)\n", rd(0x64120));
    printf("DPLL_CTRL1  = 0x%08X (0x6C040)\n", rd(0x6C040));
    printf("DPLL_CTRL2  = 0x%08X (0x6C044)\n", rd(0x6C044));
    printf("DPLL_A_CTL  = 0x%08X (0x6C048)\n", rd(0x6C048));
    printf("DPLL_B_CTL  = 0x%08X (0x6C04C)\n", rd(0x6C04C));
    printf("DPLL_C_CTL  = 0x%08X (0x6C050)\n", rd(0x6C050));
    printf("DPLL_STATUS = 0x%08X (0x6C060)\n", rd(0x6C060));
    printf("PP_STATUS   = 0x%08X (0x61200)\n", rd(0x61200));
    printf("PP_CONTROL  = 0x%08X (0x61204)\n", rd(0x61204));
    printf("BLC_PWM_CTL = 0x%08X (0x61250)\n", rd(0x61250));

    IOServiceClose(conn);
    return 0;
}