#!/usr/bin/env python3
import os, sys, re, time, shutil, hashlib, subprocess, statistics
from datetime import datetime

REPO = "/Users/ppbk/Documents/GitHub/IntelReviveGPU-Gen-10-12-on-Hackintosh"
REPORT_PATH = os.path.expanduser("~/Desktop/Bottleneck_Report_MyIntelGPU.text")
TMP_DIR = "/tmp/myintel_bn_test"
os.makedirs(TMP_DIR, exist_ok=True)

lines_out = []
problems = []

def log(msg=""):
    print(msg, flush=True)
    lines_out.append(msg)

def run_cmd(cmd, timeout=20):
    t0 = time.perf_counter()
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, errors="replace", timeout=timeout)
        return p.returncode, p.stdout, p.stderr, time.perf_counter() - t0, False
    except subprocess.TimeoutExpired:
        return -9, "", "TIMEOUT", time.perf_counter() - t0, True
    except Exception as e:
        return -1, "", str(e), time.perf_counter() - t0, False

def find_ffmpeg():
    for cand in [shutil.which("ffmpeg"), "/opt/homebrew/bin/ffmpeg", "/usr/local/bin/ffmpeg"]:
        if cand and os.path.isfile(cand) and os.access(cand, os.X_OK):
            return cand
    for root_base in ["/Volumes/DATA", "/Users/ppbk", "/Volumes"]:
        if not os.path.exists(root_base):
            continue
        for root, dirs, files in os.walk(root_base):
            depth = root[len(root_base):].count(os.sep)
            if depth > 5:
                dirs[:] = []
                continue
            if "ffmpeg" in files:
                full = os.path.join(root, "ffmpeg")
                if os.path.isfile(full) and os.access(full, os.X_OK):
                    return full
    return None

def find_decoder():
    cands = [
        os.path.join(REPO, "build/myintel-decode/myintel-decode"),
        os.path.join(REPO, "build/myintel-decode"),
    ]
    for c in cands:
        if os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    if os.path.isdir(REPO):
        run_cmd(["make", "-C", REPO, "myintel-decode"], timeout=60)
        for c in cands:
            if os.path.isfile(c) and os.access(c, os.X_OK):
                return c
    return None

def run_decode_smart(dec_bin, h264_path, out_path, extra_flag=None, timeout=20):
    if not extra_flag:
        return run_cmd([dec_bin, h264_path, out_path], timeout=timeout)
    r1 = run_cmd([dec_bin, extra_flag, h264_path, out_path], timeout=timeout)
    if r1[0] == 0 and os.path.exists(out_path) and os.path.getsize(out_path) > 0:
        return r1
    if os.path.exists(out_path):
        try: os.remove(out_path)
        except OSError: pass
    r2 = run_cmd([dec_bin, h264_path, out_path, extra_flag], timeout=timeout)
    return r2 if (r2[0] == 0 or r1[0] != 0) else r1

