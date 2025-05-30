#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#define SIZE_128MB (128 * 1024 * 1024)
#define SIZE_1024MB (1024 * 1024 * 1024)
#define PAGE_SIZE 4096  // 4KB页大小（Linux默认）
#define ALIGN_PAGE(x) ((uintptr_t)(x) & ~(PAGE_SIZE - 1))

void print_status() {
    char cmd[100];
    sprintf(cmd, "cat /proc/%d/status | grep -E 'VmSize|VmRSS|VmData'", getpid());
    system(cmd);
}

int main() {
    void *ptr[6];
    
    // 分配6个128MB空间
    for (int i = 0; i < 6; i++) {
        ptr[i] = malloc(SIZE_128MB);
        printf("Allocated 128MB block %d at %p\n", i+1, ptr[i]);
	printf("Align page address: %p\n", (void *)ALIGN_PAGE(ptr[i]));
        print_status();
    }
    
    // 释放第2、3、5号块
    free(ptr[1]); free(ptr[2]); free(ptr[4]);
    printf("Freed blocks 2, 3, 5\n");
    print_status();
    
    // 分配1024MB
    void *big_ptr = malloc(SIZE_1024MB);
    printf("Allocated 1024MB at %p\n", big_ptr);
    print_status();
    
    // 保持进程运行以便观察/proc/pid/maps
    printf("Process PID: %d \n", getpid());
    pause();
    return 0;
}
