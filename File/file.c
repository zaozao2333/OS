#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include "file.h"
#define FS_MAGIC 0x4D454D46 // 'MEMF'

int fs_init(MemoryFS *fs, void *addr, uint32_t size)
{
    if (size < (sizeof(SuperBlock) + MAX_BLOCKS / 8 + 100 * sizeof(Inode) + BLOCK_SIZE))
    {
        return -1; // 空间不足
    }

    uint8_t *base = (uint8_t *)addr;

    // 初始化超级块
    fs->super.magic = FS_MAGIC;
    fs->super.block_size = BLOCK_SIZE;
    fs->super.total_blocks = MAX_BLOCKS;
    fs->super.free_blocks = MAX_BLOCKS;
    fs->super.root_inode = 1; // 根目录inode号为1

    // 计算各部分位置
    fs->bitmap = base + sizeof(SuperBlock);
    fs->inodes = (Inode *)(fs->bitmap + (MAX_BLOCKS + 7) / 8);
    fs->blocks = (uint8_t *)(fs->inodes + 100); // 假设最多100个inode

    // 初始化位图 (全部置0表示空闲)
    memset(fs->bitmap, 0, (MAX_BLOCKS + 7) / 8);

    // 初始化inode表
    memset(fs->inodes, 0, 100 * sizeof(Inode));

    // 创建根目录
    Inode *root_inode = &fs->inodes[0];
    root_inode->inode_no = 1;
    root_inode->type = FT_DIRECTORY;
    root_inode->size = sizeof(Directory);
    root_inode->blocks = 1;
    root_inode->block_ptrs[0] = 0; // 使用第一个数据块

    sem_init(&root_inode->wrt, 1, 1);   // 初始值为1
    sem_init(&root_inode->mutex, 1, 1); // 初始值为1
    root_inode->read_count = 0;
    root_inode->is_readonly = 0; // 目录可写

    // 标记使用的块
    fs->bitmap[0] = 1; // 位图第一个bit表示第一个块已使用

    // 初始化根目录内容
    Directory *root_dir = (Directory *)(fs->blocks + root_inode->block_ptrs[0] * BLOCK_SIZE);
    root_dir->count = 0;

    fs->inode_count = 1;

    return 0;
}

int fs_mkdir(MemoryFS *fs, const char *path)
{
    // 解析路径，找到父目录和要创建的目录名
    char parent_path[MAX_PATH_LEN];
    char dir_name[MAX_NAME_LEN];
    parse_path(path, parent_path, dir_name);

    // 查找父目录
    Inode *parent_inode = find_inode_by_path(fs, parent_path);
    if (!parent_inode || parent_inode->type != FT_DIRECTORY)
    {
        return -1; // 父目录不存在或不是目录
    }

    // 检查目录是否已存在
    Directory *parent_dir = (Directory *)(fs->blocks + parent_inode->block_ptrs[0] * BLOCK_SIZE);
    for (uint32_t i = 0; i < parent_dir->count; i++)
    {
        if (strcmp(parent_dir->entries[i].name, dir_name) == 0)
        {
            return -1; // 目录已存在
        }
    }

    // 分配新的inode
    if (fs->inode_count >= 100)
    {
        return -1; // inode用尽
    }

    Inode *new_inode = &fs->inodes[fs->inode_count];
    new_inode->inode_no = fs->inode_count + 1;
    new_inode->type = FT_DIRECTORY;
    new_inode->size = sizeof(Directory);
    new_inode->blocks = 1;

    // 分配数据块
    uint32_t block_no = find_free_block(fs);
    if (block_no == (uint32_t)-1)
    {
        return -1; // 没有空闲块
    }

    new_inode->block_ptrs[0] = block_no;
    mark_block_used(fs, block_no);

    // 初始化目录内容
    Directory *new_dir = (Directory *)(fs->blocks + block_no * BLOCK_SIZE);
    new_dir->count = 0;

    // 添加"."和".."条目
    strcpy(new_dir->entries[0].name, ".");
    new_dir->entries[0].inode_no = new_inode->inode_no;
    strcpy(new_dir->entries[1].name, "..");
    new_dir->entries[1].inode_no = parent_inode->inode_no;
    new_dir->count = 2;

    // 添加到父目录
    strcpy(parent_dir->entries[parent_dir->count].name, dir_name);
    parent_dir->entries[parent_dir->count].inode_no = new_inode->inode_no;
    parent_dir->count++;

    fs->inode_count++;
    fs->super.free_blocks--;

    return 0;
}

