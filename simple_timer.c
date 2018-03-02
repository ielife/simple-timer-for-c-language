#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "simple_timer.h"

#define MAXFDS 128

static int SetNonBlock (int fd)
{
    int flags = fcntl (fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if (-1 == fcntl (fd, F_SETFL, flags)) {
		printf("%s,%d errors:SetNonBlock fcntl \n",__FUNCTION__,__LINE__);
        return -1;
    }
    return 0;
}

static void* timer_run(void* arg)
{
	char buf[128];

	timer_server_handle_t *handle = (timer_server_handle_t *)arg;
	if (NULL == handle) return NULL;

	if (0 != pthread_mutex_init(&handle->mutex_, NULL)) {
		printf("%s,%d errors: pthread_mutex_init error \n",__FUNCTION__,__LINE__);
		return NULL;
	}

    for (; handle->active_ ; ) {
        struct epoll_event events[MAXFDS] ;
        int nfds = epoll_wait (handle->epoll_fd_, events, MAXFDS, -1);
        for (int i = 0; i < nfds; ++i)
        {
    		ctimer *timer = NULL;
            ctimer_callback cb = NULL;
			pthread_mutex_lock(&handle->mutex_);
            HASH_FIND_INT(handle->timer_node_, &events[i].data.fd, timer);
            while (read(events[i].data.fd, buf, 128) > 0);
            if (timer) {
                cb = timer->timer_cb_;
            }
			pthread_mutex_unlock(&handle->mutex_);

			if (0 == handle->active_)
				break;

			if (cb && timer->count_ != 0) {
				cb(timer);
				if (timer->count_ > 0)
			        timer->count_--;
			}
        }
    }
	pthread_mutex_lock(&handle->mutex_);
    close(handle->epoll_fd_);
    handle->epoll_fd_ = -1;
    for(int i = 0; i <= handle->fd_max_; ++i) {
		ctimer *timer = NULL;
		HASH_FIND_INT(handle->timer_node_, &i, timer);
		if(timer != NULL) {
			HASH_DEL(handle->timer_node_, timer);
			free(timer);
		}
	}
	pthread_mutex_unlock(&handle->mutex_);
	pthread_mutex_destroy(&handle->mutex_);
	free(handle);
    handle = NULL;

	printf("%s,%d Standard simple_timer thread exit  \n",__FUNCTION__,__LINE__);
    return NULL ;
}

timer_server_handle_t * timer_server_init(int max_num /* 最大定时器个数 */)
{
	timer_server_handle_t *handle = (timer_server_handle_t *)malloc(sizeof(timer_server_handle_t));
	if (NULL == handle) {
		fprintf(stderr, "timer_server create failed\n");
		return NULL;
	}

	handle->init_success_ = 1;
	handle->active_ = 1;
	handle->fd_max_ = -1;

    handle->epoll_fd_ = epoll_create(max_num);
    if (-1 == handle->epoll_fd_) {
		fprintf(stderr, "epoll_create failed\n");
		goto timer_epoll_create_failed;
    }

    pthread_create(&handle->tid_, NULL, timer_run, (void *)handle);

    return handle;

timer_epoll_create_failed:
	free(handle);
	handle = NULL;
timer_handle_failed:

	return NULL;
}

void timer_server_uninit(timer_server_handle_t *timer_handle)
{
	timer_handle->active_ = 0;
	pthread_join(timer_handle->tid_, NULL);
}

int timer_server_addtimer(timer_server_handle_t *timer_handle, ctimer *t)
{
	int nRet;

	ctimer *timer = (ctimer *)malloc(sizeof(ctimer));
	if (NULL == timer) {
		fprintf(stderr, "malloc ctimer failed\n");
		return -1;
	}

	timer->count_ = t->count_;
	timer->timer_internal_ = t->timer_internal_;
	timer->timer_cb_ = t->timer_cb_;
	timer->user_data_ = t->user_data_;
	timer->fd = t->fd = timerfd_create(CLOCK_REALTIME, 0);
    if (-1 == timer->fd) {
		printf("%s,%d errors: timer_create error m_nTimerfd=%d \n",__FUNCTION__,__LINE__,timer->fd); 
		return -1;
	}
    SetNonBlock(timer->fd);
    if (timer_handle->fd_max_ < timer->fd) timer_handle->fd_max_ = timer->fd;

	printf("%s,%d Information: add timerfd=%d \n",__FUNCTION__,__LINE__, timer->fd);

    struct epoll_event ev;
    ev.data.fd = timer->fd;
    ev.events = EPOLLIN | EPOLLET;

	pthread_mutex_lock(&timer_handle->mutex_);
	struct itimerspec ptime_internal ;
    ptime_internal.it_value.tv_sec = (int)timer->timer_internal_;
    ptime_internal.it_value.tv_nsec = (timer->timer_internal_ - (int) timer->timer_internal_)*1000000000;
    if(0 != timer->count_) {
        ptime_internal.it_interval.tv_sec = ptime_internal.it_value.tv_sec;
        ptime_internal.it_interval.tv_nsec = ptime_internal.it_value.tv_nsec;
		nRet = timerfd_settime(timer->fd, 0, &ptime_internal, NULL);
    	if (-1 == nRet) {
			fprintf(stderr, "%s,%d  errors timerfd_settime\n",__FUNCTION__,__LINE__);
    	}
    }

	if (-1 == epoll_ctl(timer_handle->epoll_fd_, EPOLL_CTL_ADD, timer->fd, &ev)) {
		printf("%s,%d errors epoll_ctl add_timer \n",__FUNCTION__,__LINE__);
		nRet = -1;
	}
	else {
		int fd = timer->fd;
		HASH_ADD_INT(timer_handle->timer_node_, fd, timer);
	}
	pthread_mutex_unlock(&timer_handle->mutex_);

	return nRet;
}

void timer_server_deltimer(timer_server_handle_t *timer_handle, int timer_fd)
{
	printf("%s,%d Information: start del timerfd=%d \n",__FUNCTION__,__LINE__, timer_fd);

	ctimer *timer = NULL;
	
	pthread_mutex_lock(&timer_handle->mutex_);
	HASH_FIND_INT(timer_handle->timer_node_, &timer_fd, timer);
	if (timer) {
		printf("delete fd:%d\n", timer_fd);
		struct epoll_event ev;
		ev.data.fd = timer_fd;
		ev.events = EPOLLIN | EPOLLET;
		epoll_ctl(timer_handle->epoll_fd_, EPOLL_CTL_DEL, timer_fd, &ev);
		close(timer_fd);
		HASH_DEL(timer_handle->timer_node_,timer);
		free(timer);
	}
	pthread_mutex_unlock(&timer_handle->mutex_);
}
