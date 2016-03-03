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

bool http_hander::read()
{
	return true;
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
}

http_hander::HTTP_CODE http_hander::process_request()
{
	return FORBIDDEN_REQUEST;
}


http_hander::HTTP_CODE http_hander::parse_request_line(char* text)
{
	return FORBIDDEN_REQUEST;
}

http_hander::HTTP_CODE http_hander::parse_request_header(char* text)
{
	return FORBIDDEN_REQUEST;
}

http_hander::HTTP_CODE http_hander::parse_request_content(char* text)
{
	return FORBIDDEN_REQUEST;
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
