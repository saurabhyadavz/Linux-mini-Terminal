#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<errno.h>
#include<wait.h>
#include<pwd.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<termios.h>

int errno;
int curpid=-1;
char*arg[100],*res;//arg is used to store all arguments after parsing the input command and res is a variable used in parsing
int argc,status;//argc is the number of arguments and status is a variable for wait

//the structure is used for storing background processes
struct process
{
    pid_t pid;//process id
    char cname[100][100];//argument list
    int stat;//status- 1 if active else 0
    int argcnt;//number of arguments in this cmd
    int stop;//1 if process is stopped 
};

struct process parr[1000];//parr stores background processes

int parrptr=0;//stores the number of background processes

//prints the information regarding process with pid given
void pinfo(int pid)
{
    fprintf(stdout,"pid -- %d\n",pid);
    char d[100],c[100],buf[33],exname[100],a[2]=" ",line[10],vm[100],path[100];
    int i;
    for(i=0;i<100;i++)
	  path[i]='\0';
    
    strcpy(c,"/proc/");
    sprintf(buf,"%d",pid);//converts pid from integer to string
    strcat(c,buf);
    strcpy(d,c);
    strcat(c,"/status");//c contains /proc/pid/status
    strcat(d,"/exe");//d contains /proc/pid/exe
    readlink(d,path,100);//gets the executable path in path array
    
    int f=open(c,0444);//open status file
    
    if(f<0)// if no status file then show error
    {
	  fprintf(stdout,"no such process\n");
	  return;
    } 
    read(f,line,5);//read Name: in line
    while(a[0]==' ')
    {
	read(f,a,1);
    }
    exname[0]=a[0];//exname is name of process
    int p=1;
    while(a[0]!='\n')//read exname char by char
    {
	read(f,a,1);
	exname[p++]=a[0];
    }
    exname[p]='\0';
    read(f,line,6);//read Status: in line 
    line[6]='\0';
    strcpy(a," ");//set a to blank
    char status;
    while(!(a[0]>='A'&&a[0]<='Z'))//read char by char till status is read
    {
	read(f,a,1);
    }
    status=a[0];//get status of process
    fprintf(stdout,"Process Status -- %c\n",status);
    int lcnt=0;
    while(lcnt!=10)//skip 10 lines to reach line with "VmSize:"
    {
	read(f,a,1);
	if(a[0]=='\n')
	    lcnt++;
    }
    read(f,line,7);//read VmSize in line
    line[7]='\0';

    strcpy(a," ");//set a to blank
    if(!strcmp(line,"VmSize:"))//compare variables line and VmSize:
    {
	while(a[0]==' ')//read all blanks
	{
	    read(f,a,1);
	}
	vm[0]=a[0];//read vm size char by char
	int p=1;
	while(a[0]!='\n')
	{
	    read(f,a,1);
	    vm[p++]=a[0];
	}
	vm[p]='\0';
	fprintf(stdout,"memory -- %s\n",vm);
    }
    else
	fprintf(stdout,"memory -- no information available\n");//if VmSize not present in status info is not available
    fprintf(stdout,"Executable Path -- %s\n",path);
}
void sig_handler(int signum)//signal handler for SIGCHLD and SIGSTPT
{
    int i;
    if(signum==SIGCHLD)
    {
	int pid,stat;
	pid=waitpid(WAIT_ANY,&stat,WNOHANG);//find pid of process terminated/signalling
	if(pid>0)
	{
	    for(i=0;i<parrptr;i++)//find the particular background process that signalled
		if(parr[i].pid==pid)
		    break;
	    if(stat!=0)//if stat!=0 then the process has an error 
	    {
		fprintf(stdout,"\n UNKNOWN COMMAND(press enter to continue)\n");
		if(i<parrptr)
		    parr[i].stat=0;
	    }
	    else
	    {
		if(i<parrptr)//if process found
		{
		    parr[i].stat=0;//change status from running to terminated
		    if(WIFEXITED(stat))//if exited print the exit status
			fprintf(stdout,"\n%s %d terminated normally with exit status %d \n",parr[i].cname[0],parr[i].pid,WEXITSTATUS(stat));
		    if(WIFSIGNALED(stat))//else print the signal sent by process
			fprintf(stdout,"\n%s %d terminated by signal  %d \n",parr[i].cname[0],parr[i].pid,WTERMSIG(stat));
		}
	    }
	}
	fflush(stdout);//print output to terminal
	signal(SIGCHLD,sig_handler);//if there is a signal during execution of handler
	return;
    }
    if(signum==SIGTSTP)
    {
	if(curpid!=-1)//checks if process is running in foreground
	{
	    fprintf(stdout,"received SIGTSTP\n");//send SIGSTPT signal to current process if it is a foreground process
	    kill(getpid(),SIGTSTP);
	}
    }
    fflush(stdout);//print output to terminal
}
void check_status()//checks the status of all background processes
{
    int j=0,pst;
    for(j=0;j<parrptr;j++)
    {
	if(parr[j].stat==1)
	{
	    pid_t rid=waitpid(parr[j].pid,&pst,WNOHANG);
	    if(rid==-1 || rid>0&&rid==parr[j].pid)
	    {
		parr[j].stat=0;
		if(WIFEXITED(pst))//if exited print the exit status
		    fprintf(stdout,"\n%s %d terminated normally with exit status %d \n",parr[j].cname[0],parr[j].pid,WEXITSTATUS(pst));
		if(WIFSIGNALED(pst))//else print the signal sent by process
		    fprintf(stdout,"\n%s %d terminated by signal  %d \n",parr[j].cname[0],parr[j].pid,WTERMSIG(pst));
		fflush(stdout);//print output to terminal
	    }
	}
    }
}
int main(void)
{
    signal(SIGTSTP,sig_handler);//call signal_handler
    pid_t spgid;
    int isinter,ret,shell=STDIN_FILENO;
    struct termios shell_tmodes;
    isinter=isatty(shell);
    if(isinter)
    {
	while(tcgetpgrp(shell)!=(spgid=getpgrp()))//if shell not in foreground process group send SIGINT signal to it 
	    kill(-spgid,SIGTTIN);
	signal(SIGINT,SIG_IGN);//reset behaviour of signals for interactive shell
	signal(SIGQUIT,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGTTOU,SIG_IGN);
	spgid=getpid();//find pid of current process
	if(setpgid(spgid,spgid)<0)//put process in foregroung process group
	{
	    perror("cant put shell in its process grp");//error if failed
	    exit(1);
	}
	tcsetpgrp(shell,spgid);//set group attributes for shell
	tcgetattr(shell,&shell_tmodes);//store shell attributes in shell_tmodes
    }
    pid_t pid;
    int curpid,len,i,j,k,status,background=0;
    char c,home[100],cmd[1000],prompt[400],cwd[100],uname[100],hname[100];
    getlogin_r(uname,100);//find username of currently logged in user
    gethostname(hname,100);//find system_name of the host
    prompt[0]='<';//construct prompt
    prompt[1]='\0';
    strcat(prompt,uname);//append username
    prompt[strlen(prompt)+1]='\0';
    prompt[strlen(prompt)]='@';
    strcat(prompt,hname);//append sysname
    prompt[strlen(prompt)+1]='\0';
    prompt[strlen(prompt)]=':';
    len=strlen(prompt);//find length of prompt
    const char *homedir = getenv("HOME");//find the name of homedir
    chdir(homedir);//The directory from which the shell is invoked will be the home directory of the shell
    while(1)
    {
	check_status();//check the status of all background processes print if terminated
	background=0;//set variable bg to 0
	if(getcwd(cwd,100)!=NULL)//find cwd
	{
	    strcpy(home,cwd);//copy it to home
	    home[strlen(homedir)]='\0';
	    char*q=cwd;//point q to cwd
	    char*p=q+strlen(homedir);//point p to part of cwd after len(homedir)
	    if(!strcmp(home,homedir))//compare if home and homedir are same
	    {
		cwd[0]='~';//replace cwd by ~
		cwd[1]='\0';
		strcat(cwd,p);//and concatenate p after ~ to get remaining dir name
	    }
	    prompt[len]='\0';
	    strcat(prompt,cwd);//concatenate cwd to prompt
	}
	else
	    perror("getcwd() error");//if getcwd = NULL error
	prompt[strlen(prompt)+1]='\0';
	prompt[strlen(prompt)]='>';//add last char of prompt
	do//get a executable command
	{
	    check_status();//check the status of all background processes print if terminated
	    cmd[0]='\0';//set cmd to NULL
	    fprintf(stdout,"%s",prompt);//print prompt
	    scanf("%[^\n]",cmd);//get cmd
	    getchar();//get the newline character
	    for(i=0;i<100;i++)//set all argument to NULL initially
		arg[i]=NULL;
	    i=0;
	    res=strtok(cmd," \t");//tokenize the string i/p
	    while(res!=NULL)
	    {	
		arg[i++]=res;
		res=strtok(NULL," \t");
	    }
	    argc=i;//set no. of arguments
	}while(arg[0]==NULL);//keep taking commands until we get an executable(handles cases with only enters and tabs
	int flag=0,flag2=0;
	for(i=0;i<argc;i++)//check for redirection and pipe
	{
	    if(!strcmp(arg[i],">")||!strcmp(arg[i],"<")||!strcmp(arg[i],">>"))//set flag to 1 if redirection present
		flag=1;
	    else if(!strcmp(arg[i],"|"))//set flag2 to 1 if pipe present
		flag2=1;
	}
	if(flag2==1)//if pipe
	{
	    char* cmd[100][100];//store commands separated by pipes
	    for(i=0;i<100;i++)//allocate storage for each command
		for(j=0;j<100;j++)
		    cmd[i][j]=(char*)malloc(sizeof(char)*100);
	    j=k=0;
	    for(i=0;i<argc;i++)//traverse the arguments
	    {
		if(!strcmp(arg[i],"|"))//goto next command if pipe found
		{
		    cmd[j][k]=NULL;
		    j++;
		    k=0;
		    continue;
		}
		strcpy(cmd[j][k],arg[i]);//set cmd to args
		k++;
	    }
	    cmd[j][k]=NULL;//set last value to NULL
	    int num_pipes=j;//store number of pipes
	    int pip=num_pipes*2;//pip is number of pipeds to create
	    int *pipes=malloc(sizeof(int)*pip);//allocate an array pipes
	    for(i=0;i<num_pipes;i++)//create pipes
		if(pipe(pipes+2*i)<0)
		{
		    perror("Couldn't Pipe");//if return value is -ve error
		    _exit(1);		    
		}

	    for(i=0;i<=num_pipes;i++)//loop through all commands(num_pipes+1)
	    {
		if((pid=fork())<0)//fork a new process
		{
		    perror("error forking");
		    exit(EXIT_FAILURE);
		}
		if(!pid)//in child process
		{
		    int ip=0,op=0,fd,x,y,z,ff,j1,j2,j3;
		    for(x=0;cmd[i][x]!=NULL;x++)//check redirection
		    {
			if(!strcmp(cmd[i][x],"<"))//checkk if <
			{
			    ip=1;//set ip to 1 as input source is already changed
			    j1=x;
			    fd=open(cmd[i][j1+1],O_RDONLY,0666);//open the file in read mode
			    if(fd<0)//if file descriptor -ve error
			    {
				perror("error opening file");
				exit(EXIT_FAILURE);
			    }
			    dup2(fd,0);//set input for process to fd
			    close(fd);//close fd
			    cmd[i][x]=NULL;//change cmd to NULL
			    break;
			}
			else if(!strcmp(cmd[i][x],">"))//checkk if >
			{
			    j2=x;
			    op=1;//set op to 1 as output source is already changed
			    fd=open(cmd[i][j2+1],O_WRONLY|O_CREAT,0666);//open file in write or create mode
			    if(fd<0)//if file descriptor -ve error
			    {
				perror("error opening file");
				exit(EXIT_FAILURE);
			    }
			    dup2(fd,1);//set output for process to fd
			    close(fd);//close fd
			    cmd[i][x]=NULL;//change cmd to NULL
			    break;
			}
			else if(!strcmp(cmd[i][x],">>"))//check if >>
			{
			    j3=x;
			    fd=open(cmd[i][j3+1],O_WRONLY|O_APPEND|O_CREAT,0666);//open file in write/creat/append mode
			    op=1;//set op to 1 as output source is already changed
			    if(fd<0)//if file descriptor -ve error
			    {
				perror("error opening file");
				exit(EXIT_FAILURE);
			    }
			    dup2(fd,1);//set output for process to fd
			    close(fd);//close fd
			    cmd[i][x]=NULL;//change cmd to NULL
			    break;
			}
		    }

		    if(i!=0&&!ip)//if not first command and ip not already changed 
		    {
			if(dup2(pipes[2*(i-1)],0)<0)//change input to the previous pipe 
			{
			    perror("dup2()");//error if failed
			    exit(EXIT_FAILURE);
			}
		    }
		    if(i<num_pipes&&!op)//if not last command and output not already changed
		    {
			if(dup2(pipes[2*i+1],1)<0)//change output to next pipe
			{
			    perror("dup2()");//error if failed
			    exit(EXIT_FAILURE);
			}
		    }
		    for(k=0;k<pip;k++)//close all pipes
			close(pipes[k]);
		    if(execvp(cmd[i][0],cmd[i])<0)//execute the commmands
		    {
			perror("error executing");//if return value < 0 error
			exit(EXIT_FAILURE);
		    }
		}
	    }
	    for(i=0;i<pip;i++)//close all pipes
		close(pipes[i]);
	    int stat;
	    for(i=0;i<=num_pipes;i++)//wait for all child processes
		wait(&stat);
	    continue;
	}
	if(!strcmp(arg[argc-1],"&"))//check if background process
	{
	    arg[argc-1]=NULL;
	    background=1;
	}
	else if(!strcmp(arg[0],"quit"))//if cmd is quit then stop the shell
	    break;
	else if(!strcmp(arg[0],"cd"))//check if cd
	{
	    if(arg[1]==NULL || strcmp(arg[1],"~")==0  ||strcmp(arg[1],"~/")==0|| strcmp(arg[1],"")==0)//check if change to home
	    {
		if(chdir(getenv("HOME"))<0)//if chdir fails error
		{
		    perror("error changing");
		}
	    }
	    else
	    {
		ret=chdir(arg[1]);//change dir to arg1
		if(ret<0)//if chdir fails error
		    perror("directory does not exist");
	    }
	    continue;	
	}
	else if(!strcmp(arg[0],"pinfo")&&arg[1]==NULL){//pinfo command
	    int procid=getpid();//find pid of running process
	    pinfo(procid);//call pinfo with that pid
	    continue;
	}	    
	else if(!strcmp(arg[0],"pinfo")&&arg[1]!=NULL&&arg[2]==NULL){//pinfo command with given pid
	    pinfo(atoi(arg[1]));//pass arg to pinfo by converting to integer the arg[1]
	    continue;
	}
	else if(!strcmp(arg[0],"jobs"))//print all background processes
	{
	    check_status();//check the status of all background processes
	    int j=0,k,n=1,fd;

	    if(arg[1]!=NULL&&!strcmp(arg[1],">"))
	    {
		fd=open(arg[2],O_WRONLY|O_CREAT,0666);//open file in write or create mode
		if(fd<0)//if file descriptor -ve error
		{
		    perror("error opening file");
		    exit(EXIT_FAILURE);
		}
		dup2(fd,1);//set output for process to fd
		close(fd);//close fd
	    }
	    for(j=0;j<parrptr;j++)//for all processes in parr
	    {
		if(parr[j].stat)//check if status is running
		{
		    fprintf(stdout,"[%d] ",n);//print job number
		    for(k=0;k<parr[j].argcnt;k++)//print all arguments
			fprintf(stdout,"%s ",parr[j].cname[k]);
		    fprintf(stdout,"[%d]\n",parr[j].pid);//print pid
		    n++;//increment job number
		}
	    }
	    dup2(STDIN_FILENO,1);//reset output to stdin
	    continue;
	}
	else if(!strcmp(arg[0],"kjob")&&arg[1]!=NULL&&arg[2]!=NULL&&arg[3]==NULL)//send signal in arg[2] to job in arg[1]
	{
	    int r,id,i,a,b,n;
	    a=atoi(arg[1]);//convert string to int
	    b=atoi(arg[2]);
	    n=0;
	    for(i=0;n<a&&i<parrptr;i++)//find the job in the parr
	    {
		if(parr[i].stat==1)
		    n++;
	    }
	    if(a!=n)//if job not found error
		fprintf(stdout,"no such job\n");
	    else
	    {
		id=parr[i-1].pid;//find pid of job
		r=kill(id,b);//send signal to job
		if(r==-1)//if return value -1 error
		    perror("signal not sent");
		else if(r==0)//else sent
		    fprintf(stdout,"signal sent\n");
	    }
	    continue;
	}
	else if(!strcmp(arg[0],"fg")&&arg[1]!=NULL&&arg[2]==NULL)//brinf job stored in arg[1] to foreground
	{
	    int st,id,i,a,n;
	    a=atoi(arg[1]);//convert str to int
	    n=0;
	    for(i=0;n<a&&i<parrptr;i++)//find the job in parr
	    {
		if(parr[i].stat==1)
		    n++;
	    }
	    if(a!=n)//if job not found error
		fprintf(stdout,"no such job\n");
	    else
	    {
		id=parr[i-1].pid;//find pid of job
		if(parr[i-1].stop==2)//if job was stopped
		{
		    curpid=id;//set curpid
		    kill(id,SIGCONT);//send continue signal to the process
		    parr[i-1].stop=0;//set stop to 1
		}
		waitpid(id,&status,WUNTRACED);//wait for signal from process
		if(WIFSTOPPED(status))//if process stopped again 
		{
		    parr[i-1].stop=2;//set stop to 2
		}
	    }
	    continue;
	}
	else if(!strcmp(arg[0],"overkill")&&arg[1]==NULL)//kill all background processes
	{
	    int i;
	    for(i=0;i<parrptr;i++)
	    {
		if(parr[i].stat)//if process is running
		{
		    kill(parr[i].pid,SIGKILL);//send SIGKILL signal
		}
	    }
	    continue;
	}

	if(flag==1)//if redirection
	{
	    int ppid;
	    if(ppid=fork())//create a child process
	    {
		int pid;
		wait(&pid);//in parent wait for child process
	    }
	    else if(ppid<0)
		perror("fork()");
	    else//in child process
	    {
		int f1,f2,f3,f4;
		int i,j1,j2,j3;
		int rt=0;
		int fd;
		char** arr=(char **)malloc(sizeof(char)*100);//stores the commands without the redirestion symbols
		for(i=0;i<100;i++)//allocate space to store commands
		    arr[i]=(char *)malloc(sizeof(char)*100);
		f1=f2=f3=0;//set flags to 0
		for(i=0;i<argc;i++)//check all arguments if >,< or >>
		{
		    if(!strcmp(arg[i],"<"))//if < cmd
		    {
			j1=i;
			f1=1;
			fd=open(arg[j1+1],O_RDONLY,0666);//open the file in next arg in read only mode
			if(fd<0)//if return value -ve error
			{
			    perror("error opening file");
			    _exit(1);
			}
			dup2(fd,0);//change input to new file descriptor
			close(fd);//close file descriptor
		    }
		    else if(!strcmp(arg[i],">"))//if > cmd
		    {
			j2=i;
			f2=1;
			fd=open(arg[j2+1],O_WRONLY|O_CREAT,0666);//open file in next arg in write only or create mode
			if(fd<0)//if return value -ve error
			{
			    perror("error opening file");
			    _exit(1);
			}
			dup2(fd,1);//change output to new file descriptor
			close(fd);//close file descriptor
		    }
		    else if(!strcmp(arg[i],">>"))
		    {
			j3=i;
			f3=1;
			fd=open(arg[j3+1],O_WRONLY|O_APPEND|O_CREAT,0666);//open file next arg in write only or create mode or append mode
			if(fd<0)//if return value -ve error
			{
			    perror("error opening file");
			    _exit(1);
			}
			dup2(fd,1);//change output to new file descriptor
			close(fd);//close file descriptor
		    }
		    if(!f1&&!f2&&!f3)//if not redirection
		    {
			strcpy(arr[rt],arg[i]);//copy arg in arr
			rt++;
		    }
		}
		arr[rt]='\0';//set last arg to NULL
		if(execvp(arr[0],arr)<0)//execute the command
		    perror("execvp()");//error if return value negative
	    }
	}
	else//if normal process
	{
	    pid_t pid1=fork();//create new process
	    if(pid1==-1)//error if fork return -1
	    {
		perror("Error in creating fork : ");
		exit(EXIT_FAILURE);
	    }
	    else if(pid1==0)//child process
	    {
		ret=execvp(arg[0],arg);//execute the process
		if (ret == -1)//it return value -ve error
		{
		    perror("no such command");
		    fflush(stdout);
		    _exit(1);
		}
	    }
	    else if(pid1>0)//in parent 
	    {

		if(background==1 )//if background process set curpid to -1
		{
		    curpid=-1;
		    for(j=0;j<100;j++)//store process in parr
			parr[parrptr].cname[j][0]='\0';
		    parr[parrptr].pid=pid1;//copy pid
		    for(j=0;j<argc-1;j++)//copy all args except &
		    {
			strcpy(parr[parrptr].cname[j],arg[j]);
		    }
		    parr[parrptr].stat=1;//set stat to running
		    parr[parrptr].argcnt=argc-1;//argcnt to argc - 1
		    parr[parrptr].stop=0;//set stop to 0
		    parrptr++;//increment parr pointer
		}
		if(background==0)//for foreground process
		{

		    waitpid(pid1,&status,WUNTRACED);//wait for process
		    if(WIFSTOPPED(status)) //if status id is stopped push to background
		    {
			parr[parrptr].pid = pid1;//set pid
			parr[parrptr].stat = 1; //set stat
			for(i=0;i<argc;i++)//copy all args
			    strcpy(parr[parrptr].cname[i],arg[i]);
			parr[parrptr].argcnt=argc;//set argcnt to argc
			parr[parrptr].stop=2;//change stop to 2
			parrptr++; //increment parr pointer
		    }
		}
	    }
	}
	check_status();//checks the status of all background processes
    }//exit while loop
    for(i=0;i<parrptr;i++)//kill all running proccesses
    {
	if(parr[i].stat)//if process is still running
	{
	    kill(parr[i].pid,SIGKILL);//send kill signal
	}
}
    return 0;
}
