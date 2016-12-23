#define _GNU_SOURCES
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
//#include <time.h>
//#include <fcntl.h>
#include <pthread.h>

#include <net/if.h>
#include <linux/ip.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
//#include <poll.h>

#include <dirent.h>

#include "rb_tree.h"

#define	DEBUG

struct rb_tree	stat_tree;


void stat_print_file(char *fpath);
void stat_print_all(char *path);

int 	net_createsocket(int *socketfd);
//void	scan_packets(int socketfd, );
void scan_packets(int socketfd);
int		net_own_ip(void);
void	scan_start(void);
void	scan_stop(void);
void 	Log_Write(char *msg);




int DaemonProc(void);



/*-----------------------------------------------------------------------------------*/
#define	CFG_FILENAME	"/etc/daemon1/config.cfg"
#define	IFACE_NAME_LEN	20
char	iface_name[IFACE_NAME_LEN];
char	iface_def[]="eth0";

void save_cfg(char *iface_name) {
	FILE	*fp;
	struct stat st = {0};

	if (stat("/etc/daemon1", &st) == -1) {
		if (mkdir("/etc/daemon1",0777) == -1) {
			puts("can't create directory /etc/daemon1");
			exit(-1);
		}

	}

	if ((fp = fopen(CFG_FILENAME,"w")) != NULL) {
		fputs(iface_name, fp);
		fclose(fp);
	}
	else
		puts("can't open config file");
}

int get_cfg(void) {
	FILE	*fp;

	if (access(CFG_FILENAME,0) == 0) { //file exist

		if ((fp = fopen(CFG_FILENAME,"r")) != NULL) {
			if (fgets(iface_name, IFACE_NAME_LEN, fp) != NULL) {
				return 1;
			}
			Log_Write("can't read file");
		}
		else
			Log_Write("can't open config file for reading");
	}
	else {//config file not exist set default iface
		strcpy(iface_name, iface_def);
		return 1;
	}
	return 0;

}
/*-----------------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------------*/
void load_stat(struct rb_tree *tree) {
	struct stat st = {0};
	char	stat_path[64];

	if (stat("/etc/daemon1/stat", &st) == -1) {
		if (mkdir("/etc/daemon1/stat",0777) == -1) {
			Log_Write("can't create directory /etc/daemon1/stat");
			return;
		}
	}

	if (strlen(iface_name) > 0) {
		strcpy(stat_path,"/etc/daemon1/stat/");
		strcat(stat_path,iface_name);
		//puts(stat_path);
		rb_loadfromfile(stat_path, tree);
	}

}

void save_stat(struct rb_tree tree){
	struct stat st = {0};
	char	stat_path[64];

	if (stat("/etc/daemon1/stat", &st) == -1) {
		if (mkdir("/etc/daemon1/stat",0777) == -1) {
			Log_Write("can't create directory /etc/daemon1/stat");
			return;
		}
	} else {
		if (strlen(iface_name) > 0) {
			strcpy(stat_path,"/etc/daemon1/stat/");
			strcat(stat_path,iface_name);
			//puts(stat_path);
			rb_savetofile(stat_path, tree);
		}
	}
}
/*-----------------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------------*/
void Log_Write(char *msg) {
	time_t	rawtime;
	time(&rawtime);
	struct tm *timeinfo = localtime(&rawtime);
	FILE	*flog;

	if ((flog = fopen("/var/log/mylog.log", "a")) != NULL) {
		//puts("opened");
		//fprintf(flog, "%s\n", msg);

		fprintf(flog,"user %d: %s%s\n", getuid(), asctime(timeinfo), msg);
		fclose(flog);
	}

}
/*-----------------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------------*/
#define	PID_FILE	"/var/run/daemon1.pid"

void SetPIDFile(void) {
	FILE	*f;

	if ((f = fopen(PID_FILE, "w+")) != NULL) {
		fprintf(f, "%u", getpid());
		fclose(f);
	}
	else
		Log_Write("Error:Can't create/access PID file:%s");
}

