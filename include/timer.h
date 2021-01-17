#pragma once

#include <time.h>

#define NS_SEC  1000000000ULL

typedef uint64_t ns_t;

struct timestamp {
	uint16_t sec_msb;
	uint32_t sec_lsb;
	uint32_t ns;
} __attribute__((packed));
typedef struct timestamp tstamp_t;

static inline ns_t ns_add(ns_t ns1, ns_t ns2)
{
	return ns1 + ns2;
}

static inline ns_t ns_sub(ns_t ns1, ns_t ns2)
{
	return ns1 - ns2;
}

static inline int ns_cmp(ns_t ns1, ns_t ns2)
{
	return ns1 == ns2 ? 0 : ns1 > ns2 ? 1 : -1;
}

static inline void ns_gettime(ns_t *ns)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);  // 6475.630167683
	// clock_gettime(CLOCK_REALTIME, &t);  // 1610703922.324663055

	*ns = (ns_t)(t.tv_sec * NS_SEC + t.tv_nsec);
}

static inline ns_t tstamp_to_ns(tstamp_t *ts)
{
	ns_t sec = ((ns_t)ntohs(ts->sec_msb) << 32) + (ns_t)ntohl(ts->sec_lsb);
	return (sec * NS_SEC + (ns_t)ts->ns);
}