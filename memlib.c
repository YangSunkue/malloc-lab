/*
 * memlib.c - a module that simulates the memory system.  Needed because it 
 *            allows us to interleave calls from the student's malloc package 
 *            with the system's malloc package in libc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"
#include "config.h"

// 이 파일엔 정의되어 있지 않고 외부에서 정의되어 있다는 뜻
extern int mm_init(void);
extern void *mm_malloc(size_t size);
extern void mm_free(void *ptr);

/* Private global variables */
static char *mem_start_brk; // 힙의 첫 바이트, 교재는 mem_heap
static char *mem_brk;  // EpBlock 1워드 뒤
static char *mem_max_addr;

// /* private variables */
// static char *mem_start_brk;  /* points to first byte of heap */ 
// static char *mem_brk;        /* points to last byte of heap */
// static char *mem_max_addr;   /* largest legal heap address */ 

/* 
 * mem_init - initialize the memory system model
 */
void mem_init(void)
{
    /* allocate the storage we will use to model the available VM */
    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) {
	fprintf(stderr, "mem_init_vm: malloc error\n");
	exit(1);
    }

    mem_brk = (char *)mem_start_brk;                  /* heap is empty initially */
    mem_max_addr = (char *)(mem_start_brk + MAX_HEAP);  /* max legal heap address */
}

/* 
 * mem_deinit - free the storage used by the memory system model
 */
void mem_deinit(void)
{
    free(mem_start_brk);
}

/*
 * mem_reset_brk - reset the simulated brk pointer to make an empty heap
 */
void mem_reset_brk()
{
    mem_brk = mem_start_brk;
}

/* 
 * mem_sbrk - simple model of the sbrk function. Extends the heap 
 *    by incr bytes and returns the start address of the new area. In
 *    this model, the heap cannot be shrunk.
 */
// 실제로 힙을 확장하는 함수 (brk를 늘리기)
// 기존 brk 포인터를 리턴한다
void *mem_sbrk(int incr) 
{
    // 기존 brk 저장해두기 ( 리턴해야됨 )
    char *old_brk = mem_brk;

    // 음수가 요청되거나(힙줄이기) 가용된 힙 주소를 벗어났을 경우 -1 리턴
    if ( (incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
	errno = ENOMEM;
	fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
	return (void *)-1;
    }

    // 입력받은 bytes 만큼 brk 늘리기
    mem_brk += incr;
    // 기존 brk 리턴
    return (void *)old_brk;
}

/*
 * mem_heap_lo - return address of the first heap byte
 */
void *mem_heap_lo()
{
    return (void *)mem_start_brk;
}

/* 
 * mem_heap_hi - return address of last heap byte
 */
void *mem_heap_hi()
{
    return (void *)(mem_brk - 1);
}

/*
 * mem_heapsize() - returns the heap size in bytes
 */
size_t mem_heapsize() 
{
    return (size_t)(mem_brk - mem_start_brk);
}

/*
 * mem_pagesize() - returns the page size of the system
 */
size_t mem_pagesize()
{
    return (size_t)getpagesize();
}
