#include "ObjectManager.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

// STRUCTS
typedef struct Node {
    Ref id;      
    ulong size;     // Memory is stored for this block
    ulong refCnt;   // Number of pointers that reference this block
    ulong start;    // The point in the buffer where this block starts
    struct Node *next;    // Next block in the buffer
} node;

// VARIABLES
static void *buffer1; // Double buffers share MEMORY_SIZE bytes
static void *buffer2; 
static void *targetBuffer;
static void *inactiveBuffer;
static int freeptr;  // index of the first unused byte of memory
static node *head;
static Ref ref;

// INVARIANT
void validateState(node *curr); // verify the state of the objectManager

// compact()
//
// This method defragments the targetBuffer so that
// all of the objects that have no references are 
// removed from the buffer and the linked list
// We use a double buffer technique to move over the
// contents that aren't garbage to a new, clean, 
// defragmented buffer and use that one instead of the
// fragmented one. There is no need to clear fragmented buffers.
void compact()
{
    assert(inactiveBuffer != NULL && targetBuffer != NULL && buffer2 != NULL && buffer1 != NULL);

    ulong numObjects = 0;
    ulong numBytes = 0;
    ulong bytesCollected = 0;
    freeptr = 0;

    node* curr = head;
    node* temp;

    // iterate over all objects, collect garbage, update buffer
    while(curr != NULL) { 
       
        validateState(curr);

        if(curr->refCnt == 0) {
      
            bytesCollected += curr->size;
            temp = curr;
            curr = curr->next;
            free(temp);
            head = curr;

        } else if(curr->refCnt > 0) { // non-garbage object
     
            //update curr->next to non-garbage object
            temp = curr->next;
            
            // iterate over list until reach end of list, or found non-garbage object
            while(temp != NULL && temp->refCnt == 0) {

                    validateState(temp);

                    // found a garbage object (refCnt != 0)
                    // might as well collect it
                    node *garbage = temp;
                    bytesCollected += garbage->size;
                    temp = temp->next; // skip over garbage object
                    free(garbage);
            }
            // if temp reaches end of list without finding non-garbage object,
            // the curr->next == NULL, if it does find non-garbage object,
            // curr->next points to that object.
           curr->next = temp;
       
            // copy the non-garbage object contents from old(target) buffer to new(inactive) buffer
            for(int i = 0; i < curr->size; i++) {
                ((uchar *)inactiveBuffer)[freeptr + i] = ((uchar *)targetBuffer)[curr->start + i];
            }
        
            validateState(curr);

            curr->start = freeptr;
            freeptr += curr->size;
            numObjects++;
            numBytes += curr->size;
            
            validateState(curr);
            curr = curr->next;
        }
    }
    
    // Garbage collection complete, switch target buffers
    void *temp2 = targetBuffer;
    targetBuffer = inactiveBuffer;
    inactiveBuffer = temp2;

    printf("\nGarbage collector statistics:\n");
    printf("objects: %lu  bytes in use: %lu  bytes freed: %lu\n\n", numObjects, numBytes, bytesCollected);

    assert(bytesCollected >= 0 && numBytes >= 0 && numObjects >= 0  && freeptr >= 0);
    assert(inactiveBuffer != NULL && targetBuffer != NULL && buffer2 != NULL && buffer1 != NULL);
}

// insertObject(ulong size)
//
// This method inserts a new block at the start of our
// linked list containing all of the blocks in the buffer.
// We can assume that the block will always be inserted in the list.
// If there is not enough space in the buffer, we call compact()
// to try to defragment the buffer to make space.  We will then only
// insert if there is enough space in the buffer after defragmentation.
//
// RETURNS : The id that can be used to access the memory of the object.
Ref insertObject(ulong size ){
    // call compact if there is not enough space
    // how do we know if there is enough space in the buffer?
    assert(size > 0); // assume we always create new block

    Ref id = NULL_REF;
    
    /*
    Need to malloc the block to put it on the 
    heap memory, or else it will be erased when
    the function terminates
    */

    node* block = (node*)malloc(sizeof(node));
    block->id = ref;
    block->refCnt = 1; // initial ref count
    block->size = size;
    block->start = freeptr;
    
    validateState(block);

    if((freeptr + size) > MEMORY_SIZE){
        // Defragment buffer to try to make space
        compact();
    }

    if((freeptr + size) < MEMORY_SIZE) { 
        // insert new block at the head of the list
        block->next = head;
        head = block;
        
        freeptr += size;
        id = ref++;
        assert(head != NULL && ref > 1 && id == ref - 1 && freeptr >= block->size);
    
    }
    else {
    
        printf("\nObject could not be inserted in buffer, there is not enough space in the buffer. \n");
    
    }
    assert(id >= NULL_REF && freeptr >=0 && block->id > 0 && block->size > 0);
    
    return id;
}

