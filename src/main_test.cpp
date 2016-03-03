#include<iostream>
#include "_threadpool.h"
#include "_httphander.h"

using namespace std;

int main()
{
	//_hander *h1 = new _hander();
	thread_poll<http_hander> *th_p = new thread_poll<http_hander>(4,100);
	for(int i = 0; i < 10; i++)
	{
		th_p->append_request(new http_hander());
		std::cout<<i<<std::endl;
	}


	while(1)
		;
	return 0;
}
