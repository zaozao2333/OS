#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "shm_com_sem.h"

int main(void) {
    void* shared_memory = (void*)0;//共享内存(缓冲区指针)
    struct shared_mem_st* shared_stuff;
    //将无类型共享存储区转换为shared_mem_st类型的指针


    int shmid;//共享内存的id
    int num_read;
    pid_t fork_result;

    sem_t* sem_queue, * sem_queue_empty, * sem_queue_full;
    //访问共享内存的互斥量、空缓冲区、满缓冲区信号量，皆为信号量指针

    /*
     * TO DO: 获取共享内存区，并挂入内存
     */

     // 获取已存在的共享内存
    shmid = shmget((key_t)1234, sizeof(struct shared_mem_st), 0666);
    // 挂接共享内存
    shared_memory = shmat(shmid, NULL, 0);




    shared_stuff = (struct shared_mem_st*)shared_memory;
    //将缓冲区指针转换为shared_mem_st类型

    /*
     * TO DO: 获取producer创建的三个信号量，根据名字"queue_mutex""queue_empty""queue_full"来识别
     */

     // 打开已存在的信号量
    sem_queue = sem_open(queue_mutex, 0);
    sem_queue_empty = sem_open(queue_empty, 0);
    sem_queue_full = sem_open(queue_full, 0);




    //创建了两个进程
    fork_result = fork();
    if (fork_result == -1) {
        fprintf(stderr, "Fork failure\n");
    }
    if (fork_result == 0) {//子进程
        while (1) {
            /*
             * TO DO:信号量操作，打印消费内容及进程号，发现quit推出
             */
            sem_wait(sem_queue_full);  // 等待有数据
            sem_wait(sem_queue);       // 获取互斥锁

            // 读取数据
            char* current_line = shared_stuff->buffer[shared_stuff->line_read];
            printf("[PID:%d]Son Consumed: %s", getpid(), current_line);

            // 检测退出条件
            if (strcmp(current_line, "quit\n") == 0) {
                sem_post(sem_queue);
                sem_post(sem_queue_empty); // 增加空槽位
                sem_post(sem_queue_full); // 确保父进程能继续执行
                break;
            }

            // 更新读取指针
            shared_stuff->line_read = (shared_stuff->line_read + 1) % NUM_LINE;

            sem_post(sem_queue);       // 释放互斥锁
            sem_post(sem_queue_empty); // 增加空槽位

        }

    }
    else {
        while (1) {//父进程，操作与子进程相似
            /*
             * TO DO:信号量操作，打印消费内容及进程号，发现quit推出
             */
            sem_wait(sem_queue_full);  // 等待有数据
            sem_wait(sem_queue);       // 获取互斥锁

            // 读取数据
            char* current_line = shared_stuff->buffer[shared_stuff->line_read];
            printf("[PID:%d]Father Consumed: %s", getpid(), current_line);

            // 检测退出条件
            if (strcmp(current_line, "quit\n") == 0) {
                sem_post(sem_queue);
                sem_post(sem_queue_empty); // 增加空槽位
                sem_post(sem_queue_full); // 确保子进程能继续执行
                break;
            }

            // 更新读取指针
            shared_stuff->line_read = (shared_stuff->line_read + 1) % NUM_LINE;

            sem_post(sem_queue);       // 释放互斥锁
            sem_post(sem_queue_empty); // 增加空槽位
        }


    }

}