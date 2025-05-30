#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <malloc.h>

#define THREAD_NUM 16
#define MB (1024 * 1024)

int *array;
int length;
int count = 0;  // 全局计数，初始化为0
pthread_mutex_t m;

void *count3s_thread(void *id) {
    int my_id = *(int *)id;
    int length_per_thread = length / THREAD_NUM;
    int start = my_id * length_per_thread;
    int end = start + length_per_thread;
    int local_count = 0;
    
    // 先进行本地计数
    for (int i = start; i < end; i++) {
        if (array[i] == 3) {
            local_count++;
        }
    }
    
    // 然后一次性更新全局计数
    pthread_mutex_lock(&m);
    count += local_count;
    pthread_mutex_unlock(&m);
    
    return NULL;
}

int main() {
    pthread_mutex_init(&m, NULL);
    pthread_t threads[THREAD_NUM];
    int thread_ids[THREAD_NUM];
    
    length = 64 * MB;
    array = malloc(length * sizeof(int));
    
    // 初始化数组
    for (int i = 0; i < length; i++) {
        array[i] = (i % 2) ? 4 : 3;
    }
    
    // 创建线程
    for (int t = 0; t < THREAD_NUM; t++) {
        thread_ids[t] = t;
        if (pthread_create(&threads[t], NULL, count3s_thread, &thread_ids[t])) {
            printf("create thread error!\n");
            return 1;
        }
    }
    
    // 等待所有线程
    for (int t = 0; t < THREAD_NUM; t++) {
        pthread_join(threads[t], NULL);
    }
    
    printf("Total count= %d\n", count);
    pthread_mutex_destroy(&m);
    free(array);
    return 0;
}
