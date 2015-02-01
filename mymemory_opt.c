#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#define page 4096
/* mymalloc_init: initialize any data structures that your malloc needs in
                  order to keep track of allocated and free blocks of 
                  memory.  Get an initial chunk of memory for the heap from
                  the OS using sbrk() and mark it as free so that it can be 
                  used in future calls to mymalloc()
*/
#define USED 1
#define UNUSED 0

char* current;
char* end;

size_t last_size = 0;

int freed = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;




typedef struct node{
  size_t sz;
  struct node *next;
  short use;
} node;

typedef struct node List;
//typedef struct node List;

pthread_mutex_t lock;

//head of the linked list
List *h = NULL;


//insert into freed block by first fit
List* check_avaible_space(List *h, size_t size){
	List *cur = h;
	while (cur != NULL) {
		
		//first avaliable space
		//size + 8 for safe
		if(((cur->use) == UNUSED) && ((size + 8) < (cur->sz))){
			
			//special case for last insert node, the node is end of the
			//linked list
			if(cur->next == NULL){
				cur->use = USED;
				size_t s_cp = cur->sz;
				cur->sz = size; 
				current = current - (s_cp - size);
				last_size = size;
				freed += 1;
				return cur;
			}
			
			//general case
			cur->use = USED;
			size_t s_copy = cur->sz;
			cur->sz = size;
			char* adr = (char *) cur;
			node *new_block = (node*) (adr + size);
						
			new_block->use = UNUSED;
			new_block->sz = s_copy - size;
			new_block->next = cur->next;
			cur->next = new_block;
			freed += 1;
			return cur;						
		}		
		cur = cur->next;	
	}
	return NULL;
}

//insert at the end of the linked list
List *insert_node(size_t size, char* add_posi){
  
  node *myblock = (node *) add_posi;
  List *cur = h;
  node *last_node;
  
  
  //the linked list is empty
  if (cur == NULL){
    myblock->sz = size;
    myblock->next = NULL;
    myblock->use = USED; 
    
    
	//create the link list
    h = myblock;
    
    return myblock;
  }
  //the linked list is not empty
  //store at the end of the list
  else{
		last_node = (node*) (current - last_size);
		myblock->sz = size;
        myblock->next = NULL;
        myblock->use = USED;
        last_node->next  = myblock; 		    
    return h;
  }
}

//free the node
int remove_node(void *ptr){
	List *cur = h;
	node *prev = NULL;
	
	while (1){
		if (cur == NULL){
			return 1;
		}

		if(cur->next == NULL){
			//change the tag
			if(cur == ptr){
			   cur->use = UNUSED;
			   //coalescing
			   if(prev != NULL && prev->use == UNUSED){
					prev->sz = prev->sz + cur->sz;
					prev->next = cur->next;
					last_size = prev->sz;
			   }
			   return 0;
			}
			return 1;
		}
				
		
		if (cur == ptr){
			//change the tag
			cur->use = UNUSED;
			//coalescing
			if(prev != NULL && prev->use == UNUSED){
				prev->sz = prev->sz + cur->sz;
				prev->next = cur->next;
				cur = NULL;
			}
			//coalescing
			else if(cur->next->use == UNUSED){
				cur->sz = cur->sz + cur->next->sz;
				cur->next = cur->next->next;
			}			
			return 0;							
		}				
		
		prev = cur;
		cur = cur->next;	
	}	

}


int mymalloc_init() {

	// placeholder so that the program will compile
  pthread_mutex_lock(&lock);
  
  if(h == NULL){
  current = sbrk(0);
  }
  
  //error checking
  if(current == ((void*) -1)){
	fprintf(stderr, "sbrk(0) error\n");
	pthread_mutex_lock(&lock);
	return 0;
  }
  
  //sbrk one page in every init
  sbrk(page);
   
  end = sbrk(0);
  //error checking
  if(end == ((void *) - 1)) {
    fprintf(stderr, "sbrk(4096) error\n");
	pthread_mutex_lock(&lock);
	return 0;
  }  
  
  
  pthread_mutex_unlock(&lock);
  return 0; // non-zero return value indicates an error
}




/* mymalloc: allocates memory on the heap of the requested size. The block
             of memory returned should always be padded so that it begins
             and ends on a word boundary.
     unsigned int size: the number of bytes to allocate.
     retval: a pointer to the block of memory allocated or NULL if the 
             memory could not be allocated. 
             (NOTE: the system also sets errno, but we are not the system, 
                    so you are not required to do so.)
*/
void *mymalloc(unsigned int size) {


  size_t rough_size = sizeof(node) + size; //might not be divide by 8
  
  size_t act_size = (((rough_size-1)>>3)<<3) + 8; //can be divide by 8
  
  pthread_mutex_lock(&lock);
  char* cp = current;  
  
  
  //is not any freed block, just insert at the bottom of the linkedlist
  if (freed == 0) {
  	size_t avb = end - current;
	int i = 0;
  
	if(act_size>=avb){
		i = ((act_size - avb) >> 12) + 1;
		sbrk(i*page);
	}
	
	end = sbrk(0);
	
	//error checking
	if(end == ((void *) - 1)) {
    fprintf(stderr, "sbrk(4096) error\n");
	pthread_mutex_lock(&lock);
	return NULL;
	}  
	
	insert_node(act_size, cp);
  
   //last block current use now
   current = current + act_size;
   last_size = act_size;
   pthread_mutex_unlock(&lock);
   return cp;
  }
  
  else{ 
  
  //check avaliable space insert bewteen linked list
  List *inside = check_avaible_space(h, act_size);
  
  if (inside != NULL) {
	pthread_mutex_unlock(&lock);
	return inside;
  }
  
  //no such space,insert at the end of linked list
  else{
	size_t avb = end - current;
	int i = 0;
  
	if(act_size>=avb){
		i = ((act_size - avb) >> 12) + 1;
		sbrk(i*page);
	}
	end = sbrk(0);
	insert_node(act_size, cp);
  

  //last block current use now
  current = current + act_size;
  last_size = act_size;

  //return the start of the avaliable space address instead of node 
  //address		
  pthread_mutex_unlock(&lock);
  return cp;
	}
  }
}


/* myfree: unallocates memory that has been allocated with mymalloc.
     void *ptr: pointer to the first byte of a block of memory allocated by 
                mymalloc.
     retval: 0 if the memory was successfully freed and 1 otherwise.
             (NOTE: the system version of free returns no error.)
*/
unsigned int myfree(void *ptr) {	 
	pthread_mutex_lock(&lock);
	if (remove_node(ptr) == 0){
		freed += 1;	
		pthread_mutex_unlock(&lock);
		return 0;
		}
	
	else{
		fprintf(stderr, "The block have not be malloced and cannot be freed\n");
		pthread_mutex_unlock(&lock);
		return 1;
	}		   	
}



