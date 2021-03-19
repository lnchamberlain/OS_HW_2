/*  
    Copyright 2018-21 by
    University of Alaska Anchorage, College of Engineering.
    All rights reserved.
    Contributors:  ...
		   ...                 and
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

/*store address as a value in a long long so it can be used to create an ordered list */
typedef struct node{
  void *addr;
  size_t size;
  int beenFreed;
  struct node *next;
  struct node *prev;
}node;
//typedef struct node_struct node;
/*
typedef struct allocatedNode {
	size_t size;
        void *addr;
  int beenFreed;
	struct allocatedNode *next;
  }allocatedNode;
*/
//Define Head and counters for number of malloc calls and number of free calls
node *head = NULL;
int NUM_ALLOCATIONS = 0;
int NUM_FREED = 0;
//allocatedNode *ANhead;
//blockNode *blockHead = NULL;
/*Begin ordered list functions */
//void insertAN(void *ptr);

//void insertBN(blockNode *bn);
//size_t getBlockSize(void *ptr);
void* __malloc_impl(size_t size);
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

//Inset allocated Nodes, sort by address because 
/*void insertAN(allocatedNode *an){
  if(ANhead == null){
    ANhead = an;
  }
  allocatedNode *curr;
  elseif(ANhead < an){
    an->next = ANhead;
    ANhead = an;
  }    
  else{
    curr = ANhead;
    while(curr->next != NULL && curr->next < an){
      curr= curr->next;
    }
    an->next = curr->next;
    curr->next = an;
  }
}
    
*/


