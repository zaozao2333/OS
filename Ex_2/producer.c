
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>

#include "shm_com_sem.h"

int main(void) {
	void* shared_memory = (void*)0;//�����ڴ�(������ָ��)
	struct shared_mem_st* shared_stuff;
	//�������͹���洢��ת��Ϊshared_mem_st���͵�ָ��
	char key_line[256];

	int shmid;//�����ڴ��id

	sem_t* sem_queue, * sem_queue_empty, * sem_queue_full;
	//���ʹ����ڴ�Ļ��������ջ����������������ź�������Ϊ�ź���ָ��

	/*
	 * TO DO: ��ȡ�����ڴ������������ڴ�
	 */

	shmid = shmget((key_t)1234, sizeof(struct shared_mem_st), 0666 | IPC_CREAT);
	// �ҽӹ����ڴ浽���̵�ַ�ռ�
	shared_memory = shmat(shmid, NULL, 0);



	shared_stuff = (struct shared_mem_st*)shared_memory;
	//��������ָ��ת��Ϊshared_mem_st����

	/*
	 * TO DO: ���洴�������ź���
	 */
	 // ���������ź�������ʼֵΪ1��
	sem_queue = sem_open(queue_mutex, O_CREAT, 0666, 1);
	// �ջ������ź�������ʼΪNUM_LINE��
	sem_queue_empty = sem_open(queue_empty, O_CREAT, 0666, NUM_LINE);
	// ���������ź�������ʼΪ0��
	sem_queue_full = sem_open(queue_full, O_CREAT, 0666, 0);





	//��дָ���ʼ������ʼ��ָ���0��
	shared_stuff->line_write = 0;
	shared_stuff->line_read = 0;

	//���ϴӿ���̨���밴��������ַ���
	while (1)
	{
		//��ʾ�������룬����gets()���밴���е�key_line��
		printf("Enter your text('quit' for exit): ");
		fgets(key_line, sizeof(key_line), stdin);

		//�������"quit"���˳�
		if (strcmp(key_line, "quit\n") == 0) {
			// �ȴ��ղ�λ
			sem_wait(sem_queue_empty);
			// ��ȡ������
			sem_wait(sem_queue);
			// д�뻺����
			strncpy(shared_stuff->buffer[shared_stuff->line_write], key_line, LINE_SIZE);
			shared_stuff->line_write = (shared_stuff->line_write + 1) % NUM_LINE;
			// �ͷŻ�����
			sem_post(sem_queue);
			sem_post(sem_queue_full);
			break;
		}

		/*
		* TO DO: ���������д�뻺������Ҫ���ź�������
		*/
		// �ȴ��ղ�λ
		sem_wait(sem_queue_empty);
		// ��ȡ������
		sem_wait(sem_queue);

		// д�뻺����
		strncpy(shared_stuff->buffer[shared_stuff->line_write], key_line, LINE_SIZE);
		shared_stuff->line_write = (shared_stuff->line_write + 1) % NUM_LINE;

		// �ͷŻ�����
		sem_post(sem_queue);
		// ��������λ����
		sem_post(sem_queue_full);

	}




	//�����"quit"��ǰ��while()ѭ�����������˴��������Ƴ�ǰ���ͷ��ź���

	/*
	 * TO DO: �ͷ��ź��������������ڴ��ڱ����̵Ĺ���ӳ��ɾ�������ڴ�����
	 */
	 // �ر��ź���
	sem_close(sem_queue);
	sem_close(sem_queue_empty);
	sem_close(sem_queue_full);
	// ��Producer�˳�ǰ���
	sem_unlink(queue_mutex);
	sem_unlink(queue_empty);
	sem_unlink(queue_full);

	// ���빲���ڴ�
	shmdt(shared_memory);


}