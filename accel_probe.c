/* accel_probe.c — drive surface flow on live MyIntelAccelerator node
 * sel7 SetIDMode(mode) · sel6 SetShapeBacking(region 1920x1080)
 * sel14 WriteLock · sel10 Flush  → triggers kernel SURF-FMT probe */
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <string.h>

static const char *kClassName = "MyIntelAccelerator";

int main(void)
{
    io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault,
                                                   IOServiceMatching(kClassName));
    if (!svc) { fprintf(stderr, "FAIL: MyIntelAccelerator not found\n"); return 2; }
    io_connect_t conn = 0;
    kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
    IOObjectRelease(svc);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceOpen fail 0x%x (client class mismatch?)\n", kr);
        return 3;
    }
    printf("[open] OK conn=%u\n", conn);

    uint64_t inS7[2] = { 4097, 1 };                    /* surfaceID 4097, mode 1 */
    kr = IOConnectCallMethod(conn, 7, inS7, 2, NULL, 0, NULL, NULL, NULL, NULL);
    printf("[sel7 SetIDMode] 0x%x\n", kr);

    /* region: num_rects=1 + bounds{x,y,w,h} = 5 * 4B? bounds s32 x4 =16B,
     * num_rects 4B -> structInput 20B matches WS probe era */
    /* IOAccelDeviceRegion: num_rects u32 + IOAccelBounds(SInt16 x,y,w,h)
     * + rects[] — matches kernel expectation (mem line 358). */
    struct __attribute__((packed)) Bounds { int16_t x, y, w, h; };
    struct __attribute__((packed)) Region {
        uint32_t numRects;
        struct Bounds bounds;
        struct Bounds rect0;
    } reg = { 1, {0,0,1920,1080}, {0,0,1920,1080} };
    kr = IOConnectCallMethod(conn, 6, NULL, 0, &reg, sizeof(reg),
                             NULL, NULL, NULL, NULL);
    printf("[sel6 SetShapeBacking] 0x%x\n", kr);

    uint64_t inLock[1] = { 4097 };
    kr = IOConnectCallMethod(conn, 14, inLock, 1, NULL, 0, NULL, NULL, NULL, NULL);
    printf("[sel14 WriteLock] 0x%x\n", kr);

    kr = IOConnectCallMethod(conn, 10, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);
    printf("[sel10 Flush] 0x%x\n", kr);

    IOServiceClose(conn);
    printf("done — kernel log now has [SURF-FMT] if backing lived\n");
    return 0;
}
