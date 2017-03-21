#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/cred.h>
#include <linux/decompress/mm.h>
#include <uapi/asm-generic/errno-base.h>
#include <asm/uaccess_64.h>
#include "sched/sched.h"

#define EXIT_SUCCESS 0

/**
 * nr_pids: number of process; pids: array of pids of processes
 * to get nice values from; nices: array of nice values to store
 * requested nice values of processes; retval: array ro store return
 * values, -EINVAL for NULL task
 */
asmlinkage long sys_getnicebypid(int nr_pids, pid_t *pids, int *nices, int *retvals){
    int i;
    pid_t *kpids = (pid_t*)malloc(nr_pids*sizeof(pid_t));
    int *knices = (int*)malloc(nr_pids*sizeof(int));
    struct task_struct *task;

    if(nr_pids < 0) return EFAULT;

    /*Check weather addr is ligeal*/
    if(copy_from_user(kpids, pids, nr_pids*sizeof(pid_t)) != 0)
        return EFAULT;
    if(copy_from_user(knices, nices, nr_pids*sizeof(int)) != 0)
        return EFAULT;
    
    for(i = 0; i < nr_pids; ++i){
        task = find_task_by_vpid(pids[i]);
        if(task == NULL){
            retvals[i] = -EINVAL;
            continue;
        }
        nices[i] = _TASK_NICE(task);
        retvals[i] = 0;
    }
    
    return EXIT_SUCCESS;
}

/**
 * color: colors to search
 * nice: nice value to set
 */
asmlinkage long sys_setnicebycolor(u_int64_t color, int nice){
    struct task_struct *start_task;
    struct task_struct *temp;
    struct task_struct *finded_task;
    int _find = 0;

    start_task = find_task_by_vpid(1);
    temp = start_task;

    if(temp == NULL)
        return -1;

    while(temp->diff_color_next != start_task){
        if(temp->color == color){
            _find = 1;
            break;
        }
        temp = temp->diff_color_next;
    }

    if(temp->diff_color_next == start_task && temp->color == color)
        _find = 1;

    if(_find == 0)
        return -2;

    finded_task = temp;
    finded_task->static_prio = NICE_TO_PRIO(nice);
    temp = temp->same_color_next;

    if(temp == NULL)
        return 1;

    while(temp != finded_task){
        temp->static_prio = NICE_TO_PRIO(nice);
        temp = temp->same_color_next;
    }

    return 0;
}
