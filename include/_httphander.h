#ifndef HTTP_HANDER_H
#define HTTP_HANDER_H
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <exception>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h>

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
		/*satus of read buffer*/
		enum READ_INTO_BUF_STATUS {BUF_FULL, CLIENT_CLOSED, FAIL_OTHER, 
			READ_SUCCESS};
		/*status of parsing line*/
		enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN};
		/*all of request headers*/
		enum REQUEST_HEADERS {ACCEPT, ACCEPT_CHARSET, ACCEPT_ENCODING, 
			ACCEPT_LANGUAGE, ACCEPT_RANGE, AUTHORIZTION, CACHE_CONTROL, 
			CONNECTION, COOKIE, CONTENT_LENGTH, CONTENT_TYPE, DATE, 
			EXPECT, FROM, HOST, IF_MATCH, IF_MODIFIED_SINCE, IF_NONE_MATCH, 
			IF_RANGE, IF_UNMODIFIED_SINCE, MAX_FORWARDS, PRAGMA, 
			PROXY_AUTHORIZATION, RANGE, REFERER, TE, UPGRADE, USER_AGENT, 
			VIA, WARING};

	public:
		/*constructor*/
		http_hander();
		virtual ~http_hander();
	public:
		/*using this function to init the instance of this class. Arg 
		 *socketfd is the value that accept() returned.*/
		void init(int socketfd,const struct sockaddr_in& addr);
		/*close the tcp connection*/
		void close_conn(bool is_close = true);
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
		/*size of read buffer*/
		static const int READ_BUFFER_SIZE = 2048;
		/*size of write buffer*/
		static const int WRITE_BUFFER_SIZE = 1024;
		/*max length of request file real path*/
		static const int MAX_FILENAME_LEN = 156;
		/*project root directry*/
		static const char* ROOT_DOC_DIR;
	private:
		/*init some inner variables*/
		void init();
		/*entrypoint function of parsing HTTP request message*/
		HTTP_CODE process_request();
		/*following seven functions will be called by process_request() to parse 
		 *request line, request header, request content, get line from read buffer
		 *respectively.*/
		HTTP_CODE parse_request_line(char* text);
		bool parse_request_method(char* method);
		HTTP_CODE parse_request_header(char* text);
		HTTP_CODE parse_request_content(char* text);
		LINE_STATUS parse_line();
		char* get_line();
		HTTP_CODE do_request();

		/*entrypoint function of sending response to client.*/
		bool process_response(HTTP_CODE resp_code);
		bool add_response_line(int resp_code, const char* title);
		bool add_response_header(int content_len);
		void case_a(char* text);
		void case_c(char* text);
		void case_u(char* text);
		void case_h(char* text);
		void case_r(char* text);
		void case_i(char* text);
		void case_default(char* text);
		bool add_response_blank_line();
		bool add_response_content(const char* content);
		bool add_response(const char* format, ...);

		bool unmap();

		/*function _split is applied to split string and result will be stored 
		 *into arg splits*/
		void _split(const std::string *src,const char* sp, 
				std::vector<std::string>* splits)const;
		void _split(const char *src,const char* sp,
				std::vector<std::string>* splits)const;
	private:
		/*save the socketfd accept() returned*/
		int socketfd;
		/*save addr of client*/
		const struct sockaddr_in* addr;
		/*it is a util*/
		epoll_util eputil;
	private:
		/*next position of the last byte of read buffer that have been filled
		 *with datas*/
		int idel_r_buffer_idx;
		int idel_w_buffer_idx;
		/*position that new line ready to be parsed in read buffer*/
		int cur_start_line_idx;
		/*position of the character parsed currently in read buffer*/
		int checked_line_idx;
		/*record check status that maybe CHECK_STATUS_REQUESTLINE, 
		  *CHECK_STATUS_HEADER and CHECK_STATUS_CONTENT*/
		CHECK_STATUS checked_status;

		/*record request method*/
		METHODS request_method;
		/*record url of request file*/
		char* request_url;
		/*http version in request line*/
		char* http_version;
		/*request file real directry*/
		char real_file_dir[MAX_FILENAME_LEN];
		/*stat of request file*/
		struct stat request_file_stat;
		/*address of function mmap returned*/
		char* request_file_mmap_addr;

		/*map used to store key-value of request headers*/
		std::map<REQUEST_HEADERS, char*> map_request_headers;
		/*map used to store ky-value of request parameters*/
		std::map<char*, char*> map_request_params;

		/*read buffer that request message saved*/
		char read_buffer[READ_BUFFER_SIZE];
		char write_buffer[WRITE_BUFFER_SIZE];
		struct iovec iov[2];
		int iovcnt;
};
#endif
