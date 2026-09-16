/*
 * accel_steps.c — Phase B step-machine trigger
 * Usage:
 *   accel_steps alloc  [mode]   mode: 1=attach-under-this, 2=under-PCI
 *   accel_steps attach [parentSel] 0=this,1=PCI,2=IOResources
 *   accel_steps start            DANGER: publishes nubs (freeze watch!)
 */
#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *kClassName = "MyIntelGPU";

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s alloc|attach|start [arg]\n", argv[0]);
        return 1;
    }
    uint32_t sel;
    uint64_t in[1] = {0};
    if (!strcmp(argv[1], "alloc"))  { sel = 20; if (argc >= 3) in[0] = strtoul(argv[2], NULL, 0); }
    else if (!strcmp(argv[1], "attach")) { sel = 21; if (argc >= 3) in[0] = strtoul(argv[2], NULL, 0); }
    else if (!strcmp(argv[1], "start"))  { sel = 22; }
    else if (!strcmp(argv[1], "pad"))    { sel = 23; }
    else if (!strcmp(argv[1], "batchblit")) { sel = 24; }
    else { fprintf(stderr, "unknown step '%s'\n", argv[1]); return 1; }

    io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault,
                                                   IOServiceMatching(kClassName));
    if (!svc) { fprintf(stderr, "FAIL: MyIntelGPU not found\n"); return 2; }
    io_connect_t conn = 0;
    kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
    IOObjectRelease(svc);
    if (kr != KERN_SUCCESS) { fprintf(stderr, "FAIL: open=0x%x\n", kr); return 3; }

    uint64_t out[1] = {0};
    uint32_t outCnt = 1;
    kr = IOConnectCallMethod(conn, sel, in, 1, NULL, 0, out, &outCnt, NULL, 0);
    IOServiceClose(conn);
    printf("[%s] kr=0x%x result=%llu -> %s\n",
           argv[1], kr, out[0], (kr == KERN_SUCCESS && out[0] == 1) ? "OK" : "FAIL");
    return (kr == KERN_SUCCESS && out[0] == 1) ? 0 : 4;
}
