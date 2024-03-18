# myMalloc

Custom implementation of malloc() and free(), using 8 byte words and 4KB pages.

Uses explicit doubly-linked list of free blocks and first-fit method to choose blocks.