int fs_rmdir(MemoryFS *fs, const char *path)
{
    // 解析路径，找到父目录和要删除的目录名
    char parent_path[MAX_PATH_LEN];
    char dir_name[MAX_NAME_LEN];
    parse_path(path, parent_path, dir_name);

    // 查找父目录
    Inode *parent_inode = find_inode_by_path(fs, parent_path);
    if (!parent_inode || parent_inode->type != FT_DIRECTORY)
    {
        return -1; // 父目录不存在或不是目录
    }

    // 查找要删除的目录
    Directory *parent_dir = (Directory *)(fs->blocks + parent_inode->block_ptrs[0] * BLOCK_SIZE);
    uint32_t dir_index = (uint32_t)-1;
    uint32_t dir_inode_no = 0;

    for (uint32_t i = 0; i < parent_dir->count; i++)
    {
        if (strcmp(parent_dir->entries[i].name, dir_name) == 0)
        {
            dir_index = i;
            dir_inode_no = parent_dir->entries[i].inode_no;
            break;
        }
    }

    if (dir_index == (uint32_t)-1)
    {
        return -1; // 目录不存在
    }

    // 获取要删除的目录的inode
    Inode *dir_inode = find_inode_by_no(fs, dir_inode_no);
    if (!dir_inode || dir_inode->type != FT_DIRECTORY)
    {
        return -1;
    }

    // 检查目录是否为空（只应包含"."和".."）
    Directory *dir = (Directory *)(fs->blocks + dir_inode->block_ptrs[0] * BLOCK_SIZE);
    if (dir->count > 2)
    {
        return -1; // 目录不为空
    }

    // 释放数据块
    mark_block_free(fs, dir_inode->block_ptrs[0]);

    // 从父目录中移除
    for (uint32_t i = dir_index; i < parent_dir->count - 1; i++)
    {
        parent_dir->entries[i] = parent_dir->entries[i + 1];
    }
    parent_dir->count--;

    // 标记inode为未使用
    memset(dir_inode, 0, sizeof(Inode));

    fs->super.free_blocks++;

    return 0;
}

int fs_open(MemoryFS *fs, const char *path)
{
    // 解析路径，找到父目录和要创建的文件名
    char parent_path[MAX_PATH_LEN];
    char file_name[MAX_NAME_LEN];
    parse_path(path, parent_path, file_name);

    // 查找父目录
    Inode *parent_inode = find_inode_by_path(fs, parent_path);
    if (!parent_inode || parent_inode->type != FT_DIRECTORY)
    {
        return -1; // 父目录不存在或不是目录
    }

    // 检查文件是否已存在
    Directory *parent_dir = (Directory *)(fs->blocks + parent_inode->block_ptrs[0] * BLOCK_SIZE);
    for (uint32_t i = 0; i < parent_dir->count; i++)
    {
        if (strcmp(parent_dir->entries[i].name, file_name) == 0)
        {
            return -1; // 文件已存在
        }
    }

    // 分配新的inode
    if (fs->inode_count >= 100)
    {
        return -1; // inode用尽
    }

    Inode *new_inode = &fs->inodes[fs->inode_count];
    new_inode->inode_no = fs->inode_count + 1;
    new_inode->type = FT_REGULAR;
    new_inode->size = 0;
    new_inode->blocks = 0;

    // 设置只读属性（示例：特定文件名）
    if (strstr(path, "ro_") == path)
    { // 以"ro_"开头的文件为只读
        new_inode->is_readonly = 1;
    }
    else
    {
        new_inode->is_readonly = 0;
    }
    new_inode->read_count = 0;
    sem_init(&new_inode->wrt, 1, 1);
    sem_init(&new_inode->mutex, 1, 1);

    // 添加到父目录
    strcpy(parent_dir->entries[parent_dir->count].name, file_name);
    parent_dir->entries[parent_dir->count].inode_no = new_inode->inode_no;
    parent_dir->count++;

    fs->inode_count++;

    return 0;
}

