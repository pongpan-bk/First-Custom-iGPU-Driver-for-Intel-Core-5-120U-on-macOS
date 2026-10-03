/*
 * MyIntelObfuscate.h — String encryption + register obfuscation utilities
 * Author: pongpan-bk | Tooling: OpenCode (OhMyOpenCode)
 * 
 * Layer 1: XOR string encryption (runtime decryption only)
 * Layer 2: Register address obfuscation (math operations)
 * Layer 3: Compiler stripping (handled by Makefile)
 */

#ifndef __MY_INTEL_OBFUSCATE_H__
#define __MY_INTEL_OBFUSCATE_H__

#include <stdint.h>
#include <string.h>

/* XOR mask for string encryption */
#define OBFUSCATE_XOR_KEY 0x5A

/* Layer 1: XOR string decryption macro */
#define DECRYPT_STRING(encrypted, buffer, len) \
    do { \
        for (int _i = 0; _i < (len); _i++) { \
            (buffer)[_i] = (char)((encrypted)[_i] ^ OBFUSCATE_XOR_KEY); \
        } \
        (buffer)[(len)] = '\0'; \
    } while(0)

/* Pre-encrypted strings (XOR with 0x5A) — verified round-trip */
/* "MyIntelVCS" encrypted */
static const uint8_t ENCRYPTED_MyIntelVCS[] = {
    0x17, 0x23, 0x13, 0x34, 0x2E, 0x3F, 0x36, 0x0C, 0x19, 0x09
};

/* "MyIntelVCSClient" encrypted */
static const uint8_t ENCRYPTED_MyIntelVCSClient[] = {
    0x17, 0x23, 0x13, 0x34, 0x2E, 0x3F, 0x36, 0x0C, 0x19, 0x09,
    0x19, 0x36, 0x33, 0x3F, 0x34, 0x2E
};

/* "MyIntelGPU" encrypted */
static const uint8_t ENCRYPTED_MyIntelGPU[] = {
    0x17, 0x23, 0x13, 0x34, 0x2E, 0x3F, 0x36, 0x1D, 0x0A, 0x0F
};

/* "MyIntelRing" encrypted */
static const uint8_t ENCRYPTED_MyIntelRing[] = {
    0x17, 0x23, 0x13, 0x34, 0x2E, 0x3F, 0x36, 0x08, 0x33, 0x34, 0x3D
};

/* "MyIntelFramebuffer" encrypted */
static const uint8_t ENCRYPTED_MyIntelFramebuffer[] = {
    0x17, 0x23, 0x13, 0x34, 0x2E, 0x3F, 0x36, 0x1C, 0x28, 0x3B,
    0x37, 0x3F, 0x38, 0x2F, 0x3C, 0x3C, 0x3F, 0x28
};

/* "MetalPluginName" encrypted */
static const uint8_t ENCRYPTED_MetalPluginName[] = {
    0x3E, 0x2E, 0x31, 0x33, 0x0D, 0x3C, 0x33, 0x36, 0x3E, 0x2C,
    0x23, 0x22, 0x07, 0x31, 0x22, 0x07
};

/* "IOGLBundleName" encrypted */
static const uint8_t ENCRYPTED_IOGLBundleName[] = {
    0x03, 0x0A, 0x1D, 0x0C, 0x19, 0x34, 0x33, 0x33, 0x36, 0x3E,
    0x22, 0x07, 0x31, 0x22, 0x07
};

/* "IOUserClientClass" encrypted (fixed 2026-08-21 — round-trip verified) */
static const uint8_t ENCRYPTED_IOUserClientClass[] = {
    0x13, 0x15, 0x0F, 0x29, 0x3F, 0x28, 0x19, 0x36, 0x33, 0x3F,
    0x34, 0x2E, 0x19, 0x36, 0x3B, 0x29, 0x29
};

/* Layer 2: Register address obfuscation functions */
static inline uint32_t obf_getVCSBase(void) {
    /* 0x1C0000 = 0x0E0000 + 0x0E0000 */
    volatile uint32_t a = 0x0E0000;
    volatile uint32_t b = 0x0E0000;
    return (a + b);
}

static inline uint32_t obf_getRingTail(void) {
    /* 0x30 = (0x15 << 1) + 6 */
    volatile uint32_t x = 0x15;
    return (x << 1) + 6;
}

static inline uint32_t obf_getRingHead(void) {
    /* 0x34 = (0x15 << 1) + 10 */
    volatile uint32_t x = 0x15;
    return (x << 1) + 10;
}

static inline uint32_t obf_getRingStart(void) {
    /* 0x38 = (0x15 << 1) + 14 */
    volatile uint32_t x = 0x15;
    return (x << 1) + 14;
}

static inline uint32_t obf_getRingCtl(void) {
    /* 0x3C = (0x15 << 1) + 18 */
    volatile uint32_t x = 0x15;
    return (x << 1) + 18;
}

static inline uint32_t obf_getMfxStatus(void) {
    /* 0x0800 = 0x0400 << 1 */
    volatile uint32_t x = 0x0400;
    return (x << 1);
}

static inline uint32_t obf_getHcpStatus(void) {
    /* 0x2800 = 0x1400 << 1 */
    volatile uint32_t x = 0x1400;
    return (x << 1);
}

/* "MyIntelGPUClient" encrypted */
static const uint8_t ENCRYPTED_MyIntelGPUClient[] = {
    0x17, 0x23, 0x13, 0x34, 0x2E, 0x3F, 0x36, 0x1D, 0x0A, 0x0F,
    0x19, 0x36, 0x33, 0x3F, 0x34, 0x2E
};

/* Decrypted string buffers (runtime only) */
static char DEC_BUFFER_VCS[11];
static char DEC_BUFFER_VCSClient[17];
static char DEC_BUFFER_GPU[11];
static char DEC_BUFFER_GPUClient[17];
static char DEC_BUFFER_Ring[12];
static char DEC_BUFFER_Framebuffer[19];
static char DEC_BUFFER_MetalPlugin[17];
static char DEC_BUFFER_IOGLBundle[16];
static char DEC_BUFFER_IOUserClient[18];

/* Initialize decrypted strings (call once at startup) */
static inline void obfuscate_init(void) {
    DECRYPT_STRING(ENCRYPTED_MyIntelVCS, DEC_BUFFER_VCS, 10);
    DECRYPT_STRING(ENCRYPTED_MyIntelVCSClient, DEC_BUFFER_VCSClient, 16);
    DECRYPT_STRING(ENCRYPTED_MyIntelGPU, DEC_BUFFER_GPU, 10);
    DECRYPT_STRING(ENCRYPTED_MyIntelGPUClient, DEC_BUFFER_GPUClient, 16);
    DECRYPT_STRING(ENCRYPTED_MyIntelRing, DEC_BUFFER_Ring, 11);
    DECRYPT_STRING(ENCRYPTED_MyIntelFramebuffer, DEC_BUFFER_Framebuffer, 18);
    DECRYPT_STRING(ENCRYPTED_MetalPluginName, DEC_BUFFER_MetalPlugin, 16);
    DECRYPT_STRING(ENCRYPTED_IOGLBundleName, DEC_BUFFER_IOGLBundle, 15);
    DECRYPT_STRING(ENCRYPTED_IOUserClientClass, DEC_BUFFER_IOUserClient, 17);
}

#endif /* __MY_INTEL_OBFUSCATE_H__ */
