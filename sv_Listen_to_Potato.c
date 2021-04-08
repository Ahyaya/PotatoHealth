#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#define MAXDATASIZE 256

FILE *fp;
static int potato_is_up=0;
static char str_time[128];

int load_meta(){
	fprintf(fp,"<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n");
	fprintf(fp,"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=10.0\">\n");
	return 0;
}

int load_css(){
	fprintf(fp,"<style>\n");
	fprintf(fp,"\t@import\"css/general.css\";\n");
	fprintf(fp,"\t@import\"css/LED.css\";\n");
	fprintf(fp,"\t@import\"css/battery.css\";\n");
	fprintf(fp,"\t@import\"css/status.css\";\n");
	fprintf(fp,"\t@import\"css/roundbar.css\";\n");
	fprintf(fp,"</style>\n");
	return 0;
}

int load_ctime(){
	time_t var_time_t=time(NULL);
    const struct tm *ptr_local_time=localtime(&var_time_t);
    strftime(str_time,128,"%B %d %Y <b style=\"color:yellow\">%H:%M</b> %Z",ptr_local_time);
    fprintf(fp,"<p class=\"ctime\">%s</p><br>\n",str_time);
	return 0;
}

int status_update(float stat_bty, float stat_cpu, float stat_ram, float stat_dsk, int qual){
	FILE *css_write;
	int pf=0, rgb_r, rgb_g, cpuL, cpuR, ramL, ramR, dskL,dskR, qual_r, qual_g, stat_qual=(int)(2.5*qual+225);
	stat_qual<0?0:stat_qual;
	stat_qual>100?100:stat_qual;
	rgb_r=stat_bty>50?(int)(((100-stat_bty)*0xff+(stat_bty-50)*0xcc)/50.0):(0xff);
	rgb_g=stat_bty>50?(0xff):(int)((stat_bty*0xff)/50.0);
	cpuL=stat_cpu>50?180:(int)(stat_cpu*3.6);
	cpuR=stat_cpu>50?(int)(stat_cpu*3.6-180):0;
	ramL=stat_ram>50?180:(int)(stat_ram*3.6);
	ramR=stat_ram>50?(int)(stat_ram*3.6-180):0;
	dskL=stat_dsk>50?180:(int)(stat_dsk*3.6);
	dskR=stat_dsk>50?(int)(stat_dsk*3.6-180):0;
	qual_r=stat_qual>50?(int)(((100-stat_qual)*0xff+(stat_qual-50)*0xcc)/50.0):(0xff);
	qual_g=stat_qual>50?(0xff):(int)((stat_qual*0xff)/50.0);
	while((css_write=fopen("/html/css/status.css","w"))==NULL){++pf;usleep(200000);if(pf>3){return -1;}}
	fprintf(css_write,"@keyframes btykf{0\%{width:0\%;background-color:#000;}100\%{width:%d\%;background-color:#%02x%02x33;}}\n",(int) stat_bty,rgb_r,rgb_g);

	fprintf(css_write,"@keyframes cpukf_l{0\%{border-color:#000;transform:rotate(0);}100\%{border-color:red;transform:rotate(%ddeg);}}\n",cpuL);
	fprintf(css_write,"@keyframes cpukf_r{0\%{border-color:red;transform:rotate(0);}100\%{border-color:red;transform:rotate(%ddeg);}}\n",cpuR);

	fprintf(css_write,"@keyframes ramkf_l{0\%{border-color:#000;transform:rotate(0);}100\%{border-color:orange;transform:rotate(%ddeg);}}\n",ramL);
	fprintf(css_write,"@keyframes ramkf_r{0\%{border-color:orange;transform:rotate(0);}100\%{border-color:orange;transform:rotate(%ddeg);}}\n",ramR);

	fprintf(css_write,"@keyframes dskkf_l{0\%{border-color:#000;transform:rotate(0);}100\%{border-color:green;transform:rotate(%ddeg);}}\n",dskL);
	fprintf(css_write,"@keyframes dskkf_r{0\%{border-color:green;transform:rotate(0);}100\%{border-color:green;transform:rotate(%ddeg);}}\n",dskR);
	fprintf(css_write,"@keyframes qualkf{0\%{color:black;}100\%{color:#%02x%02x33;}}\n",qual_r,qual_g);
	fclose(css_write);
	return 0;
}

int load_status_bar(){
	fprintf(fp,"<div class=\"stat_bar\">\n");
	fprintf(fp,"<div class=\"status_area\"><div class=\"leftblock wrap\"><div class=\"leftcircle cpu_l\"></div></div><div class=\"rightblock wrap\"><div class=\"rightcircle cpu_r\"></div></div><b class=\"digitbar\">CPU</b></div>\n");
	fprintf(fp,"<div class=\"status_area\"><div class=\"leftblock wrap\"><div class=\"leftcircle ram_l\"></div></div><div class=\"rightblock wrap\"><div class=\"rightcircle ram_r\"></div></div><b class=\"digitbar\">RAM</b></div>\n");
	fprintf(fp,"<div class=\"status_area\"><div class=\"leftblock wrap\"><div class=\"leftcircle dsk_l\"></div></div><div class=\"rightblock wrap\"><div class=\"rightcircle dsk_r\"></div></div><b class=\"digitbar\">ROM</b></div>\n");
	fprintf(fp,"</div>\n");
	return 0;
}

