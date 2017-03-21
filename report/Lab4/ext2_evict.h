#ifndef _EXT2_EVICT_H
#define _EXT2_EVICT_H

#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/quotaops.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/mpage.h>
#include <linux/delay.h>
#include <uapi/linux/stat.h>
#include "xattr.h"
#include "ext2.h"

#include <linux/socket.h>
#include <linux/inet.h>
#include <linux/net.h>
#include <linux/in.h>
#include <net/sock.h>

#include <linux/module.h>
#include <linux/types.h>

#define MAXLEN 1024
#define MAXBLK 10232
#define EXT2SIZE 10*(1<<20)
#define EXT2_MAX_SIZE 0//7*(1<<20)

#define EVICT_WH
#define EVICT_HL
#define EVICT_TG

#define EVICT_REPEAT 0x1
#define EVICT_NOTEXIST 0x2
#define EVICT_NOSERVER 0x4
#define EVICT_NETERR  0x8

#define SERVERADDR "10.0.2.2"
#define SERVERPORT 8888

struct clfs_req{
	enum {
		CLFS_PUT,
		CLFS_GET,
		CLFS_REM	
	}type;
	int inode;
	int size;
};

enum clfs_status{
	CLFS_OK,
	CLFS_INVAL,
	CLFS_ACCESS,
	CLFS_ERROR
};

int ext2_evict(struct inode *i_node);
int ext2_fetch(struct inode *i_node);
int ext2_evict_fs(struct super_block *super);
struct super_block *ext2_get_super_block(void);
unsigned long get_ext2_usage(struct super_block *super);
extern int clear_data_blocks(struct inode *i_node);

#endif