#include <linux/ssmem.h>

/**
 * Delete a task_node node from a segment when a vma is closed or detached
 */
static int delete_vma(struct ssmem_segment *segment, struct vm_area_struct *vma){
	struct task_node *task_head, *temp, *find;
	task_head = segment->task_head;
	temp = task_head;
	find = NULL;
	while(temp != NULL){
		if(temp->vma == vma){
			find = temp;
			break;
		}
		temp = temp->next;
	}
	/* Delete find in linked list */
	if(find){
		if(!find->prev){
			if(find->next){
				find->next->prev = NULL;
				task_head = find->next;
				find->next = NULL;
			}
			else
				task_head = NULL;
		}
		else{
			if(find->next){
				find->next->prev = find->prev;
				find->prev->next = find->next;
				find->next = NULL;
				find->prev = NULL;
			}
			else{
				find->prev->next = NULL;
				find->prev = NULL;
			}
		}
		down_write(&ssmem_mutex);
		--segment->mapcnt;
		up_write(&ssmem_mutex);
		kfree(find);
	}
	else return EEXIST;
	if(segment->mapcnt == 0){
		down_write(&ssmem_mutex);
		kfree(segment);
		up_write(&ssmem_mutex);
		segment = NULL;
	}
	return 0;
}

/**
 * Page fault handler for ssmem
 */
static int ssmem_fault(struct vm_area_struct *vma, struct vm_fault *vmf){
	int id;
	struct ssmem_segment *segment;
	struct page *page;
	printk("enter fault\n");

	id = vma->segment_id;
	segment = ssmem[id];
	page = segment->page;
	if(page){
		printk("enter if\n");
		get_page(page);
		vmf->page = page;
		return 0;
	}
	page = alloc_page(GFP_HIGHUSER_MOVABLE);
	if(!page){
		printk("can not alloc page\n");
		//return VM_FAULT_OOM;
		return VM_FAULT_SIGBUS;
	}
	else{
		printk("alloc page success\n");
		get_page(page);
		vmf->page = page;
		down_write(&ssmem_mutex);
		segment->page = page;
		up_write(&ssmem_mutex);
		return 0;
	}
	dump_stack();
	return VM_FAULT_SIGBUS;
}

/**
 * Page close handler for ssmem
 */
static void ssmem_close(struct vm_area_struct *vma){
	struct ssmem_segment *segment;
	int id;

	id = vma->segment_id;
	segment = ssmem[id];
	delete_vma(segment, vma);
}

static const struct vm_operations_struct ssmem_mapping_vmops = {
	.close = ssmem_close,
	.fault = ssmem_fault,
};

/**
 * Hash a string id to int
 * An id is an identifier to find a ssmem segment.
 * Using hash method to convert a char* id to an int id
 * can make serach convinient.
 */
static int hash_id(char *str_id){
	int len, i, asicc_code, id;

	len = strlen(str_id);
	printk("len : %d\n", len);
	asicc_code = 0;
	for(i = 0; i < len; ++i)
		asicc_code += (int)str_id[i];

	id = asicc_code % MAX_SSMEM;
	return id;
}

/**
 * Check weather a task is in task list of a ssmem segment
 */
static unsigned long find_task(struct ssmem_segment *segment, struct task_struct *task){
	struct task_node *task_head, *temp;

	task_head = segment->task_head;
	temp = task_head;
	while(temp != NULL){
		if(temp->task == task)
			return temp->addr;
		temp = temp->next;
	}
	return 0;
}

/**
 * Add a task to ssmem segment task list (use find_task to check before add)
 */