int fs_rm(MemoryFS *fs, const char *path)
{
    // 解析路径，找到父目录和要删除的文件名
    char parent_path[MAX_PATH_LEN];
    char file_name[MAX_NAME_LEN];
    parse_path(path, parent_path, file_name);

    // 查找父目录
    Inode *parent_inode = find_inode_by_path(fs, parent_path);
    if (!parent_inode || parent_inode->type != FT_DIRECTORY)
    {
        return -1; // 父目录不存在或不是目录
    }

    // 查找要删除的文件
    Directory *parent_dir = (Directory *)(fs->blocks + parent_inode->block_ptrs[0] * BLOCK_SIZE);
    uint32_t file_index = (uint32_t)-1;
    uint32_t file_inode_no = 0;

    for (uint32_t i = 0; i < parent_dir->count; i++)
    {
        if (strcmp(parent_dir->entries[i].name, file_name) == 0)
        {
            file_index = i;
            file_inode_no = parent_dir->entries[i].inode_no;
            break;
        }
    }

    if (file_index == (uint32_t)-1)
    {
        return -1; // 文件不存在
    }

    // 获取要删除的文件的inode
    Inode *file_inode = find_inode_by_no(fs, file_inode_no);
    if (!file_inode || file_inode->type != FT_REGULAR)
    {
        return -1;
    }

    // 释放信号量资源
    sem_destroy(&file_inode->wrt);
    sem_destroy(&file_inode->mutex);

    // 释放所有数据块
    for (uint32_t i = 0; i < file_inode->blocks; i++)
    {
        if (i < 12)
        {
            mark_block_free(fs, file_inode->block_ptrs[i]);
        }
        else if (i == 12)
        {
            // 释放间接块
            uint32_t *indirect_block = (uint32_t *)(fs->blocks + file_inode->indirect_ptr * BLOCK_SIZE);
            for (uint32_t j = 0; j < (file_inode->blocks - 12); j++)
            {
                mark_block_free(fs, indirect_block[j]);
            }
            mark_block_free(fs, file_inode->indirect_ptr);
        }
    }

    // 从父目录中移除
    for (uint32_t i = file_index; i < parent_dir->count - 1; i++)
    {
        parent_dir->entries[i] = parent_dir->entries[i + 1];
    }
    parent_dir->count--;

    // 标记inode为未使用
    memset(file_inode, 0, sizeof(Inode));

    fs->super.free_blocks += file_inode->blocks;

    return 0;
}

int fs_ls(MemoryFS *fs, const char *path, DirEntry **entries, uint32_t *count)
{
    // 查找目录inode
    Inode *dir_inode = find_inode_by_path(fs, path);
    if (!dir_inode || dir_inode->type != FT_DIRECTORY)
    {
        return -1; // 路径不存在或不是目录
    }

    // 获取目录内容
    Directory *dir = (Directory *)(fs->blocks + dir_inode->block_ptrs[0] * BLOCK_SIZE);

    // 分配空间返回结果
    *entries = malloc(dir->count * sizeof(DirEntry));
    if (!*entries)
    {
        return -1; // 内存不足
    }

    // 复制条目
    *count = 0;
    for (uint32_t i = 0; i < dir->count; i++)
    {
            (*entries)[*count] = dir->entries[i];
            (*count)++;
    }

    return 0;
}

