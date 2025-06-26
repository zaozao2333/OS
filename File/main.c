#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include "file.h"

#define SIZE (100 * 1024 * 1024) // 100MB

void* create_shared_memory() {
    int shm_fd = shm_open("/memory_fs", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return MAP_FAILED;
    }
    
    if (ftruncate(shm_fd, SIZE) == -1) {
        perror("ftruncate");
        shm_unlink("/memory_fs");
        return MAP_FAILED;
    }
    
    void* addr = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        shm_unlink("/memory_fs");
        return MAP_FAILED;
    }
    
    memset(addr, 0, SIZE);
    close(shm_fd); // 注意：关闭fd不影响映射
    return addr; // 返回映射地址
}

int main() {
    // 创建100MB共享内存
    void* addr = create_shared_memory();
    if (addr == MAP_FAILED) {
        fprintf(stderr, "Failed to create shared memory\n");
        return 1;
    }
    
    // 初始化文件系统
    MemoryFS fs;
    if (fs_init(&fs, addr, SIZE) != 0) {
        fprintf(stderr, "Failed to initialize file system\n");
        return 1;
    }
    
    // 创建目录
    if (fs_mkdir(&fs, "/home") != 0) {
        fprintf(stderr, "Failed to create /home directory\n");
    }
    
    if (fs_mkdir(&fs, "/home/user") != 0) {
        fprintf(stderr, "Failed to create /home/user directory\n");
    }
    
    // 创建文件
    if (fs_open(&fs, "/home/user/test.txt") != 0) {
        fprintf(stderr, "Failed to create test.txt\n");
    }
    if (fs_open(&fs, "/home/user/A.txt") != 0) {
        fprintf(stderr, "Failed to create A.txt\n");
    }
    
    
    // 写入文件
    /*
    const char* content = "Hello, Memory File System!";
    if (fs_write(&fs, "/home/user/test.txt", content, strlen(content), 0) != 0) {
        fprintf(stderr, "Failed to write to test.txt\n");
    }
        */
    
    // 列出目录内容
    DirEntry* entries;
    uint32_t count;
    if (fs_ls(&fs, "/home/user", &entries, &count) == 0) {
        printf("Contents of /home/user:\n");
        for (uint32_t i = 0; i < count; i++) {
            printf("  %s\n", entries[i].name);
        }
        free(entries);
    }
    
    /*
    // 读取文件内容
    char buf[256];
    if (fs_read(&fs, "/home/user/test.txt", buf, sizeof(buf), 0) > 0) {
        printf("File content: %s\n", buf);
    }
        */
    
    // 删除文件
    if (fs_rm(&fs, "/home/user/test.txt") != 0) {
        fprintf(stderr, "Failed to delete test.txt\n");
    }
    if (fs_rm(&fs, "/home/user/A.txt") != 0) {
        fprintf(stderr, "Failed to delete A.txt\n");
    }
    
    // 删除目录
    if (fs_rmdir(&fs, "/home/user") != 0) {
        fprintf(stderr, "Failed to delete /home/user directory\n");
    }
    
    if (fs_rmdir(&fs, "/home") != 0) {
        fprintf(stderr, "Failed to delete /home directory\n");
    }
    
    // 释放资源
    //fs_destroy(&fs);
    
    return 0;
}
