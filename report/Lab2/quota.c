#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/list.h>
#include <linux/cgroup.h>
#include <linux/string.h>
#include <linux/decompress/mm.h>
#include <uapi/asm-generic/errno-base.h>
#include <asm/uaccess_64.h>

asmlinkage long sys_getusage(pid_t pid){
	struct task_struct *task;
	struct cgroup *cgp;
	struct cgroup_subsys *temp_cgp_sub, *cgp_sub;
	struct list_head *cgp_header, *pos;
	struct cftype *ctp;
	int usage;
	int flag;

	/*Find the task with pid 'pid'*/
	task = find_task_by_vpid(pid);
	cgp = task->cgroups->subsys[2]->cgroup;
	cgp_header = &(cgp->root->subsys_list);

	flag = 0;

	/*Search in the list to find 'cpuacct' sub system*/
	list_for_each(pos, cgp_header){
		if(pos == NULL)
			return -5 - flag;
		temp_cgp_sub = list_entry(pos, struct cgroup_subsys, sibling);
		printk("current_cgroup_subsys_name: %s, flag: %d\n", temp_cgp_sub->name, flag);
		if(!(strcmp(temp_cgp_sub->name, "cpuacct"))) break;
		flag++;
	}

	cgp_sub = temp_cgp_sub;
	if(strcmp(cgp_sub->name, "cpuacct")) return -1;

	ctp = cgp_sub->base_cftypes;

	usage = ctp->read_u64(cgp, ctp);

	return usage;
}

asmlinkage long sys_getquota(u_int64_t color, long long *retvals){
	int flag = 0;
	long long quota = 0;
	int cricle = 0;
	struct task_struct *task_leader, *temp, *task;
	struct cgroup *cgp;
	struct list_head *pos;
	struct list_head *cgp_header;
	struct cgroup_subsys *cgp_sub, *temp_cgp_sub;
	struct cftype *ctp;

	/*Start with process with pid 1, serach with linked list*/
	task_leader = find_task_by_vpid(1);
	if(task_leader == NULL) return 1;
	temp = task_leader->diff_color_next;


	if(task_leader->color != color){
		if(temp == NULL) return -1;
		while(temp != task_leader && temp->color != color){
			temp = temp->diff_color_next;
			if(temp == NULL) return -1;
		}

		if(temp->color == color)
			task_leader = temp;
		else return -1;
	}

	task = task_leader;

	do{
		printk("cricle: %d\n", cricle++);
		flag = 0;
		cgp = task->cgroups->subsys[1]->cgroup;
		cgp_header = &(cgp->root->subsys_list);
		
		if(cgp_header == NULL) return 5;
		
		list_for_each(pos, cgp_header){
			if(pos == NULL)
				return -5 - flag;
			temp_cgp_sub = list_entry(pos, struct cgroup_subsys, sibling);
			printk("current_cgroup_subsys_name: %s, flag: %d\n", temp_cgp_sub->name, ++flag);
			if(!(strcmp(temp_cgp_sub->name, "cpu")))
				break;
		}

		cgp_sub = temp_cgp_sub;
		printk("cg_subsys_name: %s\n", cgp_sub->name);
		ctp = cgp_sub->base_cftypes;
		if((++ctp)->read_s64 == NULL) return 5;
		printk("cftype name: %s\n", ctp->name);
		
		quota += ctp->read_s64(cgp, ctp);
		task = task->same_color_next;
	}while(task != task_leader && task != NULL);
	/*read the quota of every process with color 'color'*/

	retvals[0] = quota;

	return 0;
}

asmlinkage long sys_setquota(int quota, u_int64_t color){
	int flag = 0;
	int cricle = 0;
	int ret = 0;
	struct task_struct *task_leader, *temp, *task;
	struct cgroup *cgp;
	struct list_head *pos;
	struct list_head *cgp_header;
	struct cgroup_subsys *cgp_sub, *temp_cgp_sub;
	struct cftype *ctp;

	task_leader = find_task_by_vpid(1);
	if(task_leader == NULL) return 1;
	temp = task_leader->diff_color_next;

	if(task_leader->color != color){
		if(temp == NULL) return -1;
		while(temp != task_leader && temp->color != color){
			temp = temp->diff_color_next;
			if(temp == NULL) return -1;
		}

		if(temp->color == color)
			task_leader = temp;
		else return -1;
	}

	task = task_leader;

	do{
		printk("cricle: %d\n", cricle++);
		flag = 0;
		cgp = task->cgroups->subsys[1]->cgroup;
		cgp_header = &(cgp->root->subsys_list);
		
		list_for_each(pos, cgp_header){
			if(pos == NULL)
				return -5 - flag;
			temp_cgp_sub = list_entry(pos, struct cgroup_subsys, sibling);
			printk("current_cgroup_subsys_name: %s, flag: %d\n", temp_cgp_sub->name, ++flag);
			if(!(strcmp(temp_cgp_sub->name, "cpu")))
				break;
		}

		cgp_sub = temp_cgp_sub;
		printk("cg_subsys_name: %s\n", cgp_sub->name);
		ctp = cgp_sub->base_cftypes;
		if((++ctp)->write_s64 == NULL) return 5;
		printk("cftype name: %s\n", ctp->name);
		ret = ctp->write_s64(cgp, ctp, quota);
		task = task->same_color_next;
	}while(task != task_leader && task != NULL);

	return ret;
}
