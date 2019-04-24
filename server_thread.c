#include "request.h"
#include "server_thread.h"
#include "common.h"


int hashTable_size = 10000;

int update_lru_counter = 0;


struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;
	/* add any other parameters you need */
};

/* static functions */

//element within hashNode
// created so that we could reuse fncs for queues from previous labs
typedef struct hashNode_element{
	struct hashNode_element *next;
	struct file_data *data;
	char *name;
	int inuse_flag;

} hashNode_element;


//element within hashTable
typedef struct hashNode{

	struct hashNode_element *head;
	struct hashNode_element *back;
	int size;

} hashNode;


// hashTable
typedef struct hashTable{
	hashNode **hashNodes;
	hashNode *lru_list;
	//struct hashTable *next;//TODO: delete
	int cache_size_remaining;
	int total_cache_size;

} hashTable;


// check if the first element name in LRU is still the first we insert
// TODO: delete
void checkLRU(struct hashNode* lru_list, char* first_file_name){
    if (first_file_name!=NULL && lru_list->head!=NULL) {
        if (strcmp(lru_list->head->name, first_file_name) != 0) {
//            fprintf(stderr, "!!!!!!!!!!! first element in LRU got changed !!!!!!");
        }
    }
}

// print LRU head name is head is not NULL
//TODO: delete
void printLRUhead(struct hashNode* lru_list) {
    if (lru_list->head != NULL) {
//        fprintf(stderr, " !!!!!!!!!!!!!! LRU list->head->name is %s:\n", lru_list->head->name);
    } else {
//        fprintf(stderr, "!!!!!!!!!! LRU list ->head is NULL \n");
    }
}

// unflag an element in lru, indicate not in use
void unflagLRU(char* file_name, struct hashNode *lru_list){

    struct hashNode_element *temp = lru_list->head;
    while(temp->name != file_name) {
        temp = temp->next;
    }// now temp points at file with file name in LRU
//    fprintf(stderr,"in unflagged,  %s inuse_flag is %d \n",file_name,temp->inuse_flag);
    temp->inuse_flag -= 1;
//    fprintf(stderr,"unflagged %s \n",file_name);
}


static void file_data_free(struct file_data *data);
void update_LRU(char *file_name, struct hashNode *lru_list);

// delete all hashNode_element inside ONE hashNode
void destroyHashNodesElements(struct hashNode *q){

	struct hashNode_element *temp = q->head;
	if(temp == NULL){
		return;
	}
	struct hashNode_element  *sub = temp;
	while(temp != NULL){
		sub = temp;
		temp = temp->next;

		file_data_free(sub->data);
		free(sub);
	}

	q->head = NULL;
	q->back = NULL;
	return;
}

void destroyHashNodes(hashTable *hash_table){
	for (int i = 0; i < hashTable_size ; i++){
		if(hash_table->hashNodes[i] != NULL){
			destroyHashNodesElements(hash_table->hashNodes[i]);
			free(hash_table->hashNodes[i]);
		}
	}
}

void destroyLRUNodes(struct hashNode *q){
	struct hashNode_element *temp = q->head;
	if(temp == NULL){
		return;
	}
	struct hashNode_element  *sub = temp;
	while(temp != NULL){
		sub = temp;
		temp = temp->next;
		free(sub);
	}

	q->head = NULL;
	q->back = NULL;
	return;
}


hashTable *hash_table;
pthread_mutex_t mutex_hash;
char* first_file_name = NULL; //TODO: delete


////******these functions are used for lru operations*****////

// note: only added file name
hashNode_element *buildLRUNode(char *file_name)  { //, struct file_data *new_data

//    fprintf(stderr,"buildingLRUNode \n");
	hashNode_element *temp = (hashNode_element*)malloc(sizeof(hashNode_element));
	temp->name = file_name;
	temp->data = NULL;
    temp->next = NULL;
    temp->inuse_flag = 0;

    //checkLRU(hash_table->lru_list, first_file_name);
//    fprintf(stderr,"finish building LRU node \n");

	return temp;
}




