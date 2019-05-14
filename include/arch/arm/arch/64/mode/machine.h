/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#ifndef __ARCH_MODE_MACHINE_H
#define __ARCH_MODE_MACHINE_H

#include <config.h>
#include <stdint.h>
#include <arch/types.h>
#include <arch/object/structures.h>
#include <arch/machine/hardware.h>
#include <plat/machine/hardware.h>
#include <armv/machine.h>
#include <arch/model/smp.h>

#include <machine/io.h>
#include <mode/machine_pl2.h>
#include <mode/hardware.h>

#ifdef ENABLE_SMP_SUPPORT
/* Use the first two SGI (Software Generated Interrupt) IDs
 * for seL4 IPI implementation. SGIs are per-core banked.
 */
#define irq_remote_call_ipi        0
#define irq_reschedule_ipi         1
#endif /* ENABLE_SMP_SUPPORT */

word_t PURE getRestartPC(tcb_t *thread);
void setNextPC(tcb_t *thread, word_t v);

static inline word_t getProcessorID(void)
{
    word_t processor_id;
    MRS("midr_el1", processor_id);
    return processor_id;
}

static inline word_t readSystemControlRegister(void)
{
    word_t scr;
    MRS("sctlr_el1", scr);
    return scr;
}

static inline void writeSystemControlRegister(word_t scr)
{
    MSR("sctlr_el1", scr);
}

static inline word_t readAuxiliaryControlRegister(void)
{
    word_t acr;
    MRS("actlr_el1", acr);
    return acr;
}

static inline void writeAuxiliaryControlRegister(word_t acr)
{
    MSR("actlr_el1", acr);
}

static inline void writeTPIDRPRW(word_t reg)
{
    if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
        MSR("tpidr_el2", reg);
    } else {
        MSR("tpidr_el1", reg);
    }
}

static inline void writeTPIDRURW(word_t reg)
{
    MSR("tpidr_el0", reg);
}

static inline word_t readTPIDRURO(void)
{
    word_t reg;
    MRS("tpidrro_el0", reg);
    return reg;
}

static inline void writeTPIDRURO(word_t reg)
{
    MSR("tpidrro_el0", reg);
}

static inline word_t readTPIDRURW(void)
{
    word_t reg;
    MRS("tpidr_el0", reg);
    return reg;
}

#define TCR_EL2_RES1 (BIT(23) | BIT(31))
#define TCR_EL2_T0SZ (16)
#define TCR_EL2_IRGN0_WBWC  BIT(8)
#define TCR_EL2_ORGN0_WBWC  BIT(10)
#define TCR_EL2_SH0_ISH     (3 << 12)
#define TCR_EL2_TG0_4K      (0 << 14)
#define TCR_EL2_TCR_PS_16T  (4 << 16)

/* The default value for TCR_EL2 is for 44-bit PARange. */
#define TCR_EL2_DEFAULT (TCR_EL2_T0SZ | TCR_EL2_IRGN0_WBWC | TCR_EL2_ORGN0_WBWC | \
                 TCR_EL2_SH0_ISH | TCR_EL2_TG0_4K | TCR_EL2_TCR_PS_16T  | \
                 TCR_EL2_RES1)

/* Check if the elfloader set up the TCR_EL2 correctly. */
static inline bool_t checkTCR_EL2(void)
{
    word_t tcr_el2 = 0;
    MRS("tcr_el2", tcr_el2);

    return (tcr_el2 == TCR_EL2_DEFAULT);
}

static inline void setCurrentKernelVSpaceRoot(ttbr_t ttbr)
{
    dsb();
    if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
        MSR("ttbr0_el2", ttbr.words[0]);
        dsb();
        isb();
        asm volatile("ic ialluis");
        dsb();
    } else {
        MSR("ttbr1_el1", ttbr.words[0]);
    }
    isb();
}

static inline void setCurrentUserVSpaceRoot(ttbr_t ttbr)
{
    dsb();
    if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
        MSR("vttbr_el2", ttbr.words[0]);
    } else {
        MSR("ttbr0_el1", ttbr.words[0]);
    }
    isb();
}

