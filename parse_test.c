#include <stdio.h>
#include <stdlib.h>
#include "myintel_h264_parse.h"
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(len); fread(b, 1, len, f); fclose(f);
    H264SPS sps; H264PPS pps; H264SliceHdr sh;
    bool hs = false, hp = false, hsl = false;
    uint8_t rbsp[1024];
    H264NalIter it; h264_nal_init(&it, b, (uint32_t)len);
    const uint8_t *n; uint32_t nl; int t; int idx = 0;
    while (h264_nal_next(&it, &n, &nl, &t)) {
        printf("NAL[%d] type=%d len=%u firstBytes: %02X %02X %02X %02X\n", idx++, t, nl,
               nl>0?n[0]:0, nl>1?n[1]:0, nl>2?n[2]:0, nl>3?n[3]:0);
        if (t == 7) { uint32_t rl = h264_rbsp(n, nl, rbsp, sizeof(rbsp));
            hs = h264_parse_sps(rbsp, rl, &sps);
            printf("  SPS parse=%d %ux%u MB wMb=%u hMb=%u profile=%u\n", hs,
                sps.width, sps.height, sps.picWidthInMbs, sps.picHeightInMbs, sps.profileIdc); }
        if (t == 8) { uint32_t rl = h264_rbsp(n, nl, rbsp, sizeof(rbsp));
            printf("  PPS rbsp len=%u bytes:", rl);
            for (uint32_t i = 0; i < rl && i < 12; i++) printf(" %02X", rbsp[i]);
            printf("\n");
            hp = h264_parse_pps(rbsp, rl, &pps, &sps);
            printf("  PPS parse=%d entropy=%u initQpM26=%d deblock=%u t8x8=%u\n",
                hp, pps.entropyCoding, pps.picInitQpMinus26, pps.deblockCtrlPresent, pps.transform8x8); }
        if (t == 5 && !hsl && hs && hp) { uint32_t rl = h264_rbsp(n, nl, rbsp, sizeof(rbsp));
            hsl = h264_parse_slice(rbsp, rl, &sh, &sps, &pps, true);
            printf("  SLICE parse=%d type=%u firstMb=%u hdrBytes=%u\n", hsl, sh.sliceType, sh.firstMbInSlice, sh.sliceHeaderBytes); }
        if (idx > 8) break;
    }
    printf("RESULT sps=%d pps=%d slice=%d\n", hs, hp, hsl);
    return 0;
}
