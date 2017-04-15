#ifndef _WORKQUEUE_H_
#define _WORKQUEUE_H_

#include <pthread.h>
#include <semaphore.h>

struct task {
	void (*handler)(void *cookie);
	void *cookie;
	struct task *next;
};

struct workqueue {
	struct task *task;
	struct task **new;
	pthread_t thread;
	sem_t sem;
};

int wq_init(struct workqueue *wq);
int wq_add(struct workqueue *wq, void (*handler)(void *), void *cookie);
int wq_cancel(struct workqueue *wq);

#endif