// move one element to the end of LRU
// i.e. an existing file get used
// note: this fnc can get called only when the file is found in LRU
void moveToendLRU (struct hashNode_element *elementToMove, struct hashNode *q){ //char *file_name
//    fprintf(stderr," $$$$$calling moveToendLRU \n");
	struct hashNode_element *temp = elementToMove;
    struct hashNode_element *tempNext= temp->next;

    if(temp->next == NULL){
    	return;
    }

    while (tempNext != NULL) {
        temp = tempNext;
        tempNext = tempNext->next;
    }
    temp->next = elementToMove;
    elementToMove->next = NULL;
    //checkLRU(hash_table->lru_list, first_file_name);
    return;
}


//	hashNode_element *temp= q->head;
//	hashNode_element *tempNext= q->back;
//
//
//
//
//	while(temp->next->name != file_name){
//		temp = temp->next;
//	} // now temp points at "one element before" the file about to be moved
//
//
//
//
//
//	// if first element is the one
//	if (q->head->name == file_name){
//		while(tempNext->next != NULL){
//			tempNext = tempNext->next;
//		} // now tempNext points at last element in LRU
//
//		hashNode_element *new = malloc(sizeof(hashNode_element));
//		new->data = temp->data;
//		new->name = temp->name;
//		new->next = NULL;
//
//		// let last element points to the one being moved
//		tempNext->next = new;
//		q->head = temp->next; // move head to the "second" element
//		free(temp);
//	}
//
//
//	// if any element in the middle got moved
//	while(tempNext->next != NULL){
//		tempNext = tempNext->next;
//	} // now tempNext points at last element in LRU
//
//	hashNode_element *new = malloc(sizeof(hashNode_element));
//	new->data = temp->data;
//	new->name = temp->name;
//	new->next = NULL;
//
//	// let last element points to the one being moved
//	tempNext->next = new;
//	free(temp);








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




// if not found in LRU list, added to end of LRU, return -1
// if found in LRU, updated LRU list, return 1
void update_LRU(char *file_name, struct hashNode *lru_list){
//	fprintf(stderr, ".....**************UPDATE LRU, file name: %s...\n",file_name);

    //checkLRU(hash_table->lru_list, first_file_name);
    printLRUhead(hash_table->lru_list);


	update_lru_counter +=1;
//	fprintf(stderr, "########### update lru counter is %d \n", update_lru_counter);
    if (update_lru_counter==1){
        first_file_name = file_name;
    }

	//we can pass an element:
		//1. the LRU doesnt have any elements
		//2. the LRU has elements but this element is not there
		//3.
	struct hashNode_element *temp = lru_list->head;

	if (temp == NULL){

		struct hashNode_element *addedElement = buildLRUNode(file_name); // build an element with file_name (no data in the element)
//        fprintf(stderr, ".....LRU head empty, finished building 1st LRU element: %s \n", addedElement->name);

        lru_list->head = addedElement; // size +1
//		fprintf(stderr, "..... LRU head name is now %s...\n",lru_list->head->name);

        printLRUhead(hash_table->lru_list);

		return;
	}else if(strcmp(temp->name,file_name) == 0){
//		fprintf(stderr, ".....LRU head is the one, maybe move head of LRU to the end...\n");
        //temp->inuse_flag += 1;

        if(lru_list->head->next==NULL){ // if only one in LRU and looked up againn
//            fprintf(stderr, ".....only 1 in LRU, no need to move...\n");
            return;
        }

        lru_list->head = lru_list->head->next;
		moveToendLRU(temp,lru_list);
        printLRUhead(hash_table->lru_list);
        return;

	}else{
		struct hashNode_element *tempNext = temp->next;
		while(tempNext != NULL){
			if(strcmp(tempNext->name,file_name) == 0){
			    //temp->inuse_flag += 1;
//			    fprintf(stderr, ".....found %s in updating LRU LOOOOOP, moving to end ...\n", tempNext->name);
				temp->next = tempNext->next;
				moveToendLRU(tempNext,lru_list);
				return;
			}
			temp = tempNext;
			tempNext = tempNext->next;
		}
		struct hashNode_element *addedElement = buildLRUNode(file_name);
		temp->next = addedElement;
        printLRUhead(hash_table->lru_list);
        return;
	}

}


///*************** end of lru operations function******////












// lookup if file is in cache by checking hashTable
	// if in cache, return file_data* ; if not, return NULL
struct file_data* cache_lookup(struct server *sv, char *file_name){  // check = 0 means its first lookup, update LRU when check =0
//	fprintf(stderr, "++++++++++++cache lookup begin, looking for %s...\n", file_name);

    //checkLRU(hash_table->lru_list, first_file_name);

