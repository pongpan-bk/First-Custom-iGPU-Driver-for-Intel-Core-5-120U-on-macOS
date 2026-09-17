#import <AppKit/AppKit.h>
#include "myintelvcs_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mach/mach_time.h>

static NSMutableData *g_file;
static size_t g_pos[16384]; static uint8_t g_ty[16384]; static size_t g_n;
static volatile int g_playing = 1;
static NSImageView *g_view;

static void scan(void) {
    const uint8_t *b = (const uint8_t*)g_file.bytes; size_t len = g_file.length; g_n=0;
    for (size_t i=0; i+3<len && g_n<16384;) {
        if (b[i]==0&&b[i+1]==0){ int sc=(b[i+2]==1)?3:((i+3<len&&b[i+2]==0&&b[i+3]==1)?4:0);
            if(sc){g_pos[g_n]=i+sc; g_ty[g_n]=b[i+sc]&0x1F; g_n++; i+=sc; continue;} } i++; }
}
static uint32_t rgb[1920*1080];

@interface AppDelegate : NSObject<NSApplicationDelegate>
@end
@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)n {
    
    NSRect fr = NSMakeRect(100,100,960,540);
    NSWindow *w = [[NSWindow alloc] initWithContentRect:fr
        styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];
    [w setTitle:@"VDBOX HW Decode — Intel Core 5 120U iGPU"];
    g_view = [[NSImageView alloc] initWithFrame:fr];
    g_view.imageScaling = NSImageScaleProportionallyUpOrDown;
    w.contentView = g_view; [w makeKeyAndOrderFront:nil]; [w center];
    // key handlers: space=pause, q=quit, f=fps display

    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent*(NSEvent* e){
        NSString *ch = e.characters.lowercaseString;
        if ([ch isEqualToString:@"q"]) [NSApp terminate:nil];
        if ([ch isEqualToString:@" "]) g_playing = !g_playing;
        return e;
    }];
    [NSThread detachNewThreadSelector:@selector(decodeLoop) toTarget:self withObject:nil];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)a { return YES; }
- (void)decodeLoop {
    const uint8_t *b = (const uint8_t*)g_file.bytes;
    mvcs_ctx *ctx=NULL; if (mvcs_open(&ctx)) return;
    mach_timebase_info_data_t tb; mach_timebase_info(&tb);
    int shown=0; double acc=0; uint64_t t0=mach_absolute_time();
    while (g_playing) {
      @autoreleasepool {
        for (size_t i=0;i<g_n && g_playing;i++) {
            if (g_ty[i]!=5 && g_ty[i]!=1) continue;
            size_t end = (i+1<g_n)? g_pos[i+1] : g_file.length;
            size_t len = end - g_pos[i]; while(len>3 && b[g_pos[i]+len-1]==0) len--;
            if (len<8||len>1500000) continue;
            mvcs_frame_t fr; memset(&fr,0,sizeof fr);
            if (mvcs_decode_h264(ctx,b+g_pos[i],(uint32_t)len,1920,1080,MYVCS_SURF_FMT_NV12,&fr)) continue;
            /* NV12 -> BGRA */
            const uint8_t *Y=fr.pixels,*UV=(const uint8_t*)fr.pixels+1920*1080;
            for (uint32_t r=0;r<1080;r++) for (uint32_t c=0;c<1920;c++){
                int Yv=Y[r*1920+c]-16; int U=UV[(r>>1)*1920+(c&~1)]-128; int V=UV[(r>>1)*1920+(c&~1)+1]-128;
                int R=(298*Yv+409*V+128)>>8, G=(298*Yv-100*U-208*V+128)>>8, Bc=(298*Yv+516*U+128)>>8;
                rgb[r*1920+c]= 0xFF000000u | ((unsigned)(Bc<0?0:Bc>255?255:Bc)<<16)
                             | ((unsigned)(G<0?0:G>255?255:G)<<8) | (unsigned)(R<0?0:R>255?255:R);
            }
            CGColorSpaceRef cs=CGColorSpaceCreateDeviceRGB();
            CGContextRef ctx2=CGBitmapContextCreate(rgb,1920,1080,8,1920*4,cs,
                kCGImageAlphaNoneSkipFirst|kCGBitmapByteOrder32Little);
            CGImageRef img=CGBitmapContextCreateImage(ctx2);
            CFRelease(ctx2); CFRelease(cs);
            NSImage *im=[[NSImage alloc]initWithSize:NSMakeSize(1920,1080)];
            [im addRepresentation:[[NSBitmapImageRep alloc]initWithCGImage:img]];
            CGImageRelease(img);
            dispatch_async(dispatch_get_main_queue(),^{ g_view.image = im; });
            shown++;
            /* pace to 30fps - protect thermals */
            useconds_t spent=(useconds_t)((double)(mach_absolute_time()-t0)*tb.numer/tb.denom/1000.0);
            if (spent < 33000) usleep(33000-spent);
            t0=mach_absolute_time();
            if (shown%30==0) printf("[PLAYER] %d frames @30fps paced\n",shown);
        }
        printf("[PLAYER] stream end - looping\n");
      }
    }
    mvcs_close(ctx);
}
@end
int main(int argc,char**argv){
    FILE *f=fopen(argc>1?argv[1]:"/tmp/fhd_1920.h264","rb"); fseek(f,0,SEEK_END);
    long fl=ftell(f); fseek(f,0,SEEK_SET); g_file=[NSMutableData dataWithLength:fl];
    fread(g_file.mutableBytes,1,fl,f); fclose(f); scan();
    NSApplication *app=[NSApplication sharedApplication];
    AppDelegate *dg=[[AppDelegate alloc]init]; app.delegate=dg;
    [app setActivationPolicy:NSApplicationActivationPolicyRegular]; [app activateIgnoringOtherApps:YES];
    [app run]; return 0;
}
