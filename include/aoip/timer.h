#ifndef _timer_h_
#define _timer_h_

#define NS_SEC  1000000000L

typedef uint64_t tv_t;

typedef struct {
	uint64_t sec;
	uint32_t ns;
} tstamp_t;

static inline tv_t tv_add(tv_t ts1, tv_t ts2)
{
	return ts1 + ts2;
}

static inline tv_t tv_sub(tv_t ts1, tv_t ts2)
{
	return ts1 - ts2;
}

static inline int tv_cmp(tv_t ts1, tv_t ts2)
{
	return ts1 == ts2 ? 0 : ts1 > ts2 ? 1 : -1;
}

static inline tv_t tstamp_to_tv(tstamp_t tstamp)
{
	return tstamp.sec * NS_SEC + tstamp.ns;
}

static inline tstamp_t tv_to_tstamp(tv_t ts)
{
	tstamp_t tstamp;

	tstamp.sec = ts / NS_SEC;
	tstamp.ns = ts % NS_SEC;

	return tstamp;
}

static inline void tv_gettime(tv_t *t)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	*t = (tv_t)(ts.tv_sec * NS_SEC + ts.tv_nsec);
}

#endif  /* _timer_h_ */

