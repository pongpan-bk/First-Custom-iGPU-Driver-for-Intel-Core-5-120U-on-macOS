/* iosurface_scan.c — Door-B step1: enumerate live IOSurfaces (userspace)
 * Compile: clang iosurface_scan.c -o iosurface_scan -framework IOSurface -framework CoreFoundation */
#include <IOSurface/IOSurface.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    uint32_t maxID = argc >= 2 ? (uint32_t)strtoul(argv[1], NULL, 0) : 5000;
    uint32_t found = 0;
    printf("id      | WxH          | fmt     | seed | bytesPerRow\n");
    printf("--------|--------------|---------|------|-------------\n");
    for (uint32_t id = 1; id <= maxID; id++) {
        IOSurfaceRef surf = IOSurfaceLookup((IOSurfaceID)id);
        if (!surf) continue;
        size_t w = IOSurfaceGetWidth(surf);
        size_t h = IOSurfaceGetHeight(surf);
        uint32_t bpr = (uint32_t)IOSurfaceGetBytesPerRow(surf);
        OSType pf = IOSurfaceGetPixelFormat(surf);
        printf("%7u | %4zux%-6zu | %08X | ---- | %u\n", id, w, h, (unsigned)pf, bpr);
        CFRelease(surf);
        found++;
    }
    printf("── total live surfaces found: %u ──\n", found);
    return 0;
}
