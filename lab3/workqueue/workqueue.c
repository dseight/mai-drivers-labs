#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "workqueue.h"

void *worker_thread(void *cookie)
{
	struct task *cur_work;
	struct workqueue *wq = cookie;

	while (1) {
		sem_wait(&wq->sem);

		cur_work = wq->task;
		cur_work->handler(cur_work->cookie);

		wq->task = cur_work->next;
		free(cur_work);
	}

	return NULL;
}

int wq_init(struct workqueue *wq)
{
	int ret;

	wq->new = &wq->task;

	ret = sem_init(&wq->sem, 0, 0);
	if (ret)
		return ret;

	ret = pthread_create(&wq->thread, NULL, worker_thread, wq);
	if (ret) {
		sem_destroy(&wq->sem);
		return ret;
	}

	return 0;
}

int wq_add(struct workqueue *wq, void (*handler)(void *), void *cookie)
{
	struct task *task;

	task = malloc(sizeof *task);
	if (task == NULL)
		return -1;

	task->handler = handler;
	task->cookie = cookie;
	task->next = NULL;

	*(wq->new) = task;
	wq->new = &task->next;

	sem_post(&wq->sem);

	return 0;
}

int wq_cancel(struct workqueue *wq)
{
	return pthread_cancel(wq->thread);
}
