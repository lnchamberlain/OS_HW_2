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

#define SPACE 1600000

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
  //long long address;
  size_t size;
  struct node *next;
  struct node *prev;
}node;

node *head = NULL;
/*Begin ordered list functions */

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
    else{
      node->prev->next = node->next;
    }
    if(node->next){
      node->next->prev = node->prev;
    }
    //Do we need to call free here?
}
  void insertNode(node *node){
    node->prev = NULL;
    node->next = NULL;
    //If empty list or if address of node is less than address of current head, set head to node
    if((head == NULL) || (long long) head > (long long)node){
      if(head){
	head->prev = node;
      }
      node->next = head;
      head = node;
    }
    else{
      node *currentNode = head;
      //Iterate over list until either the end is hit or the address of node is greater than currentNode
      while((currentNode->next) && (long long)currentNode->next < (long long)currentNode){
	currentNode = currentNode->next;
      }
      //Update pointers, node is inserted after currentNode
      node->next = currentNode->next;
      node->prev = currentNode;
      currentNode->next = node;
    }
  }
  /*mergeBlocks iterates over the ordered list and looks to see if any of the blocks are consecutive, and if they are, merge them into one large block */ 
  void mergeBlocks(){
    node* currentNode = head;
    long long currentAddress, nextAddress;
    while(currentNode->next){
      currentAddress = (long long)currentNode;
      nextAddress = (long long) currentNode->next;
      //As each node is of type node, they have a header with that node variables that is of size sizeof(node). This must be added to the sizes of each node to account for this information
      
      //To see if consecutive, start at currentAddress, add the size of the current node, and then add the size of the node type to get to the next block. 
      if(currentAddress + currentNode->size + sizeof(node) == nextAddress){
	//Here the two nodes are consecutive
	//Update size by adding the length of next node + the size of the type node
	currentNode->size += currentNode->next->size + sizeof(node);
	currentNode->next = currentNode->next->next;
	//If not at end of list, update prev pointer
	if(currentNode->next){
	  currentNode->next->prev = currentNode;
	}
	else{
	  break;
	}
      }
      currentNode = currentNode->next;
    }

  }

/* End of your helper functions */

/* Start of the actual malloc/calloc/realloc/free functions */

void __free_impl(void *);

void *__malloc_impl(size_t size) {
  /* STUB */  
  int m = 1;
  // size_t temp = size;
  
  // Determine the number of nodes demanded for the given size.
  while (size >= SPACE*m) {
	  m++;
  }
  
  // Loop to remove nodes from the List of free space.
  int i;
  for (i=0; i < m; i++) {
	  // Break if head is NULL; More should probably be done to handle this.
	  if (head == NULL) {
		  break;
	  }
	  removeNode(head);
  }
  
  
  return NULL;
}

void *__calloc_impl(size_t nmemb, size_t size) {
  /* STUB */
  // compute total space needed by multiplying the number of elements by the size of an element.
  size_t totalSpace = nmeb * size;
  // int m = 1;
  
  // Check for overflow and return error if it occurs.
  if (totalSpace < 0) {
	  return NULL;
  }
  
  // We should just need to call malloc.
  
  _malloc_impl(totalSpace);
  
/*   while (totalSpace >= SPACE*m) {
	  m++;
  }
  
  int i;
  for (i = 0; i < m; i++) {
	  if (head == NULL) {
		  break;
	  }
	  
	  removeNode(head);
  } */
  
  return NULL;  
}

void *__realloc_impl(void *ptr, size_t size) {
  /* STUB */
  return NULL;  
}

void __free_impl(void *ptr) {
  /* STUB */
  void *p = mmap(NULL, SPACE, PROT_READ | PROT_WRITE, -1, 0);
  
  insertNode(p);
}

/* End of the actual malloc/calloc/realloc/free functions */
