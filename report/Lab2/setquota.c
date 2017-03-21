#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <asm/unistd_64.h>

#define __NR_getcolors 319
#define MAX_PID_NR 128

/**
 * Search in directory '/proc' to find process id with color 'color'
 */
int SetTask(u_int16_t color, char *file_name)
{
    printf("color to be searched: %d\n", color);
    printf("file_name: %s\n", file_name);
    if(color == 0) return -1;
    FILE *f = fopen(file_name, "w");
    if(f == NULL) return -1;
    
    int ret[1], pids[1];
    u_int16_t colors[1];
	struct dirent *dirp;
	DIR *dp = opendir("/proc");
	if (dp == NULL)
	{
		printf("Fail to open /proc\n");
		return -1;
	}
	while (dirp = readdir(dp))
	{
		int pid = atoi(dirp->d_name);
		int cmd_fd = -1;
		if (pid <= 0) continue;
        pids[0] = pid;
	    syscall(__NR_getcolors, 1, pids, colors, ret);
        if(colors[0] == color)
            fprintf(f, "%d\n", pid);
	}
    fclose(f);
	return 0;
}

//return the file directory
char* AddCgroup(int color, int quota){
    char dir_name[] = "/dev/cpuctl/";
    char file_name[] = "/dev/cpuctl/";
    char color_str[20];
    sprintf(color_str, "%d", color);
    const char dir_postfix[] = "/cpu.cfs_quota_us";
    const char dir_postfix_tasks[] = "/tasks";
    char *retstr = (char *)malloc(100*sizeof(char));
    int len, i;

    strcat(dir_name, color_str);
    strcat(file_name, color_str);
    
    mkdir(dir_name, S_IRWXG);
    strcat(dir_name, dir_postfix);

    FILE *f = fopen(dir_name, "r+");
    fprintf(f, "%d\n", quota);
    fclose(f);
    
    strcat(file_name, dir_postfix_tasks);
    printf("file_name : %s\n", file_name);

    len = strlen(file_name);
    for(i = 0; i < len; ++i)
        retstr[i] = file_name[i];
    retstr[len] = '\0';

    return retstr;
}

int main(int argc, char *argv[])
{
    int color = atoi(argv[1]);
    int quota = atoi(argv[2]);
    printf("[main] color: %d, quota: %d\n", color, quota);
    char *file_name;
    file_name = AddCgroup(color, quota);
    if(SetTask(color, file_name) == 0)
        printf("Success!\n");
    else
        printf("Failed!\n");

	return 0;
}
