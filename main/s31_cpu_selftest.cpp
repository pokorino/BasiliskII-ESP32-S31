#if S31_CPU_SELFTEST
#include <Arduino.h>
#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "rom_patches.h"
#include "emul_op.h"
#include "m68k.h"
#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"
#include "esp_heap_caps.h"

#if USE_JIT || USE_RV32_JIT
#error "CPU040 selftest must exercise the interpreter, not a JIT"
#endif

extern bool quit_program;
static unsigned passed, failed;
static constexpr uint32 code_base = 0x10000;
static constexpr uint32 stack_top = 0x70000;

static void check(const char *name, uint32 actual, uint32 expected) {
    bool ok = actual == expected;
    if (ok) ++passed; else ++failed;
    printf("CPU040 %s %s actual=%08lx expected=%08lx\n", ok ? "PASS" : "FAIL",
           name, (unsigned long)actual, (unsigned long)expected);
}

static void reset() {
    memset(RAMBaseHost, 0, RAMSize);
    memset(&regs, 0, sizeof(regs));
    m68k_reset();
    m68k_areg(regs, 7) = stack_top;
    regs.isp = stack_top;
    quit_program = false;
}

template<size_t N> static void run(const uint16 (&words)[N]) {
    for (size_t i = 0; i < N; ++i) put_word(code_base + i * 2, words[i]);
    // The production loop polls urgent flags every eight instructions.
    // Pad with return pseudo-ops so it cannot run beyond the test program.
    for (unsigned i = 0; i < 16; ++i) put_word(code_base + N * 2 + i * 2, M68K_EXEC_RETURN);
    m68k_setpc(code_base);
    fill_prefetch_0();
    m68k_execute();
}

