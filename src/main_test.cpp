#include<iostream>
#include "_threadpool.h"

using namespace std;

int main()
{
	//_hander *h1 = new _hander();
	thread_poll<int> *th_p = new thread_poll<int>(4,100);
	for(int i = 0; i < 10; i++)
	{
		th_p->append_request(&i);
	}

	while(1)
		;
	return 0;
}
