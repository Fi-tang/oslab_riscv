volatile static int started = 0; // started cpu number

// start() jumps here in supervisor mode on all CPUs.
void main(){
    if(cpuid() == 0){
        printf("\nxv6 kernel is booting\n\n");
        kinit();            // physical page allocator
        procinit();         // process table
        trapinit();         // trap vectors
        trapinithart();     // install kernel trap vector
        plicinit();         // set up interrupt controller
        plicinithart();     // ask PLIC for device interrupts
        binit();            // buffer cache
        virtio_disk_init(); // emulated hard disk
        userinit();         // first user process
        __sync_synchronize();
        started = 1;
    }
    else{
        while(started == 0)
            ;
        __sync_synchronize();
        printf("hart %d starting\n", cpuid());
        trapinithart(); // install kernel trap vector
        plicinithart(); // ask PLIC for device interrupts
    }
    scheduler();
}