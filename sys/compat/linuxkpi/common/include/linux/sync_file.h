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

#ifndef _LINUXKPI_LINUX_SYNC_FILE_H_
#define	_LINUXKPI_LINUX_SYNC_FILE_H_

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#include <linux/fs.h>

struct sync_file {
	struct file		*file; /* linux_file */
	char			user_name[32];

	wait_queue_head_t	wq;
	unsigned long		flags;
#define	POLL_ENABLED		0

	struct dma_fence	*fence;
	struct dma_fence_cb	cb;
};

struct sync_file *lkpi_sync_file_create(struct dma_fence *fence);
#define	sync_file_create(f)		lkpi_sync_file_create(f)
struct dma_fence *lkpi_sync_file_get_fence(int fd);
#define	sync_file_get_fence(f)		lkpi_sync_file_get_fence(f);
char *lkpi_sync_file_get_name(struct sync_file *sf, char *buf, int len);
#define	sync_file_get_name(sf, b, l)	lkpi_sync_file_get_name(sf, b, l)

#endif	/* _LINUXKPI_LINUX_SYNC_FILE_H_ */