static inline word_t getVTTBR(void)
{
    word_t vttbr;
    MRS("vttbr_el2", vttbr);
    return vttbr;
}

static inline void setKernelStack(word_t stack_address)
{
    writeTPIDRPRW(stack_address);
}

static inline void setVtable(pptr_t addr)
{
    dsb();
    if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
        MSR("vbar_el2", addr);
    } else {
        MSR("vbar_el1", addr);
    }
    isb();
}

static inline void invalidateLocalTLB_EL2(void)
{
    asm volatile("tlbi alle2");
}

static inline void invalidateLocalTLB_EL1(void)
{
    asm volatile("tlbi alle1");
}

static inline void invalidateLocalTLB(void)
{
    dsb();
    if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
        invalidateLocalTLB_EL2();
        dsb();
        invalidateLocalTLB_EL1();
    } else {
        asm volatile("tlbi vmalle1");
    }
    dsb();
    isb();
}

static inline void invalidateLocalTLB_ASID(asid_t asid)
{
    assert(asid < BIT(16));

    dsb();
    asm volatile("tlbi aside1, %0" : : "r"(asid << 48));
    dsb();
    isb();
}

static inline void invalidateLocalTLB_VAASID(word_t mva_plus_asid)
{
    dsb();
    asm volatile("tlbi vae1, %0" : : "r"(mva_plus_asid));
    dsb();
    isb();
}

/* Invalidate all stage 1 and stage 2 translations used at
 * EL1 with the current VMID which is specified by vttbr_el2 */
static inline void invalidateLocalTLB_VMALLS12E1(void)
{
    asm volatile("tlbi vmalls12e1");
    dsb();
    isb();
}

/* Invalidate IPA with the current VMID */
static inline void invalidateLocalTLB_IPA(word_t ipa)
{
    asm volatile("tlbi ipas2e1, %0" :: "r"(ipa));
    dsb();
    asm volatile("tlbi vmalle1");
    dsb();
    isb();
}

void lockTLBEntry(vptr_t vaddr);

static inline void cleanByVA(vptr_t vaddr, paddr_t paddr)
{
    asm volatile("dc cvac, %0" : : "r"(vaddr));
    dmb();
}

static inline void cleanByVA_PoU(vptr_t vaddr, paddr_t paddr)
{
    asm volatile("dc cvau, %0" : : "r"(vaddr));
    dmb();
}

static inline void invalidateByVA(vptr_t vaddr, paddr_t paddr)
{
    asm volatile("dc ivac, %0" : : "r"(vaddr));
    dmb();
}

static inline void invalidateByVA_I(vptr_t vaddr, paddr_t paddr)
{
    asm volatile("ic ivau, %0" : : "r"(vaddr));
    isb();
}

static inline void invalidate_I_PoU(void)
{
    asm volatile("ic iallu");
    isb();
}

static inline void cleanInvalByVA(vptr_t vaddr, paddr_t paddr)
{
    asm volatile("dc civac, %0" : : "r"(vaddr));
    dsb();
}

static inline void branchFlush(vptr_t vaddr, paddr_t paddr)
{

}

#define getDFSR getESR
#define getIFSR getESR
static inline word_t PURE getESR(void)
{
    word_t ESR;
    if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
        MRS("esr_el2", ESR);
    } else {
        MRS("esr_el1", ESR);
    }
    return ESR;
}

static inline word_t PURE getFAR(void)
{
    word_t FAR;
    if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
        MRS("far_el2", FAR);
    } else {
        MRS("far_el1", FAR);
    }
    return FAR;
}

static inline word_t ats1e2r(word_t va)
{
    word_t par;
    asm volatile("at s1e2r, %0" :: "r"(va));
    MRS("par_el1", par);
    return par;
}

static inline word_t ats1e1r(word_t va)
{
    word_t par;
    asm volatile("at s1e1r, %0" :: "r"(va));
    MRS("par_el1", par);
    return par;
}


static inline word_t ats2e0r(word_t va)
{
    word_t par;
    asm volatile("at s12e0r, %0" :: "r"(va));
    MRS("par_el1", par);
    return par;
}

void arch_clean_invalidate_caches(void);

#endif /* __ARCH_MODE_MACHINE_H */
