#include "_httphander.h"

int http_hander::epollfd = -1;

int http_hander::user_counter = 0;


http_hander::http_hander()
{
}

http_hander::~http_hander()
{
}

void http_hander::init(int socketfd, const struct sockaddr_in& addr)
{
	this->socketfd = socketfd;
	this->addr = &addr;
	this->eputil = *(new epoll_util());
	this->eputil.add_fd(http_hander::epollfd,this->socketfd, true);
	this->user_counter++;
	this->init();
}

void* http_hander::process()
{
	std::cout<<"http_hander.process()"<<std::endl;
	test();
	return NULL;
}

http_hander::READ_INTO_BUF_STATUS http_hander::read()
{
	if(idel_r_buffer_idx >= READ_BUFFER_SIZE)
	{
		return BUF_FULL;
	}
	int read_bytes = 0;
	for(;;)
	{
		read_bytes = recv(this->socketfd, read_buffer + idel_r_buffer_idx, 
				READ_BUFFER_SIZE - idel_r_buffer_idx, 0);
		if(read_bytes == -1)
		{
			if(errno == EWOULDBLOCK || errno == EAGAIN)
			{
				break;
			}
			return FAIL_OTHER;
		}
		else if(read_bytes == 0)
		{
			return CLIENT_CLOSED;
		}
	}
	return READ_SUCCESS;
}

bool http_hander::write()
{
	return true;
}

void http_hander::_split(const std::string *src,const char* sp,
		std::vector<std::string>* splits)const
{
	if(!sp)
	{
		throw std::exception();
	}

	std::string src_temp = *src;
	unsigned int pos_cur = 0;
	for(;;)
	{
		if((signed int)(pos_cur = src_temp.find(sp,0)) != -1)
		{
			splits->push_back(src_temp.substr(0,pos_cur));
			src_temp.assign(src_temp,pos_cur + 1,src_temp.length() - pos_cur);
		}
		else
		{
			if(src_temp.length() > 0)
			{
				splits->push_back(src_temp);
			}
			break;
		}
	}
}

void http_hander::_split(const char *src,const char* sp,
		std::vector<std::string>* splits)const
{
	if(!src)
	{
		throw std::exception();
	}
	std::string _src(src);
	_split(&_src,sp,splits);
}


/*private function()*/

void http_hander::init()
{
	this->idel_r_buffer_idx = 0;
	this->cur_start_line_idx = 0;
	this->checked_line_idx = 0;
	this->checked_status = CHECK_STATUS_REQUESTLINE;
	request_method = GET;
	request_url = NULL;
	http_version = NULL;
	map_request_headers.clear();
	memset(read_buffer,'\0',READ_BUFFER_SIZE);
}

http_hander::HTTP_CODE http_hander::process_request()
{
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret_code = NO_REQUEST;
	char* text = NULL;
	while(((checked_status == CHECK_STATUS_CONTENT) && (line_status == LINE_OK)) 
			|| ((line_status = parse_line()) == LINE_OK))
	{
		/*因为没一行的\r\n都已经被\0填上了，所以可以得到一行*/
		text = get_line();
		cur_start_line_idx = checked_line_idx;

		switch(checked_status)
		{
			case CHECK_STATUS_REQUESTLINE:
			{
				ret_code = parse_request_line(text);
				if(ret_code == BAD_REQUEST)
				{
					return BAD_REQUEST;
				}
				break;
			}
			case CHECK_STATUS_HEADER:
			{
				ret_code = parse_request_header(text);
				if(ret_code == BAD_REQUEST)
				{
					return BAD_REQUEST;
				}
				else if(ret_code == GET_REQUEST)
				{
					return do_request();
				}
				break;
			}
			case CHECK_STATUS_CONTENT:
			{
				ret_code = parse_request_content(text);
				if(ret_code == GET_REQUEST)
				{
					return do_request();
				}
				line_status = LINE_OPEN;
				break;
			}
			default:
			{
				return INTERNAL_ERROR;
				break;
			}
		}
	}
	return NO_REQUEST;
}


