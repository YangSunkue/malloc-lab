// 묵시적 코드
/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Krafton Jungle",
    /* First member's full name */
    "Yang Sunkue",
    /* First member's email address */
    "ysk9526@gmail.com",
    /* Second member's full name (leave blank if none) */
    "John",
    /* Second member's email address (leave blank if none) */
    "Smith"
};

///////// 굳이 안써도 될거같다 //////////
// 최소 할당 사이즈 ( 16 해야하는거 아닌가..? )
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

// size + (ALIGNMENT - 1) 한 후 and연산으로 끝 3개를 빼줌으로써 8의 배수로 올림한다
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// size_t에 저장된 size를 8의 배수로 올림한다
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
///////// 굳이 안써도 될거같다 //////////

// 워드, 더블워드 크기 지정
#define WSIZE 4
#define DSIZE 8
// 힙 메모리 초기 가용 크기 지정, 힙 크기를 늘릴 때도 이 단위로 늘린다(일반적으로)
#define CHUNKSIZE (1<<12)

// x, y중 큰 값을 리턴한다. (같으면 y 리턴)
#define MAX(x, y) ((x) > (y)? (x) : (y))

// 입력받은 주소에, allocated값을 추가하여 리턴한다. ( 1을 더하거나 더하지 않는다 )
#define PACK(size, alloc) ((size) | (alloc))

// p주소에 존재하는 값을 리턴한다
#define GET(p) (*(unsigned int *)(p))
// p주소에 값을 입력한다
#define PUT(p, val) (*(unsigned int *)(p) = val)

// p주소의 allocated 값을 제외한 size만을 리턴한다
#define GET_SIZE(p) (GET(p) & ~0x7)
// p주소의 size를 제외한 allocated 값만을 리턴한다
#define GET_ALLOC(p) (GET(p) & 0x1)

// 블록 포인터를 입력하면, 해당 블록의 헤더 포인터를 리턴
#define HDRP(bp) ((char *)(bp) - WSIZE)
// 블록 포인터를 입력하면 해당 블록의 풋터 포인터를 리턴
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 블록 포인터를 입력하면, 다음 가용 블록의 블록 포인터를 가리킨다
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
// 블록 포인터를 입력하면, 이전 가용 블록의 블록 포인터를 가리킨다
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// extend_heap 전방 선언
static void *extend_heap(size_t words);
// coalesce 전방 선언
static void *coalesce(void *bp);
// find_fit 전방 선언
static void *find_fit(size_t asize);
// place 전방 선언
static void place(void *bp, size_t asize);

// heap_listp 선언
static char *heap_listp;


/* 
 * mm_init - initialize the malloc package.
 */
// 힙을 최초 가용하기 전, 틀을 만드는 함수
// 패딩 블록, 프롤로그 헤더/풋터, 에필로그 헤더를 추가한다
int mm_init(void)
{
    // 4블록(16바이트) 만큼 메모리를 가용한다. ( brk를 늘린다 )
    // heap_listp 에는 기존 brk주소가 리턴된다.
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        return -1;
    }
    PUT(heap_listp, 0); // Alignment 패딩 블록 ( 최소 정렬 기준 )
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // 프롤로그 헤더 8/1
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // 프롤로그 풋터 8/1
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); // 에필로그 헤더 0/1
    heap_listp += (2*WSIZE); // 이건 대체 왜 하는거..?

    // 진짜 힙 가용받으러 가기, CHUNKSIZE 만큼
    // WSIZE로 나누는 이유는, extend_heap 가면 8의 배수로 올려주는 과정이 있어서 미리 나눠주는 것임
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

// 가용 힙을 늘리는 함수
static void *extend_heap(size_t words) {

    char *bp;
    size_t size;

    // 입력받은 값을 8의 배수로 만들어, mem_sbrk 호출하여 힙 늘리기
    // brk값은 bp에 저장하고, 만약 늘리기 실패했으면 NULL 리턴
    // bp는 기존 에필로그 헤더의 1워드 뒤를 가리키고 있음
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;  // 짝수와 4를 곱하면 무조건 8의 배수이다.
    if((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    // 헤더, 풋터, 에필로그 헤더 설정
    // 새로 가용된 블록들은 분할되어 있지 않다
    PUT(HDRP(bp), PACK(size, 0)); // 기존 에필로그 블록을 새로운 가용 블록의 헤더로 설정한다
    PUT(FTRP(bp), PACK(size, 0)); // 새로운 가용 블록의 풋터를 설정한다
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새로운 에필로그 헤더 설정

    // 이전 힙의 마지막 블록이 가용 상태였을 경우 연결해주기 위한 coalesce 연결 함수 호출
    return coalesce(bp); 
}



// /* 
//  * mm_malloc - Allocate a block by incrementing the brk pointer.
//  *     Always allocate a block whose size is a multiple of the alignment.
//  */
// void *mm_malloc(size_t size)
// {
//     int newsize = ALIGN(size + SIZE_T_SIZE);
//     void *p = mem_sbrk(newsize);
//     if (p == (void *)-1)
// 	return NULL;
//     else {
//         *(size_t *)p = size;
//         return (void *)((char *)p + SIZE_T_SIZE);
//     }
// }



// 가용 리스트에서 블록 할당하기
void *mm_malloc(size_t size) {

    // 조정된 블록 크기를 저장할 변수
    size_t asize;
    // 적합하지 않은 경우 힙 확장할 크기
    size_t extendsize;
    char *bp;

    // 요청 size가 0이면 NULL 리턴
    if(size == 0) {
        return NULL;
    }

    // size가 8 이하면 16으로 설정 ( 8단위로 정렬되니까. 8바이트만 있으면 헤더풋터 넣으면 끝임 )
    if(size <= DSIZE) {
        asize = 2*DSIZE;
    }
    // size가 9이상이면 
    else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);  // 헤더풋터(더블워드) 추가한 후 8의 배수로 올림
    }

    // 적절한 가용 블록이 있다면
    if((bp = find_fit(asize)) != NULL) {
        // 블록 분할 후 리턴
        place(bp, asize);
        return bp;
    }

    // 적절한 가용 블록을 찾지 못했을 때 힙 늘린 후 할당하기
    // 기본으로 CHUNKSIZE로 늘리지만, 요구 size가 더 클 경우 그 size만큼 할당함
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL) {
        return NULL;
    }
    // 블록 분할 후 리턴
    place(bp, asize);
    return bp;
}


