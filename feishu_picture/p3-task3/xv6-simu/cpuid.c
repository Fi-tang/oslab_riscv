typedef unsigned long uint64;

static inline uint64 r_tp(){
    uint64 x;
    asm volatile("mv %0, tp" : "=r"(x));
    return x;
}

int cpuid(){
    int id = r_tp();
    return id;
}