.global main 
main:
    li a0, 43           #  for reduction of loop, change for(int i = 2; i < number - 1)
                        # to for(int i = 2; i < (number // 2 + 1); )        p * q = number, p <= sqrt(number) <= number / 2
    li t0, 2            # t0 = 2
    div t1,a0, t0      # t1 = a0 // 2
    addi t1,t1, 1       # count from i = 2 to i < (number // 2 + 1) 
    
    bge t0, t1, done

loop:
    rem t2,a0,t0 
    beq t2, zero, false_notPrime
    addi t0, t0, 1
    blt t0, t1, loop 

done:
    li a1, 0    # 0 --> is prime 
    j end    
false_notPrime:
    li a1, 1    # 1 --> not prime 
    j end 

end:
    j end 