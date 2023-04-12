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
    "SWJungle 6b week06 5",
    /* First member's full name */
    "Seo-Hyeon Lee",
    /* First member's email address */
    "apple121626@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE   (1<<12) /* Extend heap by this amount (bytes), 2**12 */

#define MAX(x, y) ((x) > (y) ? (x) : (y)) /* macro that returns bigger val */

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))  /* combines size(bytes) of the block and allocated/free code, for header and footer */

/* Read and write a word at address p */
#define GET(p)      (*(unsigned int *)(p))          /* macro that reads and returns the word referenced by p */
#define PUT(p, val) (*(unsigned int *)(p) = (val))  /* macro that stores val(bytes size, allocation code) in the word pointed at by p */

/* Read the size and allocated field from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7) /* reads header(or footer), ignores last 3 bits, only getting the size(bytes) of the block */
#define GET_ALLOC(p)    (GET(p) & 0x1)  /* reads header(or footer), returns 1(True) if allocated, otherwise 0(False)*/

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)    ((char *)(bp) - WSIZE)                      /* get start address of payload, return address of the block header */
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) /* get start address of payload, return address of the block header */

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) /* get start address of payload, return next block pointer */
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) /* get start address of payload, return previous block pointer */

extern int mm_init(void);
extern void *mm_malloc(size_t size);
extern void mm_free(void *ptr);

/* my function prototypes */
static void *extend_heap(size_t);
static void *coalesce(void *);
static void *(find_fit(size_t));
static void place(void *, size_t);

static void *heap_listp;

#define SUCC_BLKP(bp)   *((char **)(bp))            /* get start address of a block's payload, return the address of payload of predecessor block */
#define PRED_BLKP(bp)   *((char **)((bp) + WSIZE))    /* get start address of a block's payload, return the address of payload of successor block */
#define FREE_LIST(class)    ((char **)(heap_listp + (class)*WSIZE))

int splice_out(void *);
int splice_in(void *bp);
int get_class(size_t size); // 사이즈 클래스 찾기

/* 
 * mm_init - initialize the malloc package. return 0 if successful, -1 otherwise.
 */
int mm_init(void)
{
    // printf("init\n");
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(16*WSIZE)) == (void *)-1) /* extends heap by 16 words and returns start address of new area. if fails, return -1 */
        return -1;
    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(7*DSIZE, 1));  /* Prologue header */
    for (int i = 2; i < 14; i++) {                  /* initialize SUCC for each class */
        PUT(heap_listp + (i*WSIZE), (unsigned int)NULL);
    }
    PUT(heap_listp + (14*WSIZE), PACK(7*DSIZE, 1));  /* Prologue footer */
    PUT(heap_listp + (15*WSIZE), PACK(0, 1));        /* Epilogue header */
    heap_listp += (2*WSIZE);                        /* heap_listp (always) points prologue block */

    if (extend_heap(32) == NULL)   // higher util index(+2), why?
        return -1;
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)   // if fails
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // printf("\nmalloc(%d): \t", size);
    size_t asize;       /* Adjusted block size */
    size_t extendsize;  /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious request */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. (bytes) */
    if (size <= DSIZE)
        asize = 2 * DSIZE;      // minimum block size: 2 * DSIZE = 16 bytes(4 words) -> allocated block has footer too?
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); // calculate right block size for the data
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) { // if usable free block is found
        place(bp, asize);   // place new block of size asize on the free block(분할)
        return bp; // return start address of the new block
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)   // if fails: return NULL
        return NULL;
    place(bp, asize);   // place new block of size asize on the new free block
    return bp;  // return start address of the new allocated block
}

/*
 * mm_free - Freeing a block does nothing. (changes header and footer code)
 */
void mm_free(void *ptr) // ptr is start address of the block we want to free
{
    // printf("\nfree ");
    size_t size = GET_SIZE(HDRP(ptr));   // get the size(bytes) of current block

    PUT(HDRP(ptr), PACK(size, 0));       // update header: says free!
    PUT(FTRP(ptr), PACK(size, 0));       // update footer: says free!
    coalesce(ptr);                       // check and coalesce adjecent blocks
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    // if made other functions well, it would work good without changing any(maybe)
    void *oldptr = ptr; // initiallize oldptr to given ptr
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);   // newptr is destination memory location
    if (newptr == NULL)
      return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);   // oldptr (payload?) size?
    copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;  // only payload size
    if (size < copySize)
      copySize = size;  // copySize is smaller one
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/* new fuctions */

/* Extend the empty heap with a free block of size of given words. if fails, return NULL */
static void *extend_heap(size_t words)
{
    // printf("extend_heap \n");
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; // calculate right size (bytes)
    if ((long)(bp = mem_sbrk(size)) == -1) // bp is start address of new area. unlike the first if in mm_init, use (long)
        return NULL;
    
    /* Initialize free block header/footer and the epiligue header */
    PUT(HDRP(bp), PACK(size, 0));           /* make header for the new Free block */
    PUT(FTRP(bp), PACK(size, 0));           /* make footer for the new Free block */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* make New epilogue header */

    /* Coalesce if the previous block was free, returns start address of new coalesced block */
    return coalesce(bp);
}