int GetPIDFromFile(void) {
	FILE	*f;
	unsigned int	pid;

	if ((f = fopen(PID_FILE, "r")) != NULL) {
		fscanf(f,"%u",&pid);

		fclose(f);
		return pid;
	}
	return 0;
}
/*-----------------------------------------------------------------------------------*/

//wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww


/*-----------------------------------------------------------------------------------*/
#define SERVER_PATH     "/tmp/server"
#define BUFFER_LENGTH    250
#define FALSE              0





#define	CMD_START	"START"
#define CMD_STOP	"STOP"
#define CMD_TERM	"TERM"
#define CMD_REPLY	"REPLY"
#define CMD_STAT	"STAT"

#define CMD_SHOW_IP_STATS "SHOW IP STATS"
#define CMD_SHOW_IF_STATS "SHOW IF STATS"
#define CMD_SHOW_IF_ALL_STATS "SHOW IF ALL STATS"

int	cmd_term	= 0;
int	cmd_start	= 0;
int	cmd_stop	= 0;
int cmd_reply   = 0;
int cmd_show_ip_stats = 0;		int cmd_show_ip_stats_ip = 0;
int cmd_show_if_stats = 0;		char cmd_show_if_stats_ifname[20];
int cmd_show_if_all_stats = 0;
int	cmd_stat = 0;

inline void cmd_clear(void) {
	//clear all commands except term
	cmd_start				= 0;
	cmd_stop				= 0;
	cmd_reply   			= 0;
	cmd_show_ip_stats		= 0;
	cmd_show_if_stats		= 0;
	cmd_show_if_all_stats	= 0;
	cmd_stat 				= 0;
}

void cmd_parse(char *cmd) {
	char *pch;
	static struct in_addr addr;

	if (strncmp(cmd, CMD_TERM,  sizeof(CMD_TERM))  == 0)	cmd_term  = 1;
	if (strncmp(cmd, CMD_REPLY, sizeof(CMD_REPLY)) == 0)	cmd_reply = 1;
	if (strncmp(cmd, CMD_START, sizeof(CMD_START)) == 0)	cmd_start = 1;
	if (strncmp(cmd, CMD_STOP,  sizeof(CMD_STOP))  == 0)	cmd_stop  = 1;
	if (strncmp(cmd, CMD_SHOW_IF_ALL_STATS,  sizeof(CMD_SHOW_IF_ALL_STATS))  == 0) cmd_show_if_all_stats  = 1;
	if (strncmp(cmd, CMD_STAT,  sizeof(CMD_STAT))  == 0)	cmd_stat  = 1;


	if (( pch = strchr(cmd,':')) != NULL) {
		//*pch = '\0';

		if (strncmp(cmd, CMD_SHOW_IP_STATS,  sizeof(CMD_SHOW_IP_STATS)-1)  == 0) {
			inet_aton(++pch, &addr);
			//cmd_show_ip_stats_ip = atoi(++pch);
			cmd_show_ip_stats_ip = addr.s_addr;
			cmd_show_ip_stats  = 1;
		}
		//if (strncmp(cmd, CMD_SHOW_IF_STATS,  sizeof(CMD_SHOW_IF_STATS))  == 0) {
		//	strcpy(cmd_show_if_stats_ifname, ++pch);
		//	cmd_show_if_stats  = 1;
		//}
	}
}

