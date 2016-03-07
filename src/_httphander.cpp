#include "_httphander.h"

const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You dont hava permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The request file was not found in this server.\n";
const char* error_500_title = "Internet Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";



int http_hander::epollfd = -1;

int http_hander::user_counter = 0;

const char* http_hander::ROOT_DOC_DIR = "/var/www/html";


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

/*private function()*/

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

void http_hander::init()
{
	this->idel_r_buffer_idx = 0;
	this->idel_w_buffer_idx = 0;
	this->cur_start_line_idx = 0;
	this->checked_line_idx = 0;
	this->checked_status = CHECK_STATUS_REQUESTLINE;
	this->request_method = GET;
	this->request_url = NULL;
	this->http_version = NULL;
	this->request_file_mmap_addr = NULL;
	this->map_request_headers.clear();
	this->map_request_params.clear();
	memset(read_buffer,'\0',READ_BUFFER_SIZE);
	memset(write_buffer,'\0',WRITE_BUFFER_SIZE);
	memset(real_file_dir,'\0',MAX_FILENAME_LEN);
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
	strcpy(real_file_dir, ROOT_DOC_DIR);
	int len = strlen(real_file_dir);
	strncpy(real_file_dir + len, this->request_url, MAX_FILENAME_LEN - len -1);
	if(stat(real_file_dir, &request_file_stat) < 0)
	{
		return NO_RESOURCE;
	}
	if(!(request_file_stat.st_mode & S_IROTH))
	{
		return FORBIDDEN_REQUEST;
	}
	if(request_file_stat.st_mode & S_IFDIR)
	{
		return BAD_REQUEST;
	}
	int fd = open(real_file_dir, O_RDONLY);
	request_file_mmap_addr = (char*)mmap(0, request_file_stat.st_size, 
			PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	return FILE_REQUEST;
}

bool http_hander::process_response(HTTP_CODE resp_code)
{
	switch(resp_code)
	{
		case INTERNAL_ERROR:
			{
				add_response_line(500, error_500_title);
				add_response_header(strlen(error_500_title));
				if(!add_response_content(error_500_title))
				{
					return false;
				}
				break;
			}
		case BAD_REQUEST:
			{
				add_response_line(400, error_400_title);
				add_response_header(strlen(error_400_form));
				if(!add_response_content(error_400_form))
				{
					return false;
				}
				break;
			}
		case NO_RESOURCE:
			{
				add_response_line(404, error_404_title);
				add_response_header(strlen(error_404_form));
				if(!add_response_content(error_404_form))
				{
					return false;
				}
				break;
			}
		case FORBIDDEN_REQUEST:
			{
				add_response_line(403, error_403_title);
				add_response_header(strlen(error_403_form));
				if(!add_response_content(error_403_form))
				{
					return false;
				}
				break;
			}
		case FILE_REQUEST:
			{
				add_response_line(200, ok_200_title);
				if(request_file_stat.st_size > 0)
				{
					iov[0].iov_base = write_buffer;
					iov[0].iov_len = idel_w_buffer_idx;
					iov[1].iov_base = request_file_mmap_addr;
					iov[0].iov_len = request_file_stat.st_size; 
					iovcnt = 2;
					return true;
				}
				else
				{
					const char* ok_char = "<html><body>hello world</body></html>";
					add_response_header(strlen(ok_char));
					if(!add_response_content(ok_char))
					{
						return false;
					}
				}
				break;
			}
		default :
			{
				return false;
				break;
			}
	}
	iov[0].iov_base = write_buffer;
	iov[0].iov_len = idel_w_buffer_idx;
	iovcnt = 1;
	return true;
}

bool http_hander::add_response_line(int resp_code, const char* title)
{
	return add_response("%s %d %s\r\n", this->http_version, resp_code, title);
}

bool http_hander::add_response_header(int content_len)
{
	if(map_request_headers[CONTENT_TYPE])
	{
		if(!add_response("Content-Type:%s\r\n", map_request_headers[CONTENT_TYPE]))
		{
			return false;
		}
	}
	if(map_request_headers[CONNECTION])
	{
		if(!add_response("Connection:%s\r\n", map_request_headers[CONTENT_TYPE]))
		{
			return false;
		}
	}
	if(!add_response("Content-Length:%d\r\n", content_len))
	{
		return false;
	}
	return true;
}

bool http_hander::add_response_blank_line()
{
	return add_response("%s", "\r\n");
}

bool http_hander::add_response_content(const char* content)
{
	return add_response("%s", content);
}
		
bool http_hander::add_response(const char* format, ...)
{
	if(idel_w_buffer_idx >= WRITE_BUFFER_SIZE)
	{
		return false;
	}
	va_list args_list;
	va_start(args_list, format);
	int len = vsnprintf(write_buffer + idel_w_buffer_idx, WRITE_BUFFER_SIZE - idel_w_buffer_idx - 1, 
			format, args_list);
	if(len >= (WRITE_BUFFER_SIZE - idel_w_buffer_idx - 1))
	{
		return false;
	}
	idel_w_buffer_idx += len;
	va_end(args_list);
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
