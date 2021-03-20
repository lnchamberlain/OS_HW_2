/*  
    Copyright 2018-21 by
    University of Alaska Anchorage, College of Engineering.
    All rights reserved.
    Contributors:  Logan Chamberlain
                   Benjamin Good
		   and
		   Christoph Lauter
    See file memory.c on how to compile this code.
    Implement the functions __malloc_impl, __calloc_impl,
    __realloc_impl and __free_impl below. The functions must behave
    like malloc, calloc, realloc and free but your implementation must
    of course not be based on malloc, calloc, realloc and free.
    Use the mmap and munmap system calls to create private anonymous
    memory mappings and hence to get basic access to memory, as the
    kernel provides it. Implement your memory management functions
    based on that "raw" access to user space memory.
    As the mmap and munmap system calls are slow, you have to find a
    way to reduce the number of system calls, by "grouping" them into
    larger blocks of memory accesses. As a matter of course, this needs
    to be done sensibly, i.e. without wasting too much memory.
    You must not use any functions provided by the system besides mmap
    and munmap. If you need memset and memcpy, use the naive
    implementations below. If they are too slow for your purpose,
    rewrite them in order to improve them!
    Catch all errors that may occur for mmap and munmap. In these cases
    make malloc/calloc/realloc/free just fail. Do not print out any 
    debug messages as this might get you into an infinite recursion!
    Your __calloc_impl will probably just call your __malloc_impl, check
    if that allocation worked and then set the fresh allocated memory
    to all zeros. Be aware that calloc comes with two size_t arguments
    and that malloc has only one. The classical multiplication of the two
    size_t arguments of calloc is wrong! Read this to convince yourself:
    https://bugzilla.redhat.com/show_bug.cgi?id=853906
    In order to allow you to properly refuse to perform the calloc instead
    of allocating too little memory, the __try_size_t_multiply function is
    provided below for your convenience.
    
*/

#include <stddef.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
//Each mmap will be of a minimum of 16MB to reduce the number of mmap calls neccesary
#define MIN_SIZE (size_t) 16777216
/* Predefined helper functions */

static void *__memset(void *s, int c, size_t n) {
  unsigned char *p;
  size_t i;

  if (n == ((size_t) 0)) return s;
  for (i=(size_t) 0,p=(unsigned char *)s;
       i<=(n-((size_t) 1));
       i++,p++) {
    *p = (unsigned char) c;
  }
  return s;
}

static void *__memcpy(void *dest, const void *src, size_t n) {
  unsigned char *pd;
  const unsigned char *ps;
  size_t i;

  if (n == ((size_t) 0)) return dest;
  for (i=(size_t) 0,pd=(unsigned char *)dest,ps=(const unsigned char *)src;
       i<=(n-((size_t) 1));
       i++,pd++,ps++) {
    *pd = *ps;
  }
  return dest;
}

/* Tries to multiply the two size_t arguments a and b.
   If the product holds on a size_t variable, sets the 
   variable pointed to by c to that product and returns a 
   non-zero value.
   
   Otherwise, does not touch the variable pointed to by c and 
   returns zero.
   This implementation is kind of naive as it uses a division.
   If performance is an issue, try to speed it up by avoiding 
   the division while making sure that it still does the right 
   thing (which is hard to prove).
*/
static int __try_size_t_multiply(size_t *c, size_t a, size_t b) {
  size_t t, r, q;

  /* If any of the arguments a and b is zero, everthing works just fine. */
  if ((a == ((size_t) 0)) ||
      (b == ((size_t) 0))) {
    *c = a * b;
    return 1;
  }

  /* Here, neither a nor b is zero. 
     We perform the multiplication, which may overflow, i.e. present
     some modulo-behavior.
  */
  t = a * b;

  /* Perform Euclidian division on t by a:
     t = a * q + r
     As we are sure that a is non-zero, we are sure
     that we will not divide by zero.
  */
  q = t / a;
  r = t % a;

  /* If the rest r is non-zero, the multiplication overflowed. */
  if (r != ((size_t) 0)) return 0;

  /* Here the rest r is zero, so we are sure that t = a * q.
     If q is different from b, the multiplication overflowed.
     Otherwise we are sure that t = a * b.
  */
  if (q != b) return 0;
  *c = t;
  return 1;
}

