/**
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell.test
   $ ./Shell.test
	(then type ^D to exit program)

**/

#include "job_control.h"   // remember to compile with module job_control.c

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

job* job_list; // Job list with all processes ran in background

void mySIGCHLD_Handler(int signum) {
  block_SIGCHLD();
  if (signum == 17) {
    job *current_node = job_list->next, *node_to_delete = NULL;
    int process_status, process_id_deleted;

    while (current_node) {

      /* Wait for a child process to finish.
      *    - WNOHANG: return immediately if no child has exited
      */
      waitpid(current_node->pgid, &process_status, WNOHANG);

      if (WIFEXITED(process_status)) {
        node_to_delete = current_node;
        current_node = current_node->next;
        process_id_deleted = node_to_delete->pgid;
        if (delete_job(job_list, node_to_delete)) {
          printf("Process #%d deleted from job list\n", process_id_deleted);
        } else {
          printf("Process #%d could not be deleted from job list\n", process_id_deleted);
        }
      } else {
        current_node = current_node->next;
      }
    }
  }
  unblock_SIGCHLD();
}

// -----------------------------------------------------------------------
//                            MAIN
// -----------------------------------------------------------------------

int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char* args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	int pid_fork, pid_wait; /* pid for created and waited process */
	int status;             /* status returned by wait */
	enum status status_res; /* status processed by analyze_status() */
	int info;				/* info processed by analyze_status() */

	// added variables:
  job_list = new_list("Shell tasks");

  signal(SIGCHLD, mySIGCHLD_Handler);

  ignore_terminal_signals();

	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{
		printf("COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */

		if(args[0]==NULL) continue;   // if empty command

		// Intern command checking
		if (strcmp("cd",args[0]) == 0) {
			if (chdir(args[1]) != 0) // Incorrect path
				printf("cd: Incorrect path '%s'\n", args[1]);

			continue;
		}

		pid_fork = fork();

		if (pid_fork == 0) { // Child process running, need to change executable code
			restore_terminal_signals();

			if (execvp(args[0], args) == -1) printf("Error, command not found: %s\n", args[0]);
		} else {
			if (!background) { // Command runs in foreground
				// New process group for new process and we give it terminal control
				new_process_group(pid_fork);
				set_terminal(pid_fork);

				pid_wait = getpid();

        /* Wait for a child process to finish.
        *    - WUNTRACED: return if the child has stopped.
        */
				waitpid(pid_fork, &status, WUNTRACED);

				set_terminal(pid_wait); // Return terminal control to main process

				status_res = analyze_status(status, &info);

        if (WIFSTOPPED(status)) {
          add_job(job_list, new_job(pid_fork, args[0], STOPPED));
        }

				// Print info about child process (in foreground)
				printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_fork, args[0],
									status_strings[status_res], info);

			} else {
        add_job(job_list, new_job(pid_fork, args[0], BACKGROUND));

				// Print info about child process (in background)
				printf("Background job running... pid: %d, command: %s\n", pid_fork, args[0]);
			}
		}

	} // end while
}
