.global main
main:
    li s1, 1        # number start from 1 i = 1
    li s2, 101      # number end at number < 101
    bge s1,s2, main_loop    # s1 >= 101, end ==> s1 < 101 continue 

main_loop:
    mv a0, s1       # passing parameter a0 = i 
    call is_prime

finish_calling:
    addi s1, s1, 1
    blt s1, s2, main_loop

end:
    j end 

is_prime:
    li t0, 2            # for(i = 2)
    div t1, a0, t0      # i < (number // 2) + 1
    addi t1,t1, 1       # t1 = (a0 // 2) + 1
    bge t0, t1, is_prime_finish 

loop:
    rem t2, a0, t0      # t2 = number % i 
    beq t2, zero, not_prime_false
    addi t0,t0, 1
    blt t0, t1, loop 

is_prime_finish:
    li a1, 1
    ret
not_prime_false:
    li a1, 0
    ret