// retrieveObject(Ref ref)
//
// Given a reference, we find the block in the linked list
// with the given ref.
//
// RETURNS : Pointer to the object with refs space in the buffer
void *retrieveObject(Ref ref ){
    // find the block with the given ref in the linked list
    // return the address of targetBuffer[start].
    assert(targetBuffer != NULL);

    void* obj = NULL;
    node* curr; 
    curr = head;

    // iterate through list to find ref
    while(curr != NULL){

        validateState(curr);
            
        if(curr->id == ref && curr->refCnt > 0){
            obj = &((uchar *)targetBuffer)[curr->start];
        }

        curr = curr->next;
    }

    assert(targetBuffer != NULL);
    // returns null if the object does not exist in the buffer
    
    if(obj == NULL) {
        printf("Invalid reference exception with reference %lu, returned NULL, segfault incoming!\n", ref);
        fflush(stdout);
        // since we are returning NULL, program will seg fault if we try to use obj
        // make sure user gets message before segfault by flushing stdout
    }
        
    return obj;
}

// update our index to indicate that we have another reference to the given object
void addReference( Ref ref ){
    assert(buffer1 != NULL && buffer2 != NULL);
    if(ref < 1) {
        printf("\n Error.  Trying to add ref to invalid ID %lu.\n", ref);
    } else {
        node* curr; 
        curr = head;

        // iterate through the list
        while(curr != NULL){

            validateState(curr);

            if(curr->id == ref){
                curr->refCnt += 1;           
                validateState(curr);
                break;
            }


            curr = curr->next;
        }
    }
}

// update our index to indicate that a reference is gone
void dropReference( Ref ref ){
    
    node* curr; 
    curr = head;

    // iterate through the list
    while(curr != NULL){
        
        validateState(curr);

        if(curr->id == ref){
            if(curr->refCnt > 0) {
                curr->refCnt -= 1;   
            }
            validateState(curr);
            break;
        }

        curr = curr->next;
        }
}

// initialize the object manager
void initPool(){
    assert(head == NULL && buffer1 == NULL && buffer2 == NULL);

    // initialize both buffers
    buffer1 = malloc(sizeof(uchar) * MEMORY_SIZE);
    buffer2 = malloc(sizeof(uchar) * MEMORY_SIZE);
    targetBuffer = buffer1;
    inactiveBuffer = buffer2;
    freeptr = 0; // start of the new buffer
    ref = 1;

    assert(freeptr == 0 && head == NULL && buffer1 != NULL && buffer2 != NULL);
}

// destroyPool()
// 
// This method deallocates all of the memory that was
// dynamically allocated through the use of the 
// object manager.  This includes the linked list
// and both buffers used to manage the objects.
// 
// We make sure that both buffers have memory allocated
// to them to ensure that we dont free null pointers.
void destroyPool(){
    // free all the blocks in the linked list
    assert(buffer1 != NULL && buffer2 != NULL);
    node* curr = head;
    node* next;
    
    while(curr != NULL) {
        validateState(curr);
        next = curr->next;
        free(curr);
        curr = next;
    }
    
    head = NULL;

    // free both buffers
    free(buffer1);
    free(buffer2);
    buffer1 = NULL;
    buffer2 = NULL;

    assert(head == NULL && buffer1 == NULL && buffer2 == NULL);
}

// dumpPool()
//
// This method prints out information about each
// object in the buffer in FIFO order.
// There are no pre or post conditions for printing.
void dumpPool(){
    
    node *curr;
    curr = head;

    printf("\nCurrent Pool\n");

    if(head == NULL) {
        printf("There are no objects currently in the pool\n");
    } else {
        while(curr != NULL){
            validateState(curr);
            printf("\nReference ID : %lu", curr->id);
            printf("\nStarting address: %lu", curr->start);
            printf("\nSize : %lu bytes", curr->size);
            printf("\nReference count : %lu\n\n", curr->refCnt);
            curr = curr->next;
        }
    }
}

// validateState
//  
// validate that the state of the node *curr is valid
// called whenever we use nodes.
void validateState(node *curr) {
    // if we are using nodes, buffers should hold memory
    assert(buffer1 != NULL && buffer2 != NULL && targetBuffer != NULL && inactiveBuffer != NULL);
    // valid values for contents of the current node
    // putting into seperate assertions to be better able to identify what
    // exactly was wrong with the node if assertion fails.
    assert(curr != NULL);
    assert(curr->id > 0 && curr->id <= ref);
    assert(curr->start >= 0 && curr->start < MEMORY_SIZE);
    assert(curr->size > 0 && curr->size < MEMORY_SIZE);
    assert(curr->refCnt >= 0);
    assert(&((uchar*)targetBuffer)[curr->start] != NULL);
    assert(&((uchar*)targetBuffer)[curr->size - 1] != NULL);
    assert(curr != curr->next);
}
