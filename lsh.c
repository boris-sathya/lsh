/*
Copyright (c) Sathya Prakash.K, 2012
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * The name of the contributor may not be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"
#include <signal.h>


/*
 * Prototypes
 */

void PrintCommand(int, Command *);
char * PathCheck(Pgm *);
void PrintPgm(Pgm *);
void PrintArg(Pgm *);
void stripwhite(char *);
void kill_child(int);
void pipeit(Command *);
pid_t victim = 0;
int outputfd;
char *arguments[];       
/* When non-zero, this global means the user is done using this program. */
int done = 0;

/*
 * Name: main
 *
 * Desc: Gets the ball rolling...
 *
 */
int main(void)
{
    Command cmd;
    int n,status;
    signal(SIGINT, &kill_child);
    while (! done) {
	char *line;

	line = readline("@ ");

	if (!line)
	{
	    /* Encountered EOF at top level */
	    done = 1;
	}
	else
	{
	    /*
	     * Remove leading and trailing whitespace from the line
	     * Then, if there is anything left, add it to the history list
	     * and execute it.
	     */
	    stripwhite(line);

	    if(*line)
	    {
		add_history(line);
		n = parse(line, &cmd);
		PrintCommand(n, &cmd);
               
                     
	    }
	}
	
	if(line)
	    free(line);
    }
    return 0;
}

/*
 * Name: kill_child
 *
 * Desc: Handles control + c
 *
 */

void kill_child(int signo) {

if(victim != 0)
  kill(victim, SIGKILL);
}

void zombie_reaper(int signo){
  pid_t pid;
  int stat;  
  while((pid = waitpid(-1, &stat, WCONTINUED)) > 0)
      printf("Process ID: %d Completed\n", pid);

  return;
}



/*
 * Name: PrintCommand
 *
 * Desc: Prints a Command structure as returned by parse on stdout.
 *
 */
void
PrintCommand (int n, Command *cmd)
{
    char *arg;
    const char *name = malloc(10);
    pid_t id;
    int ret;
    int fset[2];
    int fd = 0;
    PrintPgm(cmd->pgm);
    PrintArg(cmd->pgm);
    name = PathCheck(cmd->pgm);
   if(name == NULL) {
     if(cmd->pgm->next!=NULL) {
              cmd->pgm = cmd->pgm->next;
              PrintPgm(cmd->pgm);
              PrintArg(cmd->pgm);
              
             name = PathCheck(cmd->pgm);
             }
     else {
      return;}
  }
    /*cd implementation*/
 
    if(!strcmp(name, "cd")) {
                   if(!arguments[1]) {
          chdir(getenv("HOME"));
          return;
             }
          else if(arguments[2]) {
          printf("Too many arguments.\n");
          return;
         }
          if(!chdir(arguments[1])) {
                 return;
                  }
            else {
                printf("No such file or directory\n");
                return;
              }
    }
/*exit implementation*/

    if(!strcmp(name, "exit")) {
        exit(0);
    }

/*Pipeit if more than one command is found*/
    
    if(cmd->pgm->next != NULL) {
             id = fork();
             if(id == 0) {
                   outputfd = 0;
                   /*Check for output redirection*/
                   if(cmd->rstdout != NULL) {
                       outputfd = 1;
                         }
                     pipeit(cmd);
                    outputfd = 0;
            }  
                /*parent waits*/
                else {
               wait(NULL);
               }
    }     /*No pipes (Single command)*/
       else {

    id = fork();
    if(id == 0) {
              /*Check for output redirection*/
             if(cmd->rstdout)
                { 
                    fd = creat(cmd->rstdout,S_IRWXU);
                    close(1);
                    dup(fd);
                    close(fd);
                 }
               /*Check for input redirection*/ 
            if(cmd->rstdin)
              {
                 fd = open(cmd->rstdin, O_RDONLY);
                 close(0);
                 dup(fd);
                 close(fd);
             }
             if(cmd->bakground) {
              
            signal(SIGINT, SIG_IGN);
         }        
        ret = execvp(name,arguments);
        if(ret < 0) {
           printf("exec error\n");
           exit -1;
          }
     }       
    victim = id;
        if(!(cmd->bakground))  {   //wait for child to terminate if process is not a bg one
          wait(NULL);
          
    } else { 
         signal(SIGCHLD, zombie_reaper); //in case of a background process declare a signal handler to handle SIGCHLD
                                         //from prospective zombies             
       }

}
}