/* merges adjacent free blocks and returns start pointer of the new free block */
static void *coalesce(void *bp)
{
    // printf("coalesce \n");
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // previous block is allocated: 1, else: 0
    // // printf("prev_alloc:%d\n", prev_alloc);
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // next block is allocated: 1, else: 0
    // // printf("next_alloc:%d\n", next_alloc);
    size_t size = GET_SIZE(HDRP(bp));                   // size of current block
    // // printf("size:%d\n", size);

    if (prev_alloc && next_alloc)               /* Case 1, only current block is free */
        {}  // no need

    else if (prev_alloc && !next_alloc) {       /* Case 2, current and next block is free */
        splice_out(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));  // update size of current block: add next block size
        PUT(HDRP(bp), PACK(size, 0));           // update header
        PUT(FTRP(bp), PACK(size, 0));           // update footer which originally was next block's footer
    }

    else if (!prev_alloc && next_alloc) {       /* Case 3, current and prev block is free */
        splice_out(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));  // update size of current block: add prev block size
        PUT(FTRP(bp), PACK(size, 0));           // update footer
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));// update header which originally was prev block's header
        bp = PREV_BLKP(bp);                     // update current block pointer to original prev block's pointer
    }

    else {                                      /* Case 4 */
        splice_out(PREV_BLKP(bp));
        splice_out(NEXT_BLKP(bp));
        // update size of current block: add prev and next blocks' size 
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));// update header which originally was prev block's header
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));// update footer which originally was next block's footer
        bp = PREV_BLKP(bp);                     // update current block pointer to original prev block's pointer
    }

    splice_in(bp);

    return bp;  // returns current block(newly coalesced block)'s start address
}

/* finds free block to place the new allocation, by first-fit */
static void *find_fit(size_t asize) // asize: bytes
{
    // printf("find_fit ");
    // TODO: first fit for LIFO explicit list
    for (int class = get_class(asize); class <= 11; class++){
        // void *bp = free_listp[class]; // points first block in free list
        void *bp = *FREE_LIST(class);
        // 처음부터 프롤로그SUCC->SUCC->SUCC->NULL일 까지 돌면서 맞는 사이즈 있으면 그 주소 반환
        while (bp != NULL) {
            if (GET_SIZE(HDRP(bp)) >= asize)
                return bp;
            bp = SUCC_BLKP(bp);
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    // printf("place ");
    // place requested block at the beginning of the current free block(bp),
    // splitting only if the size of the remainder would equal or exeed the minimum block size
    size_t currentsize = GET_SIZE(HDRP(bp));
    splice_out(bp);
    if (currentsize - asize >= 2 * DSIZE) { // the remainder would equal or exeed the minimum block size, split
        PUT(HDRP(bp), PACK(asize, 1));              // update new allocated header, which was original header
        PUT(FTRP(bp), PACK(asize, 1));              // update new allocated footer
        
        bp = NEXT_BLKP(bp);                         // update bp to point to remainder free block
        PUT(HDRP(bp), PACK(currentsize-asize, 0));  // update header of remainder free block
        PUT(FTRP(bp), PACK(currentsize-asize, 0));  // update footer of remainder free block, which was original footer

        splice_in(bp); // splice new remainder free block into the free list
    } else { // don't split
        // update header and footer, say allocated!
        PUT(HDRP(bp), PACK(currentsize, 1));
        PUT(FTRP(bp), PACK(currentsize, 1));
    }
}

int splice_out(void *bp)
{
    // printf("splice_out \n");
    // printf("(removing block size: %d, remove from class %d) \n", GET_SIZE(HDRP(bp)), get_class(GET_SIZE(HDRP(bp))));
    /* splice out the block(bp) from the list */
    // update predecessor block's successor <- block's successor
    SUCC_BLKP(PRED_BLKP(bp)) = SUCC_BLKP(bp);
    // update successor block's predecessor <- block's predecessor
    if (SUCC_BLKP(bp) != NULL)
        PRED_BLKP(SUCC_BLKP(bp)) = PRED_BLKP(bp);

    // // update predecessor block's successor <- block's successor
    // if (PRED_BLKP(bp) != NULL) {
    //     SUCC_BLKP(PRED_BLKP(bp)) = SUCC_BLKP(bp);
    // } else {
    //     free_listp[get_class(GET_SIZE(HDRP(bp)))] = SUCC_BLKP(bp);
    // }
    // // update successor block's predecessor <- block's predecessor
    // if (SUCC_BLKP(bp) != NULL)
    //     PRED_BLKP(SUCC_BLKP(bp)) = PRED_BLKP(bp);
    

    return 0;
}

int splice_in(void *bp)
{
    // printf("splice_in \n");
    int class = get_class(GET_SIZE(HDRP(bp)));
    // printf("(inserting block size: %d, class %d) \n", GET_SIZE(HDRP(bp)), class);
    void *pred = FREE_LIST(class);

    while ((void *)SUCC_BLKP(pred) == NULL) {
        if (SUCC_BLKP(pred) == NULL) {  // meet the end of the list
            // initialize newly inserting block
            SUCC_BLKP(bp) = NULL;
            PRED_BLKP(bp) = pred;
            // splice successor and newly inserting block
            SUCC_BLKP(pred) = bp;
            return 0;
        }
        pred = SUCC_BLKP(pred); // splice predecessor and newly inserting block
    }
    SUCC_BLKP(bp) = SUCC_BLKP(pred);
    PRED_BLKP(bp) = pred;
    PRED_BLKP(SUCC_BLKP(pred)) = bp;
    SUCC_BLKP(pred) = bp;
    // // update PRED, SUCC of new free block                  
    //     PRED_BLKP(bp) = NULL;               // update PRED of new free block
    //     SUCC_BLKP(bp) = free_listp[class];         // initialize SUCC of new free block

    //     if (free_listp[class] != NULL) {           // if the list was not empty
    //         PRED_BLKP(free_listp[class]) = bp;     // update PRED of previously first free block(previously SUCC of root)
    //     }
    //     free_listp[class] = bp;                // update root(free_listp)
    return 0;
}

int get_class(size_t asize)
{
    int class = 0;
    int size = 4;
    while (size < asize) {
        size *= 2;
        class++;
        if (class >= 11) break;
    }
    return class;
}