void *cmd_thread(void *args) {
	int    sd=-1, sd2=-1,rc;
	char   buffer[BUFFER_LENGTH];
	struct sockaddr_un serveraddr;

	do {

		unlink(SERVER_PATH);

		sd = socket(AF_UNIX, SOCK_STREAM, 0);	//reailable bidirectional data streams
		if (sd < 0) {
			Log_Write("socket() failed");
			//perror("socket() failed");
			break;
		}

		//bind() function gets a unique name for the socket.
		memset(&serveraddr, 0, sizeof(serveraddr));
		serveraddr.sun_family = AF_UNIX;
		strcpy(serveraddr.sun_path, SERVER_PATH);

		rc = bind(sd, (struct sockaddr *)&serveraddr, SUN_LEN(&serveraddr));
		if (rc < 0) {
			Log_Write("bind() failed");
			//perror("bind() failed");
			break;
		}

		// The listen() function allows the server to accept incoming
		// client connections.  In this example, the backlog is set to 1.
		// This means that the system will queue 1 incoming connection
		// requests before the system starts rejecting the incoming requests
		rc = listen(sd, 1);
		if (rc< 0) {
			Log_Write("listen() failed");
			//perror("listen() failed");
			break;
		}

    	#ifdef DEBUG
		Log_Write("Ready for client connect().");
		//printf("Ready for client connect().\n");
		#endif


		do {
			// The server uses the accept() function to accept an incoming
			// connection request.  The accept() call will block indefinitely
			// waiting for the incoming connection to arrive.
			sd2 = accept(sd, NULL, NULL);
			if (sd2 < 0) {
				Log_Write("accept() failed");
				//perror("accept() failed");
				break;
			}

			rc = recv(sd2, buffer, sizeof(buffer), 0);
			if (rc < 0) {
				Log_Write("recv() failed");
				//perror("recv() failed");
				break;
			}
			buffer[rc] = '\0';
			if ((rc > 0) && (rc < sizeof(buffer)))
				cmd_parse(buffer);







			//process commands
			if (cmd_reply) {
				//char mystr[] = "TER2M";
				char ReplyStr[] = "Loooooooooooooooooooooooong str";
				char ReplyStr2[] = "Loooooooooooooooooooooooong str2";

				/*rc = */send(sd2, ReplyStr, strlen(ReplyStr), 0);
				/*rc = */send(sd2, ReplyStr2, strlen(ReplyStr2), 0);
				if (rc < 0) {
					Log_Write("send() failed");
					//perror("send() failed");
					break;
				}
			}


			char	snd_str[64];
			if (cmd_start) {
				scan_start();
				//sprintf(snd_str, "start scan on iface = %s", iface_name);
				sprintf(snd_str, "accepted START  CMD");
				send(sd2, snd_str, strlen(snd_str), 0);
			}

			if (cmd_stop) {
				scan_stop();
				sprintf(snd_str, "accepted STOP  CMD");
				send(sd2, snd_str, strlen(snd_str), 0);
			}

			if (cmd_show_ip_stats) {
				struct in_addr addr;
				addr.s_addr = cmd_show_ip_stats_ip;
				//char str_str


				//sprintf(snd_str, "accepted SHOW IP STATS CMD WITH IP = %d = %s", cmd_show_ip_stats_ip, inet_ntoa(addr));
				//send(sd2, snd_str, strlen(snd_str), 0);

				struct rb_node node;
				if (rb_find(stat_tree, &node, cmd_show_ip_stats_ip)) {
					//struct in_addr addr;
					addr.s_addr = node.ip_addr;
					sprintf(snd_str,"STATISTICS FOR %15s = [%u]", inet_ntoa(addr), node.ip_cnt);
					send(sd2, snd_str, strlen(snd_str), 0);
				}
			}

			if (cmd_stat) {
				send(sd2, CMD_STAT, sizeof(CMD_STAT), 0);
				save_stat(stat_tree);
			}

			/*if (cmd_show_if_stats) {
				sprintf(snd_str, "accepted SHOW IF STATS CMD WITH IF = %s", cmd_show_if_stats_ifname);
				send(sd2, snd_str, strlen(snd_str), 0);
			}

			if (cmd_show_if_all_stats) {
				sprintf(snd_str, "accepted SHOW IF_ALL STATS CMD");
				send(sd2, snd_str, strlen(snd_str), 0);
			}*/


			/*int	cmd_term	= 0;
			int	cmd_start	= 0;
			int	cmd_stop	= 0;
			int cmd_reply   = 0;
			int cmd_show_ip_stats = 0;		int cmd_show_ip_stats_ip = 0;
			int cmd_show_if_stats = 0;		char cmd_show_if_stats_ifname[20];
			int cmd_show_if_all_stats = 0;*/




			//process commands
			cmd_clear();
			close(sd2);
		} while (!cmd_term);
	} while (FALSE);

	if (sd	!= -1) close(sd);
	if (sd2	!= -1) close(sd2);

	unlink(SERVER_PATH);

	pthread_exit(NULL);
	return 0;
}

