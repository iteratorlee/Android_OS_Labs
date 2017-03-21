#include "ext2/ext2_evict.h"

struct super_block *
ext2_get_super_block(void){
	struct super_block *sb;
	int cnt = 0;
	list_for_each_entry(sb, &super_blocks, s_list){
		if(sb)
			printk("[test] super block #%d name : %s\n", cnt++, sb->s_type->name);
		if(sb->s_op->evict_fs)
			return sb;
	}
	return NULL;
}

/**
 * A kernel monitor thread scanning ext2 filesystem
 * every minute.
 */
static int do_evictd(void* args){
	struct super_block *super = NULL;

	/* Get super block */
	super = ext2_get_super_block();
	if(super){
		printk("super block found\n");
		printk("[name] %s\n", super->s_type->name);
		printk("[size] %ld\n", super->s_blocksize);
	}
	else{
		printk("ext2 super block not found\n");
	}

	printk("kfs_evictd started\n");
	while(1){
		msleep(60000);
		printk("kfs_evictd working...\n");
		ext2_evict_fs(super);
	}
	return 0;
}

/**
 * Create a kernel monitor thread when booting. 
 */
static __init int kfs_evictd(void){
	const char *name = "kfs_evictd";
	struct task_struct *task;
	task = kthread_run(do_evictd, NULL, name);

	if(task)
		printk("[kfs] create success!\n");
	else
		printk("[kfs] fail to create!\n");

	return 0;
}
late_initcall_sync(kfs_evictd);