
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>

#include "shm_com_sem.h"

int main(void) {
	void* shared_memory = (void*)0;//共享内存(缓冲区指针)
	struct shared_mem_st* shared_stuff;
	//将无类型共享存储区转换为shared_mem_st类型的指针
	char key_line[256];

	int shmid;//共享内存的id

	sem_t* sem_queue, * sem_queue_empty, * sem_queue_full;
	//访问共享内存的互斥量、空缓冲区、满缓冲区信号量，皆为信号量指针

	/*
	 * TO DO: 获取共享内存区，并挂入内存
	 */

	shmid = shmget((key_t)1234, sizeof(struct shared_mem_st), 0666 | IPC_CREAT);
	// 挂接共享内存到进程地址空间
	shared_memory = shmat(shmid, NULL, 0);



	shared_stuff = (struct shared_mem_st*)shared_memory;
	//将缓冲区指针转换为shared_mem_st类型

	/*
	 * TO DO: 下面创建三个信号量
	 */
	 // 创建互斥信号量（初始值为1）
	sem_queue = sem_open(queue_mutex, O_CREAT, 0666, 1);
	// 空缓冲区信号量（初始为NUM_LINE）
	sem_queue_empty = sem_open(queue_empty, O_CREAT, 0666, NUM_LINE);
	// 满缓冲区信号量（初始为0）
	sem_queue_full = sem_open(queue_full, O_CREAT, 0666, 0);





	//读写指针初始化，开始都指向第0行
	shared_stuff->line_write = 0;
	shared_stuff->line_read = 0;

	//不断从控制台读入按键输入的字符行
	while (1)
	{
		//提示可以输入，并用gets()读入按键行到key_line中
		printf("Enter your text('quit' for exit): ");
		fgets(key_line, sizeof(key_line), stdin);

		//如果键入"quit"则退出
		if (strcmp(key_line, "quit\n") == 0) {
			// 等待空槽位
			sem_wait(sem_queue_empty);
			// 获取互斥锁
			sem_wait(sem_queue);
			// 写入缓冲区
			strncpy(shared_stuff->buffer[shared_stuff->line_write], key_line, LINE_SIZE);
			shared_stuff->line_write = (shared_stuff->line_write + 1) % NUM_LINE;
			// 释放互斥锁
			sem_post(sem_queue);
			sem_post(sem_queue_full);
			break;
		}

		/*
		* TO DO: 将输入的行写入缓冲区，要有信号量操作
		*/
		// 等待空槽位
		sem_wait(sem_queue_empty);
		// 获取互斥锁
		sem_wait(sem_queue);

		// 写入缓冲区
		strncpy(shared_stuff->buffer[shared_stuff->line_write], key_line, LINE_SIZE);
		shared_stuff->line_write = (shared_stuff->line_write + 1) % NUM_LINE;

		// 释放互斥锁
		sem_post(sem_queue);
		// 增加满槽位计数
		sem_post(sem_queue_full);

	}




	//因键入"quit"从前面while()循环中跳出到此处，程序推出前，释放信号量

	/*
	 * TO DO: 释放信号量，结束共享内存在本进程的挂载映像，删除共享内存区域
	 */
	 // 关闭信号量
	sem_close(sem_queue);
	sem_close(sem_queue_empty);
	sem_close(sem_queue_full);
	// 在Producer退出前添加
	sem_unlink(queue_mutex);
	sem_unlink(queue_empty);
	sem_unlink(queue_full);

	// 分离共享内存
	shmdt(shared_memory);


}