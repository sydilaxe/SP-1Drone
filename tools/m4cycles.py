#!/usr/bin/env python3
"""Cortex-M4 cycle estimate for a bare-metal ELF exposing bench_init() and bench_block().

Runs the code in Unicorn and weights each executed instruction with an approximate M4F cost
(ARM Cortex-M4 TRM): VDIV/VSQRT 14, VMLA family 3, loads 2, divides 7, a taken branch +1..2.
Flash wait states and cache misses are ignored, so the real figure is higher.

  m4cycles.py ELF [-v]     (GCC in the environment: the arm-zephyr-eabi-gcc used to build it)
"""
import bisect, collections, os, subprocess, sys, tempfile
import capstone
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_C1_C0_2, UC_ARM_REG_FPEXC)

elf = sys.argv[1]
verbose = "-v" in sys.argv
frames = int(os.environ.get("FRAMES", "128"))
gcc = os.environ["GCC"]
tool = lambda name: gcc[: -len("gcc")] + name
tmp = tempfile.mkdtemp()

syms = {}
for line in subprocess.check_output([tool("nm"), elf]).decode().splitlines():
    p = line.split()
    if len(p) == 3:
        syms[p[2]] = int(p[0], 16)
text, data = os.path.join(tmp, "t.bin"), os.path.join(tmp, "d.bin")
subprocess.check_call([tool("objcopy"), "-O", "binary", "-j", ".text", "-j", ".rodata", elf, text])
subprocess.check_call([tool("objcopy"), "-O", "binary", "-j", ".data", elf, data])

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
uc.mem_map(0x0, 0x10000)
uc.mem_map(0x10000, 0x200000)
uc.mem_map(0x20000000, 0x100000)
uc.mem_write(0x10000, open(text, "rb").read())
uc.mem_write(0x20000000, open(data, "rb").read())
uc.reg_write(UC_ARM_REG_C1_C0_2, uc.reg_read(UC_ARM_REG_C1_C0_2) | (0xF << 20))   # FPU access
uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)

md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB | capstone.CS_MODE_MCLASS)
LOADSTORE_MULTI = {"vpush", "vpop", "vldm", "vldmia", "vstm", "vstmia", "vstmdb", "push", "pop", "ldm", "stm",
                   "ldmia", "stmia", "stmdb"}
MAC = {"vmla", "vmls", "vfma", "vfms", "vnmla", "vnmls", "vfnma", "vfnms"}


def cost(ins):
    m = ins.mnemonic.split(".")[0]
    if m in ("vdiv", "vsqrt"):
        return 14
    if m in MAC:
        return 3
    if m in LOADSTORE_MULTI:
        return 1 + ins.op_str.count(",") + 1
    if m in ("vldr", "vstr") or m.startswith("ldr"):
        return 2
    if m in ("sdiv", "udiv"):
        return 7
    return 1


funcs = sorted((a & ~1, n) for n, a in syms.items() if 0x10000 <= a < 0x20000000)
starts = [a for a, _ in funcs]
memo, fmemo = {}, {}
total = collections.Counter()
perfn = collections.Counter()
hot = collections.Counter()
state = {"on": False, "last": None}


def func_of(addr):
    f = fmemo.get(addr)
    if f is None:
        i = bisect.bisect_right(starts, addr) - 1
        f = fmemo[addr] = funcs[i][1] if i >= 0 else "?"
    return f


def hook(uc_, addr, size, _):
    if not state["on"]:
        return
    c = memo.get(addr)
    if c is None:
        ins = next(md.disasm(bytes(uc_.mem_read(addr, size)), addr))
        c = memo[addr] = cost(ins)
    last = state["last"]
    if last is not None and addr != last[0] + last[1]:
        c += 2   # a taken branch refills the pipeline
    total["cyc"] += c
    total["ins"] += 1
    perfn[func_of(addr)] += c
    hot[addr] += c
    state["last"] = (addr, size)


uc.hook_add(UC_HOOK_CODE, hook)


def call(name):
    uc.reg_write(UC_ARM_REG_SP, 0x200F0000)
    uc.reg_write(UC_ARM_REG_LR, 0x0FFF1)
    state["last"] = None
    uc.emu_start(syms[name] | 1, 0x0FFF0)


call("bench_init")
for _ in range(int(os.environ.get("WARM", "60"))):
    call("bench_block")
state["on"] = True
n = int(os.environ.get("BLOCKS", "20"))
for _ in range(n):
    call("bench_block")
cyc = total["cyc"] / n
budget = 64e6 * frames / float(os.environ.get("RATE", "48000"))
print(f"cycles per {frames}-frame block: {cyc:.0f} of {budget:.0f} ({100 * cyc / budget:.0f}% of the 64 MHz M4)")
if verbose:
    for name, c in perfn.most_common(14):
        print(f"  {name:30s} {c / n / frames:8.1f} cycles/frame")
if os.environ.get("HOT"):
    want = os.environ["HOT"]
    for a, c in sorted(hot.items()):
        if func_of(a).startswith(want) and c / n / frames >= float(os.environ.get("HOT_MIN", "1")):
            ins = next(md.disasm(bytes(uc.mem_read(a, 4)), a))
            print(f"  {a:08x} {c / n / frames:6.1f}  {ins.mnemonic} {ins.op_str}")
