#ifndef SOCKETEXECTOR_H
#define SOCKETEXECTOR_H
#include "_threadpool.h"
#include "_httphander.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <assert.h>

class socket_exector
{
	public:
		socket_exector(char* ip_addr, int listen_port, 
				int max_conns = 50000);
		virtual ~socket_exector();
	public:
		bool init(int threads_num = 5, int max_request = 1000);
		void run();
		void stop();
		void destory();
	public:
		static const int MAX_EVENTS_NUMBER = 1000;
	private:
		char* ip_addr;
		int listen_port;
		int max_conns;
	private:
		thread_poll<http_hander>* th_poll;
		http_hander* http_handers;
		struct epoll_event q_events[MAX_EVENTS_NUMBER];
		struct sockaddr_in sock_addr;
		int listenfd;
		int epollfd;
		bool is_run; 
	private:
		int socketfd;
		struct sockaddr_in client_addr;
		socklen_t client_sockaddr_len;
		int nready;
};
#endif