/* End of predefined helper functions */

/* Your helper functions 
   You may also put some struct definitions, typedefs and global
   variables here. Typically, the instructor's solution starts with
   defining a certain struct, a typedef and a global variable holding
   the start of a linked list of currently free memory blocks. That 
   list probably needs to be kept ordered by ascending addresses.
*/

/*typedef node defines nodes to hold the address, the size in bytes of the memory node, and pointers to the next and previous
 nodes*/
typedef struct node{
  void *addr;
  size_t size;
  struct node *next;
  struct node *prev;
}node;

//Define Head and counters for number of malloc calls and number of free calls
node *head = NULL;
int NUM_ALLOCATIONS = 0;
int NUM_FREED = 0;

void* __malloc_impl(size_t size);

/*  removeNode takes a memory node pointer as an argument and removes it from the list of free nodes. Previous and next 
    pointers are updated */

void removeNode(node* node){
  if(node->prev == NULL){
    if(node->next){
      //If node has no previous but has a next, update pointer to head
      head = node->next;
    }
    //If node has no previous and no next, list is of length one, return NULL
    else{
      head = NULL;
    }
  }
    else{
      node->prev->next = node->next;
    }
    if(node->next){
     node->next->prev = node->prev;
    }
}

/*   mergeBlocks iterates over the ordered list and checks if any of the blocks are consecutive, and if they are, merge 
     them into one large block */

  void mergeBlocks(){
    node* currentNode = head;
    void* currentAddress, *nextAddress;
    while(currentNode->next != NULL){
      currentAddress = currentNode;
      nextAddress = currentNode->next;
      if(currentAddress + currentNode->size == nextAddress){
	//Here the two nodes are consecutive
	//Update size by adding the length of next node + the size of current node
	currentNode->size += currentNode->next->size;
	currentNode->next = currentNode->next->next;
	//If not at end of list, update prev pointer
	if(currentNode->next != NULL){
	  currentNode->next->prev = currentNode;
	}
	else{
	  break;
	}
      }
      currentNode = currentNode->next;
    }
  }

/*
  insertNode takes a node pointer as an argument and inserts it into the doubly linked list, sorted by order of ascending
  addresses. Stored in this fashion to allow for the mergeBlocks function above 

*/
void insertNode(node *node){
    node->prev = NULL;
    node->next = NULL;
    //If empty list or if address of node is less than address of current head, set head to node
    if((head == NULL) || head > node){
      if(head){
        head->prev = node;
      }
      struct node *newNode = head;
      node->next = newNode;
      head = node;
      }
    else{
      //Iterate over list until either the end is hit or the address of node is greater than the current node
      while((head->next) && head->next < head){
       head = head->next;
      }
      //Update pointers, node is inserted after currentNode
      node->next = head->next;
      node->prev = head;
      head->next = node;
    }
}

/*
  searchList takes a size in bytes and iterates over the list of free memory nodes until it finds a node of sufficently large
  size. A 'slice' is taken out of the found node of the requested size. This new node is returned and the remainder of the 
  existing node is updated with the new size. Returns NULL is no node of large enough size is found.  

*/ 

node* searchList(size_t size){
     node *temp = head;
     node* newNode;
     //Account for case empty list
     if(head == NULL){
       return NULL;
     }
    //returns the first memory node with size greater than size
     while(temp != NULL){
       /*Iterate over list until we find a node of at least size that has at least sizeof(node) leftover after removing up
       until 'size'*/
      if(size <  temp->size && (temp->size - size > sizeof(node))){
	//Create a new node out of the remainder of the block, starting at size bytes
	newNode = (node*)(((void*) temp) + size);
	//Update the node size after slice
	newNode->size = temp->size - size;
	temp->size = size;
	newNode->next = temp->next;
	//If temp was the only node in the list, update head to be newNode
	if(temp->next == NULL){
	  head = newNode;
	}
	//Remove temp from list
	else{	  
	  newNode->prev = temp->prev;
	  if(newNode->prev != NULL){
	    newNode->prev->next = newNode;
	  }
	}
	return temp;
      }
      temp = temp->next;
    }
     //If no node found, return NULL
    return NULL;
  }

