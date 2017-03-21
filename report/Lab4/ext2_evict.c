#include "ext2_evict.h"

/**
 * Get use ratio of ext2 filesystem
 */
unsigned long get_ext2_usage(struct super_block *super){
	unsigned long unused;
	unsigned long total;
	unsigned long used;
	double ratio;

	total = MAXBLK;
	unused = ext2_count_free_blocks(super);
	used = total - unused;
	ratio = (used * 1.0) / (total * 1.0);

	return (unsigned long)(ratio*100);
}

static void *read_blocks(struct inode *i_node)
{

	/*
	 * for each index:
	 *	test if the page is in the cache
	 *	no:
	 *		allocate a page
	 *		add_to_page_cache_lru()
	 *		set page mapping.
	 *		set page index
	 *		increment mapping page count
	 *	readpage: mpage_readpages -> do_mpage_readpage
	 *
	 */
	
	void *file_buffer;
	unsigned long curr_addr;
	unsigned long size, remaining, offset, index;
	struct page *curr_page;
	struct address_space *mapping;
	char *kaddr;
	unsigned long nr_segs, error;

	nr_segs = 0;
	size = i_size_read(i_node);
	mapping = i_node->i_mapping;
	printk("inode size = %lu\n", size);
	printk("page size = %lu\n", PAGE_SIZE);

	/* file_buffer = vmalloc(size, GFP_KERNEL); */
	file_buffer = vmalloc(size);
	curr_addr = (unsigned long)file_buffer;
	printk("file_buffer = %p\n", file_buffer);
	printk("curr_addr = %lu\n", curr_addr);
	printk("file_buffer + size = %p\n", file_buffer + size);
	if (file_buffer != NULL) {
		printk("file_buffer != NULL\n");
		remaining = size;
		index = 0;
		while (curr_addr < ((unsigned long) file_buffer) + size) {
			/*
			 * Get page functionality goes here.
			 * curr_page = virt_to_page((void *) curr_addr);
			 *
			 * find_get_page - increments the ref count.
			 */
			curr_page = find_get_page(mapping, index);
			printk("before alloc curr_page = %p\n", curr_page);
			if (!curr_page) {
				curr_page = page_cache_alloc_cold(mapping);
				printk("after alloc, curr_page = %p\n", curr_page);
				if (!curr_page) {
					printk("NULL page.\n");
					kfree(file_buffer);
					return NULL;
				} else {
					error = add_to_page_cache_lru(curr_page, mapping, index, GFP_KERNEL);
					if (error) {
						printk("error adding lru.\n");
						kfree(file_buffer);
						return NULL;
					}
					printk("added page to lru cache.\n");
					printk("curr_page = %p\n", curr_page);
					printk("Found page and preparing to read.\n");
					mpage_readpage(curr_page,
							ext2_get_block);
				}
			}

			printk("Found and read page.\n");
			/*
			 * Based on filemap.file_read_actor
			 */
			offset = 0;
			kaddr = kmap(curr_page);
			if (remaining < PAGE_SIZE) {
				memcpy((void *) curr_addr, kaddr, remaining);
				remaining = 0;
			} else {
				memcpy((void *) curr_addr, kaddr + offset,
						PAGE_SIZE);
				remaining -= PAGE_SIZE;
			}
			kunmap(curr_page);
			page_cache_release(curr_page);
			curr_addr += PAGE_SIZE;
			index++;
		}
		printk("Finished reading the blocks.\n");
	} else {
		printk("Failed to allocate memory to read file.\n");
		/* vfree(file_buffer); */
		kfree(file_buffer);
		return NULL;
	}
	return file_buffer;	
}