	int hash_value = hashValue(file_name);
	int hash_index = hash_value % hashTable_size;

	if (hash_table->hashNodes[hash_index] == NULL || hash_table->hashNodes[hash_index]->head == NULL){
//		fprintf(stderr, ".....cache lookup begin: nothing in the hashNodes[%d]...\n",hash_index);

        return NULL;
	} else{
//		fprintf(stderr, ".....cache lookup begin: something in the hashNodes[%d], going thru thishashNode...\n",hash_index);
		struct hashNode *p = hash_table->hashNodes[hash_index];
		struct hashNode_element *temp = p->head;
		struct hashNode_element *tempNext = p->head->next;

		while(temp != NULL){
			if(strcmp(temp->name,file_name) == 0){
//			    fprintf(stderr,"found this file in cache, no need to read from disk \n");
				return temp->data;
			}
			temp = tempNext;
			tempNext = tempNext->next;
		}
//        checkLRU(hash_table->lru_list, first_file_name);
       // printLRUhead(hash_table->lru_list);

        return NULL;

	}
}

// poll the name of the first element of in LRU
// i.e. remove the "least recently used" element

// return name of the deleted file
// if nothing in LRU, return NULL



char *pollFromLRU(struct hashNode *q){
//	fprintf(stderr, ".....polling from LRU...\n");
//	fprintf(stderr, "hash_table->lru_list->head->name is %s \n:",hash_table->lru_list->head->name);

	if(q->head == NULL){
		return NULL;
	}
	else{
		hashNode_element *temp = q->head;
		hashNode_element *nextTemp = q->head->next;
		free(temp);

		q->head = nextTemp;
//		fprintf(stderr, "temp->name is %s \n:",temp->name);

		if(q->head == NULL){
			q->back = NULL;
		}



		return temp->name;
	}
}



//char *pollFromLRU(struct hashNode *q){
//
//
//	fprintf(stderr, ".....polling from LRU:hash_table->lru_list->head->name is %s...\n",hash_table->lru_list->head->name);
////    checkLRU(hash_table->lru_list, first_file_name);
//
//
//	if(q->head == NULL){
//		return NULL;
//	}
//	else{
//		hashNode_element *temp = q->head;
//		hashNode_element *nextTemp = q->head->next;
//        hashNode_element *beforeTemp = temp;
//
//		// if head of LRU is not in use, delete lru head, return head name
//		if(temp->inuse_flag != 1){
//            q->head = nextTemp;
//            fprintf(stderr, "----head not in use, return name is : %s ----\n",temp->name);
//
//            if(q->head == NULL){
//                q->back = NULL;
//            }
////            checkLRU(hash_table->lru_list, first_file_name);
//            printLRUhead(hash_table->lru_list);
//
//            return temp->name;//TODO: free temp?
//	    }
//		else{ // if head is IN use, increment to the next not in use and delete
//		    fprintf(stderr, "======================LRU head: %s in use =================\n",temp->name);
//
//		    if (temp->inuse_flag == 1 &&nextTemp ==NULL){ // if only 1 element and its in use...
//                fprintf(stderr,"Only 1 element: %s in LRU and its in use! Cannot poll from LRU :(  \n",temp->name);
//                goto out;
//		    }
//
//		    while(temp->inuse_flag == 1 && nextTemp !=NULL){
//
//		        temp = nextTemp;
//		        nextTemp = nextTemp->next;
//		    } // new temp points at the first one not in use
//
//
//
//            while(beforeTemp->next != temp){
//                beforeTemp = beforeTemp->next;
//            } // now beforeTemp points at the element before tobe deleted one
//            beforeTemp->next = nextTemp; // skip temp
//            return temp->name; //TODO: free temp?
//
//		}
//
//
//
//
//		}
//
//    out:
//    return NULL;
//
//}




// delete file in cache
// delete in LRU; delete in hashTable

// return -1 if invalid input
// return 1 if evicted 1