/*
  void insertNode(node *node){
    
    //node->addr = node;
    node->prev = NULL;
    node->next = NULL;
    printf("In insert Node, head is %p, node is %p\n", head, node);
    //If empty list or if address of node is less than address of current head, set head to node
    if((head == NULL) || head > node){
      printf("Head is null\n");
      if(head){
	head->prev = node;
      }
      //node->next = head;
      head = node;
      printf("Head here is %p\n", head);
    }
    else{
      curr = head;
      //Iterate over list until either the end is hit or the address of node is greater than currentNode
      while((curr->next) && curr->next < curr){
	curr = curr->next;
      }
      //Update pointers, node is inserted after currentNode
      node->next = curr->next;
      node->prev = curr;
      curr->next = node;
    }
    printf("At the end of the function head is %p\n", head);
  }
*/
  /*mergeBlocks iterates over the ordered list and looks to see if any of the blocks are consecutive, and if they are, merge them into one large block */ 
  void mergeBlocks(){
    node* currentNode = head;
    void* currentAddress, *nextAddress;
    printf("\n \n MERGEBLOCKS TESTING ALERT \n");
    printf("Hello hello I am in mergeBlocks, currentNode is %p\n", currentNode);
    printf("Before merging: \n");
    printLists();
    
    while(currentNode->next != NULL){
      currentAddress = currentNode;
      nextAddress = currentNode->next;
      printf("Curr address is %p next Address is %p\n", currentAddress, nextAddress);
      //As each node is of type node, they have a header with that node variables that is of size sizeof(node). This must be added to the sizes of each node to account for this information
      
      //To see if consecutive, start at currentAddress, add the size of the current node, and then add the size of the node type to get to the next block. 
      if(currentAddress + currentNode->size == nextAddress){
	//Here the two nodes are consecutive
	//Update size by adding the length of next node + the size of the type node
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
    printf("After merging \n");
    printLists();
  }

void insertNode(node *node){
    //node->addr = node;
    node->prev = NULL;
    node->next = NULL;
    printf("In insert Node, head is %p, node is %p\n", head, node);
    //If empty list or if address of node is less than address of current head, set head to node
    if((head == NULL) || head > node){
      if(head){
        head->prev = node;
      }
      struct node *newNode = head;
      node->next = newNode;
      head = node;
      //head = node;
      printf("Head here is %p\n", head);
    }
    else{
      //Iterate over list until either the end is hit or the address of node is greater than currentNode
      while((head->next) && head->next < head){
       head = head->next;
      }
      //Update pointers, node is inserted after currentNode
      node->next = head->next;
      node->prev = head;
      head->next = node;
    }
    printf("At the end of the function head is %p\n", head);
    if(head->next == NULL){
      printf("Head ->next is null\n");
    }
    else{
      printf("Head ->next is %p \n", head->next);
    }
}

   // checks if every address in both lists are equal
/*
  int areNodesEqual() {

	  // return False if one of the heads is NULL while the other is not.
	  if ( (head == NULL && blockHead != NULL) || (head != NULL && blockHead == NULL) ) {
		  return 0;
	  }
	  node *curr = head;
	  blockNode *currBN = blockHead;

	  // loop through the lists while the addresses are equal
	  while (curr->addr == currBN->addr) {
		  // check if the next node in one list is NULL while the next node in the other list is not
		  // if the condition is true, that means the lists arent' equivalent and the functionr returns False.
		  if ( (curr->next == NULL && currBN->next != NULL) || (curr->next != NULL && currBN->next == NULL) ) {
			  return 0;
		  }

		  // if next == NULL for both nodes, that would mean that the addresses have been equal
		  // and the lists are of the same length, so the function returns True.
		  if (curr->next == NULL && currBN->next ==  NULL) {
			  return 1;
		  }
		  curr = curr->next;
		  currBN = currBN->next;
	  }

	  return 0;
  }
*/

 node* searchList(size_t size){
     node *temp = head;
     node* newNode;
     printf("Hello there I am in searchList, temp is %p \n", temp);
     //Account for case empty list
     if(head == NULL){
       return NULL;
     }
    //returns the first memory node with size greater than size
    while(temp != NULL){
      printf("Temp->size is %x\n", temp->size);
      //Iterate over list until we find a node of at least size that has at least sizeof(node) leftover after removing up until size
      if(size <  temp->size && (temp->size - size > sizeof(node))){
	//Create a new node out of the remainder of the block, starting at size bytes
	newNode = (node*)(((void*) temp) + size);
	printf("Created a new node object in search list, of size %x at address %p\n", temp->size - size, newNode);
	newNode->beenFreed = 0;
	newNode->size = temp->size - size;
	temp->size = size;
	printf("Made it past one assignment \n");
	//Store addr of the mmap
	newNode->addr = newNode;
	printf("Made it past two assignments \n");
	newNode->next = temp->next;
	temp->next = newNode;
	printf("Made it past more assignments \n");
	printf("Temp ->prev = %p\n", temp->prev);
	printf("Node information: beenFreed = %d, size = %x, located at %p, addr = %p, next = %p\n", newNode->beenFreed, newNode->size, newNode, newNode->addr, newNode->next);
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
	printf("Temp is the node being returned, at address %p of size %x\n", temp, temp->size);
	printf("Made it all the way to return temp\n");
	return temp;
      }
      temp = temp->next;
    }
    return NULL;
  }
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
    //Leave space for null terminator
    sizeRequest = size - sizeof(node);
    //Handle overflow
    if(sizeRequest < 0){
	return;
     }
    sizeRequest /= sizeof(node);
    if(sizeRequest < minSize){
      sizeRequest = minSize;
    }
    //Set newSize to a multipile of sizeof(node) that is a minimum of 16MB
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
    //Populate fields and create new node
    newBlock = (node*) p;
    newBlock->size = newSize;
    newBlock->addr = p;
    newBlock->beenFreed = 0;
    newBlock->prev = NULL; 
    newBlock->next = NULL;
    printf("Hello there I am now in createBlock, newBlock is is %p\n", newBlock);
    printf("New block size is %x \n", newBlock->size);
    
	// adding an item to the BN list with data used in mmap.
    //node* nodeHeader = (node*)mmap(NULL, sizeof(node), PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, 0);
    //blockNode* blocknodeHeader = (blockNode*)mmap(NULL, sizeof(blockNode), PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, 0);
    //TODO CHANGE BELOW TO __MALLOC_IMPL once it works
    /*blockNode *bn = malloc(sizeof(blockNode));
     node *newNode = malloc(sizeof(node));
    */
    // blockNode *bn = __malloc_impl(sizeof(blockNode));
    // node *newNode = __malloc_impl(sizeof(node));
    
    //printf("Block node has been created, bn is %p\n", &blocknodeHeader);
    //Account for size of node header, not blockNode header as node will be used for comparison later
    //blocknodeHeader->addr = &p; //- sizeof(node);
    //printf("bn addr is %p\n", blocknodeHeader->addr);
    //blocknodeHeader->size = size;
    //printf("bn size is %x\n", blocknodeHeader->size);
    //Allocated but not freed, set beenFreed to false
    //blocknodeHeader->beenFreed = 0;
    //insertBN(blocknodeHeader);
    //Add the free space to the LL
    //Might get an overflow when computing space
    /*int overflowCatch = 0;
    if(p != NULL){
      //Space must be a multiple of sizeof(node) so there won't be any errors adding or deleting nodes 
      size_t s = sizeof(node);
      for(int i = 0; 1; i++){
	if(s * i > size){
	  s = s * i;
	    break;
	}
	//Case: overflow, throw not enough space error?
	if(s * i < 0){
	  int overflowCatch = 1;
	  break;
	}
      }
      if(overflowCatch){
	return NULL;
      }
      //Declare size for comparisions in searchList
      /*->size = s;
    */
      insertNode(newBlock);
      printf("After insert node, head is %p\n", head);
      //After inserting node, mergeBlocks in case the new mapping is consecutive with existing blocks
      //mergeBlocks();
      // printf("After mergeblocks head is %p\n", head);
      
    
  }
  
  // blockList functions
/*  void insertBN(blockNode *bn) {
	  // basic insertion for linked lists
	  if (blockHead == NULL) {
		  blockHead = bn;
	  }
	  else {
		  blockNode *curr = blockHead;
		  
		  while (curr->next) {
			  curr = curr->next;
		  }
		  
		  curr->next = bn;
	  }
	  bn->next = NULL;
  }
  
  // basic removal function
  void removeBN(blockNode *bn) {
	  if (blockHead == NULL) {
		  return;
	  }
	  
	  blockNode *curr = blockHead;
	  while (curr->next) {
		  if (curr->next == bn) {
			  curr->next = bn->next;
			  break;
		  }
		  else {
			  curr = curr->next;
		  }
	  }
  }
  
  // This will get the size by comparing the address stored in the node to the pointer passed.
  size_t getBlockSize(void *ptr) {
  blockNode *curr = blockHead;
  //For some reason we need to account for the added int?
  ptr -= 8;
  //void * newptr = ptr - 8;
  //printf("ptr is %p, newptr is %p\n", ptr, newptr);
  //ptr += (size_t) 2;
  printf("I am in getBlockSize, ptr is %p \n", ptr);
  while (curr != NULL) {
  // compare the address in the node to ptr, deciding on how they should be checked for equivalence.
    printf("Curr is %p \n", curr);
    if (curr == ptr) {
      curr->beenFreed = 1;
      return curr->size;
		  }
		  curr = curr->next;
	  }
	  
	  return NULL;
  }
*/
void unmapBlocks(){
  printf("All nodes have been freed, in munMapBlocks()\n");
  //blockNode *curr = blockHead;
  node *curr = head;
  node* next = curr->next;
  //blockNode *next = curr;
  int i = 1;
  while(curr != NULL){
    //next = curr->next;
    printf("unmapping block number %d, at address %p of size %x\n", i, curr, curr->size);
    if(munmap(curr, curr->size) < 0){
      printf(stderr,"Error munmapping: %s\n", strerror(errno));
      return;
    }
    printf("Made it to here\n");
    if(next == NULL){
      printf("Next is null\n");
    }
    
   
    if(next != NULL){
      next = next->next;
    }
    printf("Made it to here pt2\n");
    curr = next;
    i++;
  }
}
/*
void unMapCheck(){
  //blockNode *curr = blockHead;
  node *curr = head;
  while(curr != NULL){
    if(curr->beenFreed == 0){
      printf("Not all blocks have been freed\n");
      return;
    }
    curr = curr->next;
  }
  //If we make it to here, every node has been freed, call munMapBlocks
  munMapBlocks();
}
/*
  /*
if(unmapMemory(mappedFile, fileLength) < 0){
      return 1;
    }
  */
/* End of your helper functions */

/* Start of the actual malloc/calloc/realloc/free functions */

void __free_impl(void *);

void *__malloc_impl(size_t size) {
  if(size == (size_t) 0){
    return NULL;
  }
  printf("Hello I am in malloc, the requested size is %x\n", size);
  //account for the header size
  size_t sizeofBlock = size + sizeof(node);
  printf("Accounting for sizeofnode, the size is %x\n", sizeofBlock);
  //Handle overflow error
  if(sizeofBlock < 0){
    printf("Not enough memory available \n");
    return NULL;
  }
  node *ptr = searchList(sizeofBlock);
  printf("I am back in malloc\n");
  //Found a block large enough to hold the requested size
  if(ptr != NULL){
    //again account for size of header
    void* startofFreeBlock = ptr + sizeof(node);
    //ptr has been allocated and so remove from list
    removeNode(ptr);
    NUM_ALLOCATIONS++;
    return startofFreeBlock;
  }
  //If no block of the right size is in the list, create a new one.
  createBlock(sizeofBlock);
  //Search again, ptr should exist
  ptr = searchList(sizeofBlock);
  printf("Node returned from search List is at address %p of size %x\n", ptr, ptr->size);
 
  //Found a block large enough to hold the requested size
  if(ptr != NULL){
    //again account for size of header
    void* startofFreeBlock = ptr + sizeof(node);
    printf("startofFreeBlock is %p\n", startofFreeBlock);
    //ptr has been allocated and so remove from list
    printLists();
    removeNode(ptr);
    printLists();
    NUM_ALLOCATIONS++;
    printf("\n Testing casting and not casting \n");
    
    return startofFreeBlock;
  }
  return NULL;
}

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
    printf("Error multiplying bytes \n");
    return NULL;
  }
}

