#include "VirtualMemory.h"
#include "PhysicalMemory.h"

#define PATH_TO_REPLACE 1
#define EMPTY_TABLE 2


/*
 * Clears the table at index frameIndex
 */
void clearTable(uint64_t frameIndex)
{
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}


/*
 * Initialize the virtual memory
 */
void VMinitialize()
{
    clearTable(0);
}


/*
 * Deletes the frame at frameIndex and creates a pointer to it at ptrIndex.
 */
void newBlankFrame(word_t& frameIndex, uint64_t& ptrIndex)
{
    PMwrite(ptrIndex, frameIndex);
    for (unsigned int i = 0; i < PAGE_SIZE; ++i)
    {
        PMwrite((frameIndex << OFFSET_WIDTH) + i, 0);
    }
}


/*
 * Evicts a frame from the physical memory and deletes the pointer to it.
 */
word_t deleteFrame(uint64_t& toDeletePtr, uint64_t& virtualAddr)
{
    word_t toDelete;
    PMread(toDeletePtr, &toDelete);
    PMwrite(toDeletePtr, 0);
    PMevict(toDelete, virtualAddr);
    return toDelete;
}


/*
 * Finds an address in order to to place a new frame in, recursively. Starts at the root of the page table, and saves
 * the current frame at curFrameIndex. depth keeps the current depth at the page table. lastEmptyTable is used to
 * determine whether the table isn't full. All other parameters are used to choose a frame to replace if it is needed.
 */
int treeDFS(uint64_t curFrameIndex, word_t* maxUsedFrame, uint64_t* lastEmptyTable, uint64_t* toReplace,
            unsigned int* highestScore, uint64_t* virtualAddrRep, uint64_t* emptyTablePtr, uint64_t toAvoid,
            unsigned int curScore, uint64_t curVirtualAddr, unsigned int depth)
{
    unsigned int score = WEIGHT_EVEN;
    score = (curFrameIndex & 1U) ? WEIGHT_ODD : WEIGHT_EVEN;
    if (depth == TABLES_DEPTH)
    {
        score += ((curVirtualAddr >> OFFSET_WIDTH) & 1U) ? WEIGHT_ODD : WEIGHT_EVEN;
        if (curScore + score > *highestScore || (curScore + score == *highestScore &&
                                                 (curVirtualAddr >> OFFSET_WIDTH) < *virtualAddrRep))
        {
            *highestScore = curScore + score;
            return PATH_TO_REPLACE;
        }
        return 0;
    }

    word_t candFrameIndex;
    bool isEmpty = true;
    for (unsigned int i = 0; i < PAGE_SIZE; i++)
    {
        PMread(curFrameIndex * PAGE_SIZE + i, &candFrameIndex);
        if (candFrameIndex != 0)
        {
            isEmpty = false;
            if (candFrameIndex > *maxUsedFrame)
            {
                *maxUsedFrame = candFrameIndex;
            }
            int caseInd = treeDFS(candFrameIndex, maxUsedFrame, lastEmptyTable, toReplace, highestScore, virtualAddrRep,
                                  emptyTablePtr, toAvoid, curScore + score, curVirtualAddr << OFFSET_WIDTH, depth + 1);
            if (caseInd == PATH_TO_REPLACE)
            {
                *virtualAddrRep = curVirtualAddr;
                *toReplace = curFrameIndex * PAGE_SIZE + i;
            }
            if (caseInd == EMPTY_TABLE)
            {
                *emptyTablePtr = curFrameIndex * PAGE_SIZE + i;
            }
        }
        curVirtualAddr++;
    }
    if (isEmpty && curFrameIndex != toAvoid)
    {
        *lastEmptyTable = curFrameIndex;
        return EMPTY_TABLE;
    }
    return 0;
}


/*
 * Traverses the page table tree to find the data at virtualAddress, it writes it to targetAddr. Fetches the data from
 * the main memory if it isn't in the cache.
 */
void treeLogic(uint64_t& virtualAddress, uint64_t* targetAddr)
{
    uint64_t pageAddr = 0;
    word_t nextAddr, maxFrameIndex = 0;
    uint64_t indexPtr, newTable = 0;
    uint64_t emptyTable, dataFrame, curVirtualAddr, emptyTablePtr;
    unsigned int highestScore;
    uint64_t mask =
            ((1ULL << OFFSET_WIDTH) - 1) << (OFFSET_WIDTH * TABLES_DEPTH);
    for (unsigned int i = 0; i < TABLES_DEPTH; i++)
    {
        emptyTable = emptyTablePtr = dataFrame = highestScore = 0;
        curVirtualAddr = -1;
        indexPtr = pageAddr + ((virtualAddress & mask) >> ((TABLES_DEPTH - i) * OFFSET_WIDTH));
        mask = mask >> OFFSET_WIDTH;
        PMread(indexPtr, &nextAddr);
        if (nextAddr == 0)
        {
            treeDFS(0, &maxFrameIndex, &emptyTable, &dataFrame, &highestScore, &curVirtualAddr, &emptyTablePtr,
                    newTable, 0, 0, 0);
            if (maxFrameIndex + 1 < NUM_FRAMES)
            {
                nextAddr = maxFrameIndex + 1;
            }
            else if (emptyTable > 0)
            {
                nextAddr = (word_t) emptyTable;
                PMwrite(emptyTablePtr, 0);
            }
            else
            {
                nextAddr = deleteFrame(dataFrame, curVirtualAddr);
            }
            newBlankFrame(nextAddr, indexPtr);
            if (i + 1 == TABLES_DEPTH)
            {
                PMrestore(nextAddr, virtualAddress >> OFFSET_WIDTH);
            }
        }
        newTable = nextAddr;
        pageAddr = nextAddr * PAGE_SIZE;
    }
    indexPtr = virtualAddress & mask;
    *targetAddr = pageAddr + indexPtr;
}


/* reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value)
{
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
    {
        return 0;
    }
    uint64_t targetAddr;
    treeLogic(virtualAddress, &targetAddr);
    PMread(targetAddr, value);
    return 1;  // Achshli hayakar
}


/* writes a word to the given virtual address
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite(uint64_t virtualAddress, word_t value)
{
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
    {
        return 0;
    }
    uint64_t targetAddr;
    treeLogic(virtualAddress, &targetAddr);
    PMwrite(targetAddr, value);
    return 1;  // Achshli hayakar
}
