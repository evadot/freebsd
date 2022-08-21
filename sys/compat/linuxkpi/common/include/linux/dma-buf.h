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

#ifndef _LINUXKPI_LINUX_DMA_BUF_H_
#define	_LINUXKPI_LINUX_DMA_BUF_H_

#include <linux/file.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/dma-fence.h>
#include <linux/wait.h>
#include <linux/module.h>

struct device;
struct dma_buf;
struct dma_buf_attachment;

struct dma_buf_export_info {
	const char *exp_name;
	struct module *owner;
	const struct dma_buf_ops *ops;
	size_t size;
	int flags;
	struct dma_resv *resv;
	void *priv;
};

#define DEFINE_DMA_BUF_EXPORT_INFO(a)	\
	struct dma_buf_export_info a = { .exp_name = KBUILD_MODNAME, \
					 .owner = THIS_MODULE }

struct dma_buf_ops {
	bool cache_sgt_mapping;

	int (*attach)(struct dma_buf *, struct dma_buf_attachment *);

	void (*detach)(struct dma_buf *, struct dma_buf_attachment *);

	int (*pin)(struct dma_buf_attachment *attach);

	void (*unpin)(struct dma_buf_attachment *attach);

	struct sg_table * (*map_dma_buf)(struct dma_buf_attachment *,
						enum dma_data_direction);
	void (*unmap_dma_buf)(struct dma_buf_attachment *,
						struct sg_table *,
						enum dma_data_direction);
	void (*release)(struct dma_buf *);

	int (*begin_cpu_access)(struct dma_buf *, enum dma_data_direction);
	int (*end_cpu_access)(struct dma_buf *, enum dma_data_direction);

	void *(*map_atomic)(struct dma_buf *, unsigned long);
	void (*unmap_atomic)(struct dma_buf *, unsigned long, void *);

	void *(*map)(struct dma_buf *, unsigned long);
	void (*unmap)(struct dma_buf *, unsigned long, void *);

	int (*mmap)(struct dma_buf *, struct vm_area_struct *vma);

	void *(*vmap)(struct dma_buf *);
	void (*vunmap)(struct dma_buf *, void *vaddr);
};

#undef file
struct dma_buf {
	size_t size;
	struct file *linux_file; /* Native struct file, not struct linux_file */
	struct list_head attachments;
	const struct dma_buf_ops *ops;
	/* mutex to serialize list manipulation, attach/detach and vmap/unmap */
	struct mutex lock;
	unsigned vmapping_counter;
	void *vmap_ptr;
	const char *exp_name;
	struct module *owner;
	struct list_head list_node;
	void *priv;
	struct dma_resv *resv;

	/* poll support */
	wait_queue_head_t poll;

	struct dma_buf_poll_cb_t {
		struct dma_fence_cb cb;
		wait_queue_head_t *poll;

		unsigned long active;
	} cb_excl, cb_shared;
};

struct dma_buf_attachment {
	struct dma_buf *dmabuf;
	struct device *dev;
	struct list_head node;
	struct sg_table *sgt;
	enum dma_data_direction dir;
	bool peer2peer;
	const struct dma_buf_attach_ops *importer_ops;
	void *importer_priv;
	void *priv;
};

struct dma_buf_attach_ops {
	bool allow_peer2peer;
	void (*move_notify)(struct dma_buf_attachment *attach);
};

#define file linux_file
static inline void
get_dma_buf(struct dma_buf *dmabuf)
{

	while(!fhold(dmabuf->file)) {
		pause("fhold", hz);
	}
}

struct dma_buf_attachment *lkpi_dma_buf_attach(struct dma_buf *db,
    struct device *dev);
#define	dma_buf_attach(db, dev)				\
	lkpi_dma_buf_attach(db, dev)
struct dma_buf_attachment *lkpi_dma_buf_dynamic_attach(struct dma_buf *db,
    struct device *dev, const struct dma_buf_attach_ops *ops, void *priv);
#define	dma_buf_dynamic_attach(db, dev, ops, priv)	\
	lkpi_dma_buf_dynamic_attach(db, dev, ops, priv)
void lkpi_dma_buf_detach(struct dma_buf *db, struct dma_buf_attachment *dba);
#define	dma_buf_detach(db, dba)				\
	lkpi_dma_buf_detach(db, dba)
int lkpi_dma_buf_pin(struct dma_buf_attachment *dba);
#define	dma_buf_pin(dba)				\
	lkpi_dma_buf_pin(dba)
void lkpi_dma_buf_unpin(struct dma_buf_attachment *dba);
#define	dma_buf_unpin(dba)				\
	lkpi_dma_buf_unpin(dba)
struct sg_table *lkpi_dma_buf_map_attachment(struct dma_buf_attachment *dba,
    enum dma_data_direction dir);
#define	dma_buf_map_attachment(dba, dir)		\
	lkpi_dma_buf_map_attachment(dba, dir)
void lkpi_dma_buf_unmap_attachment(struct dma_buf_attachment *dba,
    struct sg_table *sg, enum dma_data_direction dir);
#define	dma_buf_unmap_attachment(dba, sg, dir)		\
	lkpi_dma_buf_unmap_attachment(dba, sg, dir)
void lkpi_dma_buf_move_notify(struct dma_buf *db);
#define	dma_buf_move_notify(db)				\
	lkpi_dma_buf_move_notify(db)
void *lkpi_dma_buf_vmap(struct dma_buf *db);
#define	dma_buf_vmap(db)				\
	lkpi_dma_buf_vmap(db)
void lkpi_dma_buf_vunmap(struct dma_buf *db, void *vaddr);
#define	dma_buf_vunmap(db, vaddr)			\
	lkpi_dma_buf_vunmap(db, vaddr)
int lkpi_dma_buf_fd(struct dma_buf *db, int flags);
#define	dma_buf_fd(db, flags)				\
	lkpi_dma_buf_fd(db, flags)
struct dma_buf *lkpi_dma_buf_get(int fd);
#define	dma_buf_get(fd)					\
	lkpi_dma_buf_get(fd)
void lkpi_dma_buf_put(struct dma_buf *db);
#define	dma_buf_put(db)					\
	lkpi_dma_buf_put(db)
struct dma_buf *lkpi_dma_buf_export(const struct dma_buf_export_info *exp_info);
#define	dma_buf_export(exp_info)			\
	lkpi_dma_buf_export(exp_info)

#endif /* _LKPI_LINUX_DMA_BUF_H_ */
