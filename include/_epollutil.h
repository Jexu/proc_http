#ifndef EPOLL_UTIL_H
#define EPOLL_UTIL_H
#include <exception>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

/*this class function as a util that contains several public functions used to change 
  *attributes of file descriptor*/
class epoll_util
{
	public:
		/*constructor*/
		epoll_util();
		virtual ~epoll_util();
		/*set nonblocking attribute of file descriptor*/
		int set_nonblocking(int fd);
		/*add socketfd into epoll kernel table that epollfd associated*/
		void add_fd(int epollfd, int socketfd,bool is_oneshot = true);
		/*remove socketfd from epoll kernel table that epollfd associated*/
		void remove_fd(int epollfd, int socketfd);
		/*modify attribute of socketfd and re-register it into epoll kernel table again*/
		void modify_fd(int epollfd, int socketfd, int ev);
	private:
		/*check that whether fd is smaller than 0. if it dose, then throws an exception*/
		void check_fd(int fd);
};
#endif
