/*  ns = no scroll and limit output to a default of 30 lines.

    Copyright (C) 2015  Daniel Cardenas

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * ns : no scroll.
 *      Capture the output of a program display it from top to bottom and start at top again.
 *      Makes it easier to view a single line when scrolling is too fast to track.
 *      Saves the output in a log file.
 *      Restores the output with the last 30 lines of output logged.
 * 
 * Compile:
 *   gcc -o ns ns.c -lterminfo
 */
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <unistd.h>   // Defines getopt()

#include <sys/types.h>  // For waitpid()
#include <sys/wait.h>

#define LINE_BUFFER_SIZE 8192
int n_screen_log_lines = 30;
char * child_command_line = NULL;
FILE *log_file;

FILE * open_log_file() {
    char * filename;
    char filename_buf[128];
    filename = filename_buf;
    // Use first word of executed command as file name.
    // Scan for space.
    char c;
    int space_loc = 0;
    while ((c = child_command_line[space_loc]) != 0) {
        if (c == ' ') {
            break;
        }
        space_loc++;
    }
    int last_char_loc = space_loc;
    if (child_command_line[last_char_loc] != ' ') last_char_loc -= 1;
    if (last_char_loc > 123 ) last_char_loc = 123;
    if (last_char_loc < 0 ) last_char_loc = 0;
    strncpy(filename, child_command_line, last_char_loc);
    strcpy(&filename[last_char_loc], ".log");
    printf("\t\t\t\tLog file: %s\n", filename );
    return fopen(filename, "w");
}


int process_command_line_args(int argc, char * const * argv) {
    int option;
    opterr = 0;
    while ((option = getopt (argc, argv, "0123456789ac:mn:st")) != -1)
      switch (option) {
      case '0': n_screen_log_lines = 0;      break;
      case '1': n_screen_log_lines = 1;      break;
      case '2': n_screen_log_lines = 2;      break;
      case '3': n_screen_log_lines = 3;      break;
      case '4': n_screen_log_lines = 4;      break;
      case '5': n_screen_log_lines = 5;      break;
      case '6': n_screen_log_lines = 6;      break;
      case '7': n_screen_log_lines = 7;      break;
      case '8': n_screen_log_lines = 8;      break;
      case '9': n_screen_log_lines = 9;      break;
      case 'a': puts("\e[?1049h")     ;      exit(0);
      case 'c': child_command_line = optarg; break;
      case 'm': puts("\e[?1049l")     ;      exit(0);
      case 'n':
        sscanf(optarg, "%d", &n_screen_log_lines);
        break;
      case 's': printf("%d", 45)      ;      exit(0);
      case 't': printf("%c", 45)      ;      exit(0);
      case '?':
        if (optopt == 'c')
          fprintf (stderr, "Option -c is for the command to execute."
                "  Please supply a command to execute after the -c parameter.\n");
        else if (optopt == 'n')
          fprintf (stderr, "Option -n specified the number of output lines to keep after program termination.\n");
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
        return 1;
      default:
        abort ();
      }

    return n_screen_log_lines;
}

void my_putp(char * string) {
    tputs(string, 2, putchar);
}


void terminfo_reset_on_exit() {
    sleep(1);
    putp(exit_ca_mode);    // ca = cursor addressing.
    // When your program exits you should restore the tty modes to their original
    // state. To do this, call the reset_shell_mode() function.
    reset_shell_mode();   // terminfo reset.
}

int num_buffers;

int read_input_and_write(FILE * fd, char buffer[LINE_BUFFER_SIZE], int * buf_index_ptr, int * num_lines_written)
{
    char * fgets_return = fgets(buffer, LINE_BUFFER_SIZE, fd);
    if (fgets_return == NULL) {
        if (errno == EOF || feof(fd)) {
            return EOF;
        } else {
            perror("parent: fgets on pipe.");
            return errno;
        }
    }
#if 0
    printf("\t strlen: %zd ", strlen(buffer));
    fflush(stdout);
    //if (strlen(buffer) == 1 )
        printf("\t char: %d\t", (int) buffer[0]);
    fflush(stdout);
#endif
    my_putp(buffer);
    fputs(buffer, log_file);
    (*buf_index_ptr)++;
    if (*buf_index_ptr >= num_buffers) *buf_index_ptr = 0;
    (*num_lines_written)++;
    return 0;
}

int parent(int std_out_child) {
    int buf_index = 0;
    num_buffers = n_screen_log_lines ? : 2;
    char buffer[num_buffers][LINE_BUFFER_SIZE];
    int num_lines_written = 0;

    log_file = open_log_file();
    setupterm((char *)0, 1, (int *)0);    // Set up terminfo
    // If your program uses cursor addressing, it should output the enter_ca_mode
    // string at startup and the exit_ca_mode string when it exits.
    putp(enter_ca_mode);    // ca = cursor addressing.

    int num_rows = tigetnum("lines");
    int num_columns = tigetnum("cols");
    //tputs(tiparm(cursor_address, 0, 0), 2, putchar);
    //char debug_str[128];
    //sprintf(debug_str, "\t\t\t\t\t terminal: columns: %d rows %d\n", num_columns, num_rows);
    //fputs(debug_str, stdout);
    //my_putp(debug_str);
    //printf("^[[1;37m");   // Bold white.
    
    FILE *out_child = fdopen(std_out_child, "r");

    int out_err;
    int loop_count = 0;

    while (errno != EOF) {
        out_err = read_input_and_write(out_child, buffer[buf_index], &buf_index, &num_lines_written);
        if (out_err) {
            break;
        }
        loop_count++;
    }

    terminfo_reset_on_exit();
    // Log the last few lines to stdout.
    //printf("\t\t\t\t n_screen_log_lines: %d , num_lines_written: %d , buf_index: %d\n",
    //    n_screen_log_lines, num_lines_written, buf_index);
    if (n_screen_log_lines > num_lines_written) {
        n_screen_log_lines = num_lines_written;
        buf_index = 0;
    }
    int j;
    for (j = 0; j < n_screen_log_lines; j++) {
        fputs(buffer[buf_index], stdout);
        buf_index += 1;
        if (buf_index >= n_screen_log_lines) buf_index = 0;
    }
    return out_err;
}


int main(int argc, char * const * argv) {
    process_command_line_args(argc, argv);
    
    // http://www.gnu.org/software/libc/manual/html_node/Creating-a-Pipe.html
    // Pipe command takes two file descriptors to connect.
    int out_fd[2];      // Where child is going to write to.

    int rc;
    // http://www.tldp.org/LDP/lpg/node11.html
    if ((rc = pipe(out_fd)) != 0) return rc;

    // Start external program using a knife and fork.
    // Just kidding what is this fork business anyways?
    int pid = fork();
    if (pid == 0) {
        close(out_fd[0]);
        // We are the child process.
        // Close our outputs and use the pipes instead.
        close(STDOUT_FILENO);        dup2(out_fd[1], STDOUT_FILENO);
        close(STDERR_FILENO);        dup2(out_fd[1], STDERR_FILENO);
        close(out_fd[1]);
        int rc = system(child_command_line);
        printf("Child exiting.  return code %d.\n", rc);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        return rc;
    } else if (pid == -1) {
        perror("fork");
        return -1;
    } else {
        close(out_fd[1]);
        //printf("\t\t\t\t\t I'm the parent.\n");
        int parent_exit_status = parent(out_fd[0]);
        // Get exit status of child.
        int child_exit_status = 0;
        waitpid(pid, &child_exit_status, 0);
        if (child_exit_status) return child_exit_status;
        else                   return parent_exit_status;
    }
}