static unsigned long add_task(int id, struct task_struct *task, int flags){
	unsigned long addr, start_brk;
	unsigned long prot;
	struct task_node *task_head, *temp;
	struct task_node *node;
	struct vm_area_struct *vma;
	struct ssmem_segment *segment;
	node = (struct task_node *)kmalloc(sizeof(struct task_node), GFP_KERNEL);
	segment = ssmem[id];

	if(segment == NULL) return 0;
	down_write(&ssmem_mutex);
	++segment->mapcnt; /* Plus the number of task mapping to this area */
	up_write(&ssmem_mutex);
	task_head = segment->task_head;
	temp = task_head;

	start_brk = task->mm->start_brk;
	prot = PROT_READ;
	if(flags & SSMEM_FLAG_WRITE)
		prot = prot | PROT_WRITE;
	addr = ssmem_vm_brk(start_brk, segment->length);
	vma = find_vma(task->mm, addr);
	vma->segment_id = id;
	vma->vm_ops = &ssmem_mapping_vmops;
	if(flags & SSMEM_FLAG_WRITE)
		vma->vm_flags |= VM_WRITE;

	node->task = task;
	node->next = NULL;
	node->prev = NULL;
	node->addr = addr;
	node->vma = find_vma(task->mm, addr);

	while(temp->next != NULL)
		temp = temp->next;
	temp->next = node;
	node->prev = temp;
	return addr;
}

static void trace_vma(struct mm_struct *mm){
	struct vm_area_struct *vma = mm->mmap;
	struct vm_area_struct *temp;
	int flag;
	temp = vma;
	printk("begin\n");
	while(temp){
		if(temp->vm_file) flag = 1;
		else flag = 0;
		printk("vma_start : 0x%lx, vma_end : 0x%lx, vm_file : %d\n", temp->vm_start, temp->vm_end, flag);
		temp = temp->vm_next;
	}
	printk("end\n");

}

static void print_segment(void){
	struct ssmem_segment *seg;
	struct task_node *task_head, *temp;
	int i, flag, cnt;

	for(i = 0; i < MAX_SSMEM; ++i){
		seg = ssmem[i];
		if(seg){
			cnt = 0;
			if(seg->page) flag = 1;
			else flag = 0;
			task_head = seg->task_head;
			temp = task_head;
			printk("segment_%d :\n", i);
			printk("mapcnt : %d\n", seg->mapcnt);
			printk("page : %d\n", flag);
			printk("task:\n");
			while(temp){
				printk("task_%d->pid : %d\n", cnt, temp->task->pid);
				printk("task_%d->addr : 0x%lx\n", cnt++, temp->addr);
				temp = temp->next;
			}
			printk("end segment_%d\n\n", i);
		}
	}
}

/**
 * Attach a shared vma to a process
 * A 'ssmem' is a simple shared memory area used to implement IPC
 * A 'ssmem' is created based based on a task, and some information about it
 * is stored in a global array ssmem whose type is ssmem_segment.
 * In this struct we store some status of current ssmem, including address,
 * number of task mapping this area, flag and so on.
 * Other task can visit this area and after their visiting ,they join it into 
 * their vma list in mm_struct 
 */
