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

#include <linux/export.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/anon_inodes.h>
#include <linux/sync_file.h>
#include <linux/dma-fence.h>
#include <uapi/linux/sync_file.h>

static int sync_file_release(struct inode *inode, struct file *file);
static __poll_t sync_file_poll(struct file *file, poll_table *wait);
static long sync_file_ioctl(struct file *file, unsigned int cmd,
    unsigned long arg);

static const struct file_operations sync_file_fops = {
	.release = sync_file_release,
	.poll = sync_file_poll,
	.unlocked_ioctl = sync_file_ioctl,
	.compat_ioctl = sync_file_ioctl,
};

static int
sync_file_release(struct inode *inode, struct file *file)
{
	struct sync_file *sf;

	sf = file->private_data;
	if (test_bit(POLL_ENABLED, &sf->flags))
		dma_fence_remove_callback(sf->fence, &sf->cb);
	dma_fence_put(sf->fence);
	kfree(sf);
	return (0);
}

static void
fence_check_cb_func(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct sync_file *sf;

	sf = container_of(cb, struct sync_file, cb);
	wake_up_all(&sf->wq);
}

static __poll_t
sync_file_poll(struct file *file, poll_table *wait)
{
	struct sync_file *sf;

	sf = file->private_data;
	poll_wait(file, &sf->wq, wait);
	if (list_empty(&sf->cb.node) &&
	    !test_and_set_bit(POLL_ENABLED, &sf->flags)) {
		if (dma_fence_add_callback(sf->fence, &sf->cb,
		    fence_check_cb_func) < 0)
			wake_up_all(&sf->wq);
	}
	return (dma_fence_is_signaled(sf->fence) ? POLLIN : 0);
}

/* Alloc a new sync_file */
static struct sync_file *
sf_alloc(void)
{
	struct sync_file *sf;

	sf = kzalloc(sizeof(struct sync_file), GFP_KERNEL);
	if (sf == NULL)
		goto err;

	sf->file = anon_inode_getfile("sync_file",
	    &sync_file_fops,
	    sf, 0);
	if (IS_ERR(sf->file))
		goto err;
	init_waitqueue_head(&sf->wq);
	INIT_LIST_HEAD(&sf->cb.node);
	return (sf);

err:
	kfree(sf);
	return (NULL);
}

/* Get a sync_file from a file descriptor */
static struct sync_file *
sf_from_fd(int fd)
{
	struct sync_file *sf;
	struct file *file = NULL;

	sf = NULL;
	if ((file = fget(fd)) == NULL)
		return (NULL);
	if (file->f_op != &sync_file_fops)
		return (NULL);
	sf = (struct sync_file *)file->private_data;
	fput(file);
	return (sf);
}

/* Get the fences associated to a sync_file */
static struct dma_fence **
sf_get_fences(struct sync_file *sf, int *nfences)
{
	struct dma_fence_array *array;

	if (dma_fence_is_array(sf->fence)) {
		array = to_dma_fence_array(sf->fence);

		*nfences = array->num_fences;
		return (array->fences);
	}

	*nfences = 1;
	return (&sf->fence);
}

/* Add a fences only if it's not signaled */
static bool
add_fence(struct dma_fence **fences, struct dma_fence *fence, int pos)
{

	if (dma_fence_is_signaled(fence) == false) {
		fences[pos] = fence;
		dma_fence_get(fence);
		return (true);
	}

	return (false);
}

