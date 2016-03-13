#include "_socketexecutor.h"
using namespace std;

int main()
{

	socket_exector *se = new socket_exector(NULL,50000);
	se->init();
	se->run();
	return 0;
}
