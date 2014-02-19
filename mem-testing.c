#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MEM_TESTING_C

#include "mem-testing.h"

/* the number of allocated memory size
when programming exit, the size must be zero */
static size_t allocated_size = 0;

/* get the memheader */
static struct memheader* get_memheader(void *ptr) {

    struct memheader *header;

    header = (struct memheader*)ptr - 1;

    return header;
}

/* using std malloc and update the size */
void *mem_test_malloc(size_t size) {

    struct memheader* header;
    void *ptr;

    header = malloc(sizeof(*header) + size);

    if (header == NULL) {
        return NULL;
    }

    header->size = size;

    /* update the allocated size */
    allocated_size += size;

    /* skip the header and return the mem for using */
    return header + 1;
}

/* using std free and update the size */
void* mem_test_free(void *ptr) {

    struct memheader* header;
    size_t block_size;

    if (ptr == NULL) {
        return;
    }

    /* get the header and retrieve the size */
    header = get_memheader(ptr);

    /* get the block size */
    block_size = header->size;

    free(header);

    /* update the allocated size */
    allocated_size -= block_size;
}

/* realloc the mem */
void *mem_test_realloc(void *ptr, size_t size) {

    struct memheader *header;
    void *new_ptr;
    size_t copy_size;

    /* alloc a new block */
    new_ptr = mem_test_malloc(size);

    if (new_ptr == NULL) {
        return NULL;
    }
    
    if (ptr != NULL) {
        header = get_memheader(ptr);

        copy_size = header->size;

        memcpy(new_ptr, ptr, copy_size);

        mem_test_free(ptr);
    }

    return new_ptr;
}

/* calloc the block */
void *mem_test_calloc(size_t nmemb, size_t size) {

    void *res;
    size_t total_bytes = nmemb * size;

    /* alloc a block */
    res = mem_test_malloc(total_bytes);

    if (res == NULL) {
        return NULL;
    }

    memset(res, 0, total_bytes);

    return res;
}

/* string dup */
char *mem_test_strdup(const char *string) {

    char *res;
    
    /* add the extra one char to store the '\0' */
    res = mem_test_malloc(strlen(string) + 1);

    if (res == NULL) {
        return NULL;
    }

    strcpy(res, string);

    return res;
}

/*get the allocated size */
size_t mem_test_get_allocated() {

    return allocated_size;
}