void	cmd_thread_create(void) {
	pthread_t	hThread;
	int 		id1;

	//properties->c/c++Build->setting->GCC C++ linker->libraries in top part add "pthread"
	if (pthread_create(&hThread, NULL, cmd_thread, &id1) != NULL)
		Log_Write("Can't create cmd thread");
}

void cmd_snd_daemon(char *cmd, int isReply) {
	int    sd=-1, rc, bytesReceived;
	char   buffer[BUFFER_LENGTH];
	struct sockaddr_un serveraddr;

	do {
		sd = socket(AF_UNIX, SOCK_STREAM, 0);
	    if (sd < 0) {
	    	perror("socket() failed");
	    	break;
	    }

	    /*struct timeval timeout;
	    timeout.tv_sec = 50;
	    timeout.tv_usec = 5000;

	    if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
	    	perror("setsockopt() failed");
	    	close(sd);
	    	return;
	    }*/

	    memset(&serveraddr, 0, sizeof(serveraddr));
	    serveraddr.sun_family = AF_UNIX;
	    strcpy(serveraddr.sun_path, SERVER_PATH);

	    // Use the connect() function to establish a connection to the server.
	    rc = connect(sd, (struct sockaddr *)&serveraddr, SUN_LEN(&serveraddr));
	    if (rc < 0) {
	    	perror("connect() failed");
	        break;
	    }

	    rc = send(sd, cmd, strlen(cmd), 0);
	    if (rc < 0) {
	    	perror("send() failed");
	        break;
	    }


	    /*bytesReceived = 0;
	    while (bytesReceived < BUFFER_LENGTH) {
			rc = recv(sd, & buffer[bytesReceived], BUFFER_LENGTH - bytesReceived, 0);
	         if (rc < 0) {
	            perror("recv() failed");
	            break;
	         }
	         else if (rc == 0) {
	            printf("The server closed the connection\n");
	            break;
	         }

	         bytesReceived += rc;	// Increment the number of bytes that have been received so far
	      }*/


	    /*if (isReply) {
		    struct pollfd fd;
		    int ret;
		    fd.fd = sd;
		    fd.events = POLLIN;

	    	do {
	    		ret = poll(&fd,1,1000000);
	    		switch (ret) {
	    			case -1: perror("poll() failed"); break;
	    			case  0: return;	//close sock desk
	    			default:
	    				rc = recv(sd, buffer, BUFFER_LENGTH, 0);
	    				buffer[rc] = '\0';
	    				puts(buffer);
	    				puts("data");
	    		}

	    	} while(ret > 0);
	    }*/



	    /*if (isReply) {
	    	rc = 0;
	    	//do {
	    		rc = recv(sd, buffer, BUFFER_LENGTH, 0);
	    	//} while (rc <= 0);
    		buffer[rc] = '\0';
    		puts(buffer);

	    }*/

	    if (isReply) {
	    	rc = 0;
	    	do {
	    		sleep(1);
	    		rc = recv(sd, buffer, BUFFER_LENGTH, 0);
	    		if (rc > 0) {
	    			buffer[rc] = '\0';
	    			puts("-------------");
	    			puts(buffer);
	    		}

	    	} while (rc > 0);
	    }

	} while (FALSE);

	if (sd != -1) close(sd);
}
/*-----------------------------------------------------------------------------------*/

//struct rb_tree	stat_tree;