// 读文件实现
int fs_read(MemoryFS *fs, const char *path, void *buf, uint32_t size)
{
    Inode *file_inode = find_inode_by_path(fs, path);
    if (!file_inode || file_inode->type != FT_REGULAR)
        return -1;

    // 读者进入协议
    sem_wait(&file_inode->mutex);
    file_inode->read_count++;
    if (file_inode->read_count == 1)
    {
        sem_wait(&file_inode->wrt); // 第一个读者阻塞写者
    }
    sem_post(&file_inode->mutex);

    // 读取数据（简化实现）
    uint32_t block_offset = file_inode->block_ptrs[0] * BLOCK_SIZE; // 乘以块大小
    memcpy(buf, fs->blocks + block_offset, size);

    // 读者退出协议
    sem_wait(&file_inode->mutex);
    file_inode->read_count--;
    if (file_inode->read_count == 0)
    {
        sem_post(&file_inode->wrt); // 最后一个读者释放写锁
    }
    sem_post(&file_inode->mutex);

    return size;
}

// 写文件实现
int fs_write(MemoryFS *fs, const char *path, const void *data, uint32_t size)
{
    Inode *file_inode = find_inode_by_path(fs, path);
    if (!file_inode || file_inode->type != FT_REGULAR)
        return -1;
    sem_wait(&file_inode->mutex);
    int readonly = file_inode->is_readonly;
    sem_post(&file_inode->mutex);

    if (readonly)
        return -1;
    sem_wait(&file_inode->wrt);

    // 写入数据（简化实现）
    uint32_t block_offset = file_inode->block_ptrs[0] * BLOCK_SIZE; // 乘以块大小
    memcpy(fs->blocks + block_offset, data, size);

    // 写者退出协议
    sem_post(&file_inode->wrt);

    return size;
}

int fs_rename(MemoryFS *fs, const char *old_path, const char *new_path) {
    // 1. 解析旧路径的父目录和文件名
    char old_parent_path[MAX_PATH_LEN];
    char old_name[MAX_NAME_LEN];
    parse_path(old_path, old_parent_path, old_name);

    // 2. 解析新路径的父目录和文件名
    char new_parent_path[MAX_PATH_LEN];
    char new_name[MAX_NAME_LEN];
    parse_path(new_path, new_parent_path, new_name);

    // 3. 查找旧路径的父目录和文件/目录inode
    Inode *old_parent_inode = find_inode_by_path(fs, old_parent_path);
    if (!old_parent_inode || old_parent_inode->type != FT_DIRECTORY) {
        return -1; // 旧父目录无效
    }

    Directory *old_parent_dir = (Directory *)(fs->blocks + old_parent_inode->block_ptrs[0] * BLOCK_SIZE);
    uint32_t old_entry_index = (uint32_t)-1;
    uint32_t old_inode_no = 0;

    // 查找旧目录项
    for (uint32_t i = 0; i < old_parent_dir->count; i++) {
        if (strcmp(old_parent_dir->entries[i].name, old_name) == 0) {
            old_entry_index = i;
            old_inode_no = old_parent_dir->entries[i].inode_no;
            break;
        }
    }
    if (old_entry_index == (uint32_t)-1) {
        return -1; // 旧路径不存在
    }

    Inode *target_inode = find_inode_by_no(fs, old_inode_no);
    if (!target_inode) {
        return -1; // inode无效
    }

    // 4. 检查目标是否只读（如果是文件）
    if (target_inode->type == FT_REGULAR && target_inode->is_readonly) {
        return -1; // 只读文件不可重命名
    }

    // 5. 处理新路径
    Inode *new_parent_inode = find_inode_by_path(fs, new_parent_path);
    if (!new_parent_inode || new_parent_inode->type != FT_DIRECTORY) {
        return -1; // 新父目录无效
    }

    Directory *new_parent_dir = (Directory *)(fs->blocks + new_parent_inode->block_ptrs[0] * BLOCK_SIZE);

    // 检查新名称是否已存在
    for (uint32_t i = 0; i < new_parent_dir->count; i++) {
        if (strcmp(new_parent_dir->entries[i].name, new_name) == 0) {
            return -1; // 新名称已存在
        }
    }

    // 6. 执行重命名/移动（原子操作）
    // 从旧目录移除
    DirEntry old_entry = old_parent_dir->entries[old_entry_index];
    for (uint32_t i = old_entry_index; i < old_parent_dir->count - 1; i++) {
        old_parent_dir->entries[i] = old_parent_dir->entries[i + 1];
    }
    old_parent_dir->count--;

    // 添加到新目录
    strcpy(new_parent_dir->entries[new_parent_dir->count].name, new_name);
    new_parent_dir->entries[new_parent_dir->count].inode_no = old_entry.inode_no;
    new_parent_dir->count++;

    // 7. 如果是目录，更新其内部的".."条目（如果跨目录移动）
    if (target_inode->type == FT_DIRECTORY && strcmp(old_parent_path, new_parent_path) != 0) {
        Directory *target_dir = (Directory *)(fs->blocks + target_inode->block_ptrs[0] * BLOCK_SIZE);
        for (uint32_t i = 0; i < target_dir->count; i++) {
            if (strcmp(target_dir->entries[i].name, "..") == 0) {
                target_dir->entries[i].inode_no = new_parent_inode->inode_no;
                break;
            }
        }
    }

    return 0;
}

