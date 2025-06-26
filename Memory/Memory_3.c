#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MEM_SIZE (256 * 1024 * 1024) // 256MB
#define PAGE_SIZE (4 * 1024) // 4KB
#define STATUS_FILE_PATH_LEN 128

void print_memory_status() {
    char status_path[STATUS_FILE_PATH_LEN];
    snprintf(status_path, STATUS_FILE_PATH_LEN, "/proc/%d/status", getpid());
    
    FILE *status_file = fopen(status_path, "r");
    if (!status_file) {
        perror("Failed to open status file");
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), status_file)) {
        if (strncmp(line, "VmSize:", 7) == 0 || 
            strncmp(line, "VmRSS:", 6) == 0 ||
            strncmp(line, "VmData:", 7) == 0) {
            printf("%s", line);
        }
    }
    fclose(status_file);
}

int main() {
    printf("初始内存状态:\n");
    print_memory_status();
    
    // 分配256MB内存
    printf("\n分配256MB内存...\n");
    char *mem = malloc(MEM_SIZE);
    if (!mem) {
        perror("malloc failed");
        return 1;
    }
    
    printf("\n分配后内存状态:\n");
    print_memory_status();
    
    // 每隔4KB进行读操作
    printf("\n每隔4KB进行读操作...\n");
    for (size_t i = 0; i < MEM_SIZE; i += PAGE_SIZE) {
        volatile char c = mem[i]; // 读取操作
    }
    
    printf("\n读操作后内存状态:\n");
    print_memory_status();
    
    // 每隔4KB进行写操作
    printf("\n每隔4KB进行写操作...\n");
    for (size_t i = 0; i < MEM_SIZE; i += PAGE_SIZE) {
        mem[i] = (char)(i % 256); // 写入操作
    }
    
    printf("\n写操作后内存状态:\n");
    print_memory_status();
    
    free(mem);
    return 0;
}