/* Merge two sync_file into a new one */
static struct sync_file *
sf_merge(const char *name, struct sync_file *sf1, struct sync_file *sf2)
{
	struct sync_file *sf;
	struct dma_fence **sf_fences, **sf1_fences, **sf2_fences;
	int sf_nfences, sf1_nfences, sf2_nfences;
	int i, i_sf1, i_sf2;

	if ((sf = sf_alloc()) == NULL)
		return (NULL);

	sf1_fences = sf_get_fences(sf1, &sf1_nfences);
	sf2_fences = sf_get_fences(sf2, &sf2_nfences);

	sf_nfences = sf1_nfences + sf2_nfences;
	sf_fences = kzalloc(sizeof(struct dma_fence *) * sf_nfences, GFP_KERNEL);
	if (sf_fences == NULL)
		goto err;

	/*
	 * Merge the fences by assuming that they are ordered and have
	 * no duplicate in the same context.
	 * This is what Linux seems to do too.
	 */
	for (i = i_sf1 = i_sf2 = 0; i_sf1 < sf1_nfences && i_sf2 < sf2_nfences; ) {
		if (sf1_fences[i_sf1]->context < sf2_fences[i_sf2]->context) {
			if (add_fence(sf_fences, sf1_fences[i_sf1++], i) == true)
				i++;
		} else if (sf1_fences[i_sf1]->context > sf2_fences[i_sf2]->context) {
			if (add_fence(sf_fences, sf2_fences[i_sf2++], 2) == true)
				i++;
		} else {
			if (lkpi___dma_fence_is_later(sf1_fences[i_sf1]->seqno,
			    sf2_fences[i_sf2]->seqno,
			    sf1_fences[i_sf1]->ops)) {
				if (add_fence(sf_fences, sf1_fences[i_sf1++], i) == true)
					i++;
			}
			else {
				if (add_fence(sf_fences, sf2_fences[i_sf2++], i) == true)
					i++;
			}
		}
	}

	/* Add the rest of the fences */
	for (; i_sf1 < sf1_nfences; ) {
		if (add_fence(sf_fences, sf1_fences[i_sf1++], i) == true)
			i++;
	}
	for (; i_sf2 < sf2_nfences; ) {
		if (add_fence(sf_fences, sf1_fences[i_sf2++], i) == true)
			i++;
	}

	/*
	 * If we didn't merge any fences, only add the first fence of
	 * the first sync_file.
	 * This is what Linux seems to do too.
	 */
	if (i == 0)
		sf_fences[i++] = lkpi_dma_fence_get(sf_fences[0]);

	/*
	 * If we didn't added all the fences, shrink the new fences to
	 * the actual size.
	 */
	if (sf_nfences > i) {
		struct dma_fence **new_fences;
		if ((new_fences = krealloc(sf_fences,
		      sizeof(struct dma_fence *) * i,
		      GFP_KERNEL)) == NULL) {
			kfree(sf_fences);
			goto err;
		}
		sf_fences = new_fences;
	}

	/* Add the fences to the new sync_file */
	if (sf_nfences == 1) {
		sf->fence = sf_fences[0];
		kfree(sf_fences);
	} else {
		struct dma_fence_array *array;

		if ((array = dma_fence_array_create(sf_nfences, sf_fences,
		    dma_fence_context_alloc(1),
		      1, false)) == NULL) {
			kfree(sf_fences);
			goto err;
		}
		sf->fence = &array->base;
	}

	strlcpy(sf->user_name, name, sizeof(sf->user_name));
	return (sf);

err:
	kfree(sf);
	return (NULL);
}

static long
sync_file_ioctl_merge(struct sync_file *sf, unsigned long arg)
{
	struct sync_file *sf2, *sf3;
	struct sync_merge_data data;
	int fd, rv;

	rv = 0;
	if ((fd = get_unused_fd_flags(O_CLOEXEC)) < 0)
		return (fd);
	if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
		rv = -EFAULT;
		goto err;
	}
	if (data.flags || data.pad) {
		rv = -EINVAL;
		goto err;
	}
	if ((sf2 = sf_from_fd(data.fd2)) == NULL) {
		rv = -ENOENT;
		goto err;
	}
	data.name[sizeof(data.name) - 1] = '\0';
	if ((sf3 = sf_merge(data.name, sf, sf2)) == NULL) {
		rv = -ENOMEM;
		fput(sf2->file);
		goto err;
	}
	data.fence = fd;
	if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
		rv = -EFAULT;
		fput(sf3->file);
		fput(sf2->file);
		goto err;
	}

	fd_install(fd, sf3->file);
	fput(sf2->file);

	return (0);

err:
	put_unused_fd(fd);
	return (rv);
}

