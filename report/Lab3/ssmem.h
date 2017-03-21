#ifndef __H_SSMEM_
#define __H_SSMEM_

#define SSMEM_FLAG_CREATE 0x1
#define SSMEM_FLAG_WRITE 0x2
#define SSMEM_FLAG_EXEC 0x4
#define EADDRNOTAVALL 0x9 /* Invalid ssmem_segment address */
#define ENOSEMEM 0xa /* No such ssmem */
#define SEGMENT_SIZE 0xfff /* 1 << 12 - 1, used to align */
#define MAX_SSMEM 1024 /* At most 1024 ssmem can be attached */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/decompress/mm.h>
#include <linux/highmem.h>
#include <linux/gfp.h>
#include <linux/page-flags.h>
#include <linux/rwsem.h>
#include <uapi/asm-generic/mman-common.h>

/* Protect ssmem array */
DECLARE_RWSEM(ssmem_mutex);

/**
 * Struct used to link tasks mapping to the same ssmem
 */
struct task_node{
	struct task_struct *task;
	struct task_node *next;
	struct task_node *prev;
	unsigned long addr; /* Virtual address in this task's virtual address space */
	struct vm_area_struct *vma; /* Vma in this task mapping to this segment */
};

/**
 * Struct used to store ssmem status
 */
struct ssmem_segment{
	struct page *page; /* Physical page mapping to this segment, initially NULL */
	int mapcnt; /* Number of task mapping to this ssmem */
	int flag; /* ssmem privilidge, create, write or exec */
	int length; /* Length of ssmem */
	int readcnt; /* Number of processes reading this shared memory */
	//DECLARE_RWSEM(rw_mutex);
	struct rw_semaphore rw_mutex; /* Mutex used to protect ssmem_segment for reader and writer */
	//DECLARE_RWSEM(cnt_mutex); 
	struct rw_semaphore cnt_mutex; /* Mutex used to protect readcnt */
	struct task_node *task_head;
} *ssmem[MAX_SSMEM] = { NULL };

static void trace_vma(struct mm_struct *mm);
static void print_segment(void);
static int hash_id(char *str_id);
static void print_segment(void);
static int delete_vma(struct ssmem_segment *segment, struct vm_area_struct *vma);
static int ssmem_fault(struct vm_area_struct *vma, struct vm_fault *vmf);
static void ssmem_close(struct vm_area_struct *vma);
int align_length(int length);
asmlinkage long sys_ssmem_attach(char* str_id, int flags, size_t length);
asmlinkage long sys_ssmem_detach(void *addr);
asmlinkage long sys_ssmem_read(void *addr, char *data);
asmlinkage long sys_ssmem_write(void *addr, char *data);

#endif