static int write_blocks(struct inode *i_node, void *file_data, unsigned long file_size)
{
    char *curr_addr = (char *) file_data;
    char *end_addr = ((char *) file_data) + file_size;
	unsigned long curr_block = 0;
	struct super_block *sb = i_node->i_sb;

	unsigned long blocksize = sb->s_blocksize;
	int tocopy;
	struct buffer_head tmp_bh;
	struct buffer_head *bh;
	int ret_val = 0;

	mutex_lock_nested(&i_node->i_mutex, I_MUTEX_QUOTA);
	while (curr_addr < end_addr) {
		printk("executing write block loop.\n");
		tocopy = blocksize < ( end_addr - curr_addr) ? blocksize : (end_addr - curr_addr);

		tmp_bh.b_state = 0;
		ret_val = ext2_get_block(i_node, curr_block, &tmp_bh, 1);

		if (ret_val < 0) {
			mutex_unlock(&i_node->i_mutex);
			return ret_val;
		}

		bh = sb_getblk(sb, tmp_bh.b_blocknr);
		if (!bh) {
			ret_val = -EIO;
			mutex_unlock(&i_node->i_mutex);
			return ret_val;
		}
		
		lock_buffer(bh);
		
		memcpy(bh->b_data, file_data, tocopy);
		flush_dcache_page(bh->b_page);
		set_buffer_uptodate(bh);
		mark_buffer_dirty(bh);

		unlock_buffer(bh);
		sync_dirty_buffer(bh);
		brelse(bh);
		curr_addr += tocopy;
		curr_block++;
	}
	
	printk("exitted the block write loop.\n");
	/* TODO: check if this modifies any metadata... */
	mark_inode_dirty(i_node);
	/* Sync to write data to disk */
	sync_mapping_buffers(i_node->i_mapping);

    mutex_unlock(&i_node->i_mutex);
        
	return ret_val;
}

/**
 * Get data from a file specified by its inode.
 */
static char *get_data_from_inode(struct inode *i_node){
	char *data;
	data = (char *)read_blocks(i_node);
	printk("data addr: %lx\n", (unsigned long)data);
	return data;
}

/**
 * Check if this inode is opened yet
 */ 
static int is_inode_open(struct inode *i_node){
	int open;
	struct dentry *curr_dentry;

	open = 0;
	hlist_for_each_entry(curr_dentry, &i_node->i_dentry, d_alias) {
		if (curr_dentry->d_count > 0) {
			printk("Dentry is open. %d\n", curr_dentry->d_count);
			open = 1;
		}
	}
	return open;
}

/**
 * Create connection between local filesystem and remote cloud
 * server. For android virtual machine, server works on 10.0.2.2,
 * port 8888, which is defined in ext2_evict.h.
 */
static struct socket * create_file_socket(void){
	struct socket *sock;  
    struct sockaddr_in s_addr;  
    int ret = 0;  

    memset(&s_addr,0,sizeof(s_addr));  
    s_addr.sin_family = AF_INET;  
    s_addr.sin_port = htons(SERVERPORT);  
       
    s_addr.sin_addr.s_addr = in_aton(SERVERADDR); /* server ip is 10.0.2.2 */  
    sock = (struct socket *)kmalloc(sizeof(struct socket),GFP_KERNEL);  
  
    /* create a socket */  
    ret = sock_create_kern(AF_INET, SOCK_STREAM, 0, &sock);  
    if(ret < 0){  
        printk("client:socket create error!\n");  
        return NULL;  
    }  
    printk("client: socket create ok!\n");  
  
    /* connect server */  
    ret = sock->ops->connect(sock, (struct sockaddr *)&s_addr, sizeof(s_addr), 0);  
    if(ret != 0){  
        printk("client:connect error!\n");  
        return NULL;  
    }  
    printk("client:connect ok!\n");  
    
    return sock;
}

/**
 * Send message to server specified by a socket object.
 */
static int ext2_send_msg(struct socket *sock, char *send_buf, int len){
	struct kvec vec;
	struct msghdr msg;
	int ret;

	vec.iov_base = send_buf;  
	if(len == -1)
    	vec.iov_len = strlen(send_buf);  
    else
    	vec.iov_len = len;
  
    memset(&msg, 0, sizeof(msg));  
  
  	/* Send message to the server */
    ret = kernel_sendmsg(sock, &msg, &vec, 1, len); /* send message */  
    if(ret < 0){  
        printk("client: kernel_sendmsg error!\n");
        return ret;  
    }else{  
        printk("client: ret = %d\n", ret);  
    }
    printk("client sned ret: %d\n : ", ret);
  
    return ret;  
}

/**
 * Receive message from server specified by a socket object.
 */
static int ext2_recv_msg(struct socket *sock, char *recv_buf, int len){
	struct kvec vec;
	struct msghdr msg;
	int ret;

	vec.iov_base = recv_buf;  
    vec.iov_len = len;
  
    memset(&msg, 0, sizeof(msg));  
  
  	/* Receive message to server */
    ret = kernel_recvmsg(sock, &msg, &vec, 1, len, 0); /* recieve message */  
    if(ret < 0){  
        printk("client: kernel_recvmsg error!\n");  
        return ret;  
    }else if(ret != 1024){  
        printk("client: ret = %d\n", ret);  
    }  
    printk("client:recv ok!\n");  
    recv_buf = vec.iov_base;
  
    return ret;  
}