/*-----------------------------------------------------------------------------------*/
void Parse_ShowUsage(void) {
	puts("Daemon usage");
	puts("term                 - terminate");
	puts("start                - start sniff packets from default interface");
	puts("stop                 - stop sniff packets");
	puts("show [ip] count      - print number of packets received from ip address");
	puts("select iface [iface] - select interface for sniffing(eth0, wlan0 ...)");
	puts("stat [iface]         - show statistics of particular interface, if iface ommited - for all interfaces");
	puts("--help               - show usage information");
}

void Parse_Cmd(int argc, char *argv[], char *cmd) {
	if (argc > 1) {
		//puts(argv[1]);
		//puts(argv[2]);
		//puts(argv[3]);
		if	(strcmp(argv[1],"term") == 0)	{strcpy(cmd,CMD_TERM);}
		else if	(strcmp(argv[1],"start") == 0)	{strcpy(cmd,CMD_START);}
		else if	(strcmp(argv[1],"stop") == 0)	{strcpy(cmd,CMD_STOP);}

		else if	((strcmp(argv[1],"show") == 0) && (strcmp(argv[3],"count") == 0))	{
			char *ipaddr = argv[2];
			if (strlen(ipaddr) > 0) {

				if (ipaddr[0] == '[') {
					ipaddr++;
					ipaddr[strlen(ipaddr)-1] = '\0';
					strcpy(cmd,CMD_SHOW_IP_STATS);
					strcat(cmd, ":");
					strcat(cmd, ipaddr);
				}
				else {
					strcpy(cmd,CMD_SHOW_IP_STATS);
					strcat(cmd, ":");
					strcat(cmd, ipaddr);
				}
				puts(cmd);
				//cmd[0] = '\0';
			}
		}

		else if	((strcmp(argv[1],"select") == 0) && (strcmp(argv[2],"iface") == 0)){
			char *ifname = argv[3];
			if (strlen(ifname) > 0) {
				if (ifname[0] == '[') {
					ifname++;
					ifname[strlen(ifname) - 1] = '\0';
				}
				save_cfg(ifname);
				exit(0);
			}
		}

		else if	(strcmp(argv[1],"stat") == 0) {strcpy(cmd, CMD_STAT);}


		else if	(strcmp(argv[1],"--help") == 0) {
			Parse_ShowUsage();
			exit(0);
		}
	}
	//else
	//	Parse_ShowUsage();
}
/*-----------------------------------------------------------------------------------*/



