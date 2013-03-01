/*
 * blastAlignmentLib.c
 *
 *  Created on: 30 Jan 2012
 *      Author: benedictpaten
 */

#include "bioioC.h"
#include "cactus.h"
#include "sonLib.h"
#include "pairwiseAlignment.h"

/*
 * Converting coordinates of pairwise alignments
 */

static void convertCoordinatesP(char **contig, int32_t *start, int32_t *end) {
    stList *attributes = fastaDecodeHeader(*contig);
    //Decode attributes
    int32_t startP;
    int32_t i = sscanf((const char *) stList_peek(attributes), "%i", &startP);
    (void) i;
    assert(i == 1);
    free(stList_pop(attributes));
    //Now relabel attributes
    free(*contig);
    *contig = fastaEncodeHeader(attributes);
    stList_destruct(attributes);
    *start = *start + startP;
    *end = *end + startP;
}

void convertCoordinatesOfPairwiseAlignment(struct PairwiseAlignment *pairwiseAlignment) {
    checkPairwiseAlignment(pairwiseAlignment);
    convertCoordinatesP(&pairwiseAlignment->contig1, &pairwiseAlignment->start1, &pairwiseAlignment->end1);
    convertCoordinatesP(&pairwiseAlignment->contig2, &pairwiseAlignment->start2, &pairwiseAlignment->end2);
    checkPairwiseAlignment(pairwiseAlignment);
}

/*
 * Routine reads in chunk up a set of sequences into overlapping sequence files.
 */

static int64_t chunkRemaining;
static FILE *chunkFileHandle = NULL;
static const char *chunksDir = NULL;
static int32_t chunkNo = 0;
static char *tempChunkFile = NULL;
static int64_t chunkSize;
static int64_t chunkOverlapSize;

static int32_t fn(char *fastaHeader, int32_t start, char *sequence, int32_t seqLength, int32_t length) {
    if (chunkFileHandle == NULL) {
        tempChunkFile = stString_print("%s/%i", chunksDir, chunkNo++);
        chunkFileHandle = fopen(tempChunkFile, "w");
    }

    int32_t i = 0;
    fastaHeader = stString_copy(fastaHeader);
    while (fastaHeader[i] != '\0') {
        if (fastaHeader[i] == ' ' || fastaHeader[i] == '\t') {
            fastaHeader[i] = '\0';
            break;
        }
        i++;
    }
    fprintf(chunkFileHandle, ">%s|%i\n", fastaHeader, start);
    free(fastaHeader);
    assert(length <= chunkSize);
    assert(start >= 0);
    if (start + length > seqLength) {
        length = seqLength - start;
    }
    assert(length > 0);
    char c = sequence[start + length];
    sequence[start + length] = '\0';
    fprintf(chunkFileHandle, "%s\n", &sequence[start]);
    sequence[start + length] = c;

    return length;
}

void finishChunkingSequences() {
    if (chunkFileHandle != NULL) {
        fclose(chunkFileHandle);
        fprintf(stdout, "%s\n", tempChunkFile);
        free(tempChunkFile);
        tempChunkFile = NULL;
        chunkFileHandle = NULL;
    }
}

static int32_t fn2(int32_t i, int32_t seqLength) {
    //Update remaining portion of the chunk.
    assert(seqLength >= 0);
    i -= seqLength;
    if (i <= 0) {
        finishChunkingSequences();
        return chunkSize;
    }
    return i;
}

void processSequenceToChunk(const char *fastaHeader, const char *sequence, int32_t length) {
    int32_t j, k, l;

    if (length > 0) {
        j = fn((char *) fastaHeader, 0, (char *) sequence, length, chunkRemaining);
        chunkRemaining = fn2(chunkRemaining, j);
        while (length - j > 0) {
            //Make the non overlap file
            k = fn((char *) fastaHeader, j, (char *) sequence, length, chunkRemaining);
            chunkRemaining = fn2(chunkRemaining, k);

            //Make the overlap file
            l = j - chunkOverlapSize / 2;
            if (l < 0) {
                l = 0;
            }
            chunkRemaining = fn2(chunkRemaining, fn((char *) fastaHeader, l, (char *) sequence, length, chunkOverlapSize));
            j += k;
        }
    }
}

void setupToChunkSequences(int64_t chunkSize2, int64_t overlapSize2, const char *chunksDir2) {
    chunkSize = chunkSize2;
    assert(chunkSize > 0);
    chunkOverlapSize = overlapSize2;
    assert(chunkOverlapSize >= 0);
    chunksDir = chunksDir2;
    chunkNo = 0;
    chunkRemaining = chunkSize;
    chunkFileHandle = NULL;
}

/*
 * Get the flowers in a file.
 */

int32_t writeFlowerSequences(Flower *flower, void(*processSequence)(const char *, const char *, int32_t), int32_t minimumSequenceLength) {
    Flower_EndIterator *endIterator = flower_getEndIterator(flower);
    End *end;
    int32_t sequencesWritten = 0;
    while ((end = flower_getNextEnd(endIterator)) != NULL) {
        End_InstanceIterator *instanceIterator = end_getInstanceIterator(end);
        Cap *cap;
        while ((cap = end_getNext(instanceIterator)) != NULL) {
            cap = cap_getStrand(cap) ? cap : cap_getReverse(cap);
            Cap *cap2 = cap_getAdjacency(cap);
            assert(cap2 != NULL);
            assert(cap_getStrand(cap2));

            if (!cap_getSide(cap)) {
                assert(cap_getSide(cap2));
                int32_t length = cap_getCoordinate(cap2) - cap_getCoordinate(cap) - 1;
                assert(length >= 0);
                if (length >= minimumSequenceLength) {
                    Sequence *sequence = cap_getSequence(cap);
                    assert(sequence != NULL);
                    char *string = sequence_getString(sequence, cap_getCoordinate(cap) + 1, length, 1);
                    char *header = stString_print("%s|%i", cactusMisc_nameToStringStatic(cap_getName(cap)), cap_getCoordinate(cap) + 1);
                    processSequence(header, string, strlen(string));
                    free(string);
                    free(header);
                    sequencesWritten++;
                }
            }
        }
        end_destructInstanceIterator(instanceIterator);
    }
    flower_destructEndIterator(endIterator);
    return sequencesWritten;
}

static FILE *sequenceFileHandle;
static const char *tempSequenceFile;
static void writeSequenceInFile(const char *fastaHeader, const char *sequence, int32_t length) {
    if (sequenceFileHandle == NULL) {
        sequenceFileHandle = fopen(tempSequenceFile, "w");
    }
    fprintf(sequenceFileHandle, ">%s\n%s\n", fastaHeader, sequence);
}

int32_t writeFlowerSequencesInFile(Flower *flower, const char *tempFile, int32_t minimumSequenceLength) {
    sequenceFileHandle = NULL;
    tempSequenceFile = tempFile;
    int32_t sequencesWritten = writeFlowerSequences(flower, writeSequenceInFile, minimumSequenceLength);
    if (sequenceFileHandle != NULL) {
        fclose(sequenceFileHandle);
    }
    return sequencesWritten;
}