#ifndef HTTP_HANDER_H
#define HTTP_HANDER_H
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <exception>

#include <errno.h>
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
		enum READ_INTO_BUF_STATUS {BUF_FULL, CLIENT_CLOSED, FAIL_OTHER, 
			READ_SUCCESS};
		enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN};

		enum REQUEST_HEADERS {ACCEPT, ACCEPT_CHARSET, ACCEPT_ENCODING, 
			ACCEPT_LANGUAGE, ACCEPT_RANGE, AUTHORIZTION, CACHE_CONTROL, 
			CONNECTION, COOKIE, CONTENT_LENGTH, CONTENT_TYPE, DATE, 
			EXPECT, FROM, HOST, IF_MATCH, IF_MODIFIED_SINCE, IF_NONE_MATCH, 
			IF_RANGE, IF_UNMODIFIED_SINCE, MAX_FORWARDS, PRAGMA, 
			PROXY_AUTHORIZATION, RANGE, TE, UPGRADE, USER_AGENT, VIA, WARING};

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
		READ_INTO_BUF_STATUS read();
		/*write response datas into the write buffer*/
		bool write();
	public:
		/*all socketfd will be registered to the same epoll kernel event 
		 *table. so epollfd is static*/
		static int epollfd;
		/*count amount of users*/
		static int user_counter;
		static const int READ_BUFFER_SIZE = 2048;
	private:
		/*init some inner variables*/
		void init();
		/*entrypoint function of parsing HTTP request message*/
		HTTP_CODE process_request();
		/*following four functions will be called by process_request() to parse 
		 *request line, request header, request content and get line from read buffer
		 *respectively.*/
		HTTP_CODE parse_request_line(char* text);
		bool parse_request_method(char* method);
		HTTP_CODE parse_request_header(char* text);
		HTTP_CODE parse_request_content(char* text);
		LINE_STATUS parse_line();
		char* get_line();
		HTTP_CODE do_request();

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
	private:
		int idel_r_buffer_idx;
		int cur_start_line_idx;
		int checked_line_idx;
		CHECK_STATUS checked_status;

		METHODS request_method;
		char* request_url;
		char* http_version;

		std::map<REQUEST_HEADERS, char*> map_request_headers;
		
		char read_buffer[READ_BUFFER_SIZE];
};
#endif