void *__realloc_impl(void *ptr, size_t size) {
  void *newptr;
  //If size is, 0, perform free on ptr
  if(size == (size_t) 0){
   __free_impl(ptr);
  }
  newptr = __malloc_impl(size);
  //If ptr is null, perform malloc with size
  if((ptr) == NULL){
    return newptr;
  }  
  // get the size of the old block
  size_t oldSize = searchList(ptr);
  
  // Copy the full size of the old block if it is smaller than the size value passed,
  // otherwise use the argument.
  if (oldSize < size) {
    __memcpy(ptr, newptr, oldSize);
  }
  else {
	__memcpy(ptr, newptr, size);
  }
  __free_impl(ptr);
  
  return newptr;
  // return NULL;  
}

 void __free_impl(void *ptr){
   //TODO Change this once __malloc_impl works!!!
   //node *freeBlock = malloc(sizeof(node));
   //Handle case free(nil)
   if(ptr == NULL){
     return;
   }
   printf("Here in free, ptr is %p\n", ptr);
   printf("size of node is %x\n", sizeof(node));
   printf("ptr - sizeof node is %p\n", ptr - sizeof(node));
   NUM_FREED++;
   node* freeBlock = (node*)ptr - sizeof(node);//node *freeBlock = __malloc_impl(sizeof(node));
   //printLists();
   printf("free block here is %p of size %x\n", freeBlock, freeBlock->size);
   insertNode(freeBlock);
   printf("\n");
   printf("Here in free above printLists()\n");
   printLists();
   printf("made it back from print lists in free \n");
   printf("Allocations = %d, freed = %d\n", NUM_ALLOCATIONS, NUM_FREED);
   mergeBlocks();
   if(NUM_FREED == NUM_ALLOCATIONS){
     // ummapBlocks();
     
     printf("All blocks freed\n");
     mergeBlocks();
     unmapBlocks();
     printf("Made it back from unmapBlocks\n");
   }
   //unMapCheck();
   //After inserting new node, call mergeBlocks in case new node is consecutive and can be condensed
}