static void selftest(void *) {
    initArduino();
    Serial.begin(115200);
    printf("CPU040 BEGIN production UAE interpreter; no SD, no Mac ROM\n");
    RAMSize = 1024 * 1024;
    ROMSize = 65536;
    RAMBaseHost = (uint8 *)heap_caps_calloc(1, RAMSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ROMBaseHost = (uint8 *)heap_caps_calloc(1, ROMSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!RAMBaseHost || !ROMBaseHost) {
        printf("CPU040 RESULT FAIL allocation\nCPU040 DONE\n"); vTaskDelete(nullptr); return;
    }
    CPUType = 4;
    FPUType = 1;
    ROMVersion = ROM_VERSION_32;
    if (!Init680x0()) {
        printf("CPU040 RESULT FAIL Init680x0\nCPU040 DONE\n"); vTaskDelete(nullptr); return;
    }
    check("68040-mode", CPUType, 4);
    reset();
    const uint16 arithmetic[] = {0x7006, 0xc0fc, 7, 0x5280}; // 6*7+1
    run(arithmetic); check("mulu-addq", m68k_dreg(regs, 0), 43);
    reset();
    const uint16 divide[] = {0x7064, 0x80fc, 7};
    run(divide); check("divu-remainder", m68k_dreg(regs, 0), 0x0002000e);
    reset();
    const uint16 loop[] = {0x7000, 0x7209, 0x5280, 0x51c9, 0xfffc};
    run(loop); check("dbra-loop", m68k_dreg(regs, 0), 10);
    check("dbra-counter", m68k_dreg(regs, 1) & 0xffff, 0xffff);
    reset(); m68k_areg(regs, 0) = 0x30000;
    const uint16 memory[] = {0x20bc, 0x1234, 0x5678, 0x2210};
    run(memory); check("move-memory", m68k_dreg(regs, 1), 0x12345678);
    check("big-endian-byte0", RAMBaseHost[0x30000], 0x12);
    check("big-endian-byte3", RAMBaseHost[0x30003], 0x78);
    reset(); m68k_dreg(regs, 0) = 0x7fffffff;
    const uint16 overflow[] = {0x5280};
    run(overflow); check("add-overflow-value", m68k_dreg(regs, 0), 0x80000000);
    check("add-overflow-V", GET_VFLG != 0, 1);
    check("add-overflow-N", GET_NFLG != 0, 1);
    reset(); m68k_dreg(regs, 0) = 0xffffffff;
    run(overflow); check("add-carry-value", m68k_dreg(regs, 0), 0);
    check("add-carry-C", GET_CFLG != 0, 1);
    check("add-carry-X", GET_XFLG != 0, 1);
    check("add-carry-Z", GET_ZFLG != 0, 1);
    reset(); m68k_dreg(regs, 0) = 0x81;
    const uint16 shift[] = {0xe188};
    run(shift); check("lsl-long", m68k_dreg(regs, 0), 0x8100);
    reset(); m68k_dreg(regs, 0) = 0x12345678;
    const uint16 bitfield[] = {0xe9c0, 0x1208}; // BFEXTU D0{8:8},D1
    run(bitfield); check("bfextu-020plus", m68k_dreg(regs, 1), 0x34);
    reset(); m68k_dreg(regs, 0) = 0xffffffff;
    const uint16 control[] = {0x4e7b, 0x0002, 0x4e7a, 0x1002};
    run(control); check("movec-040-CACR-mask", m68k_dreg(regs, 1), 0x80008000);
    reset();
    // JSR absolute to a tiny subroutine, then branch past its body.
    const uint16 subroutine[] = {0x7001, 0x4eb9, 0x0001, 0x000c, 0x6006,
                                0x4e71, 0x5480, 0x4e75};
    run(subroutine); check("jsr-rts", m68k_dreg(regs, 0), 3);
    check("jsr-stack", m68k_areg(regs, 7), stack_top);
    reset(); put_long(32 * 4, 0x20000);
    put_word(0x20000, 0x5280); put_word(0x20002, 0x4e73); // ADDQ; RTE
    const uint16 trap[] = {0x7004, 0x4e40, 0x5480};
    run(trap); check("trap-rte", m68k_dreg(regs, 0), 7);
    check("exception-stack", m68k_areg(regs, 7), stack_top);
    reset(); m68k_dreg(regs, 0) = 6; m68k_dreg(regs, 1) = 2;
    m68k_dreg(regs, 2) = 3; m68k_dreg(regs, 3) = 4;
    const uint16 floating[] = {0xf200, 0x4000, 0xf201, 0x4022, 0xf202, 0x4023,
                               0xf203, 0x4020, 0xf204, 0x6000};
    run(floating); check("fpu-move-add-mul-div", m68k_dreg(regs, 4), 6);
    reset(); m68k_areg(regs, 0) = 0x30000;
    // FMOVE.D #1.5,FP0; FADD.D #0.25,FP0; FMOVE.D FP0,(A0).
    const uint16 fractional[] = {0xf23c, 0x5400, 0x3ff8, 0x0000, 0x0000, 0x0000,
                                0xf23c, 0x5422, 0x3fd0, 0x0000, 0x0000, 0x0000,
                                0xf210, 0x7400};
    run(fractional); check("fpu-double-fraction-hi", get_long(0x30000), 0x3ffc0000);
    check("fpu-double-fraction-lo", get_long(0x30004), 0);
    // A guest shutdown during a nested 68k call must unwind without running
    // the instruction after the shutdown opcode, and remain requested.
    reset();
    put_word(0x20000, M68K_EMUL_OP_SHUTDOWN);
    put_word(0x20002, 0x70ff); // MOVEQ #-1,D0 must not execute
    put_word(0x20004, M68K_EXEC_RETURN);
    M68kRegisters shutdown_regs = {};
    shutdown_regs.d[0] = 42;
    Execute68k(0x20000, &shutdown_regs);
    check("shutdown-no-next-guest-instruction", shutdown_regs.d[0], 42);
    check("shutdown-survives-nested-call", quit_program, 1);
    printf("CPU040 RESULT %s passed=%u failed=%u\n", failed ? "FAIL" : "PASS", passed, failed);
    printf("CPU040 DONE\n");
    fflush(stdout);
    vTaskDelete(nullptr);
}

extern "C" bool s31_start_cpu_selftest() {
    return xTaskCreatePinnedToCore(selftest, "cpu040test", 32768, nullptr, 1, nullptr, 1) == pdPASS;
}
#endif
