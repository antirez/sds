#ifndef MEM_TESTING_H
#define MEM_TESTING_H

/* define mem test function malloc,free,realloc,calloc,strdup
 * using stdlib function*/
#ifndef MEM_TESTING_C
#define malloc   mem_test_malloc
#define free     mem_test_free
#define realloc  mem_test_realloc
#define calloc   mem_test_calloc
#define strdup   mem_test_strdup
#endif

/* the memheader store the block size */
struct memheader {
    size_t size;
};

void* mem_test_malloc(size_t size);

void* mem_test_free(void* ptr);

void* mem_test_realloc(void* ptr, size_t size);

void* mem_test_calloc(size_t nmemb, size_t size);

char* mem_test_strdup(const char *s);

/* get the allocated memory size */
size_t mem_test_get_allocated();

#endif