http_hander::HTTP_CODE http_hander::parse_request_line(char* text)
{
	this->request_url = strpbrk(text, "\t");
	if(!this->request_url)
	{
		return BAD_REQUEST;
	}
	*this->request_url++ = '\0';
	char* method = text;
	if(!parse_request_method(method))
	{
		return BAD_REQUEST;
	}

	this->request_url += strspn(request_url, "\t");

	this->http_version = strpbrk(this->request_url, "\t");
	if(!this->http_version)
	{
		return BAD_REQUEST;
	}
	*this->http_version++ = '\0';
	this->http_version += strspn(this->http_version, "\t");
	if(strcasecmp(this->http_version, "HTTP/1.1") != 0)
	{
		return BAD_REQUEST;
	}
	
	if(strncasecmp(this->request_url, "http://", 7) == 0)
	{
		this->request_url = strchr(this->request_url, '/');
	}
	if(!this->request_url || this->request_url[0] != '/')
	{
		return BAD_REQUEST;
	}

	checked_status = CHECK_STATUS_HEADER;

	return NO_REQUEST;
}


bool http_hander::parse_request_method(char* method)
{
	if(strcasecmp(method, "GET") == 0)
	{
		this->request_method = GET;
	}
	else if(strcasecmp(method, "POST") == 0)
	{
		this->request_method = POST;
	}
	else if(strcasecmp(method, "HEAD") == 0)
	{
		this->request_method = HEAD;
	}
	else if(strcasecmp(method, "PUT") == 0)
	{
		this->request_method = PUT;
	}
	else if(strcasecmp(method, "DELETE") == 0)
	{
		this->request_method = DELETE;
	}
	else if(strcasecmp(method, "TRACE") == 0)
	{
		this->request_method = TRACE;
	}
	else if(strcasecmp(method, "OPTIONS") == 0)
	{
		this->request_method = OPTIONS;
	}
	else if(strcasecmp(method, "CONNECT") == 0)
	{
		this->request_method = CONNECT;
	}
	else if(strcasecmp(method, "PATCH") == 0)
	{
		this->request_method = PATCH;
	}
	else 
	{
		return false;
	}
	return true;
}