int cache_evict(struct server*sv, int amount_to_evict){
//	fprintf(stderr, ".....cache evict begin...\n");
//    checkLRU(hash_table->lru_list, first_file_name);

	// if invalid input, i.e. nothing on LRU_list or amount<0
	if ( hash_table->lru_list == NULL || hash_table->lru_list->head == NULL || amount_to_evict <=0){
//		fprintf(stderr, ".....LRU empty, cannot evict...\n");
//        checkLRU(hash_table->lru_list, first_file_name);
 //       printLRUhead(hash_table->lru_list);

		return -1;
	}

	// if input valid, delete from LRU, delete in table
	else{
		// 1. delete from LRU
        //fprintf(stderr, "@@@@@@@@@@@@ in evict, printing lru list @@@@@@@@\n");

//		// TODO: delete below, just for checking purpose
//		hashNode_element * check = hash_table->lru_list->head;
//		hashNode_element * checkNext = hash_table->lru_list->head->next;
//
//		while (check!=NULL){
//            fprintf(stderr, "lru list file name is : %s\n", check->name);
//            check = checkNext;
//            if(checkNext->next == NULL){
//                break;
//            }
//            else{
//                checkNext = checkNext->next;
//                }
//        }
//        fprintf(stderr, "@@@@@@@@@@@@ finish printing lru list @@@@@@@@\n");
//        // TODO: delete above, just for checking purpose



        char *file_name_todelete = pollFromLRU(hash_table->lru_list); // file name to delete

        if (file_name_todelete ==NULL){
            return -2;
        }
//        fprintf(stderr, ".....deleted 1 element %s from LRU, will delete in hashTable...\n", file_name_todelete);


		// 2. delete in table, update hashTable cache_size_remaining, update node size //TODO: check all size problems!

		int hash_value = hashValue(file_name_todelete);
		int hash_index = hash_value % hashTable_size;
//		fprintf(stderr, ".....found hashindex: %d... will delete in hashNode\n,", hash_index);


		hashNode  *p;
		p = hash_table->hashNodes[hash_index]; // p now points at first element within hashNode
		p->size -= 1; // node size -1

//		fprintf(stderr, ".....p points a hashNode with hash_index..\n");

		struct hashNode_element *temp = p->head;
		struct hashNode_element *tempNext = p->head->next;

		// if first element is the one, delete
		if (strcmp(temp->name,file_name_todelete) == 0){
//			fprintf(stderr, ".....first element in hashNode the one to delete...\n");

			hash_table->cache_size_remaining += temp->data->file_size; //
			p->head = tempNext;
			free(temp);
//			file_data_free(temp->data);

			//free(temp->name); //TODO: solve double free issue
//			file_data_free(temp->data);
//			free(temp);
//			fprintf(stderr, ".....finished first freeing element in hashNode...\n");

			if (p->head == NULL){
				p->back = NULL;
			}
//            checkLRU(hash_table->lru_list, first_file_name);

            return 1;

		} else{ //if element to evict is not the first in hashNode
//			fprintf(stderr, ".....element to evict is not the first in hashNode...\n");

			while (tempNext != NULL) {
				if (strcmp(tempNext->name,file_name_todelete) == 0){ // if next one is the one to delete
					temp->next = tempNext->next;
					hash_table->cache_size_remaining += temp->data->file_size; //
//					file_data_free(tempNext->data);
					free(tempNext);
					return 1;
				}
				temp = temp->next;
			} // now temp is at the end of LRU
//            checkLRU(hash_table->lru_list, first_file_name);
            printLRUhead(hash_table->lru_list);

            return -1;


		}
	}
}



// insert new fetched file in cache
	// first check if file size > total capacity ,if larger, then return -1
	// then check if cache size would reach limit if insert, if yes, evict
	// return 0 if new_file exists in cache, LRU also got updated
	// return 1 if new_file not found in cache, and is able to be added to hashtable and LRU
	// return -1 if cannot be added i.e. TOO LARGE (although not in cache)

	//return -2 if failed (should never return)

