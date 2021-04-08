#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <linux/wireless.h>

#define LOCALHOST "127.0.0.1"
#define SUGARPORT 8423
#define SUGARWORD "get battery"
#define WLAN0 "wlan0"

char *ip_server;
int port_server;

typedef struct CPU_PACKED
{
    char name[20];  
    unsigned int user;
    unsigned int nice;
    unsigned int system;
    unsigned int idle;
}CPU_OCCUPY;

float cal_cpuoccupy(CPU_OCCUPY *o,CPU_OCCUPY *n)
{
    unsigned long od, nd;
    unsigned long id, sd;
    float cpu_use = 0;
    od = (unsigned long)(o->user + o->nice + o->system + o->idle);
    nd = (unsigned long)(n->user + n->nice + n->system + n->idle);

    id = (unsigned long)(n->user - o->user);
    sd = (unsigned long)(n->system - o->system);
    if((nd-od) != 0)
        cpu_use = ((sd+id)*100.0)/(nd-od);
    else 
        cpu_use = 0;
    return cpu_use;
}

void get_cpuoccupy (CPU_OCCUPY *cpust)
{
    FILE *fd;
    int n;
    char buff[256];
    CPU_OCCUPY *cpu_occupy;
    cpu_occupy = cpust;

    fd = fopen("/proc/stat","r");
    fgets(buff,sizeof(buff),fd);
    sscanf(buff,"%s %u %u %u %u",cpu_occupy->name,&cpu_occupy->user,&cpu_occupy->nice,&cpu_occupy->system,&cpu_occupy->idle);
    fclose(fd);
}

int getPi_stat(char *stat)
{
    CPU_OCCUPY cpu_stat1;
    CPU_OCCUPY cpu_stat2;
	struct sysinfo s_info;
	struct statfs diskInfo;
    float cpu,ram,vfs;
	int pf=0;
    get_cpuoccupy((CPU_OCCUPY *)&cpu_stat1);
	while(sysinfo(&s_info)){
		sleep(1);
		if(pf++>5){
			s_info.freeram=0;
			s_info.totalram=1024;
			break;
		}
	}
	ram=100.0*(s_info.totalram-s_info.freeram)/s_info.totalram;
    statfs("/",&diskInfo);
	vfs = 100-100.0*diskInfo.f_bfree/diskInfo.f_blocks;
	sleep(1);
    get_cpuoccupy((CPU_OCCUPY *)&cpu_stat2);
    cpu = cal_cpuoccupy((CPU_OCCUPY *)&cpu_stat1,(CPU_OCCUPY *)&cpu_stat2);
    sprintf(stat,"%.1f#%.1f#%.1f\0",cpu,ram,vfs);
    return 0;
}

int get_battery ( char *battery)
{
    int sockfd, num, pf;
    struct hostent *he;
    struct sockaddr_in server;

    if((sockfd=socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        return -1;
    }

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(SUGARPORT);
    server.sin_addr.s_addr = inet_addr(LOCALHOST);

    struct timeval timeout;
    timeout.tv_sec = 5; timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
    {
        return -1;
    }
    if(connect(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
	close(sockfd);
        return -1;
    }

    send(sockfd, SUGARWORD, strlen(SUGARWORD), 0);
    if((num = recv(sockfd, battery, 32, 0)) == -1)
    {
    }else{
        battery[num-4] = '\0';
    }
    close(sockfd);
    return 0;
}

int get_wlan0_ip (char *ip_addr, int *qual, char *essid)
{
    int sock_fd, device;
    struct ifreq buf[INET_ADDRSTRLEN];
    struct ifconf ifc;
	struct iwreq wreq;
	struct iw_statistics stats;
    char *local_ip = NULL;
	memset(&stats, 0, sizeof(stats));
	memset(&wreq, 0, sizeof(wreq));
	memset(essid,0,64);
	strcpy(wreq.ifr_name, WLAN0);

    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
    {
        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = (caddr_t)buf;
        if (!ioctl(sock_fd, SIOCGIFCONF, (char *)&ifc))
        {
            device = ifc.ifc_len/sizeof(struct ifreq);
            while (device > 0)
            {
				--device;
                if (!(ioctl(sock_fd, SIOCGIFADDR, (char *)&buf[device])))
                {
                    local_ip = NULL;
                    local_ip = inet_ntoa(((struct sockaddr_in*)(&buf[device].ifr_addr))->sin_addr);
                    if(!strcmp(WLAN0,buf[device].ifr_name))
                    {
                        strcpy(ip_addr, local_ip);
						break;
                    }
                }else{
					close(sock_fd);
		    		return -1;
				}
            }
			wreq.u.data.pointer = &stats;
    		wreq.u.data.length = sizeof(stats);
    		if(ioctl(sock_fd, SIOCGIWSTATS, &wreq) == -1)
    		{
				*qual=-90;
    		}else{
				*qual=(signed char)stats.qual.level;
			}
			wreq.u.essid.pointer = essid;
			wreq.u.essid.length = 32;
			if(ioctl(sock_fd,SIOCGIWESSID, &wreq) == -1) {
				strcpy(essid,"unknown\0");
			}

        }else{
	    	close(sock_fd);
	    	return -1;
		}
        close(sock_fd);
    }
    return 0;
}

int health_report(char *word)
{
    int sockfd, num, pf;
    char buf[128];
    struct hostent *he;
    struct sockaddr_in server;

    if((sockfd=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        return -1;
    }
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port_server);

    server.sin_addr.s_addr = inet_addr(ip_server);
    struct timeval timeout;
    timeout.tv_sec = 5; timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
    {
	close(sockfd);
        return -1;
    }

    if(connect(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
	close(sockfd);
        return -1;
    }
    send(sockfd, word, strlen(word), 0);
    close(sockfd);
    return 0;
}

int main(int argc, char *argv[]){
    if(argc<2){printf("Usage: %s server_ip:port\n",argv[0]);return -1;}

    int pf=0, qual;
    char cnull[]="N/A", word_to_snd[128]={0};
    char ip_wlan0[INET_ADDRSTRLEN]={0}, check_battery[32]={0}, *value_battery, pi_stat[32], essid[64];
    char *input_opt=strdup(argv[1]);
    
    ip_server=strsep(&input_opt,":");
    port_server=atoi(strsep(&input_opt,":"));
    free((void*)input_opt);

    while(1){
	if(get_wlan0_ip(ip_wlan0,&qual,essid)){
	    ip_wlan0[0]='N';ip_wlan0[1]='/';ip_wlan0[2]='A';ip_wlan0[3]=0;
	}
	if(get_battery(check_battery)){
	   value_battery=cnull;
	}else{
	    value_battery=check_battery+9;
	}
	getPi_stat(pi_stat);
	sprintf(word_to_snd,"%s#%s#%s#%d#%s",ip_wlan0,value_battery,pi_stat,qual,essid);

	if(health_report(word_to_snd)){sleep(1);health_report(word_to_snd);}
		sleep(30);
    }

    return 0;
}
