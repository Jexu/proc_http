#ifndef PTHREAD_POLL_H
#define PTHREAD_POLL_H
#include <list>
#include <exception>
#include <pthread.h>

/*It is suggested that type of 'request' in the work_queue extend this abstract class*/
class _hander
{
	public:
		virtual void* process()=0;
};

template<typename T>
/*This class is used to create a thread pool which thread number and max request
 *able to handled are specified by arg th_number and max_requests respectively. 
 *It has only one public function append_request(),applied to add requests to a queue.
 *th_number threads will handle those requests in queue concurrently*/
class thread_poll
{
	public:
		/*in this constructor, default value of th_number and max_requests are 5 and 10000*/
		thread_poll(int th_number = 5, int max_requests = 10000);
		/*arg request represents a specific task class with necessary function named process() */
		bool append_request(T *request);
		~thread_poll();
	private:
		/*callback function of the thread*/
		static void* worker(void *arg);
		/*monitor change of queue and handle request event in it*/
		void run();
	private:
		/*number of threads in the pool*/
		int th_number;
		/*max length of queue*/
		int max_requests;
		/*queue used to place request and type of which specified by T*/
		std::list<T*> work_queue;
		/*an array of threads*/
		pthread_t *threads;
		pthread_mutex_t mutex;
		pthread_cond_t cond;
		/* run or not of function run() determined by this flag*/
		bool is_run;
};

	template<typename T>
thread_poll<T>::thread_poll(int th_number, int max_requests)
{
	if(th_number <=0 || max_requests <=0)
	{
		throw std::exception();
	}

	this->th_number = th_number;
	this->max_requests = max_requests;
	this->is_run = true;
	this->threads = NULL;


	if(pthread_mutex_init(&mutex,NULL) != 0)
	{
		throw std::exception();
	}

	if(pthread_cond_init(&cond, NULL) != 0)
	{
		throw std::exception();
	}

	threads = new pthread_t[max_requests];
	if(!threads)
	{
		throw std::exception();
	}

	for(int i = 0; i < th_number; i++)
	{
		if(pthread_create(threads + i, NULL, worker, this) != 0)
		{
			delete[] threads;
			throw std::exception();
		}

		if(pthread_detach(threads[i]) != 0)
		{
			delete[] threads;
			throw std::exception();
		}
	}
}

	template<typename T>
thread_poll<T>::~thread_poll()
{
	if(threads)
	{
		delete[] threads;
		is_run = false;
	}
}

	template<typename T>
bool thread_poll<T>::append_request(T* request)
{
	if(!request)
	{
		throw std::exception();
	}

	pthread_mutex_lock(&mutex);
	if(work_queue.size() >= max_requests)
	{
		pthread_mutex_unlock(&mutex);
		return false;
	}
	work_queue.push_back(request);
	pthread_mutex_unlock(&mutex);


	if(work_queue.size() > 0 && work_queue.size() <= th_number)
	{
		//pthread_cond_broadcast(&cond);
		pthread_cond_signal(&cond);
	}
	return true;
}

	template<typename T>
void* thread_poll<T>::worker(void* arg)
{
	thread_poll* pool = (thread_poll* )arg;
	pool->run();
	return pool;
}


	template<typename T>
void thread_poll<T>::run()
{
	while(is_run)
	{
		if(work_queue.size() <=0)	
		{
			pthread_mutex_lock(&mutex);
			pthread_cond_wait(&cond,&mutex);
			pthread_mutex_unlock(&mutex);
			continue;
		}
		pthread_mutex_lock(&mutex);
		T *request = work_queue.front();
		work_queue.pop_front();
		pthread_mutex_unlock(&mutex);
		if(!request)
		{
			continue;
		}
		request->process();
	}
}
#endif
