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

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct node{
  size_t sz;
  struct node *next;
  int use;
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
			return cur;						
		}
		
		cur = cur->next;
		
	}
	return NULL;
}

//insert at the end of the linked list
List *insert_node(List *h, size_t size, char* add_posi){
  
  node *myblock = (node *) add_posi;
  List *cur = h;
  
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
    while(1){
      if (cur->next == NULL) break;
      cur = cur -> next;
    }
    
    //myblock->head = head;
    myblock->sz = size;
    myblock->next = NULL;
    myblock->use = USED;
    cur-> next = myblock;
    return h;
  }
}

//coalescing from the start of the linked list
int coalescing(){
	List *cur = h;
	while(1){
		if (cur == NULL) return 0;
		if (cur->next == NULL) return 0;
		if (cur->use == UNUSED && cur->next->use == UNUSED){
			cur->sz = cur->sz + cur->next->sz;
			cur->next = cur->next->next;
			return 0;
		}
		cur = cur->next;
	}
}

//for free
int remove_node(List *h, void *ptr){
	List *cur = h;
	//char *ptr_cp = ptr + sizeof(node); 
		
	while (1){
		if (cur == NULL){
			return 1;
		}
		if (cur == ptr){
			cur->use = UNUSED;
			return 0;
		}				
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

  pthread_mutex_lock(&lock);
  char *cp;
  size_t rough_size = sizeof(node) + size; //cannot be divide by 8
  size_t act_size; //can be divide by 8
  
  
  cp = current;
  
  //rough size can divided by 8
  if (rough_size%8 == 0){
    act_size = rough_size;
  }
  
  //cannot divided by 8
  else {
    unsigned int factor = rough_size / 8;
    act_size =  factor*8 + 8;
  }
  
  
  List *inside = check_avaible_space(h, act_size);
  
  if (inside != NULL) {
	//printf("current: %p\n",current);
    //printf("end: %p\n",end);
	pthread_mutex_unlock(&lock);
	return inside;
  }
  
  
  else{
	size_t avb = end - current;
	int i = 0;
  
	while (avb <= act_size){
		avb += 4096;
		i += 1;
	} 
	if(i != 0) sbrk(i*page);
	end = sbrk(0);
	
    //error checking
	if(end == ((void *) - 1)) {
    fprintf(stderr, "sbrk(4096) error\n");
	pthread_mutex_lock(&lock);
	return NULL;
	}
	  
	h = insert_node(h, act_size, cp);
  
    current = current + act_size;	
    
    pthread_mutex_unlock(&lock);
    return cp;
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
	
	if (remove_node(h,ptr) == 0){
		coalescing();
		pthread_mutex_unlock(&lock);
		return 0;
	}
	
	else{
		pthread_mutex_unlock(&lock);
		return 1;
	}
		   	
}



