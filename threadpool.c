#include <stdio.h> 
#include <unistd.h> 
#include <stdlib.h> 
#include <pthread.h>
#include "threadpool.h"
#define MALLOC 1
#define COND 2
#define MUTEX 3
#define THREADS_CREATE 4
#define JOIN 5
#define UP 0
#define SHUTDOWN 1
#define DONT_ACCEPT 1
#define ACCEPT 0
#define EMPTY 0
#define ERROR 1

void error(int type) //error handling
{
	if(type == MALLOC)
	{
		perror("error: malloc\n");
		exit(ERROR);
	}
	if(type == COND)
	{
		perror("error: pthread_cond_init\n");
		exit(ERROR);
	}
	if(type == MUTEX)
	{
		perror("error: pthread_mutex_init\n");
		exit(ERROR);
	}
	if(type == THREADS_CREATE)
	{
		perror("error: pthread_create\n");
		exit(ERROR);
	}
	if(type == JOIN)
	{
		perror("error: pthread_join\n");
		exit(ERROR);
	}
}
//initialize the threadpool
threadpool* create_threadpool(int num_threads_in_pool)
{
	if(num_threads_in_pool < 1 || num_threads_in_pool > MAXT_IN_POOL)
		return NULL;
	threadpool *pool = (threadpool*)malloc(sizeof(threadpool));
	if(pool == NULL)
		error(MALLOC);
	pool->num_threads = num_threads_in_pool;
	pool->qsize = EMPTY;
	pool->threads =(pthread_t*)malloc(sizeof(pthread_t)*num_threads_in_pool);
	if(pool->threads == NULL)
		error(MALLOC);
	pool->qhead = NULL;
	pool->qtail = NULL;
	if((pthread_cond_init(&pool->q_empty, NULL)!=0))
		error(COND);
	if((pthread_cond_init(&pool->q_not_empty, NULL)!=0))
		error(COND);
	if((pthread_mutex_init(&pool->qlock , NULL)!=0))
		error(MUTEX);
	pool->shutdown = UP;
	pool->dont_accept = ACCEPT;
	int i;
	for(i = 0 ; i < num_threads_in_pool ; i++)
		if((pthread_create(&pool->threads[i], NULL, do_work , (threadpool*)pool)) != 0)
			error(THREADS_CREATE);
	
	return pool;
}
//inserts new work to the pool
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
		if(from_me->dont_accept == DONT_ACCEPT)
			return;
			
		work_t *work = (work_t*)malloc(sizeof(work_t));
		if(work == NULL)
			error(MALLOC);
		work->routine = dispatch_to_here;
		work->arg = arg;
		work->next = NULL;
		
		pthread_mutex_lock(&from_me->qlock); 		
		if(from_me->qhead == NULL)
			from_me->qhead = from_me->qtail = work;		
		else
		{
			from_me->qtail->next = work;
			from_me->qtail = work;			
		}
		from_me->qsize++;
		pthread_cond_signal(&from_me->q_not_empty); //q is not empty... can take a job to do	
		pthread_mutex_unlock(&from_me->qlock); 
}
//makes the work, executes the function
void* do_work(void* p)
{	
	threadpool *pool = (threadpool*)p;
	work_t *work;
	while(1)
	{
	    pthread_mutex_lock(&pool->qlock); 
	    
	    if(pool->qsize == EMPTY && pool->dont_accept == DONT_ACCEPT)   
	    	pthread_cond_signal(&pool->q_empty); 
	   
	    if(pool->shutdown == SHUTDOWN)
		{
		    pthread_mutex_unlock(&pool->qlock); 	
		    return NULL;
		}		
		if(pool->qsize == EMPTY) //wait if the queue is empty
			pthread_cond_wait(&pool->q_not_empty, &pool-> qlock); 	    
		
		if(pool->shutdown == SHUTDOWN)
		{
		    pthread_mutex_unlock(&pool->qlock); 
		    return NULL;	
		}			
		
		work = pool->qhead;
		if(work != NULL)
		{
			pool->qhead = pool->qhead->next;
			pool->qsize--;
		
			pthread_mutex_unlock(&pool->qlock); 	
			work->routine(work->arg);	
			free(work);
			work = NULL;
		}
		else
			pthread_mutex_unlock(&pool->qlock); 	
	}
}
//destroys the pool if was called
void destroy_threadpool(threadpool* destroyme)
{	
	destroyme->dont_accept = DONT_ACCEPT;
	pthread_mutex_lock(&destroyme->qlock); 
	if(destroyme->qsize !=EMPTY)
		pthread_cond_wait(&destroyme->q_empty, &destroyme-> qlock); 
	destroyme->shutdown = SHUTDOWN;
	pthread_cond_broadcast(&destroyme->q_not_empty); 
	pthread_mutex_unlock(&destroyme->qlock); 
	int i;
	for(i = 0 ; i < destroyme->num_threads ; i++)	
		if((pthread_join(destroyme->threads[i],NULL) != 0))
			error(JOIN);  	
			
	pthread_cond_destroy(&destroyme->q_not_empty);
	pthread_cond_destroy(&destroyme->q_empty); 
	pthread_mutex_destroy(&destroyme->qlock);   
	free(destroyme->threads);
	destroyme->threads = NULL;
	free(destroyme);
	destroyme = NULL;
}



