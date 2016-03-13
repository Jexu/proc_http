#include "_socketexecutor.h"


socket_exector::socket_exector(char* ip_addr, int listen_port, int max_conns)
{
	if(listen_port <= 0 || max_conns <= 0)
	{
		throw std::exception();
	}
	this->ip_addr = ip_addr;
	this->listen_port = listen_port;
	this->max_conns = max_conns;
	this->th_poll = NULL;
	this->http_handers = NULL;
	client_sockaddr_len = sizeof(client_addr);
	nready = 0;
	is_run = true;
}

socket_exector::~socket_exector()
{
}

bool socket_exector::init(int threads_num, int max_request)
{
	this->th_poll = new thread_poll<http_hander>(threads_num, max_request);
	this->http_handers = new http_hander[this->max_conns];

	this->listenfd = socket(AF_INET, SOCK_STREAM, 0);
	assert(this->listenfd > 0);
	bzero(&sock_addr, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(listen_port);
	if(ip_addr)
	{
	inet_pton(AF_INET, ip_addr, &sock_addr.sin_addr);
	}
	else
	{
		sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}

	bind(listenfd, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
	listen(listenfd, 10);

	this->epollfd = epoll_create(100);
	assert(this->epollfd);
	http_hander::epollfd = this->epollfd;
	struct epoll_event lsfd_event;
	lsfd_event.events = EPOLLIN;
	lsfd_event.data.fd = listenfd;
	epoll_ctl(this->epollfd, EPOLL_CTL_ADD, listenfd, &lsfd_event);
	return true;
}

void socket_exector::run()
{
	while(is_run)
	{
		nready = epoll_wait(this->epollfd, this->q_events, 
				socket_exector::MAX_EVENTS_NUMBER, -1);
		if(nready <=0 )
		{
			continue;
		}
		for(int i=0; i<nready; i++)
		{
			socketfd = q_events[i].data.fd;
			if(socketfd == this->listenfd)
			{
				int connfd = accept(listenfd, (struct sockaddr*)&client_addr, 
						&client_sockaddr_len);
				if(connfd < 0)
				{
					continue;
				}
				if(http_hander::user_counter >= this->max_conns)
				{
					printf("%s\n", "server is to busy");
					continue;
				}
				this->http_handers[connfd].init(connfd, client_addr);
			}
			else if(q_events[i].events & EPOLLIN)
			{
				http_hander::READ_INTO_BUF_STATUS rs = 
					http_handers[socketfd].read();
				if(rs == http_hander::READ_SUCCESS)
				{
					this->th_poll->append_request(&this->http_handers[socketfd]);
				}
				else 
				{
					this->http_handers[socketfd].close_conn();
				}
			}
			else if(q_events[i].events & EPOLLOUT)
			{
				if(!this->http_handers[i].write())
				{
					this->http_handers[i].close_conn();
				}
			}
			else if(q_events[i].events & (EPOLLRDHUP | EPOLLHUP | 
						EPOLLERR))
			{
				this->http_handers[i].close_conn();
			}
		}
	}

	close(listenfd);
	close(epollfd);
}

void socket_exector::stop()
{
	if(is_run)
	{
		is_run = false;
	}
}

void socket_exector::destory()
{
	if(this->http_handers)
	{
		delete[] this->http_handers;
	}
	if(!this->th_poll)
	{
		delete this->th_poll;
	}
}



