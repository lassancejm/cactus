#ifndef CACTUS_CHAIN_PRIVATE_H_
#define CACTUS_CHAIN_PRIVATE_H_

#include "cactusGlobals.h"

struct _chain {
	Name name;
	Flower *flower;
	Link *link;
	int32_t linkNumber;
	int32_t chainIndex;
};

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
//Chain functions.
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////

/*
 * Constructs a chain, which in turn holds links.
 */
Chain *chain_construct2(Name name, Flower *flower);

/*
 * Add the link to the chain.
 */
void chain_addLink(Chain *chain, Link *childLink);

/*
 * Write a binary representation of the chain to the write function.
 */
void chain_writeBinaryRepresentation(Chain *chain, void (*writeFn)(const void * ptr, size_t size, size_t count));

/*
 * Loads a flower into memory from a binary representation of the flower.
 */
Chain *chain_loadFromBinaryRepresentation(void **binaryString, Flower *flower);

/*
 * Get a static instance (from the heap) with the name set.
 */
Chain *chain_getStaticNameWrapper(Name name);

/*
 * Sets the flower containing the chain.
 */
void chain_setFlower(Chain *chain, Flower *flower);

#endif