int cache_insert(struct server *sv, struct file_data *new_file){

//	fprintf(stderr, ".....cache insert begin...\n");
//    checkLRU(hash_table->lru_list, first_file_name);
    //printLRUhead(hash_table->lru_list);

//    fprintf(stderr, ".....insert new_file name is: %s, size is %d...\n",new_file->file_name, new_file->file_size);


        // if file size larger than total file size, return
        if (new_file->file_size > hash_table->total_cache_size) {
//            fprintf(stderr, ".....larger than capacity,return...\n");
//            checkLRU(hash_table->lru_list, first_file_name);
            return -1;
        }

            // if size is good, add to hashTable

        else {
            int evict_result;

//            fprintf(stderr, ".....adding to hashTable ...\n");

            //keep evicting until have enough space
            while (new_file->file_size > hash_table->cache_size_remaining) {
//                fprintf(stderr, ".....calling evict...\n");
                evict_result = cache_evict(sv, 1);
                if (evict_result <= -1){
                    break;
                }
                else{
//                fprintf(stderr, ".....evicted...\n");
                }
            }
            if (evict_result <= -2){ // return -2 when only 1 element in LRU and its in use...
                return -3;//TODO:
            }
            //now we have enough space for new_file...
//            fprintf(stderr, ".....enough space in  hashtable...\n");

            int hash_value = hashValue(new_file->file_name);
            int hash_index = hash_value % hashTable_size;

//            fprintf(stderr, ".....got hashindex %d to add to hashtable...\n", hash_index);

            // update cache size remaining
            hash_table->cache_size_remaining -= new_file->file_size;
//            fprintf(stderr, ".....remaining cache size is %d...\n", hash_table->cache_size_remaining);

            hashNode *p;
            if (hash_table->hashNodes[hash_index] == NULL) { //init hashNodes if null

//                fprintf(stderr, ".....init hashNode...\n");

                p = (hashNode *) malloc(sizeof(hashNode));
                p->head = NULL;
                p->back = NULL;
                p->size = 0;
                hash_table->hashNodes[hash_index] = p;
            }

            struct hashNode_element *temp = hash_table->hashNodes[hash_index]->head;
            // if nothing in that hashNode
            if (temp == NULL) {
//                fprintf(stderr, ".....inserting into an empty hashNode, new file name: %s...\n",new_file->file_name);


                hashNode_element *element;
                element = (hashNode_element *) malloc(sizeof(hashNode_element));
                element->data = new_file;
                element->name = new_file->file_name;
                element->next = NULL;
                element->inuse_flag = 0;

                hash_table->hashNodes[hash_index]->head = element;
//                fprintf(stderr, ".....finished adding to hashTable, about to update LRU...\n");

               // update_LRU(element->name, hash_table->lru_list);
//                checkLRU(hash_table->lru_list, first_file_name);
//                printLRUhead(hash_table->lru_list);

                return 1;

            } else { // hashNode has element, create an element and put at the end
//                fprintf(stderr, ".....!!inserting to an NON-EMPTY hashNode...\n");
                struct hashNode_element *tempNext = temp->next;

                while (tempNext != NULL) {
                    temp = tempNext;
                    tempNext = tempNext->next;
                }

                struct hashNode_element *addedElement = buildLRUNode(new_file->file_name);
                addedElement->data = new_file;
                temp->next = addedElement;
//                update_LRU(addedElement->name, hash_table->lru_list);
//                checkLRU(hash_table->lru_list, first_file_name);
//                printLRUhead(hash_table->lru_list);

                return 1;
            }


            return -2; // fail to add to hashTable
        }


}
















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
	if (sv->max_cache_size == 0) {
		ret = request_readfile(rq);
		if (ret == 0) { /* couldn't read file */
			fprintf(stderr, "***couldn't read file***\n");
			goto out;
		}

		request_sendfile(rq);
	}
	else{
	pthread_mutex_lock(&mutex_hash); //lock

	struct file_data *lookup_result;
	lookup_result = cache_lookup(sv,data->file_name);
    pthread_mutex_unlock(&mutex_hash);// unlock

	if(lookup_result != NULL){ // in cache: read from cache, set file, send file
//		fprintf(stderr, "***LOOKUP RESULT %s: IN CACHE***\n", data->file_name);

		pthread_mutex_lock(&mutex_hash); //lock
        request_set_data(rq, lookup_result);
        update_LRU(lookup_result->file_name,hash_table->lru_list);
        pthread_mutex_unlock(&mutex_hash);// unlock

		request_sendfile(rq);

//        fprintf(stderr, "sent file:%s \n",lookup_result->file_name);
//		return;
	} else{ // not in cache: read file, send file, insert into cache(not always)

	//        fprintf(stderr, "***LOOKUP RESULT %s:  NULL; READING DISK***\n",data->file_name);
	//		ret = request_readfile(rq);
	//
	//		if (ret == 0) { /* couldn't read file */
	//			fprintf(stderr, "***couldn't read file***\n");
	//			goto out;
	//		}
	//		fprintf(stderr, "*** finished reading disk: %s***\n",data->file_name);
	//
	//		request_sendfile(rq);
	//		fprintf(stderr, "*** sent file %s, about to call cache insert ***\n",data->file_name);
	//
	//		// file already sent to client
	//		// Doing insert cache below:
	//
	//		if(data->file_size <= hash_table->total_cache_size) { //only do insert only if data size < capacity
	//            pthread_mutex_lock(&mutex_hash); //lock
	//            int insert_result;
	//            insert_result = cache_insert(sv, data);
	//            fprintf(stderr, "***** finished cache insert func : %s ********\n",data->file_name);
	//            pthread_mutex_unlock(&mutex_hash); //unlock
	//
	//            if(insert_result == -2){ // second cache lookup indicates its in cache, dont cache it
	//                fprintf(stderr, "*******222 SECOND LOOKUP RESULT %s: Found in cache***\n", data->file_name);
	//                return;
	//            }
	//            else if(insert_result == -3) { // only 1 in LRU and its in use, and dont cache it
	//                fprintf(stderr, "only 1 in LRU and its in use \n ");
	//                return;
	//            }
	//        }else{ // if larger than capacity
	//		    fprintf(stderr,"^^^^^^^^^^^^^^^^NOT doing insert, file too large\n");
	//		    }
	//		fprintf(stderr,"about to return \n");
	//		return; //added
	//		//request_sendfile(rq);
	//		//file_data_free(data);//TODO: DOUBLE FREE?
	//	}

	//		fprintf(stderr, "***LOOKUP RESULT %s:  NULL; READING DISK***\n",data->file_name);
			ret = request_readfile(rq);

			if (ret == 0) { /* couldn't read file */
				fprintf(stderr, "***couldn't read file***\n");
				goto out;
			}
			pthread_mutex_lock(&mutex_hash);
			struct file_data* lookup_res_two = cache_lookup(sv,data->file_name);
			if(lookup_res_two == NULL){
				int insert_res = cache_insert(sv,data);
				if(insert_res > - 1){
					update_LRU(data->file_name,hash_table->lru_list);
				}

			} else{
				//request_set_data(rq, lookup_res_two);
				update_LRU(data->file_name,hash_table->lru_list);

			}
	//		fprintf(stderr, "***BEFORE THE LOCKCCCCC***\n");
			pthread_mutex_unlock(&mutex_hash);
			request_sendfile(rq);
	//		fprintf(stderr, "**READY TO RETURN***\n");

	//		return;
		}
	}
