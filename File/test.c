#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdatomic.h>

#define SHM_NAME "/my_shared_memory"
#include "file.h"

#define SHM_SIZE (100 * 1024 * 1024) // 100MB共享内存
#define MAX_INPUT_LEN 256
#define MAX_DATA_SIZE 1024

int main()
{
    // 1. 创建或打开共享内存对象
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open failed");
        exit(1);
    }

    // 2. 设置共享内存大小
    ftruncate(shm_fd, SHM_SIZE);

    // 3. 映射共享内存
    void *shm_addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_addr == MAP_FAILED)
    {
        perror("mmap failed");
        exit(1);
    }
    // 2. 初始化内存文件系统
    MemoryFS fs;
    if (fs_init(&fs, shm_addr, SHM_SIZE) != 0)
    {
        fprintf(stderr, "文件系统初始化失败\n");
        exit(1);
    }
    printf("内存文件系统初始化成功\n");
    printf("输入help查看可用命令\n");

    // 3. 实现文件系统操作
    char input[MAX_INPUT_LEN];
    char command[MAX_INPUT_LEN];
    char path1[MAX_PATH_LEN];
    char path2[MAX_PATH_LEN];
    char data[MAX_DATA_SIZE];

    while (1)
    {
        atomic_thread_fence(memory_order_seq_cst);
        printf("> ");
        fgets(input, MAX_INPUT_LEN, stdin);
        input[strcspn(input, "\n")] = '\0'; // 去除换行符

        if (strlen(input) == 0)
            continue;

        int args = sscanf(input, "%s %s %s", command, path1, path2);

        if (strcmp(command, "help") == 0)
        {
            printf("可用命令:\n");
            printf("  mkdir <path>        - 创建目录\n");
            printf("  rmdir <path>        - 删除目录\n");
            printf("  rename <old> <new>  - 重命名文件/目录\n");
            printf("  open <path>         - 创建/打开文件\n");
            printf("  write <path> <data> - 写入文件\n");
            printf("  read <path>         - 读取文件");
            printf("  rm <path>           - 删除文件\n");
            printf("  ls <path>           - 列出目录内容\n");
            printf("  exit                - 退出程序\n");
        }
        else if (strcmp(command, "mkdir") == 0)
        {
            if (args < 2)
            {
                printf("用法: mkdir <path>\n");
                continue;
            }
            if (fs_mkdir(&fs, path1) == 0)
            {
                printf("目录创建成功: %s\n", path1);
            }
            else
            {
                printf("目录创建失败\n");
            }
        }
        else if (strcmp(command, "rmdir") == 0)
        {
            if (args < 2)
            {
                printf("用法: rmdir <path>\n");
                continue;
            }
            if (fs_rmdir(&fs, path1) == 0)
            {
                printf("目录删除成功: %s\n", path1);
            }
            else
            {
                printf("目录删除失败\n");
            }
        }
        else if (strcmp(command, "rename") == 0)
        {
            if (args < 3)
            {
                printf("用法: rename <old_path> <new_path>\n");
                continue;
            }
            if (fs_rename(&fs, path1, path2) == 0)
            {
                printf("文件重命名成功: %s -> %s\n", path1, path2);
            }
            else
            {
                printf("文件重命名失败\n");
            }
        }
        else if (strcmp(command, "open") == 0)
        {
            if (args < 2)
            {
                printf("用法: open <path>\n");
                continue;
            }
            if (fs_open(&fs, path1) == 0)
            {
                printf("文件创建成功: %s\n", path1);
            }
            else
            {
                printf("文件创建失败\n");
            }
        }
        else if (strcmp(command, "write") == 0)
        {
            if (args < 3)
            {
                printf("用法: write <path> <data>\n");
                continue;
            }
            char *data_start = input + strlen(command) + strlen(path1) + 2;
            if (fs_write(&fs, path1, data_start, strlen(data_start)) > 0)
            {
                printf("数据写入成功\n");
            }
            else
            {
                printf("数据写入失败\n");
            }
        }
        else if (strcmp(command, "read") == 0)
        {
            if (args < 2)
            {
                printf("用法: read <path>\n");
                continue;
            }
            memset(data, 0, MAX_DATA_SIZE);
            int bytes_read = fs_read(&fs, path1, data, MAX_DATA_SIZE - 1);
            if (bytes_read > 0)
            {
                printf("读取内容: %s\n", data);
                printf("读取字节数: %d\n", bytes_read);
            }
            else
            {
                printf("读取失败\n");
            }
        }
        else if (strcmp(command, "rm") == 0)
        {
            if (args < 2)
            {
                printf("用法: rm <path>\n");
                continue;
            }
            if (fs_rm(&fs, path1) == 0)
            {
                printf("文件删除成功: %s\n", path1);
            }
            else
            {
                printf("文件删除失败\n");
            }
        }
        else if (strcmp(command, "ls") == 0)
        {
            if (args < 2)
            {
                printf("用法: ls <path>\n");
                continue;
            }
            DirEntry *entries;
            uint32_t count;
            if (fs_ls(&fs, path1, &entries, &count) == 0)
            {
                printf("目录内容:\n");
                for (uint32_t i = 0; i < count; i++)
                {
                    printf("  %s\n", entries[i].name);
                }
                free(entries);
            }
            else
            {
                printf("目录不存在\n");
            }
        }
        else if (strcmp(command, "exit") == 0)
        {
            munmap(shm_addr, SHM_SIZE);
            close(shm_fd);
            exit(0);
        }
        else
        {
            printf("未知命令: %s\n输入help查看可用命令\n", command);
        }
    }

    return 0;
}