/*-----------------------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
	int pid;
	int isDaemonRun = 0;
	struct stat st = {0};
	char	daemon_cmd[64];
	char	tmp[64];

	memset(daemon_cmd,0,64);

	Parse_Cmd(argc, argv, daemon_cmd);


	//cmd_parse("SHOW IP STATS:172.217.20.174");

	//if (strlen(daemon_cmd) > 0) puts(daemon_cmd);
	//return 0;


	//start daemon directory folder structure if still not created
	if (stat("/etc/daemon1", &st) == -1) {
		if (mkdir("/etc/daemon1",0777) == -1) {
			perror("can't create directory /etc/daemon1");
			return -1;
		}
	}
	if (stat("/etc/daemon1/stat", &st) == -1) {
		if (mkdir("/etc/daemon1/stat",0777) == -1) {
			perror("can't create directory /etc/daemon1/stat");
			return -1;
		}
	}




	//Parse_Cmd(argc, argv);



	/*scan_start();
	sleep(10);
	rb_print(stat_tree.root);
	scan_stop();
	sleep(5);
	puts("finita");
	return 0;*/






	//int sock;
	//net_createsocket(&sock);
	//while (1) {scan_packets(sock);}








	//check is Daemon already running to prevent run several copies
	if ((pid = GetPIDFromFile()) != 0) {
		#ifdef DEBUG
		printf("pid = %d\n", pid);
		#endif

		//if pid file exist it's means that daemon process is running
		//but in case daemon process crashed last time and wasn't able delete pid file
		//we must be sure that process with appropriate pid exist
		if (kill(pid,0) == 0) {
			isDaemonRun = 1;
			#ifdef DEBUG
			puts("Daemon is already running");
			#endif
		}else unlink(PID_FILE);
	}


	//Daemon func from unistd may be used for same result but for better process understanding was rejected
	if (!isDaemonRun) {
		//create child process
		if ((pid = fork()) < 0) {
			//child process creation failed
			printf("Error: Start Demon failed (%s)\n", strerror(errno));
			return -1;
		}
		else if (pid == 0) {
			//child process(Demon code)

			//set full rights for file I/O operations to avoid access problem
			umask(0);

			//create new session to be independent from parent
			setsid();//check return

			//relocate to root dir to avoid problem mount/unmount
			chdir("/");//check return

			//close std out/in/err in daemon process they don't used(for saving resources)
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);

			return DaemonProc();
		}

		//if daemon wasn't run before give daemon some time to be ready to accept data
		sleep(1);
	}
	//parent process


	//inet_aton();
	//char mycmd[64];
	//sprintf(mycmd,"%s", CMD_START);
	//sprintf(mycmd,"%s", CMD_STOP);
	//sprintf(mycmd,"%s", CMD_TERM);
	//sprintf(mycmd,"%s", CMD_REPLY);

	//sprintf(mycmd,"%s:%d", CMD_SHOW_IP_STATS, 12345);
	//sprintf(mycmd,"%s:%s", CMD_SHOW_IF_STATS, "wlan1");
	//sprintf(mycmd,"%s", CMD_SHOW_IF_ALL_STATS);

	//cmd_snd_daemon(mycmd,1);


	if (strlen(daemon_cmd) > 0) {
		printf("cmd to send = %s\n", daemon_cmd);
		cmd_snd_daemon(daemon_cmd, 1);


		if (strcmp(daemon_cmd,CMD_STAT) == 0) {
			if (argc == 2) stat_print_all("/etc/daemon1/stat");
			if (argc == 3) {
				if (strlen(argv[2]) > 0) {
					strcpy(tmp,argv[2]);
					if (tmp[0] == '[') {
						tmp[strlen(tmp) - 1] = '\0';
						stat_print_iface("/etc/daemon1/stat", tmp+1);
						puts(tmp+1);
					}
					else {
						stat_print_iface("/etc/daemon1/stat", tmp);
						puts(tmp);
					}

				}
			}
		}
	}

	//sleep(3);





	//stat_print_file("/etc/daemon1/stat/eth0");

	//parent process

	puts("Terminate..."); //parent process terminate
	return EXIT_SUCCESS;
}
/*-----------------------------------------------------------------------------------*/

void stat_print_file(char *fpath) {
	#define STR_SIZE	250
	char	str[STR_SIZE];

	FILE	*fp = fopen(fpath, "r");
	if (fp != NULL) {
		while (fgets(str, STR_SIZE, fp) != NULL) puts(str);
		fclose(fp);
	}
}

void stat_print_all(char *path) {
	struct dirent *de;
	DIR *dir = opendir(path);
	char filepath[250];


	if (dir != NULL) {
		while ((de = readdir(dir)) != NULL) {

			if (strcmp(de->d_name,".") == 0) continue;
			if (strcmp(de->d_name,"..") == 0) continue;
			//puts(de->d_name);
			printf("\n%s statistics\n------------------------\n", de->d_name);

			strcpy(filepath, path);
			strcat(filepath,"/");
			strcat(filepath, de->d_name);
			stat_print_file(filepath);
		}
		closedir(dir);
	}
}

void stat_print_iface(char *path, char *ifname) {
	char filepath[250];
	FILE	*fp;

	strcpy(filepath, path);
	strcat(filepath,"/");
	strcat(filepath, ifname);

	if (!(access(filepath,0) == 0))
		printf("no statistic on %s interface", ifname);
	else
		stat_print_file(filepath);

}


/*-----------------------------------------------------------------------------------*/
int fterminate = 0;
/*void sig_proc(int sig) {
	fterminate = 1;
	Log_Write("signal accepted");
}*/

