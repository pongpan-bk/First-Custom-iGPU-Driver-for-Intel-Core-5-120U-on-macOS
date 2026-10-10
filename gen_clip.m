/*===========================================================================
 *  gen_clip.m — สร้างคลิปทดสอบ H.264 สำหรับ test_submit_vcs (Phase 9)
 *
 *  ใช้ AVAssetWriter (hardware encoder บนเครื่อง) สร้าง .mp4 H.264 สั้นๆ
 *  ขนาด 128x96, 30 เฟรม — ภาพ gradient เลื่อนเพื่อให้เห็น motion จริง
 *
 *  Build:  cc -framework Foundation -framework AVFoundation \
 *              -framework CoreVideo -framework CoreMedia -o gen_clip gen_clip.m
 *  Run:    ./gen_clip   → ได้ test_clip.mp4
 *  ต่อด้วย extract_annexb.py เพื่อแปลงเป็น Annex-B (.h264)
 *=========================================================================*/
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMedia/CoreMedia.h>

#define W 128
#define H 96
#define FRAMES 30

int main(void)
{
    @autoreleasepool {
        NSError *err = nil;
        NSURL *outURL = [NSURL fileURLWithPath:@"test_clip.mp4"];
        [[NSFileManager defaultManager] removeItemAtPath:@"test_clip.mp4" error:NULL];

        AVAssetWriter *writer =
            [AVAssetWriter assetWriterWithURL:outURL
                                     fileType:AVFileTypeMPEG4
                                        error:&err];
        if (!writer) {
            NSLog(@"writer fail: %@", err);
            return 1;
        }

        NSDictionary *settings = @{
            AVVideoCodecKey : AVVideoCodecTypeH264,
            AVVideoWidthKey : @(W),
            AVVideoHeightKey : @(H),
        };

        AVAssetWriterInput *input =
            [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                               outputSettings:settings];
        input.expectsMediaDataInRealTime = NO;

        NSDictionary *pbAttrs = @{
            (NSString *)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA),
            (NSString *)kCVPixelBufferWidthKey : @(W),
            (NSString *)kCVPixelBufferHeightKey : @(H),
        };
        AVAssetWriterInputPixelBufferAdaptor *adaptor =
            [AVAssetWriterInputPixelBufferAdaptor
                assetWriterInputPixelBufferAdaptorWithAssetWriterInput:input
                                           sourcePixelBufferAttributes:pbAttrs];

        [writer addInput:input];
        [writer startWriting];
        [writer startSessionAtSourceTime:CMTimeMake(0, 30)];

        for (int f = 0; f < FRAMES; f++) {
            while (!input.readyForMoreMediaData)
                usleep(1000);

            CVPixelBufferRef pb = NULL;
            CVPixelBufferCreate(kCFAllocatorDefault, W, H, kCVPixelFormatType_32BGRA,
                                NULL, &pb);
            if (!pb) {
                NSLog(@"pixelbuffer fail at frame %d", f);
                return 1;
            }

            CVPixelBufferLockBaseAddress(pb, 0);
            uint8_t *base = CVPixelBufferGetBaseAddress(pb);
            size_t stride = CVPixelBufferGetBytesPerRow(pb);

            /* gradient เลื่อนตาม frame index — มี motion จริงให้ encoder ทำงาน */
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    uint8_t *p = base + y * stride + x * 4;
                    p[0] = (uint8_t)((x * 2 + f * 8) & 0xFF);         /* B */
                    p[1] = (uint8_t)((y * 2 + f * 4) & 0xFF);         /* G */
                    p[2] = (uint8_t)((x + y + f * 6) & 0xFF);         /* R */
                    p[3] = 0xFF;
                }
            }
            CVPixelBufferUnlockBaseAddress(pb, 0);

            [adaptor appendPixelBuffer:pb withPresentationTime:CMTimeMake(f, 30)];
            CVPixelBufferRelease(pb);
        }

        [input markAsFinished];
        [writer endSessionAtSourceTime:CMTimeMake(FRAMES, 30)];
        [writer finishWritingWithCompletionHandler:^{}];

        while (writer.status == AVAssetWriterStatusWriting)
            usleep(10000);

        if (writer.status == AVAssetWriterStatusCompleted) {
            NSLog(@"OK — test_clip.mp4 created (%d frames, %dx%d H.264)",
                  FRAMES, W, H);
            return 0;
        }
        NSLog(@"FAIL status=%ld error=%@", (long)writer.status, writer.error);
        return 1;
    }
}
