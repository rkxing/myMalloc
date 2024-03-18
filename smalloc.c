#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "smalloc.h"

#define PAGE_SIZE 4096
#define WORD_SIZE 8
#define HEADER_SIZE 24

typedef struct Block {
    int size; // size in bytes of whole block (header + payload)
    int allocated; // 0 if free, 1 if allocated
    struct Block* next;
    struct Block* prev;
} Block;

void* heap;
Block* head;

/*
 * my_init() is called one time by the application program to to perform any
 * necessary initializations, such as allocating the initial heap area.
 * size_of_region is the number of bytes that you should request from the OS using
 * mmap().
 * Note that you need to round up this amount so that you request memory in
 * units of the page size, which is defined as 4096 Bytes in this project.
 */
int my_init(int size_of_region) {
    if (size_of_region % PAGE_SIZE != 0) {
        size_of_region += PAGE_SIZE - (size_of_region % PAGE_SIZE);
    }

    int fd = open("/dev/zero", O_RDWR);

    heap = mmap(NULL, size_of_region, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, fd, 0);
    close(fd);

    if (heap == MAP_FAILED) {
        return -1;
    }

    head = (Block*) heap;
    head->size = size_of_region;
    head->allocated = 0;
    head->next = NULL;
    head->prev = NULL;

    return 0;
}


/*
 * smalloc() takes as input the size in bytes of the payload to be allocated and
 * returns a pointer to the start of the payload. The function returns NULL if
 * there is not enough contiguous free space within the memory allocated
 * by my_init() to satisfy this request.
 */
void *smalloc(int size_of_payload, Malloc_Status *status) {
    // word alignment
    int padding = 0;
    if (size_of_payload % WORD_SIZE != 0) {
        padding = WORD_SIZE - (size_of_payload % WORD_SIZE);
    }

    int block_size = size_of_payload + padding + HEADER_SIZE;

    // find first fit
    Block* curr = head;
    status->hops = 0;
    while (curr && (curr->size < block_size)) {
        (status->hops)++;
        curr = curr->next;
    }

    if (!curr) { // no free block fits
        status->success = 0;
        status->payload_offset = -1;
        status->hops = -1;
        return NULL;
    }

    int leftover = curr->size - block_size;
    if (leftover >= HEADER_SIZE) { // needs fragmentation
        Block* new_block = (void*) curr + block_size;
        new_block->size = leftover;
        new_block->allocated = 0;
        new_block->next = curr->next;
        new_block->prev = curr->prev;
        if (curr->prev) {
            curr->prev->next = new_block;
        }
        if (curr->next) {
            curr->next->prev = new_block;
        }
        curr->next = new_block;
    }
    else {
        padding += leftover;
        block_size = size_of_payload + padding + HEADER_SIZE;
    }

    curr->size = block_size;
    curr->allocated = 1;

    // realign free list
    if (!curr->prev) {
        head = curr->next;
    }
    else {
        curr->prev->next = curr->next;
    }
    if (curr->next) {
        curr->next->prev = curr->prev;
    }

    void* payload_start = (void*) curr + HEADER_SIZE;
    status->success = 1;
    status->payload_offset = (unsigned long) payload_start - (unsigned long) heap;

    return payload_start;
}


/*
 * sfree() frees the target block. "ptr" points to the start of the payload.
 * NOTE: "ptr" points to the start of the payload, rather than the block (header).
 */
void sfree(void *ptr)
{
    if (!ptr) {
        return;
    }

    Block* new = (void*) ptr - HEADER_SIZE;
    new->allocated = 0;

    Block* curr = head;
    if (!curr) { // start new list
        new->prev = NULL;
        new->next = NULL;
        head = new;
        return;
    }

    while (curr->next && (new > curr)) {
        curr = curr->next;
    }

    if (!curr->next && (new > curr)) { // insert at end
        curr->next = new;
        new->next = NULL;
        new->prev = curr;
    }
    else if (!curr->prev && (new < curr)) { // insert at front
        new->prev = NULL;
        new->next = curr;
        curr->prev = new;
        head = new;
    }
    else {
        new->next = curr;
        curr->prev->next = new;
        new->prev = curr->prev;
        curr->prev = new;
    }

    // coalesce next
    if (new->next && (new->next == (void*) new + new->size)) {
        new->size += new->next->size;
        if (new->next->next) {
            new->next->next->prev = new;
        }
        new->next = new->next->next;
    }
    // coalesce prev
    if (new->prev && (new == (void*) new->prev + new->prev->size)) {
        new->prev->size += new->size;
        new->prev->next = new->next;
        if (new->next) {
            new->next->prev = new->prev;
        }
    }
}