/*
 * Name: pipeit
 *
 * Desc: Recursive function implementing multiple pipes.
 *
 */


void pipeit(Command *cmd) {
     int fdset[2];
     pipe(fdset);
     if(outputfd) {
       int fd = creat(cmd->rstdout,S_IRWXU);
       dup2(fd, 1);
       close(fd);
       outputfd = 0;
  }

   if(fork() == 0) {
     
    cmd->pgm = cmd->pgm->next;
    dup2(fdset[1],1);
    close(fdset[0]);
    if(cmd->pgm->next != NULL) { 
          pipeit(cmd);
      }
   else if(cmd->rstdin != NULL) {
        int fd = open(cmd->rstdin, O_RDONLY);
        dup2(fd, 0);
        close(fd);
  }
}
   else {
            dup2(fdset[0], 0);
            close(fdset[1]);
            wait(NULL);
       }
 
   /*execvp searches for the path automatically if the first argument doesn't has a "/" */

   if(execvp(*cmd->pgm->pgmlist, cmd->pgm->pgmlist) == -1){
        printf("execvp failed\n");
    }
   exit(1);
}




/*
 * Name: PrintPgm
 *
 * Desc: Prints a list of Pgm:s
 *
 */
void PrintPgm (Pgm *p)
{
    
    if (p == NULL)
	return;
    else
    {
	char **pl = p->pgmlist;

	/* The list is in reversed order so print
         * it reversed so get right :-)
         */
	PrintPgm(p->next);
        while (*pl)
	    *pl++;
            
            
	
    }
}

/*
 * Name: PrintArg
 *
 * Desc: Parses the arguments from the cmd and null terminates it.
 * useful while checking for invalid options
 */

void PrintArg (Pgm *p)
{
    
      int i =0;
    if (p == NULL)
        return;
    else
    {
        char **pl = p->pgmlist;

        /* The list is in reversed order so print
         * it reversed so get right :-)
         */
        PrintPgm(p->next);

        while (*pl) {
            arguments[i] = *pl++;

            i++;
        }
        arguments[i] = NULL;

    }
}

/*
 * Name: PrintCommand
 *
 * Desc: checks for executables in various directories and returns back the PATH in which the executable is found.
 *
 */

char * PathCheck (Pgm *p)
{
  if( p == NULL)
     return;
  else
  {
     char **pl = p->pgmlist;
     PathCheck(p->next);
     if((!strcmp(*pl,"cd"))||(!strcmp(*pl,"exit"))) {
        
           return *pl;
      }
     char *buff = malloc(sizeof("/usr/bin/") + sizeof(*pl));
     sprintf(buff,"/usr/bin/%s",*pl);
     FILE *fp;
     if(fopen(buff,"r")) {
        
          return buff; }
    else { 
         bzero(buff, sizeof(buff));
         sprintf(buff,"/bin/%s",*pl);
         if(fopen(buff,"r")) {
        
          return buff;
              }
          else {
           bzero(buff, sizeof(buff));
           sprintf(buff,"/sbin/%s", *pl);
            if(fopen(buff,"r")) {
         
           return buff;
           }  else {
                
              printf("Command not found\n");
              return NULL;
           }

  }

    }
     
}  

}

/*
 * Name: stripwhite
 *
 * Desc: Strip whitespace from the start and end of STRING.
 */

void
stripwhite (char *string)
{
    register int i = 0;
    while (whitespace( string[i] ))
	i++;
    if (i)
	strcpy (string, string + i);

    i = strlen( string ) - 1;
    while (i> 0 && whitespace (string[i]))
	i--;
    string [++i] = '\0';
}