void sig_proc(int sig, siginfo_t *siginfo, void *context) {
	fterminate = 1;
	Log_Write("signal accepted");
}


//void (*sa_sigaction) (int i, siginfo_t *si, void *pv);

int DaemonProc(void) {
	//int		pid;
	//int		status;
	//int		need_start;


	//int			signo;
	//int			status;


	struct sigaction	sigact;
	sigact.sa_sigaction = sig_proc;
	sigact.sa_flags = SA_SIGINFO;

	//sigemptyset(&sigact.sa_mask);


	sigaddset(&sigact.sa_mask, SIGQUIT);	//Terminate by user signal


	//sigaddset(&sigact.sa_mask, SIGINT);		//Terminate by user from terminal
	//sigaddset(&sigact.sa_mask, SIGTERM);	//Terminate request signal
	sigprocmask(SIGQUIT, &sigact.sa_mask, NULL);

	sigaction(SIGQUIT, &sigact, 0);
	//sigaction(SIGINT,  &sigaction, 0);
	//sigaction(SIGTERM, &sigaction, 0);


	//sigset_t	sigset;
	//sigemtyset(&sigset);

	//sigaddset(&sigset, SIGQUIT);	//Terminate by user signal
	//sigaddset(&sigset, SIGINT);		//Terminate by user from terminal
	//sigaddset(&sigset, SIGTERM);	//Terminate request signal
	//sigaddset(&sigset, SIGCHLD);	//Received when child process status changed

	//sigact.sa_mask = sigset;


	//sigset_t	sigset;
	//siginfo_t	siginfo;


	//WWWWWWWWWWWWWWWWWWWWWWW Don't forget describe critical signal handlers
	/*sigemptyset(&sigset);

	sigaddset(&sigset, SIGQUIT);	//Terminate by user signal
	sigaddset(&sigset, SIGINT);		//Terminate by user from terminal
	sigaddset(&sigset, SIGTERM);	//Terminate request signal
	sigaddset(&sigset, SIGCHLD);	//Received when child process status changed

	sigaddset(&sigset, SIGUSR1);	//User defined signal that we will use for config update
	sigprocmask(SIG_BLOCK, &sigset, NULL);*/





	Log_Write("daemon started");
	SetPIDFile();


	cmd_thread_create();




	do{
		usleep(100000);	//100ms
	}while(!cmd_term);

	//sleep(5);


	unlink(PID_FILE);				//Delete PID file name from file system because daemon terminate
	Log_Write("daemon stopped");

	return 0;
}
/*-----------------------------------------------------------------------------------*/




/*-----------------------------------------------------------------------------------*/
int 			snif_sock;
//struct rb_tree	stat_tree;
int				own_ip;
pthread_t		hScanThread = 0;
//static pthread_mutex_t tree_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
//static pthread_mutex_t tree_mutex = PTHREAD_MUTEX_RECURSIVE_NP;
static pthread_mutex_t tree_mutex = PTHREAD_MUTEX_INITIALIZER;

int net_createsocket(int *socketfd) {
	int sd;
	/*
	 * PF_PACKET - Protocol family (PF_INET PF_INET6 )
	 * SOCK_RAW  - Socket Type
	 * ETH_P_ALL - Protocol ID
	 *
	 * PF_PACKET - low level packet
	 * SOCK_RAW	 - Socket Type RAW(direct access to network interface)
	 * ETH_P_ALL - receive all ethernet frames
	 */

	sd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sd == -1) {
		Log_Write("scan socket not created\n");
		return 0;
	}
	else {
		//printf("socket created fd = %d\n", sd);
		*socketfd = sd;

		/* Bind to [iface] interface only*/
		int rc;
		struct ifreq ifr;
		memset(&ifr, 0, sizeof(ifr));
		snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "eth0");

		if ((rc = setsockopt(sd, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr))) < 0) {
			Log_Write("Server-setsockopt() error for SO_BINDTODEVICE");
		    //perror("Server-setsockopt() error for SO_BINDTODEVICE");
		    //printf("%s\n", strerror(errno));
		    close(sd);
		    exit(-1);
		}
		return 1;
	}
}

