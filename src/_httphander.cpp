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
	map_request_params.clear();
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

	char* parems = NULL;
	if((parems = strpbrk(this->request_url, "?")) != NULL)
	{
		*parems++ = '\0';
		parse_request_content(parems);
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
	else
	{
		switch(method[0])
		{
			case 'H':
				{
					if(strcasecmp(method, "HEAD") == 0)
					{
						this->request_method = HEAD;
					}
					break;
				}
			case 'P':
				{
					if(strcasecmp(method, "PUT") == 0)
					{
						this->request_method = PUT;
					}
					else if(strcasecmp(method, "PATCH") == 0)
					{
						this->request_method = PATCH;
					}
					break;
				}
			case 'D':
				{
					if(strcasecmp(method, "DELETE") == 0)
					{
						this->request_method = DELETE;
					}
					break;
				}
			case 'T':
				{
					if(strcasecmp(method, "TRACE") == 0)
					{
						this->request_method = TRACE;
					}
					break;
				}
			case 'O':
				{
					if(strcasecmp(method, "OPTIONS") == 0)
					{
						this->request_method = OPTIONS;
					}
					break;
				}
			case 'C':
				{
					if(strcasecmp(method, "CONNECT") == 0)
					{
						this->request_method = CONNECT;
					}
					break;
				}
			default:
				{
					return false;
					break;
				}
		}
	}
	return true;
}

