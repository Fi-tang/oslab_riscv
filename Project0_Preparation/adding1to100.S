.global main 
main:
    li t0, 1        # i = 1
    li t1, 101      # end of loop number + 1
    li t2, 0        # sum = 0
    bge t0,t1, done # i >= 101
loop:
    add t2, t2, t0 
    addi t0,t0,  1
    blt t0, t1, loop
done:               # finish sum operation
    li a0,0
    ret	
end:
    j end 