int html_potato_down()
{
	fprintf(fp,"<!DOCTYPE html>\n<html>\n<head>\n");
	load_meta();
	load_css();
	fprintf(fp,"</head>\n");
	fprintf(fp,"<body>\n");
    fprintf(fp,"<h2 class=\"headline\">Potato Health Check</h2><br>\n");
	load_ctime();
    fprintf(fp,"<table><tbody>\n");
	fprintf(fp,"<tr><td><span class=\"led onred\"></span><b>My Potato</b></td><td class=\"addr_pub\" colspan=\"2\">status: <b class=\"fatal\">signal lost</b></td></tr>\n");
	fprintf(fp,"</tbody></table>\n</body>\n</html>\n");
    return 0;
}

int html_potato_health(char *ip_public, int port_public, char *msg)
{
	float stat_bty=0,stat_cpu=0,stat_ram=0,stat_dsk=0;
	char *opt=strdup(msg), *ip_local, *essid;
	int qual=-90;
	ip_local=strsep(&opt,"#");
	stat_bty=atof(strsep(&opt,"#"));
	stat_cpu=atof(strsep(&opt,"#"));
	stat_ram=atof(strsep(&opt,"#"));
	stat_dsk=atof(strsep(&opt,"#"));
	qual=(int)atof(strsep(&opt,"#"));
	essid=strsep(&opt,"#");
	free((void*)opt);
	stat_bty<1?1:stat_bty;
	stat_bty>100?100:stat_bty;
	stat_cpu<1?1:stat_cpu;
	stat_cpu>100?100:stat_cpu;
	stat_ram<1?1:stat_ram;
	stat_ram>100?100:stat_ram;
	stat_dsk<1?1:stat_dsk;
	stat_dsk>100?100:stat_dsk;
	qual=qual<-90?-90:qual;
	status_update(stat_bty,stat_cpu,stat_ram,stat_dsk,qual);

	fprintf(fp,"<!DOCTYPE html>\n<html>\n<head>\n");
	load_meta();
	load_css();
	fprintf(fp,"</head>\n");
	fprintf(fp,"<body>\n");
    fprintf(fp,"<h2 class=\"headline\">Potato Health Check</h2><br>\n");
	load_ctime();

    fprintf(fp,"<table><tbody>\n");
    fprintf(fp,"<tr><td><span class=\"led ongreen\"></span><b style=\"margin:0 100px 0 0\">My Potato</b></td>\n");
    fprintf(fp,"<td class=\"digit_blue\">%d\%<div class=\"contain_battery\"><div class=\"status_battery\"></div><div class=\"head_battery\"></div></div></td></tr>\n",(int) stat_bty);
	fprintf(fp,"</tbody></table><br><br>\n");

	load_status_bar();
    fprintf(fp,"<br><br><table><tbody>\n");
	fprintf(fp,"<tr><td class=\"varName\">SSID</td><td>%s <span class=\"qual\">%d dBm</span></td></tr>\n", essid, qual);
	fprintf(fp,"<tr><td class=\"varName\">LAN</td><td>%s</td></tr>\n",ip_local);
	fprintf(fp,"<tr><td class=\"varName\">PUB</td><td>%s:%d (udp)</td></tr>\n",ip_public,port_public);
	fprintf(fp,"</tbody></table>\n\n</body>\n</html>\n");
    return 0;
}

int main(int argc, char *argv[]){
    if(argc<2){printf("Usage: %s port_to_listen\n",argv[0]);return -1;}
    int sockfd;
    struct sockaddr_in server;
    struct sockaddr_in client;
    socklen_t len;
    int num;
    int PORT=atoi(argv[1]);
    char buf[MAXDATASIZE], msg_reply[MAXDATASIZE];
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
	return -1;
    }
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    struct timeval timeout;
    timeout.tv_sec = 40; timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
    {
        return -1;
    }

    if(bind(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
	return -1;
    }
    
    len = sizeof(client);
    while(1)
    {
        if((num = recvfrom(sockfd, buf, MAXDATASIZE, 0, (struct sockaddr *)&client, &len)) < 0)
        {
	    if(potato_is_up){
		while((fp=fopen("/html/potato.html","w"))==NULL){usleep(50000);}
		html_potato_down();
		fclose(fp);
		potato_is_up=0;
	    }
        }else{
            buf[num] = '\0';
	    while((fp=fopen("/html/potato.html","w"))==NULL){usleep(50000);}
	    html_potato_health(inet_ntoa(client.sin_addr),htons(client.sin_port),buf);
	    fclose(fp);
	    potato_is_up=1;
        }
    }
    close(sockfd);
}