http_hander::HTTP_CODE http_hander::parse_request_header(char* text)
{
	if(text[0] == '\0')
	{
		//finishing parsing request headers and coming cross to blank line
		if(atoi(map_request_headers[CONTENT_LENGTH]) != 0)
		{
			//http request message contains content
			checked_status = CHECK_STATUS_CONTENT;
			return NO_REQUEST;
		}
	}
	else if(strncasecmp(text, "accept:", 7) == 0)
	{
		text += 7;
		text += strspn(text, "\t");
		map_request_headers[ACCEPT] = text;
	}
	else if(strncasecmp(text, "accept-charset:", 15) == 0)
	{
		text += 15;
		text += strspn(text, "\t");
		map_request_headers[ACCEPT_CHARSET] = text;
	}
	else if(strncasecmp(text, "accept-encoding:", 16) == 0)
	{
		text += 16;
		text += strspn(text, "\t");
		map_request_headers[ACCEPT_ENCODING] = text;
	}
	else if(strncasecmp(text, "accept-language:", 16) == 0)
	{
		text += 16;
		text += strspn(text, "\t");
		map_request_headers[ACCEPT_LANGUAGE] = text;
	}
	else if(strncasecmp(text, "accept-range:", 13) == 0)
	{
		text += 13;
		text += strspn(text, "\t");
		map_request_headers[ACCEPT_RANGE] = text;
	}
	else if(strncasecmp(text, "authorization:", 15) == 0)
	{
		text += 15;
		text += strspn(text, "\t");
		map_request_headers[AUTHORIZTION] = text;
	}
	else if(strncasecmp(text, "cache-control:", 14) == 0)
	{
		text += 14;
		text += strspn(text, "\t");
		map_request_headers[CACHE_CONTROL] = text;
	}
	else if(strncasecmp(text, "connection:", 11) == 0)
	{
		text += 11;
		text += strspn(text, "\t");
		map_request_headers[CONNECTION] = text;
	}
	else if(strncasecmp(text, "cookie:", 7) == 0)
	{
		text += 7;
		text += strspn(text, "\t");
		map_request_headers[COOKIE] = text;
	}
	else if(strncasecmp(text, "content-length:", 16) == 0)
	{
		text += 16;
		text += strspn(text, "\t");
		map_request_headers[CONTENT_LENGTH] = text;
	}
	else if(strncasecmp(text, "content-type:", 13) == 0)
	{
		text += 13;
		text += strspn(text, "\t");
		map_request_headers[CONTENT_TYPE] = text;
	}
	else if(strncasecmp(text, "date:", 5) == 0)
	{
		text += 5;
		text += strspn(text, "\t");
		map_request_headers[DATE] = text;
	}
	else if(strncasecmp(text, "expect:", 7) == 0)
	{
		text += 7;
		text += strspn(text, "\t");
		map_request_headers[EXPECT] = text;
	}
	else if(strncasecmp(text, "from:", 5) == 0)
	{
		text += 5;
		text += strspn(text, "\t");
		map_request_headers[FROM] = text;
	}
	else if(strncasecmp(text, "host:", 5) == 0)
	{
		text +=	5; 
		text += strspn(text, "\t");
		map_request_headers[HOST] = text;
	}
	else if(strncasecmp(text, "if-match:", 9) == 0)
	{
		text += 9; 
		text += strspn(text, "\t");
		map_request_headers[IF_MATCH] = text;
	}
	else if(strncasecmp(text, "if-modified-since:", 18) == 0)
	{
		text += 18;
		text += strspn(text, "\t");
		map_request_headers[IF_MODIFIED_SINCE] = text;
	}
	else if(strncasecmp(text, "if-none-match:", 14) == 0)
	{
		text += 14; 
		text += strspn(text, "\t");
		map_request_headers[IF_NONE_MATCH] = text;
	}
	else if(strncasecmp(text, "if-range:", 9) == 0)
	{
		text += 9; 
		text += strspn(text, "\t");
		map_request_headers[IF_RANGE] = text;
	}
	else if(strncasecmp(text, "if-unmodified-since:", 20) == 0)
	{
		text += 20;
		text += strspn(text, "\t");
		map_request_headers[IF_UNMODIFIED_SINCE] = text;
	}
	else if(strncasecmp(text, "max-forwards:", 13) == 0)
	{
		text += 13;
		text += strspn(text, "\t");
		map_request_headers[MAX_FORWARDS] = text;
	}
	else if(strncasecmp(text, "pragma:", 7) == 0)
	{
		text += 7;
		text += strspn(text, "\t");
		map_request_headers[PRAGMA] = text;
	}
	return NO_REQUEST;
}

http_hander::HTTP_CODE http_hander::parse_request_content(char* text)
{
	return FORBIDDEN_REQUEST;
}

http_hander::LINE_STATUS http_hander::parse_line()
{
	char temp;
	for(;checked_line_idx < idel_r_buffer_idx; ++checked_line_idx)
	{
		temp = read_buffer[checked_line_idx];
		if(temp == '\r')
		{
			if(read_buffer[checked_line_idx + 1] == '\n')
			{
				read_buffer[checked_line_idx++] = '\0';
				read_buffer[checked_line_idx++] = '\0';
				return LINE_OK;
			}
			else if(checked_line_idx + 1 == idel_r_buffer_idx)
			{
				return LINE_OPEN;
			}
			return LINE_BAD;
		}
		else if(temp == '\n')
		{
			if(checked_line_idx > 1 && read_buffer[checked_line_idx -1] == '\r')
			{
				read_buffer[checked_line_idx -1] = '\0';
				read_buffer[checked_line_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}

char* http_hander::get_line()
{
	return read_buffer + cur_start_line_idx;
}

http_hander::HTTP_CODE http_hander::do_request()
{
	return NO_REQUEST;
}

bool http_hander::process_response()
{
	return true;
}











void http_hander::test()
{
	std::vector<std::string> splits;
	std::string src("version:111:go");
	_split(&src,(char*)":",&splits);
	for(unsigned int i = 0; i < splits.size(); i++)
	{
		std::cout<<splits[i]<<"  "<<std::endl;
	}
	splits.clear();
	std::string _src = "bal_ka_hg_";
	_split(&_src,(char*)"_",&splits);
	for(unsigned int i = 0; i < splits.size(); i++)
	{
		std::cout<<splits[i]<<"  "<<std::endl;
	}

}
