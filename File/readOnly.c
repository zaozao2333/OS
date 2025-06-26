#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include "file.h"

#define SHM_NAME "/memfs_shm"
#define SHM_SIZE (100 * 1024 * 1024) // 100MB

// 创建并初始化共享内存
void* create_shared_memory() {
    // 创建共享内存对象
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    
    // 设置共享内存大小
    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }
    
    // 映射共享内存到进程地址空间
    void* addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    
    close(shm_fd); // 文件描述符不再需要
    
    return addr;
}

// 创建只读文件并测试多进程访问
void test_readonly_file(MemoryFS* fs) {
    // 创建只读文件
    if (fs_open(fs, "ro_datafile") != 0) {
        fprintf(stderr, "Failed to create readonly file\n");
        exit(EXIT_FAILURE);
    }
    
    // 查找文件inode
    Inode* file_inode = find_inode_by_path(fs, "ro_datafile");
    if (!file_inode) {
        fprintf(stderr, "Failed to find readonly file\n");
        exit(EXIT_FAILURE);
    }
    
    // 分配数据块并写入一些测试数据
    uint32_t block_no = find_free_block(fs);
    if (block_no == (uint32_t)-1) {
        fprintf(stderr, "No free blocks available\n");
        exit(EXIT_FAILURE);
    }
    
    file_inode->block_ptrs[0] = block_no;
    file_inode->blocks = 1;
    mark_block_used(fs, block_no);
    
    // 写入测试数据
    const char* test_data = "This is readonly test data";
    memcpy(fs->blocks + block_no * BLOCK_SIZE, test_data, strlen(test_data) + 1);
    file_inode->size = strlen(test_data) + 1;
    
    // 创建多个子进程测试并发读取
    const int num_readers = 3;
    pid_t pids[num_readers];
    
    for (int i = 0; i < num_readers; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            // 子进程 - 读取文件内容
            char buf[256];
            int bytes_read = fs_read(fs, "ro_datafile", buf, sizeof(buf));
            
            if (bytes_read > 0) {
                printf("Reader %d (PID %d) read: %s\n", i+1, getpid(), buf);
            } else {
                printf("Reader %d (PID %d) failed to read\n", i+1, getpid());
            }
            
            // 尝试写入文件 - 应该失败
            if (fs_write(fs, "ro_datafile", "Attempt to write", 16) == -1) {
                printf("Reader %d (PID %d) correctly failed to write to readonly file\n", i+1, getpid());
            }
            
            exit(EXIT_SUCCESS);
        } else if (pids[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
    }
    
    // 父进程等待所有子进程结束
    for (int i = 0; i < num_readers; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

int main() {
    // 创建并映射共享内存
    void* shm_addr = create_shared_memory();
    
    // 初始化文件系统
    MemoryFS fs;
    if (fs_init(&fs, shm_addr, SHM_SIZE) != 0) {
        fprintf(stderr, "Failed to initialize filesystem\n");
        exit(EXIT_FAILURE);
    }
    
    // 测试只读文件和多进程读取
    test_readonly_file(&fs);
    
    // 清理
    if (munmap(shm_addr, SHM_SIZE) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }
    
    // 删除共享内存对象
    if (shm_unlink(SHM_NAME) == -1) {
        perror("shm_unlink");
        exit(EXIT_FAILURE);
    }
    
    return 0;
}