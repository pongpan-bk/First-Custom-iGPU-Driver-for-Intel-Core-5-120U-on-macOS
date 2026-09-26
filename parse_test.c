#include <stdio.h>
#include <stdlib.h>
#include "myintel_h264_parse.h"

static int test_nal_boundaries(void)
{
    const uint8_t fourByte[] = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x11,
        0x00, 0x00, 0x00, 0x01, 0x68, 0x22,
    };
    const uint8_t threeByte[] = {
        0x00, 0x00, 0x01, 0x67, 0x33,
        0x00, 0x00, 0x01, 0x68, 0x44,
    };
    const uint8_t *nal;
    uint32_t nalLen;
    int nalType;
    H264NalIter it;

    /* Given: two NALs with a four-byte start code. */
    h264_nal_init(&it, fourByte, sizeof(fourByte));
    nal = NULL; nalLen = 0; nalType = -1;
    /* When: the iterator returns each NAL. */
    if (!h264_nal_next(&it, &nal, &nalLen, &nalType) || nalType != 7 ||
        nalLen != 2 || nal[0] != 0x67 || nal[1] != 0x11) {
        fprintf(stderr, "FAIL four-byte start code: first NAL len=%u type=%d\n",
                nalLen, nalType);
        return 1;
    }
    if (!h264_nal_next(&it, &nal, &nalLen, &nalType) || nalType != 8 ||
        nalLen != 2 || nal[0] != 0x68 || nal[1] != 0x22) {
        fprintf(stderr, "FAIL four-byte start code: second NAL len=%u type=%d\n",
                nalLen, nalType);
        return 1;
    }
    if (h264_nal_next(&it, &nal, &nalLen, &nalType)) {
        fprintf(stderr, "FAIL four-byte start code: extra NAL\n");
        return 1;
    }

    /* Given: two NALs with a three-byte start code. */
    h264_nal_init(&it, threeByte, sizeof(threeByte));
    /* When: the iterator returns each NAL. */
    if (!h264_nal_next(&it, &nal, &nalLen, &nalType) || nalType != 7 ||
        nalLen != 2 || nal[0] != 0x67 || nal[1] != 0x33) {
        fprintf(stderr, "FAIL three-byte start code: first NAL len=%u type=%d\n",
                nalLen, nalType);
        return 1;
    }
    if (!h264_nal_next(&it, &nal, &nalLen, &nalType) || nalType != 8 ||
        nalLen != 2 || nal[0] != 0x68 || nal[1] != 0x44) {
        fprintf(stderr, "FAIL three-byte start code: second NAL len=%u type=%d\n",
                nalLen, nalType);
        return 1;
    }
    if (h264_nal_next(&it, &nal, &nalLen, &nalType)) {
        fprintf(stderr, "FAIL three-byte start code: extra NAL\n");
        return 1;
    }

    puts("NAL boundary test: PASS");
    return 0;
}

int main(int argc, char **argv) {
    if (test_nal_boundaries() != 0) {
        return 2;
    }
    if (argc < 2) {
        fprintf(stderr, "usage: %s file.h264\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return 1; }
    uint8_t *b = malloc((size_t)len); fread(b, 1, (size_t)len, f); fclose(f);
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
    free(b);
    printf("RESULT sps=%d pps=%d slice=%d\n", hs, hp, hsl);
    return 0;
}