out:
	request_destroy(rq);
//	file_data_free(data);
}

/* entry point functions */
struct wait_queue *q;


pthread_cond_t full;
pthread_cond_t empty;
pthread_mutex_t mutex;

pthread_t * arrayOfPThreads;



struct server * server_init(int nr_threads, int max_requests, int max_cache_size)
{
//	fprintf(stderr, "******server init begin ********\n");

	/* Lab 4: create worker threads when nr_threads > 0 */
	/* Lab 4: create queue of max_request size when max_requests > 0 */
	struct server *sv;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	sv->max_requests = max_requests;
	sv->max_cache_size = max_cache_size;
	sv->exiting = 0;

	if (nr_threads > 0 || max_requests > 0 ) {

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




	/* Lab 5: init server cache and limit its size to max_cache_size */
	if (max_cache_size > 0){

		hash_table = (struct hashTable*)malloc(sizeof(struct hashTable)); //malloc for table itself

		// init LRU list
		hash_table->lru_list = (struct hashNode*)malloc(sizeof(struct hashNode));
		hash_table->lru_list->head = NULL;
		hash_table->lru_list->back = NULL;
		hash_table->lru_list->size = 0;
		hash_table->total_cache_size = max_cache_size;
		hash_table->cache_size_remaining = max_cache_size;

		// malloc for hashNodes
		hash_table->hashNodes = (struct hashNode**)malloc(sizeof(struct hashNode*)*hashTable_size);

		// iterate hashNodes and set to NULL
		for (int i = 0; i<hashTable_size; i++){
			hash_table->hashNodes[i] = NULL;
		}

		// init mutex for hash table
		pthread_mutex_init(&mutex_hash, NULL);

	}

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
	//fprintf(stderr, "*****server exit ********\n");

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

	//free hashTable
	if(sv->max_cache_size > 0){
		destroyHashNodes(hash_table);
		destroyLRUNodes(hash_table->lru_list);
		free(hash_table->lru_list);
		free(hash_table);
	}


	/* make sure to free any allocated resources */
	free(sv);
}
