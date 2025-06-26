#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#define SIZE_64MB (64 * 1024 * 1024)
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
    pause();
}