log("======================================================================")
log("  รายงานผ่าพิสูจน์คอขวดและปัญหาการใช้งานจริง MyIntelGPU (ฉบับเข้าใจง่าย)")
log(f"  เวลาที่ตรวจ: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
log("======================================================================\n")

ffmpeg_bin = find_ffmpeg()
dec_bin = find_decoder()

log(f"[เตรียมเครื่องมือ]")
log(f"  - ตัวสร้างคลิปทดสอบ (ffmpeg) : {ffmpeg_bin or 'ไม่พบ!'}")
log(f"  - ตัวถอดรหัสการ์ดจอของเรา    : {dec_bin or 'ไม่พบ!'}")

if not ffmpeg_bin or not dec_bin:
    log("\n[หยุดทำงาน] ไม่พบเครื่องมือทดสอบครบทั้ง 2 ตัว กรุณาเช็กพาธโปรเจกต์ก่อนครับ")
    sys.exit(1)

# สำรวจออปชันในตัวถอดรหัส
_, help_out, help_err, _, _ = run_cmd([dec_bin, "--help"], timeout=5)
bin_bytes = open(dec_bin, "rb").read()
has_detile_wide = (b"--detile-wide" in bin_bytes) or ("--detile-wide" in (help_out + help_err))
log(f"  - โหมดปลดล็อกจอกว้าง (--detile-wide) ในโค้ด: {'มีพร้อมทดสอบ' if has_detile_wide else 'ยังไม่มีในไบนารีนี้'}\n")

# ----------------------------------------------------------------------
# ด่าน 1: วัดความเร็ว FPS + แยกคอขวด เปิดโปรแกรม vs รายเฟรม vs เขียนดิสก์
# ----------------------------------------------------------------------
log("----------------------------------------------------------------------")
log("ด่านที่ 1: วัดความเร็วถอดรหัสจริง และหาว่าช้าที่จุดไหน (คลิป 128x96)")
log("----------------------------------------------------------------------")

h264_60 = os.path.join(TMP_DIR, "t60.h264")
h264_600 = os.path.join(TMP_DIR, "t600.h264")
ref_60 = os.path.join(TMP_DIR, "ref60.nv12")
out_60 = os.path.join(TMP_DIR, "out60.nv12")
out_600 = os.path.join(TMP_DIR, "out600.nv12")

run_cmd([ffmpeg_bin, "-y", "-loglevel", "error", "-f", "lavfi", "-i", "testsrc2=size=128x96:rate=60",
         "-frames:v", "60", "-c:v", "libx264", "-profile:v", "baseline", "-pix_fmt", "yuv420p", "-bf", "0",
         "-f", "h264", h264_60])
run_cmd([ffmpeg_bin, "-y", "-loglevel", "error", "-f", "lavfi", "-i", "testsrc2=size=128x96:rate=60",
         "-frames:v", "600", "-c:v", "libx264", "-profile:v", "baseline", "-pix_fmt", "yuv420p", "-bf", "0",
         "-f", "h264", h264_600])
run_cmd([ffmpeg_bin, "-y", "-loglevel", "error", "-i", h264_60, "-pix_fmt", "nv12", "-f", "rawvideo", ref_60])

# วัด CPU ล้วน
_, _, _, cpu_t600, _ = run_cmd([ffmpeg_bin, "-loglevel", "error", "-i", h264_600, "-pix_fmt", "nv12", "-f", "null", "-"])
cpu_fps = 600.0 / max(cpu_t600, 0.0001)

# วัด GPU 60 เฟรม และ 600 เฟรม (ลงไฟล์ vs ลง /dev/null)
rc60, out_s60, err_s60, t60, to60 = run_decode_smart(dec_bin, h264_60, out_60)
rc600, out_s600, err_s600, t600_disk, to600 = run_decode_smart(dec_bin, h264_600, out_600, timeout=30)
rc600_null, _, _, t600_null, _ = run_decode_smart(dec_bin, h264_600, "/dev/null", timeout=30)

frame_bytes_128 = 128 * 96 * 3 // 2
got_frames_600 = (os.path.getsize(out_600) // frame_bytes_128) if os.path.exists(out_600) else 0

if rc600 == 0 and got_frames_600 > 0:
    hw_fps_disk = got_frames_600 / max(t600_disk, 0.0001)
    hw_fps_pure = got_frames_600 / max(t600_null, 0.0001) if rc600_null == 0 else hw_fps_disk
    per_frame_ms = max((t600_disk - t60) / max(got_frames_600 - 60, 1) * 1000.0, 0.01)
    startup_ms = max((t60 * 1000.0) - (60 * per_frame_ms), 0.0)
    disk_overhead_pct = max((t600_disk - t600_null) / max(t600_disk, 0.0001) * 100.0, 0.0) if rc600_null == 0 else 0.0

    exact_60 = os.path.exists(out_60) and os.path.exists(ref_60) and (open(out_60, "rb").read() == open(ref_60, "rb").read())

    log(f"  - จำนวนเฟรมที่ถอดรหัสออกมาได้จริง : {got_frames_600} / 600 เฟรม")
    log(f"  - ความแม่นยำของเม็ดสีภาพ          : {'ตรงเป๊ะ 100% (ภาพใส ไม่เพี้ยน)' if exact_60 else 'เม็ดสีไม่ตรงต้นฉบับ!'}")
    log(f"  - ความเร็วการ์ดจอของเรา (รวมเขียนไฟล์): {hw_fps_disk:,.1f} ภาพ/วินาที (ใช้เวลา {t600_disk:.2f} วินาที)")
    log(f"  - ความเร็วการ์ดจอล้วน (ไม่คิดเขียนไฟล์): {hw_fps_pure:,.1f} ภาพ/วินาที")
    log(f"  - ความเร็วซีพียูเทียบอ้างอิง         : {cpu_fps:,.1f} ภาพ/วินาที")
    log(f"  - ผ่าเวลาทำงาน: ตอนเปิดเชื่อมต่อครั้งแรก ~{startup_ms:.1f} ms | เวลาเฉลี่ยต่อ 1 ภาพ ~{per_frame_ms:.2f} ms | เสียเวลาเขียนไฟล์ {disk_overhead_pct:.1f}%")

    if got_frames_600 < 600:
        problems.append((
            f"คลิป 600 เฟรม ถอดรหัสออกมาได้แค่ {got_frames_600} เฟรม (หายไป {600 - got_frames_600} เฟรม)",
            "ตัวตัดแบ่งข้อมูลวิดีโอหรือตัวนับเฟรมหยุดก่อนจบไฟล์ หรือบัฟเฟอร์เต็มกลางทาง",
            "แก้ลูปอ่านสตรีม H.264 / การคืนสล็อตหน่วยความจำใน myintel-decode ให้รันครบทุกเฟรมจนจบไฟล์"
        ))
    if hw_fps_pure < 60:
        problems.append((
            f"ความเร็วถอดรหัสยังต่ำกว่า 60 ภาพ/วินาที (ได้ {hw_fps_pure:.1f} fps หรือภาพละ {per_frame_ms:.2f} ms)",
            "โปรแกรมส่งงานทีละ 1 ภาพแล้วนั่งรอชิปทำเสร็จแบบตั้งเวลาหลับรอ (Polling/Sleep) หรือจองหน่วยความจำใหม่ทุกเฟรม",
            "เปลี่ยนจากหลับรอทีละเฟรมเป็นรอสัญญาณเตือนฮาร์ดแวร์ (IRQ) ที่ตอนนี้ทำงานแล้ว และใช้บัฟเฟอร์ซ้ำไม่ต้องจองใหม่ทุกเฟรม"
        ))
    elif per_frame_ms > 2.0:
        problems.append((
            f"เวลาต่อ 1 ภาพบนจอเล็กอยู่ที่ {per_frame_ms:.2f} ms พอขยายเป็น Full HD อาจตกลงต่ำกว่า 60 fps",
            "มีการก๊อปปี้ข้อมูลข้ามไปมาระหว่างโปรแกรมกับไดรเวอร์ หรือแปลงเรียงเม็ดสีด้วยซีพียูทีละเฟรม",
            "ใช้เอนจิน BCS Blitter (ที่เร็ว 0.048 ms) ช่วยก๊อปปี้/เรียงบล็อกภาพแทนซีพียู หรือทำคิวส่งงานล่วงหน้า"
        ))
else:
    log(f"  - [พบปัญหา!] ถอดรหัส 600 เฟรมไม่สำเร็จ (รหัสออก={rc600}, ค้างตัดเวลา={to600})")
    log(f"  - ข้อความแจ้งเตือน: {(err_s600 or out_s600).strip()[:240]}")
    problems.append((
        "รันคลิปยาว 600 เฟรมไม่จบ",
        f"โปรแกรมฟ้องเออเรอร์หรือติดลิมิตจำนวนเฟรมสูงสุด ({(err_s600 or out_s600).strip()[:120]})",
        "ตรวจสอบตัวจำกัดจำนวนเฟรมหรือการคืนหน่วยความจำใน myintel-decode"
    ))

# ----------------------------------------------------------------------
# ด่าน 2: ทดสอบความละเอียดใช้งานจริง (320x240 -> 640x360 -> 720p -> 1080p)
# ----------------------------------------------------------------------
log("\n----------------------------------------------------------------------")
log("ด่านที่ 2: ทดสอบขยายขนาดจอภาพใช้งานจริง (ตั้งแต่จอเล็ก ถึง 1080p Full HD)")
log("----------------------------------------------------------------------")

res_list = [
    ("320x240 (จอเล็ก)", 320, 240),
    ("640x360 (360p)", 640, 360),
    ("1280x720 (HD)", 1280, 720),
    ("1920x1080 (Full HD)", 1920, 1080),
]

wide_blocked_default = False
wide_broken_detile = False
wide_failed_1080 = False

for label, w, h in res_list:
    h264_r = os.path.join(TMP_DIR, f"r_{w}x{h}.h264")
    ref_r = os.path.join(TMP_DIR, f"ref_{w}x{h}.nv12")
    out_def = os.path.join(TMP_DIR, f"out_def_{w}x{h}.nv12")
    out_wide = os.path.join(TMP_DIR, f"out_wide_{w}x{h}.nv12")

    run_cmd([ffmpeg_bin, "-y", "-loglevel", "error", "-f", "lavfi", "-i", f"testsrc2=size={w}x{h}:rate=30",
             "-frames:v", "15", "-c:v", "libx264", "-profile:v", "baseline", "-pix_fmt", "yuv420p", "-bf", "0",
             "-f", "h264", h264_r])
    run_cmd([ffmpeg_bin, "-y", "-loglevel", "error", "-i", h264_r, "-pix_fmt", "nv12", "-f", "rawvideo", ref_r])

    rc_d, so_d, se_d, t_d, to_d = run_decode_smart(dec_bin, h264_r, out_def, timeout=15)
    sz_d = os.path.getsize(out_def) if os.path.exists(out_def) else 0
    exp_sz = (w * h * 3 // 2) * 15

    if rc_d == 0 and sz_d == exp_sz:
        ref_b = open(ref_r, "rb").read()
        out_b = open(out_def, "rb").read()
        status_d = "ผ่านปกติ ภาพตรง 100%" if ref_b == out_b else "รันออกแต่ภาพเพี้ยน!"
    else:
        wide_blocked_default = True
        msg_d = (se_d or so_d).strip().splitlines()[-1] if (se_d or so_d).strip() else f"rc={rc_d}, size={sz_d}"
        status_d = f"โหมดปกติปฏิเสธ ({msg_d[:70]})"

    status_w = "ไม่ได้ทดสอบ"
    if has_detile_wide and (rc_d != 0 or sz_d == 0):
        rc_w, so_w, se_w, t_w, to_w = run_decode_smart(dec_bin, h264_r, out_wide, extra_flag="--detile-wide", timeout=15)
        sz_w = os.path.getsize(out_wide) if os.path.exists(out_wide) else 0
        if to_w:
            status_w = "ค้างเกิน 15 วินาที (ระบบตัดทัน)"
            if w == 1920: wide_failed_1080 = True
        elif rc_w == 0 and sz_w > 0:
            ref_b = open(ref_r, "rb").read()
            out_b = open(out_wide, "rb").read()
            fps_w = (sz_w / (w * h * 3 // 2)) / max(t_w, 0.0001)
            if ref_b == out_b:
                status_w = f"โหมดจอกว้างผ่านเป๊ะ 100%! ({fps_w:.1f} fps)"
            else:
                wide_broken_detile = True
                row0_128_ok = (ref_b[:128] == out_b[:128])
                match_sample = sum(1 for a, b in zip(ref_b[::64], out_b[::64]) if a == b) / max(len(ref_b[::64]), 1) * 100.0
                hint = "ต้นบรรทัดตรง แต่ต่อจากความกว้าง 128 เพี้ยน -> สูตรเรียงบล็อกภาพ (TileY stride) คำนวณผิด!" if row0_128_ok else f"เม็ดสีตรงแค่ {match_sample:.1f}% -> ขนาดบัฟเฟอร์หรือค่า Pitch ในคำสั่งฮาร์ดแวร์ยังไม่รองรับจอกว้าง"
                status_w = f"ได้ไฟล์ ({fps_w:.1f} fps) แต่ภาพเพี้ยน: {hint}"
        else:
            if w == 1920: wide_failed_1080 = True
            msg_w = (se_w or so_w).strip().splitlines()[-1] if (se_w or so_w).strip() else f"rc={rc_w}, size={sz_w}"
            status_w = f"โหมดจอกว้างยังไม่ผ่าน ({msg_w[:80]})"

    log(f"  - ขนาด {label:<20}: โหมดปกติ = {status_d}")
    if status_w != "ไม่ได้ทดสอบ":
        log(f"    {'':<26}  โหมดจอกว้าง = {status_w}")

if wide_blocked_default:
    problems.append((
        "เปิดคลิปความละเอียดจริง (เช่น 720p / 1080p) ในโหมดปกติไม่ได้ เพราะถูกล็อกไว้แค่ความกว้าง 128 จุด",
        "โค้ดตั้งกำแพงกั้นความกว้าง 128 พิกเซลไว้เพื่อความปลอดภัยช่วงแรก",
        "ต้องแก้และยืนยันความถูกต้องของฟังก์ชันแปลงบล็อกภาพจอกว้าง (--detile-wide) และขนาดบัฟเฟอร์ในเคอร์เนลให้รองรับถึง 1920x1080 แล้วปลดล็อกเป็นค่าเริ่มต้น"
    ))
if wide_broken_detile:
    problems.append((
        "พอเปิดโหมดจอกว้าง (--detile-wide) ถอดรหัสออกแต่ภาพแตก/เรียงตำแหน่งเม็ดสีผิด",
        "ฮาร์ดแวร์ Intel เขียนภาพออกมาเป็นตารางบล็อก TileY (กว้างบล็อกละ 128 ไบต์) พอเกิน 1 บล็อก สูตรคำนวณระยะข้ามบล็อก (Pitch/Stride) ยังเรียงไม่ตรง",
        "แก้สูตรคำนวณระยะกว้าง (aligned width 128 ไบต์ และความสูงคูณ 32 แถวของ TileY) ในตัว detile ฝั่ง userspace แล้วเทียบกับไฟล์ต้นฉบับจนตรง 100%"
    ))
if wide_failed_1080:
    problems.append((
        "ความละเอียด 1920x1080 (Full HD) รันไม่ออกหรือบัฟเฟอร์ไม่พอ",
        "ภาพ 1080p 1 เฟรมใช้พื้นที่ประมาณ 3.1 MB (รวมเฟรมอ้างอิงหลายเฟรมอาจเกินขนาดสล็อตหน่วยความจำที่ไดรเวอร์แบ่งไว้)",
        "ขยายขนาดบัฟเฟอร์รับส่งภาพใน UserClient / Kext VCS slot ให้รองรับขนาดเฟรมระดับ 1080p/4K"
    ))

# ----------------------------------------------------------------------
# ด่าน 3: ทดสอบฟอร์แมตคลิปจริงในชีวิตประจำวัน (Main / High Profile / B-Frames)
# ----------------------------------------------------------------------
log("\n----------------------------------------------------------------------")
log("ด่านที่ 3: ทดสอบลูกเล่นวิดีโอที่เจอในคลิป YouTube / มือถือทั่วไป (บนขนาด 128x96)")
log("----------------------------------------------------------------------")

codec_cases = [
    ("Main Profile (เข้ารหัส CABAC มาตรฐานเว็บ)", ["-profile:v", "main", "-bf", "0"]),
    ("High Profile (มาตรฐาน YouTube / กล้องมือถือ)", ["-profile:v", "high", "-bf", "0"]),
    ("คลิปที่มี B-Frames (เฟรมสลับลำดับเวลาทั่วไป)", ["-profile:v", "main", "-bf", "2"]),
]

for cname, extra_ff in codec_cases:
    ch264 = os.path.join(TMP_DIR, f"c_{ abs(hash(cname)) % 10000 }.h264")
    cref = os.path.join(TMP_DIR, f"cref_{ abs(hash(cname)) % 10000 }.nv12")
    cout = os.path.join(TMP_DIR, f"cout_{ abs(hash(cname)) % 10000 }.nv12")

    run_cmd([ffmpeg_bin, "-y", "-loglevel", "error", "-f", "lavfi", "-i", "testsrc2=size=128x96:rate=30",
             "-frames:v", "30", "-c:v", "libx264", "-pix_fmt", "yuv420p"] + extra_ff + ["-f", "h264", ch264])
    run_cmd([ffmpeg_bin, "-y", "-loglevel", "error", "-i", ch264, "-pix_fmt", "nv12", "-f", "rawvideo", cref])

    rc_c, so_c, se_c, t_c, to_c = run_decode_smart(dec_bin, ch264, cout, timeout=15)
    sz_c = os.path.getsize(cout) if os.path.exists(cout) else 0
    exp_c = frame_bytes_128 * 30

    if rc_c == 0 and sz_c == exp_c:
        rb = open(cref, "rb").read()
        ob = open(cout, "rb").read()
        if rb == ob:
            log(f"  - {cname:<45}: ผ่านฉลุย ตรงเป๊ะ 100%!")
        else:
            # เช็กว่าเป็นแค่ลำดับเฟรมสลับเพราะ B-frames หรือภาพแตก
            f0_in_out = rb[:frame_bytes_128] == ob[:frame_bytes_128]
            log(f"  - {cname:<45}: ถอดรหัสออกครบ 30 เฟรม แต่ภาพไม่ตรงต้นฉบับ!")
            problems.append((
                f"คลิปแบบ {cname} ภาพเพี้ยนหรือเรียงลำดับเฟรมผิด",
                "ถ้าเป็น B-frame เกิดจากส่งภาพออกตามลำดับถอดรหัสแทนลำดับเวลาแสดงผล (POC) หรือถ้าเป็น High Profile เกิดจากตารางสเกล 8x8 ยังส่งเข้าชิปไม่ครบ",
                "เพิ่มการจัดเรียงลำดับเฟรมตามค่า POC สำหรับ B-frames และตรวจพารามิเตอร์ MFX_QM_STATE / 8x8 transform สำหรับ High Profile"
            ))
    else:
        msg_c = (se_c or so_c).strip().splitlines()[-1] if (se_c or so_c).strip() else f"rc={rc_c}, size={sz_c}/{exp_c}"
        log(f"  - {cname:<45}: ยังไม่รองรับ ({msg_c[:75]})")
        problems.append((
            f"ยังเปิดคลิปแบบ {cname} ไม่ได้ ({msg_c[:60]})",
            "คลิปจริงจากเน็ต/มือถือเกือบทั้งหมดใช้ High Profile และมี B-Frames ถ้ายังไม่รองรับจะเปิดเล่นไฟล์จริงข้างนอกไม่ได้",
            "อัปเดตตัวแกะหัวไฟล์ H.264 (SPS/PPS/Slice header) ใน myintel-decode และ myintel_dec.c ให้รองรับ CABAC, 8x8dct และ B-frames"
        ))

# ----------------------------------------------------------------------
# ด่าน 4: ยิงรัวต่อเนื่อง 10 รอบ เช็กอาการล้า / แรมรั่ว / จองสล็อตเต็ม
# ----------------------------------------------------------------------
log("\n----------------------------------------------------------------------")
log("ด่านที่ 4: ทดสอบรันซ้ำติดต่อกัน 10 รอบ (เช็กอาการความเร็วตก หรือแรมการ์ดจอเต็ม)")
log("----------------------------------------------------------------------")

run_times = []
stress_fail = 0
for i in range(10):
    sout = os.path.join(TMP_DIR, "stress_out.nv12")
    rc_s, so_s, se_s, ts, tos = run_decode_smart(dec_bin, h264_60, sout, timeout=10)
    if rc_s == 0:
        run_times.append(ts * 1000.0)
    else:
        stress_fail += 1

if stress_fail == 0 and len(run_times) == 10:
    slowdown = run_times[-1] / max(run_times[0], 0.1)
    log(f"  - ผลรัน 10 รอบติดกัน : ผ่านครบ 10/10 รอบ ไม่หลุด ไม่ค้าง!")
    log(f"  - รอบแรกใช้เวลา {run_times[0]:.1f} ms | รอบที่ 10 ใช้เวลา {run_times[-1]:.1f} ms | ค่ากลาง {statistics.median(run_times):.1f} ms")
    if slowdown > 1.6:
        problems.append((
            f"ยิ่งรันหลายรอบความเร็วยิ่งตก (รอบท้ายช้าลง {slowdown:.1f} เท่า)",
            "อาจมีการสะสมหน่วยความจำในไดรเวอร์ที่ยังคืนไม่หมดหลังปิดการเชื่อมต่อ",
            "ตรวจสอบฟังก์ชัน clientClose / cleanup ใน MyIntelGPUClient ว่าคืนสล็อต GGTT/IOMemoryDescriptor ครบทุกตัวหรือไม่"
        ))
else:
    log(f"  - [พบปัญหา!] รันติดต่อกัน 10 รอบ พลาดไป {stress_fail} รอบ!")
    problems.append((
        f"รันซ้ำติดกันแล้วหลุด/เออเรอร์ {stress_fail} จาก 10 รอบ",
        "พื้นที่พักข้อมูลในการ์ดจอหรือสล็อตคำสั่งถูกใช้แล้วคืนไม่ทัน",
        "ตรวจเช็กการคืนสล็อตใน MyIntelGPUClient ตอนปิดโปรแกรม"
    ))

# ----------------------------------------------------------------------
# ด่าน 5: ผ่าบันทึกข้างในไดรเวอร์ + อาการหน่วงของหน้าจอ macOS (2 นาทีล่าสุด)
# ----------------------------------------------------------------------
log("\n----------------------------------------------------------------------")
log("ด่านที่ 5: ผ่าเวลาทำงานข้างในไดรเวอร์เคอร์เนล & เช็กสุขภาพหน้าจอ macOS")
log("----------------------------------------------------------------------")

_, klog, _, _, _ = run_cmd([
    "/usr/bin/log", "show", "--style", "compact", "--last", "2m",
    "--predicate", 'eventMessage CONTAINS "MyIntelGPU" OR eventMessage CONTAINS "Fence timeout" OR eventMessage CONTAINS "CARenderServer"'
], timeout=12)

bcs_totals = [int(x) for x in re.findall(r'\[bcs-time\].*?total=(\d+)', klog)]
vcs_times = re.findall(r'\[vcs[^\]]*\][^\n]+', klog)
irq_lines = re.findall(r'\[irq\][^\n]+', klog)
fence_timeouts = [ln for ln in klog.splitlines() if ("Fence timeout" in ln or "render pipelines are not implemented" in ln)]
sandbox_denies = [ln for ln in klog.splitlines() if ("Sandbox:" in ln and "MyIntelGPU" in ln)]
k_errors = [ln for ln in klog.splitlines() if re.search(r'MyIntelGPU.*?(error|timeout|fail|hang|no memory|0xe000)', ln, re.I)]

log(f"  - สัญญาณเตือนจากชิปถอดรหัส (IRQ) 2 นาทีล่าสุด : พบ {len(irq_lines)} รายการ")
if bcs_totals:
    steady_bcs = [x for x in bcs_totals if x < 1000] or bcs_totals
    log(f"  - ความเร็วเอนจินก๊อปปี้ภาพ (BCS) ข้างในไดรเวอร์ : เฉลี่ย {statistics.median(steady_bcs):.1f} ไมโครวินาที/คำสั่ง (เร็วมาก)")
log(f"  - อาการหน้าจอ macOS สะดุด/ค้าง (Fence Timeout) : {'ไม่พบเลย (0 ครั้ง — หน้าจอลื่นปกติ!)' if not fence_timeouts else f'พบ {len(fence_timeouts)} ครั้ง!'}")
log(f"  - แอปของระบบถูก macOS กั้นสิทธิ์ไม่ให้เข้าถึงไดรเวอร์: พบ {len(sandbox_denies)} รายการ")
log(f"  - ข้อความฟ้องเออเรอร์จากข้างในไดรเวอร์         : {'ไม่พบเลย (0 ครั้ง — ไดรเวอร์สะอาด!)' if not k_errors else f'พบ {len(k_errors)} รายการ!'}")

if k_errors:
    for ke in k_errors[-3:]:
        log(f"      > {ke[-110:]}")
    problems.append((
        "พบข้อความเตือนเออเรอร์/รอนานจากข้างในไดรเวอร์ระหว่างทดสอบ",
        k_errors[-1][-100:],
        "นำบรรทัดเออเรอร์ข้างต้นไปเช็กจุด timeout หรือขนาดบัฟเฟอร์ในเคอร์เนล"
    ))

if fence_timeouts:
    problems.append((
        f"หน้าจอหลัก macOS แอบฟ้อง Fence Timeout {len(fence_timeouts)} ครั้ง",
        "WindowServer รอคำตอบจากกราฟิกนานเกินกำหนด ทำให้แอนิเมชันหน้าต่างกระตุกเป็นจังหวะ",
        "ตรวจสอบส่วนตอบกลับคำสั่งจำลองของ Metal / Display Pipe"
    ))

if sandbox_denies:
    problems.append((
        "เซอร์วิสวิดีโอของ macOS (เช่น ตัวเล่นวิดีโอของระบบ) ถูก Sandbox กั้นไม่ให้คุยกับไดรเวอร์ของเรา",
        "แอปมาตรฐานของ Apple มีเกราะ Sandbox กั้น UserClient ที่ไม่ได้เซ็นด้วยใบรับรองระบบของ Apple",
        "ใช้งานผ่านโปรแกรมเล่นวิดีโอภายนอกอย่าง FFmpeg / MPV / VLC แทนแอปติดเครื่อง จะเข้าถึงฮาร์ดแวร์ได้เต็ม 100% โดยไม่ติดบล็อก"
    ))

# ----------------------------------------------------------------------
# สรุปฟันธงภาษาคน: ตอนนี้ติดอะไรบ้าง และต้องสั่งแก้จุดไหนทันที!
# ----------------------------------------------------------------------
log("\n======================================================================")
log("  สรุปฟันธงภาษาคน: ปัญหาที่ขวางการใช้งานจริงตอนนี้ & วิธีสั่งแก้ทันที")
log("======================================================================")

if not problems:
    log("\n  ยอดเยี่ยมมาก! ไม่พบคอขวดหรือปัญหาในการทดสอบชุดนี้เลย ระบบพร้อมใช้งานจริงเต็มรูปแบบครับ!\n")
else:
    for idx, (title, why, fix) in enumerate(problems, 1):
        log(f"\n  [จุดที่ {idx}] ปัญหาที่เห็นตอนใช้งาน : {title}")
        log(f"           ทำไมถึงเกิดอาการนี้   : {why}")
        log(f"           สั่งแก้ตรงไหนได้ทันที  : {fix}")

log("\n======================================================================")
log(f"  บันทึกไฟล์รายงานฉบับเต็มไว้ที่หน้าจอ Desktop แล้ว: {REPORT_PATH}")
log("======================================================================")

with open(REPORT_PATH, "w", encoding="utf-8") as f:
    f.write("\n".join(lines_out) + "\n")

shutil.rmtree(TMP_DIR, ignore_errors=True)