void scan_packets(int socketfd) {
	char	buffer[ETH_FRAME_LEN];
	struct ether_header *eh	 = (struct ether_header *)buffer;
	struct iphdr		*iph = (struct iphdr *)(buffer + sizeof(struct ether_header));
	struct in_addr		addr;

	if (recv(socketfd, buffer, ETH_FRAME_LEN, 0) > 0) {
		addr.s_addr = iph->saddr;

		if (((unsigned long)addr.s_addr) == ((unsigned long)own_ip)) {
			//this is our incomiing packet
			printf("accepted %d %s\n", iph->saddr, inet_ntoa(addr));
			addr.s_addr = iph->daddr;
			printf("accepted %d %s\n", iph->daddr, inet_ntoa(addr));


			//rb_insert(&stat_tree, addr.s_addr);
			pthread_mutex_lock(&tree_mutex);
			rb_insert(&stat_tree, iph->daddr);
			pthread_mutex_unlock(&tree_mutex);
		}
		//printf("accepted ------ %d %s\n", iph->saddr, inet_ntoa(addr));
		//puts("accepted");
	}

}

int net_own_ip(void) {
	struct ifreq ifr;
	int fd = socket(PF_INET, SOCK_DGRAM, 0/*IPPROTO_IP*/);
	struct sockaddr_in sain;

	ifr.ifr_addr.sa_family = AF_INET;

	strcpy(ifr.ifr_ifrn.ifrn_name, iface_name);

	ioctl(fd, SIOCGIFADDR, &ifr);

	close(fd);

	sain.sin_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;

	own_ip = sain.sin_addr.s_addr;

	/*printf("iface name = %s ::: own ip = %s\n", iface_name, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	printf("%d\n", own_ip);
	printf("%d\n", sain.sin_addr);*/
	return 0;
}

void *scan_thread(void *args) {
	int sd;

	//clear tree data
	Log_Write("Thread created");
	rb_clear(&stat_tree);

	//read config from file to determine iface name
	get_cfg();

	//get interface ip
	net_own_ip();

	//load past statistic for this interface
	load_stat(&stat_tree);

	//create socket for sniffing
	if (net_createsocket(&sd)) {
		Log_Write("scan packets run");
		for(;;) {
			scan_packets(sd);
		}
	}
	pthread_exit(NULL);
	return 0;
}

void	scan_start(void) {  //create thread to start scan incoming packets
	int 		id1;

	if (hScanThread == 0) {
		//properties->c/c++Build->setting->GCC C++ linker->libraries in top part add "pthread"
		if (pthread_create(&hScanThread, NULL, scan_thread, &id1) != NULL) {
			hScanThread = 0;
			Log_Write("Can't create scan thread");

			//puts("started");
		}
	}
}

void	scan_stop(void) {
	if (hScanThread != 0) {

		pthread_mutex_lock(&tree_mutex);
		//if (pthread_kill(hScanThread, SIGKILL) == 0) {
		if (pthread_cancel(hScanThread) == 0) {
			save_stat(stat_tree);
			hScanThread = 0;
		}
		pthread_mutex_unlock(&tree_mutex);

	}
}
/*-----------------------------------------------------------------------------------*/





































//char	workdir[100]; if (getcwd(workdir,100) != NULL) puts(workdir);
/*get_cfg(); puts(iface_name); save_cfg("eth1"); get_cfg(); puts(iface_name); save_cfg("wlan0"); get_cfg(); puts(iface_name);
struct rb_tree tree1; tree1.root = 0; tree1.count = 0; rb_insert(&tree1, 11); rb_insert(&tree1, 12); rb_insert(&tree1, 12);
save_stat(tree1); load_stat(&tree1); rb_print(tree1.root); return 0;*/




