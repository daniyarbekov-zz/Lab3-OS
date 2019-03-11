#include "request.h"
#include "server_thread.h"
#include "common.h"

struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;
	/* add any other parameters you need */
};

/* static functions */


typedef struct node {
	int fd;
	struct node *next;
} node;

struct wait_queue{
	node *head;
	node *back;
	int size;
};

node *buildNode(int connfd)  {
    node *temp = (node*)malloc(sizeof(node));
	temp->fd = connfd;
    temp->next = NULL;

	return temp;
}

void enQ (int connfd, struct wait_queue *q){
	//printf("calling build node\n");
	node *addedNode = buildNode(connfd);
	q->size = q->size + 1;

	if(q->head == NULL){
		q->head = addedNode;
		q->back = addedNode;
		return;
	}
	else if(q->head == q->back){
		q->back = addedNode;
		q->head->next = q->back;
		return;
	} else{
		q->back->next = addedNode;
		q->back = q->back->next;
		return;
	}
}




struct int poll (struct wait_queue *q){
	
	if(q->head == NULL){
		return NULL;
	}
	else{
		q->size = q->size - 1;
		node *temp = q->head;
		node *nextTemp = q->head->next;

		r->head = nextTemp;
		int connfd = temp->fd;
		free(temp);

		if(q->head == NULL){
			q->back = NULL;
		}

		return connfd;
	}
}



/* initialize file data */
static struct file_data *
file_data_init(void)
{
	struct file_data *data;

	data = Malloc(sizeof(struct file_data));
	data->file_name = NULL;
	data->file_buf = NULL;
	data->file_size = 0;
	return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data)
{
	free(data->file_name);
	free(data->file_buf);
	free(data);
}

static void
do_server_request(struct server *sv, int connfd)
{
	int ret;
	struct request *rq;
	struct file_data *data;

	data = file_data_init();

	/* fill data->file_name with name of the file being requested */
	rq = request_init(connfd, data);
	if (!rq) {
		file_data_free(data);
		return;
	}
	/* read file, 
	 * fills data->file_buf with the file contents,
	 * data->file_size with file size. */
	ret = request_readfile(rq);
	if (ret == 0) { /* couldn't read file */
		goto out;
	}
	/* send file to client */
	request_sendfile(rq);
out:
	request_destroy(rq);
	file_data_free(data);
}

/* entry point functions */
struct wait_queue *q;


pthread_cond_t full;
pthread_condt_t empty;
pthread_mutex_t mutex;

pthread_t ** arrayOfPThreads;



struct server * server_init(int nr_threads, int max_requests, int max_cache_size)
{
	struct server *sv;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	sv->max_requests = max_requests;
	sv->max_cache_size = max_cache_size;
	sv->exiting = 0;
	
	if (nr_threads > 0 || max_requests > 0 || max_cache_size > 0) {
		
		pthread_cond_init(&full, NULL);
		pthread_cond_init(&empty, NULL);

		pthread_mutex_init(&mutex, NULL);


		if(max_requests > 0){
			q = (struct wait_queue*)malloc(sizeof(struct wait_queue));
			q->size = 0;
			numberOfRequests = max_requests;
		}
		if(nr_threads> 0){
			arrayOfPThreads = (pthread_t **)malloc(nr_threads*sizeof(pthread_t*));
			for(int i = 0; i < nr_threads; i++){
				 pthread_create(arrayOfPThreads[i], NULL, consumer, sv);
			}
		}
}



	/* Lab 4: create queue of max_request size when max_requests > 0 */

	/* Lab 5: init server cache and limit its size to max_cache_size */

	/* Lab 4: create worker threads when nr_threads > 0 */

	return sv;
}


void consumer(struct server *sv){
	while(sv->exiting == 0){
		pthread_mutex_lock(&mutex);
		while(q->size == 0){
			pthread_cond_wait(&empty, &mutex);
		}
		//crit.region
		int tfd = poll(q);
		pthread_cond_signal(&full);
		pthread_mutex_unlock(&mutex);

		do_server_request(sv,tfd);
	}
}


void server_request(struct server *sv, int connfd)
{
	if (sv->nr_threads == 0) { /* no worker threads */
		do_server_request(sv, connfd);
	} else {
		pthread_mutex_lock(&mutex);
		while(q->size == sv->max_requests){
			pthread_cond_wait(&full, &mutex);
		}
		enQ(connfd,q);
		pthread_cond_signal(&empty);
		pthread_mutex_unlock(&mutex);
	}
}


void
server_exit(struct server *sv)
{
	/* when using one or more worker threads, use sv->exiting to indicate to
	 * these threads that the server is exiting. make sure to call
	 * pthread_join in this function so that the main server thread waits
	 * for all the worker threads to exit before exiting. */
	sv->exiting = 1;
	pthread_mutex_lock(&mutex);
	pthread_cond_broadcast(&full);
	pthread_mutex_unlock(&mutex);

	//now the join part where i wait for all the threads
	for (int i = 0;i < sv->nr_threads; i++){
		pthread_join(arrayOfPThreads[i],NULL);
	}

	free(arrayOfPThreads);
	free(full);
	free(empty);
	free(mutex);
	/* make sure to free any allocated resources */
	free(sv);
}