// 적절한 가용 블록 찾기 ( first fit )
static void *find_fit(size_t asize) {

    void *bp;

    // 힙의 처음부터, 에필로그 헤더 전 까지의 모든 블록을 확인
    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {

        // 미할당 + 요청 사이즈보다 크면 해당 블록을 리턴
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    // 맞는 블록이 없으면 NULL리턴
    return NULL;
}

// asize만큼 할당하고 남은 가용 블록이, 최소블록 기준보다 같거나 큰 경우에만 분할해야 한다(남은 블록이 16byte 이상)
static void place(void *bp, size_t asize) {

    // 후보 가용 블록의 사이즈
    size_t csize = GET_SIZE(HDRP(bp));

    // 필요한 만큼 할당한 후 남은 가용 블록이, 최소블록 기준(16)보다 같거나 큰 경우 분할
    if((csize - asize) >= (2*DSIZE)) {

        // 할당할 가용 블록의 헤더/풋터 설정
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1)); // 헤드 크기를 보고 풋터를 찾아간다

        // 할당하고 남은 블록의 헤더/풋터 설정
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    
    // 남을 가용 블록이 16보다 작을 경우, 남은 걸 통째로 할당해 버린다
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}



/*
 * mm_free - Freeing a block does nothing.
 */
// 블록 포인터를 입력받고 메모리를 해제한다
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    // allocated를 0으로 설정
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    // 해제했으니 연결하러 간다
    coalesce(bp);
}

static void *coalesce(void *bp) {

    // 이전, 다음 블록의 allocated 여부 확인
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // 현재 블록의 size 확인
    size_t size = GET_SIZE(HDRP(bp));

    /*
    size와 새로운 헤더/풋터를 설정하면,
    기존 헤더/풋터는 가비지 값처럼 인식되어 무의미해 진다. 일반 블록처럼 쓸 수 있다.
    */

    // case 1 : 앞뒤 모두 할당 상태일 때
    if(prev_alloc && next_alloc) {
        return bp;
    }

    // case 2 : 뒤만 미할당 상태일 때
    else if(prev_alloc && !next_alloc) {

        // 뒤 블록 사이즈를 더한다
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        // 더해진 사이즈와 allocated를 현재블록 헤더/뒷블록 풋터에 할당한다
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // case 3 : 앞만 미할당 상태일 때
    else if(!prev_alloc && next_alloc) {

        // 앞 블록 사이즈를 더한다
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        // 더해진 사이즈와 allocated를 앞블록 헤더/현재블록 풋터에 할당한다
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        // 앞과 합쳤으니, 블록 포인터를 앞쪽으로 옮겨준다.
        bp = PREV_BLKP(bp);
    }

    // case 4 : 앞/뒤 모두 미할당 상태일 때
    else {

        // 앞/뒤 블록 사이즈를 더한다
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        // 더해진 사이즈와 allocated를 앞블록 헤더/뒷블록 풋터에 할당한다
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        // 앞/뒤와 합쳤으니, 블록 포인터를 앞쪽으로 옮겨준다.
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
// 입력받은 size 만든다.
// size만큼 메모리를 새로 할당한 후 기존 값들을 복사하여 넣고, 새로운 포인터를 반환한다
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    // 입력받은 size만큼 메모리 할당
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    // 원래 메모리 블록의 크기를 구한다
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));

    // 새로운 크기가 이전 크기보다 작다면, 새로운 크기만큼만 데이터를 복사한다
    if (size < copySize)
      copySize = size;

    // 이전 메모리 블록에서 새로운 메모리 블록으로 데이터 복사
    // 데이터를 꽉 채워 사용 중이었는데 size를 줄인다면 데이터가 날아갈 수도 있음
    memcpy(newptr, oldptr, copySize);
    // 이전 메모리 해제
    mm_free(oldptr);
    // 새로운 메모리의 포인터를 반환
    return newptr;
}