http_hander::HTTP_CODE http_hander::parse_request_header(char* text)
{
	switch(text[0])
	{
		case '\0':
			{
				//have finished parsing request headers and coming cross the blank line
				if(atoi(map_request_headers[CONTENT_LENGTH]) != 0)
				{
					//http request message contains content
					checked_status = CHECK_STATUS_CONTENT;
					return NO_REQUEST;
				}
				break;
			}
		case 'A':
			{

				if(strncasecmp(text, "accept:", 7) == 0)
				{
					text += 7;
					text += strspn(text, "\t");
					map_request_headers[ACCEPT] = text;
				}
				else
				{
					switch(text[7])
					{
						case 'C':
							{
								if(strncasecmp(text, "accept-charset:", 15) == 0)
								{
									text += 15;
									text += strspn(text, "\t");
									map_request_headers[ACCEPT_CHARSET] = text;
								}
								break;

							}
						case 'L':
							{	
								if(strncasecmp(text, "accept-language:", 16) == 0)
								{
									text += 16;
									text += strspn(text, "\t");
									map_request_headers[ACCEPT_LANGUAGE] = text;
								}

								break;
							}
						case 'E':
							{	
								if(strncasecmp(text, "accept-encoding:", 16) == 0)
								{
									text += 16;
									text += strspn(text, "\t");
									map_request_headers[ACCEPT_ENCODING] = text;
								}

								break;
							}
						case 'R':
							{
								if(strncasecmp(text, "accept-range:", 13) == 0)
								{
									text += 13;
									text += strspn(text, "\t");
									map_request_headers[ACCEPT_RANGE] = text;
								}
								break;
							}
						default:
							{	
								if(strncasecmp(text, "authorization:", 15) == 0)
								{
									text += 15;
									text += strspn(text, "\t");
									map_request_headers[AUTHORIZTION] = text;
								}
								break;
							}
					}
				}

				break;
			}
		case 'C':
			{
				switch(text[3])
				{
					case 'n':
						{
							if(strncasecmp(text, "connection:", 11) == 0)
							{
								text += 11;
								text += strspn(text, "\t");
								map_request_headers[CONNECTION] = text;
							}
							break;
						}
					case 'k':
						{
							if(strncasecmp(text, "cookie:", 7) == 0)
							{
								text += 7;
								text += strspn(text, "\t");
								map_request_headers[COOKIE] = text;
							}
							break;
						}
					case 'h':
						{
							if(strncasecmp(text, "cache-control:", 14) == 0)
							{
								text += 14;
								text += strspn(text, "\t");
								map_request_headers[CACHE_CONTROL] = text;
							}
							break;
						}
					default:
						{
							if(strncasecmp(text, "content-length:", 16) == 0)
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
							break;
						}

				}

				break;
			}
		case 'U':
			{

				if(strncasecmp(text, "user-agent:", 11) == 0)
				{
					text += 11;
					text += strspn(text, "\t");
					map_request_headers[USER_AGENT] = text;
				}
				else if(strncasecmp(text, "upgrade:", 8) == 0)
				{
					text += 8; 
					text += strspn(text, "\t");
					map_request_headers[UPGRADE] = text;
				}


				break;
			}
		case 'H':
			{

				if(strncasecmp(text, "host:", 5) == 0)
				{
					text +=	5; 
					text += strspn(text, "\t");
					map_request_headers[HOST] = text;
				}
				break;
			}
		case 'R':
			{
				if(strncasecmp(text, "range:", 6) == 0)
				{
					text += 6; 
					text += strspn(text, "\t");
					map_request_headers[RANGE] = text;
				}
				else if(strncasecmp(text, "referer:", 8) == 0)
				{
					text += 8;
					text += strspn(text, "\t");
					map_request_headers[REFERER] = text;
				}

				break;
			}
		case 'I':
			{
				switch(text[3])
				{
					case 'M':
						{
							if(strncasecmp(text, "if-match:", 9) == 0)
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
							break;
						}
					case 'N':
						{
							if(strncasecmp(text, "if-none-match:", 14) == 0)
							{
								text += 14; 
								text += strspn(text, "\t");
								map_request_headers[IF_NONE_MATCH] = text;
							}
							break;
						}
					case 'R':
						{
							if(strncasecmp(text, "if-range:", 9) == 0)
							{
								text += 9; 
								text += strspn(text, "\t");
								map_request_headers[IF_RANGE] = text;
							}
							break;
						}
					default:
						{
							if(strncasecmp(text, "if-unmodified-since:", 20) == 0)
							{
								text += 20;
								text += strspn(text, "\t");
								map_request_headers[IF_UNMODIFIED_SINCE] = text;
							}
							break;
						}
				}


				break;
			}
		default:
			{
				switch(text[1])
				{
					case 'a':
						{
							if(strncasecmp(text, "date:", 5) == 0)
							{
								text += 5;
								text += strspn(text, "\t");
								map_request_headers[DATE] = text;
							}
							else if(strncasecmp(text, "max-forwards:", 13) == 0)
							{
								text += 13;
								text += strspn(text, "\t");
								map_request_headers[MAX_FORWARDS] = text;
							}
							else if(strncasecmp(text, "warning:", 8) == 0)
							{
								text += 8;
								text += strspn(text, "\t");
								map_request_headers[WARING] = text;
							}
							break;
						}
					case 'x':
						{
							if(strncasecmp(text, "expect:", 7) == 0)
							{
								text += 7;
								text += strspn(text, "\t");
								map_request_headers[EXPECT] = text;
							}
							break;
						}
					case 'r':
						{
							if(strncasecmp(text, "from:", 5) == 0)
							{
								text += 5;
								text += strspn(text, "\t");
								map_request_headers[FROM] = text;
							}
							else if(strncasecmp(text, "pragma:", 7) == 0)
							{
								text += 7;
								text += strspn(text, "\t");
								map_request_headers[PRAGMA] = text;
							}
							else if(strncasecmp(text, "proxy-authorization:", 21) == 0)
							{
								text +=	21; 
								text += strspn(text, "\t");
								map_request_headers[PROXY_AUTHORIZATION] = text;
							}
							break;
						}
					default:
						{
							if(strncasecmp(text, "te:", 3) == 0)
							{
								text += 3; 
								text += strspn(text, "\t");
								map_request_headers[TE] = text;
							}
							else if(strncasecmp(text, "via:", 4) == 0)
							{
								text += 4; 
								text += strspn(text, "\t");
								map_request_headers[VIA] = text;
							}
						}
						break;
				}
				break;
			}
	}

	return NO_REQUEST;
}

http_hander::HTTP_CODE http_hander::parse_request_content(char* text)
{
	if(!text)
	{
		return BAD_REQUEST;
	}

	char* key;
	char* value;
	char* pr_and;
	key = text;
	while(((value = strpbrk(key, "=")) != NULL) )
	{
		*value++ = '\0';
		map_request_params[key] = value;
		if(((pr_and = strpbrk(key, "&")) != NULL))
		{
			*pr_and++ = '\0';
			key = pr_and;
		}
	}
	return NO_REQUEST;
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
