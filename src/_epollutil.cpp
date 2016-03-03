#include "_epollutil.h"


epoll_util::epoll_util()
{
}

epoll_util::~epoll_util()
{
}

int epoll_util::set_nonblocking(int fd)
{
	int old_flag = fcntl(fd,F_GETFL);
	int new_flag = old_flag | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_flag);
	return old_flag;
}

void epoll_util::add_fd(int epollfd, int socketfd, bool is_oneshot)
{
	check_fd(epollfd);
	check_fd(socketfd);
	epoll_event event;
	event.data.fd = socketfd;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	if(is_oneshot)
	{
		event.events |= EPOLLONESHOT;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, socketfd, &event);
	set_nonblocking(socketfd);
}

void epoll_util::remove_fd(int epollfd, int socketfd)
{
	check_fd(epollfd);
	check_fd(socketfd);
	epoll_ctl(epollfd, EPOLL_CTL_DEL, socketfd, NULL);
}

void epoll_util::modify_fd(int epollfd, int socketfd, int ev)
{
	check_fd(epollfd);
	check_fd(socketfd);
	epoll_event event;
	event.data.fd = socketfd;
	event.events = ev | EPOLLIN | EPOLLET | EPOLLRDHUP;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, socketfd, &event);
}

void epoll_util::check_fd(int fd)
{
	if(fd < 0)
	{
		throw std::exception();
	}
}
