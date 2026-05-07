.section .text
.global switch_sp_and_jump

switch_sp_and_jump:
    mv sp, a0
    jr a1