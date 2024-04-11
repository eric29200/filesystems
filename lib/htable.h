#ifndef _HTABLE_H_
#define _HTABLE_H_

#include <stdint.h>
#include <string.h>

#define GOLDEN_RATIO_32				0x61C88647
#define GOLDEN_RATIO_64				0x61C8864680B583EBull

#define htable_entry(ptr, type, member)		container_of(ptr, type, member)

/*
 * Hash table struct.
 */
struct htable_link {
	struct htable_link *	next;
	struct htable_link **	pprev;
};

/*
 * Hash function.
 */
static inline uint32_t hash_32(uint32_t val, uint32_t bits)
{
	return (val * GOLDEN_RATIO_32) % (1 << bits);
}

/*
 * Hash function.
 */
static inline uint32_t hash_64(uint64_t val, uint32_t bits)
{
	return (val * GOLDEN_RATIO_64) % (1 << bits);
}

/* Hash function.
 */
static inline uint32_t hash_str(const char *val, uint32_t bits)
{
	uint32_t h = (uint32_t) *val;

	if (h)
		for (++val; *val; ++val)
			h = (h << 5) - h + (uint32_t) *val;

	return h % (1 << bits);
}

/*
 * Init a hash table.
 */
static inline void htable_init(struct htable_link **htable, uint32_t bits)
{
	memset(htable, 0, sizeof(struct htable_link *) * (1 << bits));
}

/*
 * Get an element from a hash table.
 */
static inline struct htable_link *htable_lookup32(struct htable_link **htable, uint32_t key, uint32_t bits)
{
	return htable[hash_32(key, bits)];
}

/*
 * Get an element from a hash table.
 */
static inline struct htable_link *htable_lookup64(struct htable_link **htable, uint64_t key, uint32_t bits)
{
	return htable[hash_64(key, bits)];
}

/*
 * Get an element from a hash table.
 */
static inline struct htable_link *htable_lookupstr(struct htable_link **htable, const char *key, uint32_t bits)
{
	return htable[hash_str(key, bits)];
}

/*
 * Insert an element into a hash table.
 */
static inline void htable_insert32(struct htable_link **htable, struct htable_link *node, uint32_t key, uint32_t bits)
{
	int i;

	i = hash_32(key, bits);
	node->next = htable[i];
	node->pprev = &htable[i];
	if (htable[i])
		htable[i]->pprev = (struct htable_link **) node;
	htable[i] = node;
}

/*
 * Insert an element into a hash table.
 */
static inline void htable_insert64(struct htable_link **htable, struct htable_link *node, uint64_t key, uint32_t bits)
{
	int i;

	i = hash_64(key, bits);
	node->next = htable[i];
	node->pprev = &htable[i];
	if (htable[i])
		htable[i]->pprev = (struct htable_link **) node;
	htable[i] = node;
}

/*
 * Insert an element into a hash table.
 */
static inline void htable_insertstr(struct htable_link **htable, struct htable_link *node, const char *key, uint32_t bits)
{
	int i;

	i = hash_str(key, bits);
	node->next = htable[i];
	node->pprev = &htable[i];
	if (htable[i])
		htable[i]->pprev = (struct htable_link **) node;
	htable[i] = node;
}

/*
 * Delete an element from a hash table.
 */
static inline void htable_delete(struct htable_link *node)
{
	struct htable_link *next = node->next;
	struct htable_link **pprev = node->pprev;

	if (pprev)
		*pprev = next;
	if (next)
		next->pprev = pprev;
}

#endif
