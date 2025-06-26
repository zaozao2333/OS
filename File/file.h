#ifndef FILE_H
#define FILE_H
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>

#define MAX_NAME_LEN 256
#define MAX_PATH_LEN 1024
#define BLOCK_SIZE 4096
#define MAX_BLOCKS (100 * 1024 * 1024 / BLOCK_SIZE) // 100MB / 4KB = 25600 blocks

// 文件类型枚举
typedef enum {
    FT_DIRECTORY,
    FT_REGULAR
} FileType;

// 文件系统超级块
typedef struct {
    uint32_t magic;         // 文件系统魔数
    uint32_t block_size;    // 块大小
    uint32_t total_blocks;  // 总块数
    uint32_t free_blocks;   // 空闲块数
    uint32_t root_inode;    // 根目录inode号
} SuperBlock;

// inode结构
typedef struct {
    uint32_t inode_no;      // inode编号
    FileType type;         // 文件类型
    uint32_t size;         // 文件大小(字节)
    uint32_t blocks;       // 使用的块数
    uint32_t block_ptrs[12]; // 直接块指针
    uint32_t indirect_ptr; // 一级间接块指针
    time_t create_time;    // 创建时间
    time_t modify_time;    // 修改时间
    sem_t wrt;         // 写互斥信号量
    sem_t mutex;       // 读计数器互斥信号量
    int read_count;    // 当前读者数量
    int is_readonly;   // 只读标志位 (1=只读)
} Inode;

// 目录项结构
typedef struct {
    char name[MAX_NAME_LEN]; // 文件名
    uint32_t inode_no;      // 对应的inode号
} DirEntry;

// 目录内容
typedef struct {
    DirEntry entries[64];   // 每个目录最多64个条目
    uint32_t count;         // 当前条目数
} Directory;

// 文件系统结构
typedef struct {
    SuperBlock super;       // 超级块
    uint8_t* bitmap;       // 块位图
    Inode* inodes;         // inode表
    uint8_t* blocks;       // 数据块
    uint32_t inode_count;  // inode总数
} MemoryFS;

// 初始化文件系统
int fs_init(MemoryFS* fs, void* addr, uint32_t size);

// 创建目录
int fs_mkdir(MemoryFS* fs, const char* path);

// 删除目录
int fs_rmdir(MemoryFS* fs, const char* path);

// 创建文件
int fs_open(MemoryFS* fs, const char* path);

// 删除文件
int fs_rm(MemoryFS* fs, const char* path);

// 重命名文件/目录
int fs_rename(MemoryFS* fs, const char* old_path, const char* new_path);

// 列出目录内容
int fs_ls(MemoryFS* fs, const char* path, DirEntry** entries, uint32_t* count);

// 写入文件
int fs_write(MemoryFS* fs, const char* path, const void* data, uint32_t size);

// 读取文件
int fs_read(MemoryFS* fs, const char* path, void* buf, uint32_t size);

// 释放文件系统资源
//void fs_destroy(MemoryFS* fs);
void parse_path(const char* path, char* parent_path, char* name);

Inode* find_inode_by_path(MemoryFS* fs, const char* path);

Inode* find_inode_by_no(MemoryFS* fs, uint32_t inode_no);

uint32_t find_free_block(MemoryFS* fs);

void mark_block_used(MemoryFS* fs, uint32_t block_no);

void mark_block_free(MemoryFS* fs, uint32_t block_no);

#endif