#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GB (1024UL * 1024 * 1024)  // 1GB
#define MAX_ATTEMPTS 131072        // 128TB上限 (128000 * 1GB)

int main() {
    void* pointers[MAX_ATTEMPTS] = {NULL};
    size_t total_allocated = 0;
    int count = 0;

    printf("开始测试最大虚拟内存分配...\n");
    printf("每次分配1GB，最大尝试次数: %d (%.1f TB)\n", 
           MAX_ATTEMPTS, MAX_ATTEMPTS/1024.0);

    for (count = 0; count < MAX_ATTEMPTS; count++) {
        pointers[count] = malloc(GB);
        
        if (pointers[count] == NULL) {
            perror("❌ malloc失败");
            break;
        }

        // 可选：实际写入数据（测试物理内存限制）
        //memset(pointers[count], 0, GB); 
        
        total_allocated += GB;
        printf("✅ 已分配: %4d GB | 累计: %.1f TB\n", 
               count+1, total_allocated/(1024.0*GB));
    }

    // 计算结果
    const size_t gb_allocated = total_allocated / GB;
    printf("\n测试结束！\n");
    printf("=================================\n");
    printf("最大虚拟内存分配量: %zu GB (%.2f TB)\n", 
           gb_allocated, gb_allocated/1024.0);
    printf("=================================\n");

    // 释放所有内存
    for (int i = 0; i < count; i++) {
        if (pointers[i]) free(pointers[i]);
    }
    
    return 0;
}