/**
 * Evict the file identified by i_node to cloud server,
 * freeing its disk blocks and removing ang page cache pages.
 * The call should return when the file is evicted. Besides
 * the file data pointer, no other meta data, e.g., access time,
 * size, etc. should be changed. Appropriate errors should 
 * be returned. In particular, the operation should fail if the 
 * inode currently maps to an open file. Lock the inode
 * appropriately to prevent a file open operation on it while
 * it is being evicted.
 */
int ext2_evict(struct inode *i_node){
	int flags; /* Xattr setting flags */
	char req[MAXLEN]; /* Request sent to server (clfs_req) */
	int type, datasize = 0;
	unsigned long inode_index;
	char *data;
	/* Get & set xattr definations */
	const char *name = "evicted";
	__u8 e_name_index = 0;
	char buffer[1];
	int buffer_size;
	struct socket *sock;
	int error;

	printk("ext2_evict entered\n");

	/* If opened */
	if(is_inode_open(i_node)){
		printk("Cannot evict a file at a opened state\n");
		return -2;
	}

	/* Judge evicted already? */
	buffer_size = 1;
	if(!i_node) return EVICT_NOTEXIST;
	if(ext2_xattr_get(i_node, e_name_index, name, buffer, buffer_size) == -ENODATA){
		buffer[0] = 0;
		flags = XATTR_CREATE;
		ext2_xattr_set(i_node, e_name_index, name, buffer, buffer_size, flags);
	}else{
		if(buffer[0] == 1){
			printk("already evicted\n");
			return EVICT_REPEAT;
		}
	}

	if((sock = create_file_socket()) == NULL)
		return EVICT_NOSERVER;

	/* Read file */
	data = get_data_from_inode(i_node);
	if(data){
		printk("good data\n");
		/* Format request message */
		type = 1;
		inode_index = (unsigned long)i_node->i_ino;
		datasize = i_size_read(i_node);
		printk("data size: %d\n", datasize);
		printk("inode id: %ld\n", inode_index);
		sprintf(req, "%d %ld %d", type, inode_index, datasize);

		ext2_send_msg(sock, req, -1);
		ext2_send_msg(sock, data, datasize);
	}
	else{
		printk("Can not load data from file inode\n");
		return EVICT_NOTEXIST;
	}
	
	vfree(data);
	printk("inode->i_blocks : %ld\n", i_node->i_blocks);
	printk("super block free blocks : %ld\n", ext2_count_free_blocks(i_node->i_sb));
	clear_data_blocks(i_node);
	printk("inode->i_blocks(after clearing) : %ld\n", i_node->i_blocks);
	printk("super block free blocks(after clearing) : %ld\n", ext2_count_free_blocks(i_node->i_sb));
	printk("current usage : %ld\n", get_ext2_usage(i_node->i_sb));
	sock_release(sock);

	return error;
}

/**
 * Fetch the file specified by i_node from the cloud server.
 * The function should allocate space for the file on the local
 * filesystem. No other metadata on the file should be changed.
 * Lock the inode appropriately to prevent concurrent fetch 
 * operations on the same inode, and return appropriate errors.
 */
int ext2_fetch(struct inode *i_node){
	int flags; /* Xattr setting flags */
	char req[MAXLEN]; /* Request sent to server (clfs_req) */
	int type, inode_index, datasize = 0;
	char *data;
	/* Get & set xattr definations */
	const char *name = "evicted";
	__u8 e_name_index = 0;
	char buffer[1];
	int buffer_size;
	struct socket *sock;
	int error;

	/* Judge evicted already? */
	buffer_size = 1;
	if(!i_node) return EVICT_NOTEXIST;
	if(ext2_xattr_get(i_node, e_name_index, name, buffer, buffer_size) == -ENODATA){
		buffer[0] = 0;
		flags = XATTR_CREATE;
		ext2_xattr_set(i_node, e_name_index, name, buffer, buffer_size, flags);
	}else{
		if(buffer[0] == 1)
			return EVICT_REPEAT;
	}

	if((sock = create_file_socket()) == NULL)
		return EVICT_NOSERVER;
	/* Format request message */
	type = 2;
	inode_index = (int)i_node->i_ino;
	datasize = i_size_read(i_node);
	sprintf(req, "%d %d %d", type, inode_index, datasize);

	/* Read file */
	data = (char *)vmalloc(datasize);
	ext2_send_msg(sock, req, -1);
	ext2_recv_msg(sock, data, datasize);
	
	write_blocks(i_node, data, datasize);
	
	/* Save file to disk using sync */
	error = sync_mapping_buffers(i_node->i_mapping);
	sock_release(sock);

	return error;
}

