#include "request.h"
#include "server_thread.h"
#include "common.h"

struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;
	/* add any other parameters you need */
	// lab5
	struct hashTable* table;
	struct LRU_list* list;

};

// for lab5
// hashtable for cache 
struct hashTable {
	int size;
	int key;
	struct file_data* file;
};

struct LRU_list{

	struct LRU_list *next;
	char *file_name;
};


// hash func from lab1
//cse.yorku.ca/~oz/html, we use 5381 as it says it works fine with it
// returns hashvalue
unsigned long hashValue (char* str){
	//1000th prime value;
	unsigned long hash = 5381;

	while (*(str)!= '\0'){
		hash = ((hash<<5) + hash) + *str;
		str++;
	}
	return hash;
}






// insert new fetched file in cache
	// first check if cache size would reach limit if insert, if yes, evict
int cache_insert(struct server* sv, struct file_data *new_file){
	// check file size > max cache size
	if (new_file->file_size > sv->max_cache_size){
		return -1;
	}
	else{
		// if enough cache space for new file, insert
		if (new_file->file_size < sv->table->size){
			// TODO: 
			//1.add to table 
			
			
			//2. add to list
		}
		// if not enough space for new file, evict, then insert again
		else{
			cache_evict(sv,1);
			// TODO: 
			//1.add to table 
			
			
			//2. add to list
			return -2;
		} 

	}

}

// delete file in cache
int cache_evict(struct server*sv, int amount_to_evict){
	// if invalid input, i.e. nothing on LRU_list or amount<0
	if ( sv->list==NULL || amount_to_evict <=0){
		return -1;
	}

	// if input valid, retrive from list, delete in table
	else{

	}


}

// lookup if file is in cache
	// if in cache, update list
int cache_lookup(struct server *sv, char *file_name){

	hashvalue = hashValue(file_name);
	index = hashvalue%sv->table->size;

	// if not in cache
	if(sv->table[index]==NULL){
		return 0;
	}
	// if file already in cache, move it to end of list  **note: end is the most recently used
	else{


	}

}






/* static functions */



void* consumer(void *sv);

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

void destroyNodes(struct wait_queue  *q){
	//printf("in destror node start0.......\n");
	node *temp = q->head;
	if(temp == NULL){
		return;
	}
	node *sub = temp;
	while(temp != NULL){
		sub = temp;
		temp = temp->next;

/*
		#ifdef DEBUG_USE_VALGRIND
			VALGRIND_STACK_DEREGISTER((sub->thr->stack_ptr +THREAD_MIN_STACK) - (THREAD_MIN_STACK)%16 -8 );
		#endif
*/
		free(sub);
	}
	//printf("finished destroynode\n");
	q->head = NULL;
	q->back = NULL;
	return;
}




int pollFromQueue(struct wait_queue *q){
	
	if(q->head == NULL){
		return -2147483648;
	}
	else{
		q->size = q->size - 1;

		node *temp = q->head;
		node *nextTemp = q->head->next;

		q->head = nextTemp;
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
pthread_cond_t empty;
pthread_mutex_t mutex;

pthread_t * arrayOfPThreads;



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
		}
		if(nr_threads> 0){
			arrayOfPThreads = Malloc(nr_threads*sizeof(pthread_t));
			for(int i = 0; i < nr_threads; i++){
				 pthread_create(&arrayOfPThreads[i], NULL, consumer, sv);
			}
		}
}



	/* Lab 4: create queue of max_request size when max_requests > 0 */

	/* Lab 5: init server cache and limit its size to max_cache_size */

	/* Lab 4: create worker threads when nr_threads > 0 */

	return sv;
}


void* consumer(void *sv){
	struct server * server  = (struct server *) sv;
	while(server->exiting == 0){
		pthread_mutex_lock(&mutex);
		while(q->size == 0){
			pthread_cond_wait(&empty, &mutex);
			if(server->exiting == 1){
				break;
			}
		}
		//crit.region
		if(server->exiting == 1){
			pthread_mutex_unlock(&mutex);
			return NULL;
		}
		int tfd = pollFromQueue(q);
		pthread_cond_signal(&full);
		pthread_mutex_unlock(&mutex);
		do_server_request(sv,tfd);
	}
	return NULL;
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
	pthread_mutex_lock(&mutex);
	sv->exiting = 1;
	pthread_cond_broadcast(&empty);
	pthread_mutex_unlock(&mutex);

	//now the join part where i wait for all the threads
	for (int i = 0;i < sv->nr_threads; i++){
		pthread_join(arrayOfPThreads[i],NULL);
	}
	
	
	
	free(arrayOfPThreads);
	destroyNodes(q);
	free(q);
	/* make sure to free any allocated resources */
	free(sv);
}
