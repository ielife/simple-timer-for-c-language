#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "simple_timer.h"

int timer_cb1(ctimer *timer)
{
	if (timer && timer->user_data_) {
		int *user_data = timer->user_data_;
		printf("call back data:%d\n", *user_data);
	}
	return 0;
}

int timer_cb2(ctimer *timer)
{
	if (timer && timer->user_data_) {
		int *user_data = timer->user_data_;
		printf("call back data:%d\n", *user_data);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	timer_server_handle_t *timer_handle = timer_server_init(1024);
	if (NULL == timer_handle) {
		fprintf(stderr, "timer_server_init failed\n");
		return -1;
	}

	ctimer timer1;
	timer1.count_ = 3;
	timer1.timer_internal_ = 0.5;
	timer1.timer_cb_ = timer_cb1;
	int *user_data1 = (int *)malloc(sizeof(int));
	*user_data1 = 100;
	timer1.user_data_ = user_data1;
	timer_server_addtimer(timer_handle, &timer1);

	ctimer timer2;
	timer2.count_ = -1;
	timer2.timer_internal_ = 0.5;
	timer2.timer_cb_ = timer_cb2;
	int *user_data2 = (int *)malloc(sizeof(int));
	*user_data2 = 10;
	timer2.user_data_ = user_data2;
	timer_server_addtimer(timer_handle, &timer2);

	sleep(10);

	timer_server_deltimer(timer_handle, timer1.fd);
	timer_server_deltimer(timer_handle, timer2.fd);
	timer_server_uninit(timer_handle);

	getchar();

	free(user_data1);
	free(user_data2);

	return 0;
}