int ext2_evict_fs(struct super_block *super)
{
	struct ext2_sb_info *sbi;
	struct ext2_super_block *sb;
	struct inode *root;
	struct timeval tv;
	time_t scantime;
	int clockhand;
	int ret;

	unsigned long usage;
	
	int total_inos;
	struct inode *cur_inode;
	
	if(!super) {
		printk("bad super block\n");
		printk("trying getting again...\n");
		super = ext2_get_super_block();
		if(!super)
			return -1;
	}

	sbi = (struct ext2_sb_info *)super->s_fs_info;
	sb = sbi->s_es;
	root = super->s_root->d_inode;

	spin_lock(&root->i_lock);
	ret = ext2_xattr_get(root, 0, "clockhand", &clockhand, sizeof(int));
	spin_unlock(&root->i_lock);

	printk("in evict_fs\n");
	printk("getting clockhand: %d ret: %d\n", clockhand, ret);

	if (ret < 0) {
		clockhand = 0;
		spin_lock(&root->i_lock);
		ret = ext2_xattr_set(root, 0, "clockhand", &clockhand, sizeof(int), XATTR_CREATE);
		spin_unlock(&root->i_lock);
		printk("setting clockhand: %d ret: %d\n", clockhand, ret);
	}

	usage = get_ext2_usage(super);

	printk("current_usage : %ld\n", usage);
	
	if (usage > sbi->wl) {
		printk("usage > wl\n");
		total_inos = sbi->s_groups_count*sb->s_inodes_per_group;
		printk("total: %d\n", total_inos);

		while(usage > sbi->evict) {		
			cur_inode = ext2_iget(super, clockhand);

			if(IS_ERR(cur_inode)) {
				if(++clockhand > total_inos) {
					clockhand = 0;
				} 
				continue;
			} else if (!S_ISREG(cur_inode->i_mode)) {
				if(++clockhand > total_inos) {
					clockhand = 0;
				} 
				continue;
			} else if (clockhand != cur_inode->i_ino) {
				continue;
			}

			printk("getting inode %ld scantime\n", cur_inode->i_ino);
			spin_lock(&cur_inode->i_lock);
			ret = ext2_xattr_get(cur_inode, 0, "scantime", &scantime, sizeof(time_t));
			spin_unlock(&cur_inode->i_lock);

			if(ret < 0) {
				scantime = 0;
				printk("setting inode %ld scantime to 0\n", cur_inode->i_ino);
				spin_lock(&cur_inode->i_lock);
				ext2_xattr_set(cur_inode, 0, "scantime", &scantime, sizeof(time_t), XATTR_CREATE);
				spin_unlock(&cur_inode->i_lock);
				continue;
			}

			printk("scantime: %d i_atime: %d\n", (int)scantime,
					(int)cur_inode->i_atime.tv_sec);
			if(scantime > cur_inode->i_atime.tv_sec) {
				printk("evicting inode: %ld\n",
						cur_inode->i_ino);
				ext2_evict(cur_inode);
				usage = get_ext2_usage(super);
			} else {
				do_gettimeofday(&tv);
				scantime = tv.tv_sec;
				spin_lock(&cur_inode->i_lock);
				ext2_xattr_set(cur_inode, 0, "scantime",
						&scantime, sizeof(time_t),
						XATTR_REPLACE);
				spin_unlock(&cur_inode->i_lock);
				printk("resetting inode %ld scantime: %d",
						cur_inode->i_ino, (int)scantime);
			}

			if(++clockhand > total_inos) {
				clockhand = 0;
			}
		}

		spin_lock(&root->i_lock);
		ext2_xattr_set(root, 0, "clockhand", &clockhand, sizeof(int),
				XATTR_REPLACE);
		spin_unlock(&root->i_lock);
	}
	return ret;
}