static long
sync_file_ioctl_fence_info(struct sync_file *sf, unsigned long arg)
{
	struct sync_file_info info;
	struct sync_fence_info *fence_info = NULL;
	struct dma_fence **fences;
	int nfences, rv, i;

	rv = 0;
	if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
		return (-EFAULT);
	if (info.flags || info.pad)
		return (-EINVAL);

	if (dma_fence_is_array(sf->fence)) {
		struct dma_fence_array *array = to_dma_fence_array(sf->fence);

		nfences = array->num_fences;
		fences = array->fences;
	} else {
		nfences = 1;
		fences = &sf->fence;
	}

	/*
	 * For num_fences == 0 it means that the user only
	 * wants the actual number of fences and not the info
	 */
	if (info.num_fences == 0) {
		info.status = dma_fence_get_status(sf->fence);
		goto no_fences;
	} else {
		info.status = 1;
	}

	if ((fence_info = kzalloc(info.num_fences * sizeof(struct sync_fence_info),
	      GFP_KERNEL)) == NULL)
		return (-ENOMEM);

	for (i = 0; i < nfences; i++) {
		strlcpy(fence_info[i].obj_name,
		    fences[i]->ops->get_timeline_name(fences[i]),
		    sizeof(fence_info[i].obj_name));
		strlcpy(fence_info[i].driver_name,
		    fences[i]->ops->get_driver_name(fences[i]),
		    sizeof(fence_info[i].driver_name));
		fence_info[i].status = dma_fence_get_status(fences[i]);
		while (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fences[i]->flags) &&
		    !test_bit(DMA_FENCE_FLAG_TIMESTAMP_BIT, &fences[i]->flags))
			cpu_relax();
		fence_info[i].timestamp_ns =
			test_bit(DMA_FENCE_FLAG_TIMESTAMP_BIT, &fences[i]->flags) ?
			ktime_to_ns(fences[i]->timestamp) : ktime_set(0, 0);
		info.status = info.status <= 0 ?
			info.status : fence_info[i].status;
	}
	if (copy_to_user(u64_to_user_ptr(info.sync_fence_info), fence_info,
	    nfences * sizeof(struct sync_fence_info))) {
		rv = -EFAULT;
		goto out;
	}

no_fences:
	lkpi_sync_file_get_name(sf, info.name, sizeof(info.name));
	info.num_fences = nfences;
	if (copy_to_user((void __user *)arg, &info, sizeof(info)))
		rv = -EFAULT;
out:
	kfree(fence_info);
	return (rv);
}

static long
sync_file_ioctl(struct file *file, unsigned int cmd,
    unsigned long arg)
{
	struct sync_file *sf = file->private_data;

	switch (cmd) {
	case SYNC_IOC_MERGE:
		return (sync_file_ioctl_merge(sf, arg));
	case SYNC_IOC_FILE_INFO:
		return (sync_file_ioctl_fence_info(sf, arg));
	default:
		return (-ENOTTY);
	}
}

/*
 * Create a sync_file for a given fence
 */
struct sync_file *
lkpi_sync_file_create(struct dma_fence *fence)
{
	struct sync_file *sf;

	sf = sf_alloc();
	if (sf == NULL)
		return (NULL);
	sf->fence = dma_fence_get(fence);
	return (sf);
}

/*
 * Get a fence based on a sync_file fd
 */
struct dma_fence *
lkpi_sync_file_get_fence(int fd)
{
	struct sync_file *sf;

	sf = sf_from_fd(fd);
	return (dma_fence_get(sf->fence));
}

/*
 * Get the name of a sync_file
 */
char *
lkpi_sync_file_get_name(struct sync_file *sf, char *buf, int len)
{

	if (sf == NULL || buf == NULL)
		return (NULL);
	if (sf->user_name[0] != '\0') {
		strlcpy(buf, sf->user_name, len);
		goto out;
	}
	snprintf(buf, len, "%s-%s%ju-%ju",
	    sf->fence->ops->get_driver_name(sf->fence),
	    sf->fence->ops->get_timeline_name(sf->fence),
	    (uintmax_t)sf->fence->context,
	    (uintmax_t)sf->fence->seqno);
out:
	return(buf);
}