// 解析路径，分离出父路径和最后一级名称
void parse_path(const char *path, char *parent_path, char *name)
{
    const char *last_slash = strrchr(path, '/');
    if (!last_slash)
    {
        strcpy(parent_path, "/");
        strcpy(name, path);
        return;
    }

    if (last_slash == path)
    {
        // 根目录下的文件/目录
        strcpy(parent_path, "/");
        strcpy(name, last_slash + 1);
    }
    else
    {
        // 复制父路径
        strncpy(parent_path, path, last_slash - path);
        parent_path[last_slash - path] = '\0';
        strcpy(name, last_slash + 1);
    }
}

// 根据路径查找inode
Inode *find_inode_by_path(MemoryFS *fs, const char *path)
{
    if (strcmp(path, "/") == 0)
    {
        return &fs->inodes[0]; // 根目录
    }

    // 分割路径
    char *path_copy = strdup(path);
    char *token = strtok(path_copy, "/");
    uint32_t current_inode_no = 1; // 从根目录开始

    while (token)
    {
        Inode *current_inode = find_inode_by_no(fs, current_inode_no);
        if (!current_inode || current_inode->type != FT_DIRECTORY)
        {
            free(path_copy);
            return NULL;
        }

        Directory *dir = (Directory *)(fs->blocks + current_inode->block_ptrs[0] * BLOCK_SIZE);
        uint32_t found = 0;

        for (uint32_t i = 0; i < dir->count; i++)
        {
            if (strcmp(dir->entries[i].name, token) == 0)
            {
                current_inode_no = dir->entries[i].inode_no;
                found = 1;
                break;
            }
        }

        if (!found)
        {
            free(path_copy);
            return NULL;
        }

        token = strtok(NULL, "/");
    }

    free(path_copy);
    return find_inode_by_no(fs, current_inode_no);
}

// 根据inode号查找inode
Inode *find_inode_by_no(MemoryFS *fs, uint32_t inode_no)
{
    for (uint32_t i = 0; i < fs->inode_count; i++)
    {
        if (fs->inodes[i].inode_no == inode_no)
        {
            return &fs->inodes[i];
        }
    }
    return NULL;
}

// 查找空闲块
uint32_t find_free_block(MemoryFS *fs)
{
    for (uint32_t i = 0; i < MAX_BLOCKS; i++)
    {
        if (!(fs->bitmap[i / 8] & (1 << (i % 8))))
        {
            return i;
        }
    }
    return (uint32_t)-1;
}

// 标记块为已使用
void mark_block_used(MemoryFS *fs, uint32_t block_no)
{
    fs->bitmap[block_no / 8] |= (1 << (block_no % 8));
    fs->super.free_blocks--;
}

// 标记块为未使用
void mark_block_free(MemoryFS *fs, uint32_t block_no)
{
    fs->bitmap[block_no / 8] &= ~(1 << (block_no % 8));
    fs->super.free_blocks++;
}
