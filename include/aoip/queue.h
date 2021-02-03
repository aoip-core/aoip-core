#pragma once

struct aoip_queue_slot {
	uint16_t len;      /* number of valid data for each slot */
	uint8_t *data;     /* data point */  
};
typedef struct aoip_queue_slot queue_slot_t;

/* audio data buffer ring structure */
struct aoip_queue {
	uint32_t head;	/* write point */
	uint32_t tail;	/* read point */
	uint32_t mask;	/* bit mask of ring buffer */

	queue_slot_t *slot;
};
typedef struct aoip_queue queue_t;

static inline bool queue_empty(const queue_t *q)
{
	return q->head == q->tail;
}

static inline bool queue_full(const queue_t *q)
{
	return ((q->head + 1) & q->mask) == q->tail;
}

static inline void queue_read_next(queue_t *q)
{
	q->tail = (q->tail + 1) & q->mask;
}

static inline void queue_write_next(queue_t *q)
{
	q->head = (q->head + 1) & q->mask;
}

static inline const queue_slot_t *
queue_read_ptr(const queue_t *q)
{
	return &q->slot[q->tail];
}

static inline queue_slot_t *
queue_write_ptr(const queue_t *q)
{
	return &q->slot[q->head];
}
