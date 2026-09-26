#!/usr/bin/env python3
"""extract_annexb.py — แปลง MP4 (H.264/AVCC) → Annex-B (.h264)

Parse กล่อง MP4 เองแบบ minimal:
  moov/trak/mdia/minf/stbl/stsd/avc1/avcC → SPS/PPS
  stco|co64 + stsz + stsc → ตำแหน่ง+ขนาด sample ใน mdat
AVCC sample (4-byte length prefix) → แทนที่ด้วย 00 00 00 01 start code

Usage: python3 extract_annexb.py test_clip.mp4 test_clip.h264
"""
import struct
import sys


def read_boxes(data, off, end):
    while off + 8 <= end:
        size = struct.unpack(">I", data[off:off+4])[0]
        btype = data[off+4:off+8]
        if size == 1:
            size = struct.unpack(">Q", data[off+8:off+16])[0]
            hdr = 16
        else:
            hdr = 8
        if size == 0:
            size = end - off
        yield btype, off + hdr, off + size
        off += size


def find_box(data, off, end, name):
    for btype, s, e in read_boxes(data, off, end):
        if btype == name:
            return s, e
    return None, None


def main():
    src, dst = sys.argv[1], sys.argv[2]
    data = open(src, "rb").read()

    # ── หา moov / mdat ──
    moov_s, moov_e = find_box(data, 0, len(data), b"moov")
    mdat_s, mdat_e = find_box(data, 0, len(data), b"mdat")
    assert moov_s and mdat_s, "ไม่เจอ moov/mdat"

    # ── ลงไป trak → mdia → minf → stbl ──
    trak_s, trak_e = find_box(data, moov_s, moov_e, b"trak")
    mdia_s, mdia_e = find_box(data, trak_s, trak_e, b"mdia")
    minf_s, minf_e = find_box(data, mdia_s, mdia_e, b"minf")
    stbl_s, stbl_e = find_box(data, minf_s, minf_e, b"stbl")

    # ── stsd → avc1 → avcC (SPS/PPS) ──
    stsd_s, stsd_e = find_box(data, stbl_s, stbl_e, b"stsd")
    # stsd: 4B version/flags + 4B entry count แล้วต่อด้วย entry boxes
    avc1_s, avc1_e = find_box(data, stsd_s + 8, stsd_e, b"avc1")
    avcc_s, avcc_e = find_box(data, avc1_s + 78, avc1_e, b"avcC")
    p = avcc_s + 1  # configurationVersion
    p += 3          # profile / compat / level
    nlen = (data[p] & 3) + 1
    p += 1
    sps_list, pps_list = [], []
    n = data[p] & 0x1F; p += 1   # numOfSPS (3 reserved bits = 0xE0)
    for _ in range(n):
        ln = struct.unpack(">H", data[p:p+2])[0]; p += 2
        sps_list.append(data[p:p+ln]); p += ln
    n = data[p]; p += 1           # numOfPPS
    for _ in range(n):
        ln = struct.unpack(">H", data[p:p+2])[0]; p += 2
        pps_list.append(data[p:p+ln]); p += ln

    # ── stsz (sample sizes) ──
    stsz_s, stsz_e = find_box(data, stbl_s, stbl_e, b"stsz")
    q = stsz_s + 4
    sample_size = struct.unpack(">I", data[q:q+4])[0]; q += 4
    count = struct.unpack(">I", data[q:q+4])[0]; q += 4
    if sample_size:
        sizes = [sample_size] * count
    else:
        sizes = [struct.unpack(">I", data[q+4*i:q+4*i+4])[0] for i in range(count)]

    # ── stco / co64 (chunk offsets) ──
    stco_s, stco_e = find_box(data, stbl_s, stbl_e, b"stco")
    co64 = False
    if not stco_s:
        stco_s, stco_e = find_box(data, stbl_s, stbl_e, b"co64")
        co64 = True
    q = stco_s + 4
    nch = struct.unpack(">I", data[q:q+4])[0]; q += 4
    fmt = ">Q" if co64 else ">I"
    w = 8 if co64 else 4
    chunks = [struct.unpack(fmt, data[q+w*i:q+w*i+w])[0] for i in range(nch)]

    # ── stsc (sample-per-chunk) ──
    stsc_s, stsc_e = find_box(data, stbl_s, stbl_e, b"stsc")
    q = stsc_s + 4
    nsc = struct.unpack(">I", data[q:q+4])[0]; q += 4
    stsc = []
    for _ in range(nsc):
        fc, spc, sdi = struct.unpack(">III", data[q:q+12]); q += 12
        stsc.append((fc, spc))

    # map sample → offset
    offsets = []
    si = 0
    for ci, ch in enumerate(chunks):
        # หา spc ของ chunk นี้
        spc = stsc[0][1]
        for fc, s in stsc:
            if fc <= ci + 1:
                spc = s
        off = ch
        for _ in range(spc):
            if si >= count:
                break
            offsets.append(off)
            off += sizes[si]
            si += 1

    # ── เขียน Annex-B ──
    start = b"\x00\x00\x00\x01"
    with open(dst, "wb") as f:
        for sps in sps_list:
            f.write(start + sps)
        for pps in pps_list:
            f.write(start + pps)
        for i, off in enumerate(offsets):
            buf = data[off:off+sizes[i]]
            # AVCC: NAL units คั่นด้วย length prefix (4 bytes)
            j = 0
            while j + 4 <= len(buf):
                ln = struct.unpack(">I", buf[j:j+4])[0]
                f.write(start + buf[j+4:j+4+ln])
                j += 4 + ln

    print(f"OK — {dst}: {count} samples, {len(sps_list)} SPS, {len(pps_list)} PPS")


if __name__ == "__main__":
    main()
