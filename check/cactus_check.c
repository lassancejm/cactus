#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>

#include "cactus.h"
#include "commonC.h"

/*
 * The script checks the nets are structured as we expect, essentially by
 * calling net_check for each net in the tree. We do a couple of other tests also for the normalisation phase.
 */

/*
 * Normalisation phase. We haven't made these checks part of the api checks yet, as I don't know if we'll keep them
 * forever.
 */

static void checkTreeIsTerminalNormalised(Flower *net) {
    /*
     * A cactus tree is terminally normalised if all leaf nets are terminal.
     */
    if (flower_isLeaf(net)) {
        assert(flower_getBlockNumber(net) == 0);
        assert(flower_isTerminal(net));
        //The following are defensive checks.
        Group *group;
        Flower_GroupIterator *iterator = flower_getGroupIterator(net);
        while ((group = flower_getNextGroup(iterator)) != NULL) {
            assert(group_isLeaf(group));
        }
        flower_destructGroupIterator(iterator);
    }
}

static void checkChainsAreMaximal(Flower *net) {
    /*
     * Checks that each chain is maximal and consistently named..
     */
    Group *parentGroup = flower_getParentGroup(net);
    if(parentGroup != NULL) {
        End *end;
        Flower_EndIterator *endIterator = flower_getEndIterator(net);
        while((end = flower_getNextEnd(endIterator)) != NULL) {
            assert(end_getOrientation(end));
            if(end_isStubEnd(end) && end_isAttached(end)) { //is an attached stub end (inherited from a higher level)
                Link *link = group_getLink(end_getGroup(end));
                if(link != NULL) { //then the net must be a terminal net and the link is a copy of the one in the parent..
                    Chain *chain = link_getChain(link);
                    assert(flower_isLeaf(net));
                    assert(chain_getLength(chain) == 1);
                }
            }
        }
        flower_destructEndIterator(endIterator);
    }
}

static void checkNetIsNotRedundant(Flower *net) {
    /*
     * Checks that if the net is not a leaf that it contains blocks.
     */
    assert(flower_builtBlocks(net));
    if(flower_getBlockNumber(net) == 0 && flower_getGroupNumber(net) == 1) {
        assert(flower_isLeaf(net));
    }
}

static void checkNetIsNotEmpty(Flower *net) {
    /*
     * Checks that if the net contains at least one end, unless it is the parent problem
     * and the whole reconstruction is empty.
     */
    if(flower_getParentGroup(net) != NULL) {
        assert(flower_getEndNumber(net) > 0);
    }
}

static void checkGroupsNotEmpty(Flower *net) {
    /*
     * Checks that each group contains at least one end.
     */
    Group *group;
    Flower_GroupIterator *groupIt = flower_getGroupIterator(net);
    while((group = flower_getNextGroup(groupIt)) != NULL) {
        assert(group_getEndNumber(group) > 0);
    }
}



/*
 * Random other checks.
 */

static void checkBasesAccountedFor(Flower *net) {
    /*
     * Checks all the bases in a net end up in child net or a nested net.
     */
    int64_t totalBases = flower_getTotalBaseLength(net);
    int64_t blockBases = 0.0;
    int64_t childBases = 0.0;
    Flower_BlockIterator *blockIterator = flower_getBlockIterator(net);
    Block *block;
    Block_InstanceIterator *segmentIterator;
    Segment *segment;
    while ((block = flower_getNextBlock(blockIterator)) != NULL) {
        segmentIterator = block_getInstanceIterator(block);
        while ((segment = block_getNext(segmentIterator)) != NULL) {
            if (segment_getSequence(segment) != NULL) {
                blockBases += segment_getLength(segment);
            }
        }
        block_destructInstanceIterator(segmentIterator);
    }
    flower_destructBlockIterator(blockIterator);
    Flower_GroupIterator *iterator = flower_getGroupIterator(net);
    Group *group;
    while ((group = flower_getNextGroup(iterator)) != NULL) {
        int64_t size = (int64_t) group_getTotalBaseLength(group);
        if (group_getNestedFlower(group) != NULL) {
            assert(!group_isLeaf(group));
            assert(flower_getTotalBaseLength(group_getNestedFlower(group)) == size);
        } else {
            assert(group_isLeaf(group));
        }
        assert(size >= 0);
        childBases += size;
    }
    flower_destructGroupIterator(iterator);
    if (blockBases + childBases != totalBases) {
        fprintf(stderr,
                "Got %i block bases, %i childBases and %i total bases\n",
                (int) blockBases, (int) childBases, (int) totalBases);
    }
    assert(blockBases + childBases == totalBases);
}

static void checkNets(Flower *net, int32_t recursive) {
    flower_check(net);
    checkNetIsNotEmpty(net);
    checkGroupsNotEmpty(net);
    checkBasesAccountedFor(net);
    //Normalisation checks..
    checkTreeIsTerminalNormalised(net);
    checkChainsAreMaximal(net);
    checkNetIsNotRedundant(net);

    //Call problem recursively
    if (recursive) {
        Flower_GroupIterator *iterator = flower_getGroupIterator(net);
        Group *group;
        while ((group = flower_getNextGroup(iterator)) != NULL) {
            if (!group_isLeaf(group)) {
                checkNets(group_getNestedFlower(group), 1);
            }
        }
        flower_destructGroupIterator(iterator);
    }
}

void usage() {
    fprintf(stderr, "cactus_tree, version 0.2\n");
    fprintf(stderr, "-a --logLevel : Set the log level\n");
    fprintf(stderr, "-c --netDisk : The location of the net disk directory\n");
    fprintf(stderr, "-e --recursive : Check all nets recursively\n");
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
    int32_t recursive = 0;

    ///////////////////////////////////////////////////////////////////////////
    // (0) Parse the inputs handed by genomeCactus.py / setup stuff.
    ///////////////////////////////////////////////////////////////////////////

    while (1) {
        static struct option long_options[] = { { "logLevel",
                required_argument, 0, 'a' }, { "netDisk", required_argument, 0,
                'c' }, { "recursive", no_argument, 0, 'e' }, { "help",
                no_argument, 0, 'h' }, { 0, 0, 0, 0 } };

        int option_index = 0;

        int key =
                getopt_long(argc, argv, "a:c:eh", long_options, &option_index);

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
            case 'e':
                recursive = 1;
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

    //////////////////////////////////////////////
    //Load the database
    //////////////////////////////////////////////

    netDisk = cactusDisk_construct(netDiskName);
    st_logInfo("Set up the net disk\n");

    int32_t j;
    for (j = optind; j < argc; j++) {
        const char *netName = argv[j];
        st_logInfo("Processing the net named: %s", netName);

        ///////////////////////////////////////////////////////////////////////////
        // Parse the basic reconstruction problem
        ///////////////////////////////////////////////////////////////////////////

        net = cactusDisk_getFlower(netDisk, cactusMisc_stringToName(netName));
        st_logInfo("Parsed the top level net of the cactus tree to check\n");

        ///////////////////////////////////////////////////////////////////////////
        // Recursive check the nets.
        ///////////////////////////////////////////////////////////////////////////

        checkNets(net, recursive);
        st_logInfo("Checked the nets/\n");
    }

    ///////////////////////////////////////////////////////////////////////////
    // Clean up.
    ///////////////////////////////////////////////////////////////////////////

    cactusDisk_destruct(netDisk);

    return 0;
}
