// sel2-dump.mm — direct type-5 sel=2 call against our accelerator, dump the 600B response.
// Isolates whether the kernel copy-back delivers our caps struct to userspace.
// Build: clang -fobjc-arc -framework Foundation -framework IOKit -lc++ -o sel2-dump sel2-dump.mm
#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <CoreFoundation/CoreFoundation.h>
#import <mach/mach.h>

static void hexDump(const uint8_t *b, size_t n)
{
    for (size_t i = 0; i < n; i += 16) {
        printf("%04zx: ", i);
        for (size_t j = 0; j < 16 && i + j < n; j++) printf("%02x ", b[i + j]);
        printf(" | ");
        for (size_t j = 0; j < 16 && i + j < n; j++) {
            uint8_t c = b[i + j];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
    }
}

int main(void)
{
    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault,
                                                       IOServiceMatching("MyIntelAccelerator"));
    if (!service) { printf("no MyIntelAccelerator service\n"); return 1; }
    io_connect_t conn = 0;
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 5, &conn);
    IOObjectRelease(service);
    printf("IOServiceOpen(type 5) kr=0x%x conn=%u\n", kr, conn);
    if (kr) return 1;

    // sel=9: api name (like the factory does)
    char api[16] = "Metal";
    kr = IOConnectCallStructMethod(conn, 9, api, 16, NULL, 0);
    printf("sel=9 (api 'Metal') kr=0x%x\n", kr);

    // sel=2: 600-byte caps
    uint8_t caps[600];
    memset(caps, 0xAA, sizeof(caps));  // poison — see what the kernel actually returns
    size_t outSz = sizeof(caps);
    kr = IOConnectCallStructMethod(conn, 2, api, 16, caps, &outSz);
    printf("sel=2 kr=0x%x outSz=%zu\n", kr, outSz);
    if (!kr) {
        printf("name@0x48 = '%s'\n", (char *)(caps + 0x48));
        hexDump(caps, 96);
    }
    return 0;
}