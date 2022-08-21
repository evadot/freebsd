/*-
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LINUXKPI_LINUX_DMA_FENCE_H_
#define	_LINUXKPI_LINUX_DMA_FENCE_H_

#include <linux/err.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/bitops.h>
#include <linux/kref.h>
#include <linux/sched.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>

struct dma_fence_ops;

struct dma_fence {
	spinlock_t *lock;
	const struct dma_fence_ops *ops;
	union {
		struct list_head cb_list;
		ktime_t timestamp;
		struct rcu_head rcu;
	};
	u64 context;
	u64 seqno;
	unsigned long flags;
	struct kref refcount;
	int error;
};

struct dma_fence_cb;

typedef void (*dma_fence_func_t)(struct dma_fence *fence,
				 struct dma_fence_cb *cb);

struct dma_fence_cb {
	struct list_head node;
	dma_fence_func_t func;
};

struct dma_fence_ops {
	bool use_64bit_seqno;

	const char * (*get_driver_name)(struct dma_fence *fence);
	const char * (*get_timeline_name)(struct dma_fence *fence);
	bool (*enable_signaling)(struct dma_fence *fence);
	bool (*signaled)(struct dma_fence *fence);
	signed long (*wait)(struct dma_fence *fence,
			    bool intr, signed long timeout);
	void (*release)(struct dma_fence *fence);
	void (*fence_value_str)(struct dma_fence *fence, char *str, int size);
	void (*timeline_value_str)(struct dma_fence *fence,
				   char *str, int size);
};

enum dma_fence_flag_bits {
	DMA_FENCE_FLAG_SIGNALED_BIT,
	DMA_FENCE_FLAG_TIMESTAMP_BIT,
	DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT,
	DMA_FENCE_FLAG_USER_BITS, /* must always be last member */
};

void lkpi_dma_fence_init(struct dma_fence *fence, const struct dma_fence_ops *ops,
    spinlock_t *lock, u64 context, u64 seqno);
#define	dma_fence_init(f, o, l, c, s)	lkpi_dma_fence_init(f, o, l, c, s)
void lkpi_dma_fence_release(struct kref *kref);
#define	dma_fence_release(k)		lkpi_dma_fence_release(k)
void lkpi_dma_fence_free(struct dma_fence *fence);
#define	dma_fence_free(f)		lkpi_dma_fence_free(f)
int lkpi_dma_fence_signal(struct dma_fence *fence);
#define	dma_fence_signal(f)		lkpi_dma_fence_signal(f)
int lkpi_dma_fence_signal_locked(struct dma_fence *fence);
#define	dma_fence_signal_locked(f)	lkpi_dma_fence_signal_locked(f)
signed long lkpi_dma_fence_default_wait(struct dma_fence *fence,
    bool intr, signed long timeout);
#define	dma_fence_default_wait(f, i, t)	lkpi_dma_fence_default_wait(f, i, t)
int lkpi_dma_fence_add_callback(struct dma_fence *fence,
    struct dma_fence_cb *cb, dma_fence_func_t func);
#define	dma_fence_add_callback(f, c, x)	lkpi_dma_fence_add_callback(f, c, x)
bool lkpi_dma_fence_remove_callback(struct dma_fence *fence,
    struct dma_fence_cb *cb);
#define	dma_fence_remove_callback(f, c)	lkpi_dma_fence_remove_callback(f, c)
void lkpi_dma_fence_enable_sw_signaling(struct dma_fence *fence);
#define	dma_fence_enable_sw_signaling(f)	\
	lkpi_dma_fence_enable_sw_signaling(f)
int lkpi_dma_fence_get_status(struct dma_fence *fence);
#define	dma_fence_get_status(f)		lkpi_dma_fence_get_status(f)
signed long lkpi_dma_fence_wait_timeout(struct dma_fence *fence,
    bool intr, signed long timeout);
#define	dma_fence_wait_timeout(f, i, t)	lkpi_dma_fence_wait_timeout(f, i, t)
signed long lkpi_dma_fence_wait_any_timeout(struct dma_fence **fences,
    uint32_t count, bool intr, signed long timeout, uint32_t *idx);
#define	dma_fence_wait_any_timeout(f, c, i, t, idx)	\
	lkpi_dma_fence_wait_any_timeout(f, c, i, t, idx)
struct dma_fence *lkpi_dma_fence_get_stub(void);
#define	dma_fence_get_stub()		lkpi_dma_fence_get_stub()
u64 lkpi_dma_fence_context_alloc(unsigned num);
#define	dma_fence_context_alloc(n)	lkpi_dma_fence_context_alloc(n)
void lkpi_dma_fence_put(struct dma_fence *fence);
#define	dma_fence_put(f)		lkpi_dma_fence_put(f)
struct dma_fence *lkpi_dma_fence_get(struct dma_fence *fence);
#define	dma_fence_get(f)		lkpi_dma_fence_get(f)
struct dma_fence *lkpi_dma_fence_get_rcu(struct dma_fence *fence);
#define	dma_fence_get_rcu(f)		lkpi_dma_fence_get_rcu(f)
struct dma_fence *lkpi_dma_fence_get_rcu_safe(struct dma_fence __rcu **fencep);
#define	dma_fence_get_rcu_safe(f)	lkpi_dma_fence_get_rcu_safe(f)
bool lkpi_dma_fence_is_signaled_locked(struct dma_fence *fence);
#define	dma_fence_is_signaled_locked(f)	lkpi_dma_fence_is_signaled_locked(f)
bool lkpi_dma_fence_is_signaled(struct dma_fence *fence);
#define	dma_fence_is_signaled(f)	lkpi_dma_fence_is_signaled(f)
bool lkpi___dma_fence_is_later(uint64_t f1, uint64_t f2,
    const struct dma_fence_ops *ops);
#define	__dma_fence_is_later(f1, f2, ops)	\
	lkpi___dma_fence_is_later(f1, f2, ops)
bool lkpi_dma_fence_is_later(struct dma_fence *f1, struct dma_fence *f2);
#define	dma_fence_is_later(f1, f2)	lkpi_dma_fence_is_later(f1, f2)
struct dma_fence *lkpi_dma_fence_later(struct dma_fence *f1, struct dma_fence *f2);
#define	dma_fence_later(f1, f2)		lkpi_dma_fence_later(f1, f2)
int lkpi_dma_fence_get_status_locked(struct dma_fence *fence);
#define	dma_fence_get_status_locked(f)	lkpi_dma_fence_get_status_locked(f)
void lkpi_dma_fence_set_error(struct dma_fence *fence, int error);
#define	dma_fence_set_error(f, e)	lkpi_dma_fence_set_error(f, e)
signed long lkpi_dma_fence_wait(struct dma_fence *fence, bool intr);
#define	dma_fence_wait(f, i)		lkpi_dma_fence_wait(f, i)

#define	__dma_fence_might_wait()	\
	do {				\
	} while (0)

#define DMA_FENCE_TRACE(f, fmt, args...) \
	do {				\
	} while (0)

#define DMA_FENCE_WARN(f, fmt, args...) \
	do {				\
	} while (0)

#define DMA_FENCE_ERR(f, fmt, args...) \
	do {				\
	} while (0)

#endif	/* _LINUXKPI_LINUX_DMA_FENCE_h */
