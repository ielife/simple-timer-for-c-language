#ifndef __SIMPLE_TIMER_H
#define __SIMPLE_TIMER_H

#include <pthread.h>
#include "uthash.h"

typedef int(*ctimer_callback)(struct _ctimer *);
typedef struct _ctimer{
	int                 fd;
    double              timer_internal_;
    void*               user_data_;
	int 				count_;
    ctimer_callback     timer_cb_;
    UT_hash_handle 		hh;
}ctimer;

typedef struct
{
	int 				init_success_;
	int          		active_;
	pthread_t			tid_;
	int             	epoll_fd_;
	int 				fd_max_;
	ctimer*		 		timer_node_;
	pthread_mutex_t 	mutex_;
}timer_server_handle_t;

timer_server_handle_t * timer_server_init(int max_num /* 最大定时器个数 */);
int timer_server_addtimer(timer_server_handle_t *timer_handle, ctimer *timer);
void timer_server_deltimer(timer_server_handle_t *timer_handle, int timer_fd);
void timer_server_uninit(timer_server_handle_t *timer_handle);

#endif //__SIMPLE_TIMER_H