/* End of the actual malloc/calloc/realloc/free functions */
 void printLists(){
   node*curr = head;
   //blockNode* currBN = blockHead;
   int i;
   if(head == NULL){
     printf("List is empty\n");
   }
   else{
     i = 1;
     //printf("List has %d items. Current node is at %p of size %x \n", i, curr, curr->size);
     while(curr != NULL){
     printf("List has %d items. Current node is at %p of size %x \n", i, curr, curr->size);
     i++;
     curr = curr->next;
   }
   }
   /*
   if(blockHead == NULL){
     printf("List two is empty\n");
   }
   else{
   i= 1;
   //printf("BN has %d items. Current node is at %p and of size %x \n", i, currBN, currBN->size);
   while(currBN != NULL){
     printf("BN has %d items. Current node is at %p and of size %x, been freed is %d\n", i, currBN, currBN->size, currBN->beenFreed);
     i++;
     currBN = currBN->next;
   }
   }
   */
 }

#define SIZE (16)
int main(int argc, char **argv){
  char *ptrTest1, *ptrTest2, *ptrTest3, *ptrTest4;
  int i;
  printf("size of node is %x\n", sizeof(node));
  printf(" \n \n");
  ptrTest1 = __malloc_impl(SIZE + (size_t) 10);
  printf("\nptrTest 1 points to %p\n", ptrTest1);
  node* testNode = (node*)ptrTest1 - sizeof(node);
  printf("Node test points to %p\n",testNode);
  printf("Node test size is %x\n", testNode->size);
  
  printf(" \n \n ");
  printLists();
  printf("\n \n");
  
  ptrTest2 = __malloc_impl((size_t)10);
  printf("ptrTest2 points to %p\n", ptrTest2);
  testNode = (node*)ptrTest2 - sizeof(node);
  printf("Node test points to %p\n",testNode);
  printf("Node test size is %x\n", testNode->size);
  printf(" \n \n");
  printLists();
  printf(" \n \n");
  
  ptrTest4 = __malloc_impl(1600);
  printf("\n \n");
  
  printLists();
  
  printf("End of malloc\n");
  __free_impl(ptrTest1);
  __free_impl(ptrTest2);
  //__free_impl(ptrTest3);
  printLists();
  printf("Testing freeing last node: \n");
  __free_impl(ptrTest4);
  printf("SUCCESS!!");
  return 0;
}
