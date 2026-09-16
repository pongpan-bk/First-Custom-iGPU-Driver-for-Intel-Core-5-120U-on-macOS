// m2c-probe.mm — decisive: does Metal hand us a real MTLDevice now?
// Persistent copy lives in the repo (survives reboots; /tmp does not).
// Build: clang -fobjc-arc -framework Foundation -framework Metal -framework IOKit -o m2c-probe m2c-probe.mm
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <IOKit/IOKitLib.h>
#import <objc/message.h>

int main(void)
{
    @autoreleasepool {
        NSLog(@"[M2C-PROBE] --- MTLCreateSystemDefaultDevice ---");
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        if (dev) {
            NSLog(@"[M2C-PROBE] DEVICE != nil");
            NSLog(@"[M2C-PROBE] name        = %@", dev.name);
            NSLog(@"[M2C-PROBE] lowPower    = %d", dev.isLowPower);
            NSLog(@"[M2C-PROBE] maxThreads  = %lu", (unsigned long)dev.maxThreadsPerThreadgroup.width);
            NSLog(@"[M2C-PROBE] recommendedMaxWorkingSetSize = %llu KB",
                  (unsigned long long)(dev.recommendedMaxWorkingSetSize / 1024));
        } else {
            NSLog(@"[M2C-PROBE] DEVICE is nil  (software compositing continues)");
        }
        NSLog(@"[M2C-PROBE] --- bundle +createDevice direct ---");
        NSBundle *b = [NSBundle bundleWithPath:@"/Library/GPUBundles/MyIntelGPUMTLDriver.bundle"];
        if (b) {
            if ([b load]) NSLog(@"[M2C-PROBE] bundle load = YES, principal = %@",
                                NSStringFromClass([b principalClass]));
            Class cls = [b principalClass];
            if (cls && [cls respondsToSelector:@selector(createDevice)]) {
                SEL selCD = sel_registerName("createDevice");
                id (*createDeviceFn)(id, SEL) = (id (*)(id, SEL))(void *)objc_msgSend;
                id d2 = createDeviceFn(cls, selCD);
                NSLog(@"[M2C-PROBE] bundle createDevice = %@", d2 ? d2 : (id)@"(null)");
            } else {
                NSLog(@"[M2C-PROBE] principal class missing createDevice");
            }
        } else {
            NSLog(@"[M2C-PROBE] bundle not found at /Library/GPUBundles");
        }
    }
    return 0;
}