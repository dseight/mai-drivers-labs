#include <stdio.h>
#include <unistd.h>
#include "workqueue.h"

void handler(void *data)
{
	printf("%s", (char *)data);
}

int main(void)
{
	struct workqueue wq1, wq2;

	wq_init(&wq1);
	wq_init(&wq2);

	wq_add(&wq1, handler, "string 1 in WQ1\n");
	wq_add(&wq1, handler, "string 2 in WQ1\n");
	wq_add(&wq1, handler, "string 3 in WQ1\n");
	wq_add(&wq1, handler, "string 4 in WQ1\n");

	wq_add(&wq2, handler, "string 1 in WQ2\n");
	wq_add(&wq2, handler, "string 2 in WQ2\n");

	sleep(1);

	return 0;
}
