#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>

#include "cactus.h"
#include "avl.h"
#include "commonC.h"
#include "hashTableC.h"

/*
 * The script outputs a maf file containing all the block in a net and its descendants.
 */

char *formatSequenceHeader(Sequence *sequence) {
    const char *sequenceHeader = sequence_getHeader(sequence);
    if (strlen(sequenceHeader) > 0) {
        char *cA = st_malloc(sizeof(char) * (1 + strlen(sequenceHeader)));
        sscanf(sequenceHeader, "%s", cA);
        return cA;
    } else {
        return cactusMisc_nameToString(sequence_getName(sequence));
    }
}

static void getMAFBlockP2(Segment *segment, FILE *fileHandle) {
    assert(segment != NULL);
    Sequence *sequence = segment_getSequence(segment);
    if (sequence != NULL) {
        char *sequenceHeader = formatSequenceHeader(sequence);
        int32_t start;
        if (segment_getStrand(segment)) {
            start = segment_getStart(segment) - sequence_getStart(sequence);
        } else { //start with respect to the start of the reverse complement sequence
            start = (sequence_getStart(sequence) + sequence_getLength(sequence)
                    - 1) - segment_getStart(segment);
        }
        int32_t length = segment_getLength(segment);
        char *strand = segment_getStrand(segment) ? "+" : "-";
        int32_t sequenceLength = sequence_getLength(sequence);
        char *instanceString = segment_getString(segment);
        fprintf(fileHandle, "s\t%s\t%i\t%i\t%s\t%i\t%s\n", sequenceHeader,
                start, length, strand, sequenceLength, instanceString);
        free(instanceString);
        free(sequenceHeader);
    }
}

static void getMAFBlockP(Segment *segment, FILE *fileHandle) {
    int32_t i;
    for (i = 0; i < segment_getChildNumber(segment); i++) {
        getMAFBlockP(segment_getChild(segment, i), fileHandle);
    }
    getMAFBlockP2(segment, fileHandle);
}

void getMAFBlock(Block *block, FILE *fileHandle) {
    /*
     * Outputs a MAF representation of the block to the given file handle.
     */
    if (block_getInstanceNumber(block) > 0) {
        if(block_getRootInstance(block) != NULL) {
            /* Get newick tree string with internal labels and no unary events */
            char *newickTreeString = block_makeNewickString(block, 1, 0);
            assert(newickTreeString != NULL);
            fprintf(fileHandle, "a score=%i tree='%s'\n", block_getLength(block)
                    * block_getInstanceNumber(block), newickTreeString);
            free(newickTreeString);
            assert(block_getRootInstance(block) != NULL);
            getMAFBlockP(block_getRootInstance(block), fileHandle);
            fprintf(fileHandle, "\n");
        }
        else {
            /* Get newick tree string with internal labels and no unary events */
            fprintf(fileHandle, "a score=%i\n", block_getLength(block) * block_getInstanceNumber(block));
            Block_InstanceIterator *iterator = block_getInstanceIterator(block);
            Segment *segment;
            while((segment = block_getNext(iterator)) != NULL) {
                getMAFBlockP2(segment, fileHandle);
            }
            block_destructInstanceIterator(iterator);
            fprintf(fileHandle, "\n");
        }
    }
}

void getMAFs(Flower *net, FILE *fileHandle) {
    /*
     * Outputs MAF representations of all the block sin the net and its descendants.
     */

    //Make MAF blocks for each block
    Flower_BlockIterator *blockIterator = flower_getBlockIterator(net);
    Block *block;
    while ((block = flower_getNextBlock(blockIterator)) != NULL) {
        getMAFBlock(block, fileHandle);
    }
    flower_destructBlockIterator(blockIterator);

    //Call child nets recursively.
    Flower_GroupIterator *groupIterator = flower_getGroupIterator(net);
    Group *group;
    while ((group = flower_getNextGroup(groupIterator)) != NULL) {
        if (!group_isLeaf(group)) {
            getMAFs(group_getNestedFlower(group), fileHandle); //recursive call.
        }
    }
    flower_destructGroupIterator(groupIterator);
}