/*
  createBlock takes a size in bytes and creates a new memory mapping using mmap. The size of the mappings is at least MIN_SIZE
  (16MB), and if size is larger than MIN_SIZE, creates a mapping that is a mutiple of the header size (sizeof(node) ). This is
  to ensure that slices may be taken out of the mapping there will always be enough space for headers. The multiplication of 
  the size of node and the number of nodes required to be larger than requested size is done using the provided __try_size_t_multiply function to ensure no error mutiplying bytes. Memory mappings are made private and anonymous 

*/
  void createBlock(size_t size){
    size_t sizeRequest, minSize, newSize;
    void *p;
    node* newBlock;
    //Handle size 0 case
    newSize = (size_t) 0;
    if(size == (size_t) 0){
      return;
    }
    minSize = MIN_SIZE / sizeof(node);
    sizeRequest = size + sizeof(node);
    //Handle overflow
    if(sizeRequest < 0){
	return;
     }
    //By using integer division with the sizeof(node), sizeRequest * sizeof(node) will always be a multiple of node size
    sizeRequest /= sizeof(node);
    //If the size is less than 16MB, set sizeRequest to 16MB
    if(sizeRequest < minSize){
      sizeRequest = minSize;
    }
    //Use provided helper function to peform multiplication
    __try_size_t_multiply(&newSize, sizeRequest, sizeof(node));
    //Mutiplication overflowed if newSize is 0
    if(newSize == 0){
      return;
    }
    p = mmap(NULL, newSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    //Catch mmap errors
    if(p == MAP_FAILED){
      return;
    }
    //Populate fields and insert newNode into list
    newBlock = (node*) p;
    newBlock->size = newSize;
    newBlock->prev = NULL; 
    newBlock->next = NULL;
    insertNode(newBlock);
    
  }
  
/*
  unmapBlocks iterates over the list and calls munmap to release the mapped memory. 
  NOTE* unmapBlocks is called when the number of allocated nodes is equal to the number of freed nodes. 
  This means that if the user of these functions does not free every node they allocate, unmap will not
  be called. 

*/
void unmapBlocks(){
  node *curr = head;
  node* next = curr->next;
  while(curr != NULL){
    if(munmap(curr, curr->size) < 0){
      //Display any error messages and return if unmmap is unsuccessful
      fprintf(stderr,"Error munmapping: %s\n", strerror(errno));
      return;
    }   
    if(next != NULL){
      next = next->next;
    }
    curr = next;
  }
}
/* End of your helper functions */

/* Start of the actual malloc/calloc/realloc/free functions */

void __free_impl(void *);

/*
  __malloc_impl is an implementation of the malloc system call and functions in the same fashion. It accepts 
  a size in bytes and returns a pointer to a free memory block of the requested size. This is accomplished by 
  searching the list of free memory nodes for a node of sufficent size. If no node of sufficent size is found, 
  one of greater size is created using the above createBlocks function and a slice of requested size is returned. 
  Each node contains a header populated with information about the node, namely the size and pointers to next and
  previous nodes. __malloc_impl returns a void pointer to the free memory immediately following the header, 
  denoted startofFreeBlock. 

*/
void *__malloc_impl(size_t size) {
  //Handle case where requested size is 0
  size_t sizeofBlock;
  node *ptr;
  void* startofFreeBlock;
  if(size == (size_t) 0){
    return NULL;
  }
  //account for the header size
  sizeofBlock = size + sizeof(node);
   //Return NULL is addition overflows
  if(sizeofBlock < 0){
    return NULL;
  }
  ptr = searchList(sizeofBlock);
  if(ptr != NULL){
    //Found a block of sufficent size, account for header
    void* startofFreeBlock = ptr + sizeof(node);
    //ptr has been allocated and so remove from list
    removeNode(ptr);
    //Increment global counter NUM_ALLOCATIONS for the purpose of determining if every allocated node has been freed
    NUM_ALLOCATIONS++;
    return startofFreeBlock;
  }
  //If no block of the right size is in the list, create a new one.
  createBlock(sizeofBlock);
  //Search again, a block of large enough size should exist barring errors
  ptr = searchList(sizeofBlock); 
  if(ptr != NULL){
    //Repeat above steps 
    startofFreeBlock = ptr + sizeof(node);
    removeNode(ptr);
    NUM_ALLOCATIONS++;  
    return startofFreeBlock;
  }
  //If any errors occured and no blocks where found, return NULL
  return NULL;
}

/*

  __calloc_impl is an implemenation of the calloc system call. Memory is manually allocated and then set to 0. 
  It accepts two sizes in bytes, one for the size of the members and the total size. Multiplication of these two sizes is 
  implemented using the provided _try_size_t_multiply function. Once the total size is found, __calloc_impl calls __malloc_impl 
  to create the block. Each member is set to '0' using the provided __memset function 

*/

void *__calloc_impl(size_t nmemb, size_t size) {
  size_t sizeRequired;
  void *ptr;
  int multiplySuccess;
  //Use provided mutiply function to multiply nmeb and size, returns 1 on success, 0 on failure
  multiplySuccess = __try_size_t_multiply(&sizeRequired, nmemb, size);
  if(multiplySuccess){
  //Use new implementation of malloc
    ptr = __malloc_impl(sizeRequired);
    if(ptr != NULL){
      //Use provided memset function to set all to 0
      __memset(ptr, 0, sizeRequired);
    }
    return ptr;
  }
  else{
    //Return null in case of any errors
    return NULL;
  }
}

/*
  __realloc_impl is an implemenation of system call realloc. It takes a previously allocated node and a new size
  and adjusts the size of the node. If the pointer is null, __realloc_impl calls __malloc_impl and returns the allocated
  space. If size is 0, __realloc_impl calls __free_impl to free the node. If the new size is smaller than that of the 
  existing node, only the memory up until new size is copied. Other size it copies all of the space up until the size of 
  the existing node and leaves the remainder unpopulated. Memory copies are done using the provided __memcopy function

*/

void *__realloc_impl(void *ptr, size_t size) {
  void *newptr;
  node* nodePtr;
  size_t oldSize;
  //If size is 0, realloc function as free, call free on ptr
  if(size == (size_t) 0){
   __free_impl(ptr);
  }
  /*Information about the node, including the previous size, is
    stored in the header found at ptr - sizeof(node)*/
  nodePtr = (node*)ptr - sizeof(node);
  oldSize = nodePtr->size;
  newptr = __malloc_impl(size);
  //If ptr is null, realloc functions as malloc 
  if((ptr) == NULL){
    return newptr;
  }  
 
  /*Copy the full size of the old block if it is smaller than the size value passed,
    otherwise use the argument.*/
  if (oldSize < size) {
    //Use provided __memcpy function to copy memory from old to new memory blocks
    __memcpy(ptr, newptr, oldSize);
  }
  else {
	__memcpy(ptr, newptr, size);
  }
  //Free old pointer and return new pointer
  __free_impl(ptr);
  return newptr;
}

/*
  __free_impl is an implemenation of the system call free. It takes a void pointer to previously allocated memory 
  and adds it to the list of free memory nodes. This is done by retrieving the information about the node in the
  header that was populated when the node was allocated. NOTE* if free is called on a pointer that was not allocated, 
  casting to a node* to retrieve node information will result in a segmentation fault. A check is made if the number
  of allocations is equal to the number of freed nodes, and if so, __free_impl calls unmapBlocks to release the memory. 

*/
 void __free_impl(void *ptr){
   //Handle case free(nil)
   if(ptr == NULL){
     return;
   }
   //Increment global counter NUM_FREED to check if all nodes have been freed
   NUM_FREED++;
   //Retrieve header
   node* freeBlock = (node*)ptr - sizeof(node);
   insertNode(freeBlock);
   mergeBlocks();
   if(NUM_FREED == NUM_ALLOCATIONS){
     //Call mergeBlocks one final time to ensure the list is an condensed as possible
     mergeBlocks();
     unmapBlocks();
   }
}

