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
  job *current_node = job_list->next, *node_to_delete = NULL;
  int process_status, process_id_deleted, wait_status;

  while (current_node) {

    /* Wait for a child process to finish.
    *    - WNOHANG: return immediately if the process has not exited
    *    - WUNTRACED: return if process has stopped
    */
    wait_status = waitpid(current_node->pgid, &process_status, WNOHANG|WUNTRACED);

    if (wait_status != 0 && (WIFEXITED(process_status) || WIFSIGNALED(process_status))) {
      node_to_delete = current_node;
      current_node = current_node->next;
      process_id_deleted = node_to_delete->pgid;
      if (delete_job(job_list, node_to_delete)) {
        printf("Process #%d deleted from job list\n", process_id_deleted);
      } else {
        printf("Process #%d could not be deleted from job list\n", process_id_deleted);
      }
    } else if (wait_status != 0 && WIFSTOPPED(process_status)) {
      current_node->state = STOPPED;
      current_node = current_node->next;
    } else {
      current_node = current_node->next;
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
	job tmp_job2; // for storing a backup of node fg uses in its execution
  job *tmp_job;
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

		} else if (strcmp("jobs",args[0]) == 0) {
      if (args[1] == NULL) {
        if (empty_list(job_list)) {
          printf("No processes in job list.\n");
        } else {
          block_SIGCHLD();
          print_job_list(job_list);
          unblock_SIGCHLD();
        }
      } else {
        printf("ERROR - Usage: 'jobs'\n");
      }

      continue;

    } else if (strcmp("fg",args[0]) == 0) {
      // Get asked process and update status to FOREGROUND
      block_SIGCHLD();
      if (empty_list(job_list)) {
        printf("No process to put in foreground...\n");
        unblock_SIGCHLD();
      } else {
        if (args[1] == NULL) {
          tmp_job = get_item_bypos(job_list, 1);
        } else if (atoi(args[1]) > list_size(job_list)) {
          printf("Process position in list greater than list size\n");
          unblock_SIGCHLD();
          continue;
        } else {
          tmp_job = get_item_bypos(job_list, atoi(args[1]));
        }

        tmp_job->state = FOREGROUND;
        args[0] = tmp_job->command;
        pid_fork = tmp_job->pgid; // We use 'pid_fork' for use not related to fork, just to store a pid
        unblock_SIGCHLD();

        // Put it in foreground and make it continue
        set_terminal(pid_fork);
        if (killpg(pid_fork, SIGCONT) != 0) { // Cannot make it continue
          printf("Error restoring a process from suspension\n");
          continue;
        }
        
        //Backup job node
        block_SIGCHLD();
          tmp_job = get_item_bypid(job_list, pid_fork);
          tmp_job2.pgid = tmp_job->pgid;
          tmp_job2.command = strdup(tmp_job->command);
          tmp_job2.state = tmp_job->state;
        unblock_SIGCHLD();

        /* Wait for a child process to finish.
        *    - WUNTRACED: return if the child has stopped.
        */
        waitpid(pid_fork, &status, WUNTRACED); // Wait that process to finish or be stopped

        if (WIFSTOPPED(status)) { // If stopped, modify it from FOREGROUND to STOPPED
          tmp_job->state = STOPPED;

        set_terminal(pid_wait); // Return terminal control to main process

        status_res = analyze_status(status, &info);

        // Print info about taken process
        printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_fork, args[0],
									status_strings[status_res], info);
									
        } else {
			set_terminal(pid_wait); // Return terminal control to main process

        status_res = analyze_status(status, &info);

        // Print info about taken process
        printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_fork, tmp_job2.command,
									status_strings[status_res], info);
		}
      }

      continue;

    } else if (strcmp("bg",args[0]) == 0) {
      // Get asked process and update status to BACKGROUND
      block_SIGCHLD();
      if (empty_list(job_list)) {
        printf("No process available to put in background\n");
        unblock_SIGCHLD();
      } else {
        if (args[1] == NULL) {
          tmp_job = get_item_bypos(job_list, 1);
        } else if (atoi(args[1]) > list_size(job_list)) {
          printf("Process position in list greater than list size\n");
          unblock_SIGCHLD();
          continue;
        } else {
          tmp_job = get_item_bypos(job_list, atoi(args[1]));
        }
        if (tmp_job->state == BACKGROUND) {
          printf("Process #%d is already running in background\n", tmp_job->pgid);
          unblock_SIGCHLD();
          continue;
        }
        tmp_job->state = BACKGROUND;
        args[0] = tmp_job->command;
        pid_fork = tmp_job->pgid;
        unblock_SIGCHLD();

        if (killpg(pid_fork, SIGCONT) != 0) { // Put it in background
          printf("Error restoring a process from suspension\n");
        }

        // Print info about taken process
        printf("Background (from suspension) job running... pid: %d, command: %s\n", pid_fork, args[0]);
      }

      continue;

    }

		pid_fork = fork();

		if (pid_fork == 0) { // Child process running, need to change executable code
			restore_terminal_signals();

			if (execvp(args[0], args) == -1) printf("Error, command not found: %s\n", args[0]);
		} else {
      new_process_group(pid_fork); // New process group for new process
			if (!background) { // Command runs in foreground
				// Give foreground process terminal control
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