asmlinkage long sys_ssmem_attach(char* str_id, int flags, size_t length){
	int id;
	unsigned long start_brk; /* mm->start_brk */
	unsigned long addr; /* Return value of vm_mmap */
	unsigned long prot; /* Prot value of vma */
	struct task_struct *task;
	struct mm_struct *mm;
	struct ssmem_segment *segment;
	struct task_node *task_head;
	struct vm_area_struct *vma;

	task = current;
	id = hash_id(str_id);
	mm = task->mm;
	start_brk = mm->start_brk;

	if(length < 0)
		return -EINVAL;

	length =  PAGE_ALIGN(length);
	if(!length)
		return -ENOMEM;

	if((ssmem[id] == NULL) && ((flags & SSMEM_FLAG_CREATE) == 0)) /* No such ssmem for this reader (not creater) */
		return -EADDRNOTAVALL;

	if(flags & SSMEM_FLAG_CREATE){
		if(ssmem[id] != NULL) return EEXIST; /* Create conflict: a ssmem already exists there */

		prot = PROT_READ;
		segment = (struct ssmem_segment *)kmalloc(sizeof(struct ssmem_segment), GFP_KERNEL);
		task_head = (struct task_node *)kmalloc(sizeof(struct task_node), GFP_KERNEL);

		if(flags & SSMEM_FLAG_WRITE)
			prot = prot | PROT_WRITE;

		addr = ssmem_vm_brk(start_brk, length);
		vma = find_vma(mm, addr);
		/* Reverse map this segment to added vma */
		vma->segment_id = id;
		/* Assign ssmem fault handler */
		vma->vm_ops = &ssmem_mapping_vmops;

		/* Initialize task_head */
		task_head->task = task;
		task_head->next = NULL;
		task_head->prev = NULL;
		task_head->addr = addr;
		task_head->vma = vma; /* Vma of mapping to this segment */
		/* Initialize segment */
		segment->page = NULL; /* Physical address is not avilable until page fault handler's action */
		segment->mapcnt = 1;
		segment->readcnt = 0;
		segment->flag = flags;
		segment->length = length;
		segment->task_head = task_head;
		init_rwsem(&segment->rw_mutex);
		init_rwsem(&segment->cnt_mutex);

		down_write(&ssmem_mutex);
		ssmem[id] = segment;
		up_write(&ssmem_mutex);

		trace_vma(mm);
		printk("ssmem_create:\n");
		printk("segment->mapcnt : %d\n", segment->mapcnt);
		printk("segment->task->vma->vm_start : 0x%lx\n", segment->task_head->vma->vm_start);
		printk("segment->task->vma->vm_end : 0x%lx\n", segment->task_head->vma->vm_end);
	}

	else {
		if(ssmem[id] == NULL) return -EADDRNOTAVALL; /* No such ssmem */
		segment = ssmem[id];

		/* Add this task into task list of current writing vma */
		addr = find_task(segment, task);
		if(addr == 0)
			addr = add_task(id, task, flags);

		trace_vma(mm);
		printk("ssmem_write or ssmem_read:\n");
		printk("segment->mapcnt : %d\n", segment->mapcnt);
		printk("segment->task->vma->vm_start : 0x%lx\n", find_vma(mm, addr)->vm_start);
		printk("segment->task->vma->vm_end : 0x%lx\n", find_vma(mm,addr)->vm_end);
	}
	print_segment();

	dump_stack();

	return addr;
}

/**
 * Call by user explicitly, used to let a process detach 
 * a shared memory
 */
asmlinkage long sys_ssmem_detach(void *addr){
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct ssmem_segment *segment;
	unsigned long vaddr;
	int id;

	mm = current->mm;
	vaddr = (unsigned long)addr;
	vma = find_vma(mm, vaddr);
	if(!vma) return EEXIST;

	id = vma->segment_id;
	segment = ssmem[id];
	if(delete_vma(segment, vma))
		return EEXIST;

	return 0;
}

/**
 * Syscall used to read and write a shared memory
 * taking consideration of sync
 * Using model of first kind of reader-writer problem
 */
asmlinkage long sys_ssmem_read(void *addr, char* data){
	struct task_struct *task;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct ssmem_segment *segment;
	unsigned long vaddr;

	vaddr = (unsigned long)addr;
	task = current;
	mm = task->mm;
	vma = find_vma(mm, vaddr);
	if(!vma) 
		return EEXIST;
	segment = ssmem[vma->segment_id];

	down_write(&segment->cnt_mutex);
	++segment->readcnt;
	/* First reader */
	if(segment->readcnt == 1)
		down_write(&segment->rw_mutex);
	up_write(&segment->cnt_mutex);

	/* Reading */
	data = strcpy(data, (char *)addr);

	down_write(&segment->cnt_mutex);
	--segment->readcnt;
	/* Last reader */
	if(segment->readcnt == 0)
		up_write(&segment->rw_mutex);
	up_write(&segment->cnt_mutex);

	return 0;
}

asmlinkage long sys_ssmem_write(void *addr, char *data){
	struct task_struct *task;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct ssmem_segment *segment;
	unsigned long vaddr;

	vaddr = (unsigned long)addr;
	task = current;
	mm = task->mm;
	vma = find_vma(mm, vaddr);
	if(!vma) 
		return EEXIST;
	segment = ssmem[vma->segment_id];

	down_write(&segment->rw_mutex);
	/* writing */
	sprintf((char *)addr, "%s", data);
	up_write(&segment->rw_mutex);

	return 0;
}