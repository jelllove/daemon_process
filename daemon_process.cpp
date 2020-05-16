
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#define MAX_PROCESS_NUM 500


#pragma pack(1)

class ChildProcess
{
public:
	pid_t m_pid;

	int m_argc;
	char **m_argv;
	char m_chExe[1024];

	ChildProcess()
	{
		strcpy(m_chExe, "");
		m_pid = -1;
		m_argc = 0;
		m_argv = NULL;
	}

	void Kill()
	{
		if (m_pid > 0)
		{
			kill(m_pid, SIGQUIT);
			m_pid = -1;
		}
		
		if (m_argv != NULL)
		{
			for (int i = 0; i < m_argc; i++)
			{
				delete []m_argv[i];
				m_argv[i] = NULL;
			}

			delete []m_argv;
			m_argv = NULL;
		}
	}
};

#pragma pack()


//char **g_parg;
int g_pid_num;
ChildProcess *g_pid_group;

void MustExit(int nSignal)
{
	if (nSignal == SIGUSR2)
	{
		//i don't care the child process any more
		signal(SIGCHLD, SIG_IGN);

		for (int i = 0; i < g_pid_num; i++)
		{
			if (g_pid_group[i].m_pid > 0)
			{
				g_pid_group[i].Kill();
				//kill all alive child process
//				kill(g_pid_group[i], SIGQUIT);
//				g_pid_group[i] = -1;
			}
		}

		delete []g_pid_group;

		exit(0);
	}
}

void ChildExit(int nSignal)
{
	if (nSignal == SIGCHLD)
	{
		int nStatus = 0;

		//find all exit process
		for (int i = 0; i < g_pid_num; i++)
		{
			if (g_pid_group[i].m_pid > 0)
			{
				pid_t pWait = waitpid(g_pid_group[i].m_pid, &nStatus, WNOHANG);

				if (pWait != 0)
				{
					printf("child process '%d' exit by status: %d, it is %s\n", pWait, nStatus, WIFEXITED(nStatus) ? "normal exit" : "abnormal exit");

					if (WIFEXITED(nStatus))
					{
						//normal exit
						g_pid_group[i].Kill();
                        g_pid_num--;
					}
					else
					{
						g_pid_group[i].m_pid = -2;
						printf("daemon process is trying restart the child process...\n");
						pid_t pNew = fork();

						if (pNew < 0)
						{
							g_pid_group[i].Kill();
							printf("failed to fork a new child process.\n");
						}
						else if (pNew == 0)
						{
							if (-1 == execvp(g_pid_group[i].m_chExe, g_pid_group[i].m_argv))
							{
								printf("failed to start process(%s), error:%s", g_pid_group[i].m_chExe, strerror(errno));
								exit(0);
							}
						}
						else
						{
							printf("success to restart child process.\n");
							g_pid_group[i].m_pid =  pNew;
						}
					}

	//				break;	
				}
			}
		}
		
	
	}
}




int main(int argc, char *argv[])
{
	if  (argc < 4)
	{
		printf("usage: arg is less than 4\n      ./MultiProcessDaemon child_process_num (child_process_exe argc args) (child_process_exe argc args) (child_process_exe argc args)......\n");
		printf("Here is an example:\n");
		printf("\tIf you want to run \033[0;31m3\033[0m processes, they are:\n");
		printf("\t\t\033[0;32mping -f 192.168.1.1\033[0m\n");
		printf("\t\t\033[0;33mnetstat -c\033[0m\n");
		printf("\t\t\033[0;34mtop\033[0m\n");
		printf("\tThen you can run them with MultiProcessDaemon, like:\n");
		printf("\t\tMultiProcessDaemon \033[0;31m3 \033[0;32mping 2 -f 192.168.1.1 \033[0;34mtop 0 \033[0;33mnetstat 1 -c\033[0m\n");
		exit(-1);
	}

	//check the number child process need to create
	g_pid_num = atoi(argv[1]);

	if (g_pid_num <= 0 || g_pid_num > MAX_PROCESS_NUM)
	{
		printf("child process number must more than 0 and less then 500\n");
		exit(-1);
	}


	//fork a child process
	pid_t pp = fork();

	if (pp < 0)
	{
		printf("error: failed to fork a new process\n");
                exit(-1);
	}
	else if (pp > 0)
	{
		//let the main process exit quiet
		exit(0);
	}

	//set the sid to new 
	setsid();

	g_pid_group = new ChildProcess[g_pid_num];

	if (g_pid_group == NULL)
	{
		perror("failed to create memory:");
		exit(-1);
	}

	int i;
	int nArgStart = 2;
	
	for (i = 0; i < g_pid_num; i++)
	{
		if (nArgStart >= argc)
		{
			printf("wrong argv.\n");
			exit(-1);
			break;
		}
		strcpy(g_pid_group[i].m_chExe, argv[nArgStart]);
		nArgStart++;


		if (nArgStart >= argc)
		{
			printf("wrong argv.\n");
			exit(-1);
			break;
		}
		g_pid_group[i].m_argc = atoi(argv[nArgStart]);
		nArgStart++;

		if (g_pid_group[i].m_argc < 0)
		{
			printf("wrong argv.\n");
			exit(-1);
			break;
		}
		else
		{
			g_pid_group[i].m_argv = new char*[g_pid_group[i].m_argc + 2];

			g_pid_group[i].m_argv[0] = new char[strlen(g_pid_group[i].m_chExe) + 1];
			strcpy(g_pid_group[i].m_argv[0], g_pid_group[i].m_chExe);
			
			g_pid_group[i].m_argv[g_pid_group[i].m_argc + 1] = NULL;
		}

		for (int k = 0; k < g_pid_group[i].m_argc; k++)
		{
			if (nArgStart >= argc)
			{
				printf("wrong argv.\n");
				exit(-1);
				break;
			}
			int nLen = strlen(argv[nArgStart]) + 1;
			g_pid_group[i].m_argv[k + 1] = new char[nLen];
			strcpy(g_pid_group[i].m_argv[k + 1], argv[nArgStart]);
			nArgStart++;
		}

		g_pid_group[i].m_argc += 2;
	}
	
	
	signal(SIGUSR2, MustExit);
	
	//register a SIGCHILD for catch a child process's exit message
	signal(SIGCHLD, ChildExit);

	for (int i = 0; i < g_pid_num; i++)
	{
		pid_t pNewChild = fork();

		if (pNewChild < 0)
		{
			//-2 that's mean it was failed at first
			g_pid_group[i].m_pid = -2;
			printf("error: failed to fork a new process\n");
		}
		else if (pNewChild == 0)
		{
		 /* //only for debug
			printf("-----------------------------------------------\n");
			printf("prepare to start process: %s, argv(%d) as below:\n", g_pid_group[i].m_chExe, g_pid_group[i].m_argc);

			for (int n = 0; n < g_pid_group[i].m_argc; n++)
			{
				printf("     argv[%d]:%s\n", n, g_pid_group[i].m_argv[n]);
			}
			printf("-----------------------------------------------\n");
			*/
			//child process
			if (-1 == execvp(g_pid_group[i].m_chExe, g_pid_group[i].m_argv))
			{
				printf("failed to start process(%s), error:%s", g_pid_group[i].m_chExe, strerror(errno));

				g_pid_group[i].Kill();

				exit(-1);
			}
		}
		else
		{
			//success to fork a child process
			g_pid_group[i].m_pid = pNewChild;
		}
	}

	while (1)
	{
		sleep(1);
        if (g_pid_num <= 0)
        {
            printf("Main progress exit\n");
            break;
        }
	}

	exit(0);
}
