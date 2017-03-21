#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/list.h>
#include <linux/decompress/mm.h>
#include <uapi/asm-generic/errno-base.h>
#include <asm/uaccess_64.h>
#define EXIT_SUCCESS 0

/* nr_pids contains the number of entries in
   the pids, colors, and the retval arrays. The colors array contains the
   color to assign to each pid from the corresponding position of
   the pids array. Return 0 if all set color requests
   succeed. Otherwise, the array retval contains per-request
   error codes -EINVAL for an invalide pid, or 0 on success.
*/
asmlinkage long sys_setcolors(int nr_pids, pid_t *pids, u_int16_t *colors, int *retval){
    int i;
    pid_t *kpids = (pid_t*)malloc(nr_pids*sizeof(pid_t));
    u_int16_t *kcolors = (u_int16_t*)malloc(nr_pids*sizeof(u_int16_t));
    struct task_struct *task, *top_task, *zero_task;
    struct task_struct *temp_1, *temp_2, *temp;//used to search position to insert temp_2 task

    /*Check priority of the current user*/
    if(current_uid() > 500) return -EACCES;

    if(nr_pids < 0) return EFAULT;
    
    /*Check weather addr is ligeal*/
    if(copy_from_user(kpids, pids, nr_pids*sizeof(pid_t)) != 0)
        return EFAULT;
    if(copy_from_user(kcolors, colors, nr_pids*sizeof(u_int16_t)) != 0)
        return EFAULT;
    
    /*Find process which is color 0*/    
    zero_task = find_task_by_vpid(1);
    while(zero_task->color != 0)
        zero_task = zero_task->diff_color_prev;
    
    for(i = 0; i < nr_pids; ++i){
        task = find_task_by_vpid(pids[i]);
        if(task == NULL){
            retval[i] = -EINVAL;
            continue;
        }
        top_task = find_task_by_vpid(task->tgid);

        /*Top task has not been arrenged a color yet*/
        if((top_task->color == 0) && (colors[i] != 0)){
            top_task->color = colors[i];
            task->color = colors[i];
            retval[i] = 1;//Color succeed

            /*Insert a task node of linked list*/
            if(zero_task->diff_color_next == NULL){
                zero_task->diff_color_next = top_task;
                zero_task->diff_color_prev = top_task;
                top_task->diff_color_next = zero_task;
                top_task->diff_color_prev = zero_task;
            }
            else{
                temp_1 = zero_task;
                temp_2 = zero_task->diff_color_next;
                while((colors[i] > temp_2->color) && temp_2 != zero_task){
                    temp_1 = temp_2;
                    temp_2 = temp_1->diff_color_next;
                }
                if(colors[i] < temp_2->color){
                    //no this color before
                    temp_2->diff_color_next = top_task;
                    top_task->diff_color_prev = temp_2;
                    top_task->diff_color_next = temp_1;
                    temp_1->diff_color_prev = top_task;

                    //insert this task to top_task same color list
                    if(task != top_task){
                        task->same_color_next = top_task;
                        top_task->same_color_next = task;
                        task->same_color_prev = top_task;
                        top_task->same_color_prev = task;
                    }
                }
                else if(colors[i] == temp_2->color){
                    //there's same color before, insert the top task into it
                    if(temp_2->same_color_next == NULL){
                        temp_2->same_color_next = top_task;
                        top_task->same_color_next = temp_2;
                        temp_2->same_color_prev = top_task;
                        top_task->same_color_prev = temp_2;
                    }
                    else{
                        temp = temp_2->same_color_next;
                        while(temp != temp_2)
                            temp = temp->same_color_next;
                        temp->same_color_next = top_task;
                        top_task->same_color_prev = temp;
                        top_task->same_color_next = temp_2;
                        temp_2->same_color_prev = top_task;
                    }

                    //then insert current task into it
                    temp = temp_2->same_color_next;
                    while(temp != temp_2)
                        temp = temp->same_color_next;
                    temp->same_color_next = task;
                    task->same_color_prev = temp;
                    task->same_color_next = temp_2;
                    temp_2->same_color_prev = task;
                }
            }
        }

        /*Otherwise other threads must have the same color as the top task*/
        else{
            if(colors[i] == top_task->color){
                task->color = colors[i];
                retval[i] = 1;
            }
            else{
                task->color = top_task->color;
                retval[i] = 0;//Color failed
            }
        }   
    }
    
    return EXIT_SUCCESS;
}

/* Gets the colors of the processes
   contained in the pids array. Returns 0 if all set color requests
   succeed. Otherwise, an error code is returned. The array
   retval contains per-request error codes: -EINVAL for an
   invalid pid, or 0 on success.
*/
asmlinkage long sys_getcolors(int nr_pids, pid_t *pids, u_int16_t *colors, int *retval){
    int i;
    pid_t tmpid;
    pid_t *kpids = (pid_t*)malloc(nr_pids*sizeof(pid_t));
    u_int16_t *kcolors = (u_int16_t*)malloc(nr_pids*sizeof(u_int16_t));
    struct task_struct *task, *top_task;
    
    if(nr_pids < 0) return EFAULT;
    
    for(i = 0; i < nr_pids; ++i){
        if(pids[i] < 0)
            return EFAULT;
    }
    
    if(copy_from_user(kpids, pids, nr_pids*sizeof(pid_t)) != 0)
        return EFAULT;
    if(copy_from_user(kcolors, colors, nr_pids*sizeof(u_int16_t)) != 0)
        return EFAULT;
    for(i = 0; i < nr_pids; ++i){
        tmpid = pids[i];
        task = find_task_by_vpid(tmpid);
        if(task == NULL){
            retval[i] = -EINVAL;
            continue;
        }
        top_task = find_task_by_vpid(task->tgid);
        if(task->color != 0){
            colors[i] = task->color;
            retval[i] = 1;
        }
        else{
            if(top_task->color != 0){
                task->color = top_task->color;
                colors[i] = task->color;
                retval[i] = 2;
            }
            else{
                colors[i] = 0;
                retval[i] = 3;
            }
        }
    }
    
    return EXIT_SUCCESS;
}