void makeMAFHeader(Flower *net, FILE *fileHandle) {
    fprintf(fileHandle, "##maf version=1 scoring=N/A\n");
    char *cA = eventTree_makeNewickString(flower_getEventTree(net));
    fprintf(fileHandle, "# cactus %s\n\n", cA);
    free(cA);
}

void usage() {
    fprintf(stderr, "cactus_mafGenerator, version 0.2\n");
    fprintf(stderr, "-a --logLevel : Set the log level\n");
    fprintf(stderr, "-c --netDisk : The location of the net disk directory\n");
    fprintf(stderr, "-d --netName : The name of the net (the key in the database)\n");
    fprintf(stderr, "-e --outputFile : The file to write the MAFs in.\n");
    fprintf(stderr, "-h --help : Print this help screen\n");
}

int main(int argc, char *argv[]) {
    CactusDisk *netDisk;
    Flower *net;

    /*
     * Arguments/options
     */
    char * logLevelString = NULL;
    char * netDiskName = NULL;
    char * netName = NULL;
    char * outputFile = NULL;

    ///////////////////////////////////////////////////////////////////////////
    // (0) Parse the inputs handed by genomeCactus.py / setup stuff.
    ///////////////////////////////////////////////////////////////////////////

    while (1) {
        static struct option long_options[] = { { "logLevel",
                required_argument, 0, 'a' }, { "netDisk", required_argument, 0,
                'c' }, { "netName", required_argument, 0, 'd' }, {
                "outputFile", required_argument, 0, 'e' }, { "help",
                no_argument, 0, 'h' }, { 0, 0, 0, 0 } };

        int option_index = 0;

        int key = getopt_long(argc, argv, "a:c:d:e:h", long_options,
                &option_index);

        if (key == -1) {
            break;
        }

        switch (key) {
            case 'a':
                logLevelString = stString_copy(optarg);
                break;
            case 'c':
                netDiskName = stString_copy(optarg);
                break;
            case 'd':
                netName = stString_copy(optarg);
                break;
            case 'e':
                outputFile = stString_copy(optarg);
                break;
            case 'h':
                usage();
                return 0;
            default:
                usage();
                return 1;
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    // (0) Check the inputs.
    ///////////////////////////////////////////////////////////////////////////

    assert(netDiskName != NULL);
    assert(netName != NULL);
    assert(outputFile != NULL);

    //////////////////////////////////////////////
    //Set up logging
    //////////////////////////////////////////////

    if (logLevelString != NULL && strcmp(logLevelString, "INFO") == 0) {
        st_setLogLevel(ST_LOGGING_INFO);
    }
    if (logLevelString != NULL && strcmp(logLevelString, "DEBUG") == 0) {
        st_setLogLevel(ST_LOGGING_DEBUG);
    }

    //////////////////////////////////////////////
    //Log (some of) the inputs
    //////////////////////////////////////////////

    st_logInfo("Net disk name : %s\n", netDiskName);
    st_logInfo("Net name : %s\n", netName);
    st_logInfo("Output MAF file : %s\n", outputFile);

    //////////////////////////////////////////////
    //Load the database
    //////////////////////////////////////////////

    netDisk = cactusDisk_construct(netDiskName);
    st_logInfo("Set up the net disk\n");

    ///////////////////////////////////////////////////////////////////////////
    // Parse the basic reconstruction problem
    ///////////////////////////////////////////////////////////////////////////

    net = cactusDisk_getFlower(netDisk, cactusMisc_stringToName(netName));
    st_logInfo("Parsed the top level net of the cactus tree to check\n");

    ///////////////////////////////////////////////////////////////////////////
    // Recursive check the nets.
    ///////////////////////////////////////////////////////////////////////////

    int64_t startTime = time(NULL);
    FILE *fileHandle = fopen(outputFile, "w");
    makeMAFHeader(net, fileHandle);
    getMAFs(net, fileHandle);
    fclose(fileHandle);
    st_logInfo("Got the mafs in %i seconds/\n", time(NULL) - startTime);

    ///////////////////////////////////////////////////////////////////////////
    // Clean up.
    ///////////////////////////////////////////////////////////////////////////

    cactusDisk_destruct(netDisk);

    return 0;
}
