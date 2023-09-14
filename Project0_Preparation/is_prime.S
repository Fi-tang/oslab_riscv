.global main 
main:
    li a0, 6                # testing from 1 to 100, a0 means number
    li t0, 2                # for(i = 2)
    div t1, a0, t0          # i < (number // 2) + 1
    addi t1, t1, 1          # t1 = (number // 2) + 1
    bge t0, t1, loop_finish

loop:
    rem t2, a0, t0 
    beq t2 ,zero, not_prime_false
    addi t0,t0, 1
    blt t0, t1, loop 

loop_finish:
    li a1, 1
    j end 
not_prime_false:
    li a1, 0
    j end 
end:
    j end 
