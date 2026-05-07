.section ".text.boot"
.global _start
_start:
    la a3, __bss_start
    la a4, __bss_stop
    ble a4, a3, clear_bss_done
clear_bss:
    sd zero, (a3)
    add a3, a3, 8
    blt a3, a4, clear_bss
clear_bss_done:
    la a2, handle_exception # load addr to a2, cannot directly use label
    csrw stvec, a2 # csr write: write a2 to the csr register stvec, stvec saves addr of n trap routine
    la sp, _end
    tail start_kernel

.global handle_exception
.align 2 # align to 2^2 = 4 bytes, RISC-V instruction size is 4 bytes
/*
CPU prepares on trap entry:
  sepc         = address of trapped supervisor instruction
  scause       = trap cause
  stval        = extra fault info
  sstatus.SPP  = 0/1              // previous mode was U-mode/S-mode
  sstatus.SPIE = old sstatus.SIE
  sstatus.SIE  = 0              // Supervisor Interrupt Enable, if 0, all interrupt cannot trigger
  pc           = stvec
  privilege    = S-mode         // still trap into supervisor handler
*/
/*
If trap entered from U-mode, before trap happened:
  sscratch = kernel_sp
  sp       = user_sp
  privilege= U-mode
*/
/*
If trap entered from S-mode:, before trap happened:
  sscratch = 0
  sp       = kernel_sp
  privilege= S-mode
*/
handle_exception:
    /*
     * Retrieve kernel sp from sscratch and
     * save user sp to sscratch
     */
    csrrw sp, sscratch, sp #csrrw rd, csr, rs1: read old csr, write rs1 into that csr, put the old csr value into rd
    bnez sp, save_context # check if sscratch is non-zero before swap
    # sscratch is non-zero: kernel stack ready, likely trap came from user mode
    # sscratch is zero: likely already in kernel mode, so there is no separate user→kernel stack switch to do here

    # before swap: sp = kernel_sp
    #              sscratch = 0
    # in the case of kernel to kernel mode, read back to kernel stack pointer
    csrr sp, sscratch # read sscratch into sp, which is the kernel_sp

#now sp is kernel_sp
#saves the current trap context into a trap frame on the stack
save_context:
    /* Save context */
    addi sp, sp, -8 * 35 #move stack pointer downward, 280 bytes, [sp + 0]  ... [sp + 8*34] is area for save
    #GPR
    # store double
    sd ra,  8 *  0(sp)
    sd gp,  8 *  2(sp)
    sd tp,  8 *  3(sp)
    sd t0,  8 *  4(sp)
    sd t1,  8 *  5(sp)
    sd t2,  8 *  6(sp)
    sd s0,  8 *  7(sp)
    sd s1,  8 *  8(sp)
    sd a0,  8 *  9(sp)
    sd a1,  8 * 10(sp)
    sd a2,  8 * 11(sp)
    sd a3,  8 * 12(sp)
    sd a4,  8 * 13(sp)
    sd a5,  8 * 14(sp)
    sd a6,  8 * 15(sp)
    sd a7,  8 * 16(sp)
    sd s2,  8 * 17(sp)
    sd s3,  8 * 18(sp)
    sd s4,  8 * 19(sp)
    sd s5,  8 * 20(sp)
    sd s6,  8 * 21(sp)
    sd s7,  8 * 22(sp)
    sd s8,  8 * 23(sp)
    sd s9,  8 * 24(sp)
    sd s10, 8 * 25(sp)
    sd s11, 8 * 26(sp)
    sd t3,  8 * 27(sp)
    sd t4,  8 * 28(sp)
    sd t5,  8 * 29(sp)
    sd t6,  8 * 30(sp)
    # trap related CSRs
    #csr read
    csrr s0, sscratch # trap from U-mode -> sscratch now holds user_sp/ trap from S-mode -> sscratch now holds kernel_sp
    csrr s1, sepc # trapped instruction pc
    csrr s2, sstatus # SPP = 0 -> previous mode was U/ SPP = 1 -> previous mode was S, SIE = 0, SPIE = old SIE
    csrr s3, scause
    csrr s4, stval
    sd s0,  8 *  1(sp)
    sd s1,  8 * 31(sp)
    sd s2,  8 * 32(sp)
    sd s3,  8 * 33(sp)
    sd s4,  8 * 34(sp)
    # clears sscratch to 0, mark that we are in kernel trap handling
    csrw sscratch, zero 
    # move: addi a0, sp, 0
    move a0, sp # passes the base address of that saved frame, argument placed at a0
    call do_trap # call the function, sp alreay placed in a0

//function return, kernel_sp still points to trap frame low addr ( like max addr for trap frame)
ret_from_exception:
    ld s0,  8 * 32(sp) # loads sstatus from saved trap frame
    andi s0, s0, (1 << 8) # check bit 8, SPP, Supervisor Previous Privilege, 1: return to S-mode, 0: return to U-mode
    bnez s0, 1f # if SPP == 1, branch to 1, we do not need to prepare sscratch for a future user→kernel stack swap right now
    #SPP == 0
    addi s0, sp, 8 * 35 # sp now points to base (high addr) of trap frame, + 8*35 is the stack pointer after popping this frame, which is the kernel stack pointer
    //this prepares for next swap, kernel sp would point to the bottom of the stack
    csrw sscratch, s0 #saves stack pointer (now s0 holds) to sscratch, so next trap from U-mode goes to correct logic
1:
    /* Restore context */
    //load double
    ld s1,  8 * 31(sp) # sepc, where to resume
    ld s2,  8 * 32(sp) # sstatus, what mode to resume in
    csrw sepc, s1
    csrw sstatus, s2
    ld ra,  8 *  0(sp)
    ld gp,  8 *  2(sp)
    ld tp,  8 *  3(sp)
    ld t0,  8 *  4(sp)
    ld t1,  8 *  5(sp)
    ld t2,  8 *  6(sp)
    ld s0,  8 *  7(sp)
    ld s1,  8 *  8(sp)
    ld a0,  8 *  9(sp)
    ld a1,  8 * 10(sp)
    ld a2,  8 * 11(sp)
    ld a3,  8 * 12(sp)
    ld a4,  8 * 13(sp)
    ld a5,  8 * 14(sp)
    ld a6,  8 * 15(sp)
    ld a7,  8 * 16(sp)
    ld s2,  8 * 17(sp)
    ld s3,  8 * 18(sp)
    ld s4,  8 * 19(sp)
    ld s5,  8 * 20(sp)
    ld s6,  8 * 21(sp)
    ld s7,  8 * 22(sp)
    ld s8,  8 * 23(sp)
    ld s9,  8 * 24(sp)
    ld s10, 8 * 25(sp)
    ld s11, 8 * 26(sp)
    ld t3,  8 * 27(sp)
    ld t4,  8 * 28(sp)
    ld t5,  8 * 29(sp)
    ld t6,  8 * 30(sp)
    ld sp,  8 *  1(sp) # sp loaded last because we need to use it for loading other saved registers
    sret # return from trap, CPU use the sepc, sstatus that we just restore 
