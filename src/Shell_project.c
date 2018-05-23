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

	/*------------- ADDED VARIABLES -------------------*/
  job *tmp_job;
  job_list = new_list("Shell tasks");

  // variables for 'mask' command
  sigset_t signalsToBlock;
  int masking = 0;
  char *numberChecking;

  // variables for 'children' command
  FILE *statFileProcess, *childrenProcesses, *fileMaxPID, *executableNameFile;
  int max_pid, pid_children_command, num_children, tmp_ints[6], current_children;
  unsigned long num_threads = 0;
  char path[100], command_name[50], tmp_char, tmp_chars[50];
  unsigned int tmp_unsigned;
  unsigned long tmp_longs[6];
  long int tmp_long_ints[6];

  /*------------- - - - - - - - - -------------------*/

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

        /* Wait for a child process to finish.
        *    - WUNTRACED: return if the child has stopped.
        */
        waitpid(pid_fork, &status, WUNTRACED); // Wait that process to finish or be stopped

        if (WIFSTOPPED(status)) { // If stopped, modify it from FOREGROUND to STOPPED
          block_SIGCHLD();
          tmp_job = get_item_bypid(job_list, pid_fork);
          tmp_job->state = STOPPED;
          unblock_SIGCHLD();
        }

        set_terminal(pid_wait); // Return terminal control to main process

        status_res = analyze_status(status, &info);

        // Print info about taken process
        printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_fork, args[0],
									status_strings[status_res], info);
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

    } else if (strcmp("mask", args[0]) == 0) {
      if (args[1] == NULL || args[2] == NULL || args[3] == NULL) {
        printf("mask: missing arguments, check input\n");
        continue;
      } else if (strtol(args[1], NULL, 10) != atoi(args[1])) {
        printf("mask: signal error\n");
        continue;
      } else if (strcmp("-c", args[2]) != 0) {
        printf("mask: command to run not found, maybe forgot to put '-c'?\n");
        continue;
      }

      masking = 1;
      sigemptyset(&signalsToBlock);
      if (sigaddset(&signalsToBlock, atoi(args[1])) != 0) { // Error adding signal
        printf("mask: signal '%s' not valid\n", args[1]);
        continue;
      }
    } else if (strcmp("children", args[0]) == 0) {
      if (args[1] != NULL) {
        printf("Usage: 'children'\n");
        continue;
      }

      /*
        Anotaciones:
         - En el fichero /proc/[pid]/stat tenemos los datos 'pid', 'command' y '#threads'
         - En el fichero /proc/[pid]/task/[pid]/children tenemos los pids de los procesos
           hijo del thread principal (contamos el numero de numeros en el fichero y eso es '#children')

         - Para iterar por las carpetas de /proc buscando mostrar info x cada proceso,
           en /proc/sys/kernel/pid_max hay un numero que es el máximo de procesos que se admiten por
           sesion, por lo que cogemos ese numero e iteramos de 0 o 1 (segun como comience), hasta dicho numero
           y vamos mostrando la info de cada proceso

      */

      if ((fileMaxPID = fopen("/proc/sys/kernel/pid_max", "rt")) == NULL) {
        printf("children: Could not access '/proc/sys/kernel/pid_max'\n");
      } else {
        if (fscanf(fileMaxPID, "%d", &max_pid) != 1) {
          printf("children: Could not read from '/proc/sys/kernel/pid_max'\n");
          fclose(fileMaxPID);
        } else {
          fclose(fileMaxPID);

          for (pid_children_command = 1; pid_children_command < max_pid; pid_children_command++) {
           sprintf(path, "/proc/%d/stat", pid_children_command);
           if ((statFileProcess = fopen(path, "rt")) == NULL) {
             //printf("children: Could not access '/proc/%d/stat'\n", pid_children_command);
             continue;
           }
           sprintf(path, "/proc/%d/task/%d/children", pid_children_command, pid_children_command);
           if ((childrenProcesses = fopen(path, "rt")) == NULL) {
             //printf("children: Could not access '/proc/%d/task/%d/children'\n", pid_children_command, pid_children_command);
             fclose(statFileProcess);
             continue;
           }
           sprintf(path, "/proc/%d/comm", pid_children_command);
           if ((executableNameFile = fopen(path, "rt")) == NULL) {
             //printf("children: Could not access '/proc/%d/task/%d/children'\n", pid_children_command, pid_children_command);
             fclose(statFileProcess);
             fclose(childrenProcesses);
             continue;
           }

           fscanf(statFileProcess, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld", &tmp_ints[0], tmp_chars,
                  &tmp_char, &tmp_ints[1], &tmp_ints[2], &tmp_ints[3], &tmp_ints[4], &tmp_ints[5], &tmp_unsigned, &tmp_longs[0], 
                  &tmp_longs[1], &tmp_longs[2], &tmp_longs[3], &tmp_longs[4], &tmp_longs[5], &tmp_long_ints[0], &tmp_long_ints[1], 
                  &tmp_long_ints[2], &tmp_long_ints[3], &num_threads);
			
			fscanf(executableNameFile, "%s", command_name);
			
		   while (fscanf(childrenProcesses, "%d ", &current_children) == 1) num_children++;
		
           printf("PID: %d, command: %s, #children: %d, #threads: %ld\n", pid_children_command, command_name, num_children, num_threads); //TODO
           
           num_children = 0;
           fclose(statFileProcess);
		   fclose(childrenProcesses);
		   fclose(executableNameFile);
          }
          }
			
        }
        continue;
      }


      
    

		pid_fork = fork();

		if (pid_fork == 0) { // Child process running, need to change executable code
			restore_terminal_signals();

      if (masking) {
        new_process_group(getpid()); // New process group for new process
        sigprocmask(SIG_SETMASK, &signalsToBlock, NULL);

        if (execvp(args[3], args+3) == -1) {
			printf("Error, command not found: %s\n", args[3]);
			return -1;
		}
      } else {
        if (execvp(args[0], args) == -1) {
			printf("Error, command not found: %s\n", args[0]);
			return -1;
		}
      }

		} else {
      if (masking) masking = 0;

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
