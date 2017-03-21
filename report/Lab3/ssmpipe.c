#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <asm/unistd_64.h>

#define __NR_ssmem_attach 325
#define __NR_ssmem_read 327
#define __NR_ssmem_write 328

#define LEN 1024

const char *str_create = "create";
const char *str_write = "write";
const char *str_read = "read";

int main(int argc, char **argv){
    char *opt = (char *)malloc(LEN*sizeof(char));
    char *name = (char *)malloc(LEN*sizeof(char));
    char *data = (char *)malloc(LEN*sizeof(char));
    unsigned long addr;
    int flag;
    int length;
    long ret;
    
    while(1){
        printf("input opeartion option: [create] or [write] or [read]\n");
        scanf("%s", opt);
        if(!strcmp(opt, str_create)){
            flag = 1;
            printf("input ssmem name:\n");
            scanf("%s", name);
            printf("input ssmem length(4K aligned recommanded):\n");
            scanf("%d", &length);
            addr = syscall(__NR_ssmem_attach, name, flag, length);
            if((long)addr > 0){
                printf("create success!\n");
                printf("ret : 0x%lx\n", addr);
            }
            else
                printf("create failed!\n");
        }else if(!strcmp(opt, str_write)){
            flag = 2;
            printf("input ssmem name:\n");
            scanf("%s", name);
            addr = syscall(__NR_ssmem_attach, name, flag, 1);
            if((long)addr > 0){
                printf("visit success!\n");
                printf("ret : 0x%lx\n", addr);
                printf("input your data to wirte:\n");
                scanf("%s", data);
                ret = syscall(__NR_ssmem_write, (void *)addr, data);
                printf("addr after write: %s\n", (char *)addr);
            }
            else
                printf("visit failed!\n");
        }else if(!strcmp(opt, str_read)){
            flag = 0;
            printf("input ssmem name:\n");
            scanf("%s", name);
            addr = syscall(__NR_ssmem_attach, name, flag, 1);
            if((long)addr > 0){
                printf("visit success!\n");
                printf("ret : 0x%lx\n", addr);
                ret = syscall(__NR_ssmem_read, (void *)addr, data);
                printf("read data : %s\n", data);
                printf("ret : %d\n", ret);
            }
            else
                printf("visit failed!\n");
        } 
    }
    
    return 0;
}
