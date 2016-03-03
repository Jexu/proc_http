#ifndef HTTP_HANDER_H
#define HTTP_HANDER_H
#include <iostream>
#include <vector>
#include <string>
#include <exception>

#include <string>
#include <vector>
#include <sys/socket.h>
#include "_threadpool.h"
#include "_epollutil.h"

/*This class is used to combine hander with some process functions. When the
  *event listened happens, new instatnce of this class will be created and added
  *to the task queue, waitting to be handled by the idle thread. As definition
  *in the class theadpool, scheduler thread will call process() function to complete
  *its task.*/
class http_hander : public _hander
{
	public:
		/*constructor*/
		http_hander();
		virtual ~http_hander();
	public:
		/*using this function to init the instance of this class. Arg 
		  *socketfd is the value that accept() returned.*/
		void init(int socketfd,const struct sockaddr_in& addr);
		/*close the tcp connection*/
		void close(bool is_close = true);
		/*This is the function defined in the abstract class _hander and 
		  *it is the entrypint function of HTTP request*/
		virtual void* process();
		/*read the whole text of HTTP request message into the read buffer*/
		bool read();
		/*write response datas into the write buffer*/
		bool write();
	public:
		/*all socketfd will be registered to the same epoll kernel event 
		  *table. so epollfd is static*/
		static int epollfd;
		/*count amount of users*/
		static int user_counter;
	public:
		/*record the status that which part of HTTP request message is 
		  *being parsed*/
		enum CHECK_STATUS {CHECK_STATUS_REQUESTLINE, CHECK_STATUS_HEADER,
			CHECK_STATUS_CONTENT};
		/*result code of parsing*/
		enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE,
			FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR,
			CLOSED_CONNECTION};
		/*methods of HTTP request*/
		enum METHODS {GET, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT,
			PATCH};
	private:
		/*init some inner variables*/
		void init();
		/*entrypoint function of parsing HTTP request message*/
		HTTP_CODE process_request();
		/*following three functions will be called by process_request() to parse 
		  *request line, request header and request content respectively.*/
		HTTP_CODE parse_request_line(char* text);
		HTTP_CODE parse_request_header(char* text);
		HTTP_CODE parse_request_content(char* text);

		/*entrypoint function of sending response to client.*/
		bool process_response();


		/*function _split is applied to split string and result will be stored 
		  *into arg splits*/
		void _split(const std::string *src,const char* sp, 
				std::vector<std::string>* splits)const;
		void _split(const char *src,const char* sp,
				std::vector<std::string>* splits)const;
		void test();
	private:
		/*save the socketfd accept() returned*/
		int socketfd;
		/*save addr of client*/
		const struct sockaddr_in* addr;
		/*it is a util*/
		epoll_util eputil;
};
#endif
