#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>

#define FIFO_PUB "../tmp/fifoSC"


int open_fifopub(int *fd_fifo){

    if ((*fd_fifo = open(FIFO_PUB, O_WRONLY, 0660)) == -1) {
        perror("erro a abrir fifo");
        return -1;
    }

    return 0;
}


int create_fifopriv(pid_t pid, char** fifo){

    int fifoPID_size =
            snprintf(NULL, 0, "../tmp/fifo%d", pid) + 1;
    char fifoPID[fifoPID_size];
    snprintf(fifoPID, fifoPID_size, "../tmp/fifo%d", pid);

    *fifo = strdup(fifoPID);

    if (mkfifo(fifoPID, 0660) == -1) {
        perror("erro a criar fifo");
        return -1;
    }

    return 0;
}


int status(){

    int fd_fifo;
    if (open_fifopub(&fd_fifo) == -1) return -1;

    pid_t pid = getpid();

    char *fifoPID;
    if (create_fifopriv(pid, &fifoPID)==-1) return -1;

    int message_size =
            snprintf(NULL, 0, "status ../tmp/fifo%d", pid) + 1;
    char info_fifo[message_size];
    snprintf(info_fifo, message_size, "status ../tmp/fifo%d", pid);

    write(fd_fifo, info_fifo, message_size);

    close(fd_fifo);


    int fd_fifoPID;
    if ((fd_fifoPID = open(fifoPID, O_RDONLY, 0660)) == -1) {
        perror("erro a abrir fifo");
        return -1;
    }

    int buffer_size = 128;
    char buffer[buffer_size];
    while (read(fd_fifoPID, buffer, buffer_size) > 0) {
        printf("%s\n", buffer);
    }

    close(fd_fifoPID);

    if (unlink(fifoPID) < 0) {
        perror("Unlinked failed");
        return -1;
    }

    free(fifoPID);

    return 0;
}


int execute(char **argv){

    if (strcmp(argv[2], "-u") != 0 && strcmp(argv[2], "-p") != 0) return 0;

    int fd_fifo;
    if (open_fifopub(&fd_fifo) == -1) return -1;


    pid_t pid = getpid();


    char *progs = strdup(argv[3]);
    int number_processes = 1;
    for (int i = 0; i < strlen(progs); i++) {
        if (progs[i] == '|')
            number_processes++;
    }

    if ((number_processes > 1 && strcmp(argv[2], "-u") == 0)) return 0;


    char *prog_names;

    char *oneprog_args[10];

    if (number_processes == 1) {
        char *token = strtok(argv[3], " ");
        int i = 0;

        for (; token != NULL; i++) {

            oneprog_args[i] = token;
            token = strtok(NULL, " ");
        }
        oneprog_args[i] = NULL;

        prog_names = oneprog_args[0];
    }

    else if (number_processes > 1) {
        char prog_names_aux[128] = "";

        progs = strtok(progs, " ");

        strcat(prog_names_aux, progs);

        progs = strtok(NULL, " ");
        while (progs != NULL) {

            if (strcmp(progs, "|") == 0) {
                strcat(prog_names_aux, " ");
                progs = strtok(NULL, " ");
                strcat(prog_names_aux, progs);
            }

            progs = strtok(NULL, " ");
        }

        prog_names = prog_names_aux;
    }

    struct timeval time_start;
    gettimeofday(&time_start, NULL);

    long timestamp_start = time_start.tv_sec * 1000 + time_start.tv_usec / 1000;

    int size_info =
            snprintf(NULL, 0, "execute %d %d %s %ld", pid, number_processes, prog_names, timestamp_start) + 1;
    char *info;
    info = malloc(size_info * sizeof(char));

    snprintf(info, size_info, "execute %d %d %s %ld", pid, number_processes, prog_names, timestamp_start);

    write(fd_fifo, info, size_info);


    if (number_processes==1) {

        printf("Running PID %d\n", pid);
        if (fork() == 0) {
            if (execvp(oneprog_args[0], oneprog_args) < 0) {
                perror("erro a executar comando");
                _exit(-1);
            }
            _exit(0);
        }

        wait(NULL);
    }

    else if (number_processes>1) {

        int fd[number_processes - 1][2];

        char *token = strtok(argv[3], "|");

        printf("Running PID %d\n", pid);
        for (int i = 0; i < number_processes; i++) {


            if (i < number_processes - 1) {
                if (pipe(fd[i]) < 0) {
                    perror("erro a criar pipe anónimo");
                    return -1;
                }
            }


            if (i == 0) {

                if (fork() == 0) {

                    close(fd[i][0]);

                    if (dup2(fd[i][1], STDOUT_FILENO) < 0) {
                        perror("[filho] erro dup2");
                        _exit(-1);
                    }

                    close(fd[i][1]);

                    char *name = strdup(token);
                    name = strtok(name, " ");

                    char *exec_arg[10];
                    token = strtok(argv[3], " ");
                    int i_t = 0;

                    for (; token != NULL; i_t++) {
                        exec_arg[i_t] = token;
                        token = strtok(NULL, " ");
                    }
                    exec_arg[i_t] = NULL;

                    if (execvp(name, exec_arg)< 0) {
                        perror("erro a executar comando");
                        _exit(-1);
                    }

                    _exit(0);
                }

                close(fd[i][1]);
            } else if (i < number_processes - 1) {

                if (fork() == 0) {

                    if (dup2(fd[i - 1][0], STDIN_FILENO) < 0) {
                        perror("[filho] erro dup2");
                        _exit(-1);
                    }

                    close(fd[i - 1][0]);

                    if (dup2(fd[i][1], STDOUT_FILENO) < 0) {
                        perror("[filho] erro dup2");
                        _exit(-1);
                    }

                    close(fd[i][1]);

                    char *name = strdup(token);
                    name = strtok(name, " ");

                    char *exec_arg[10];
                    token = strtok(token, " ");
                    int i_t = 0;

                    for (; token != NULL; i_t++) {
                        exec_arg[i_t] = token;
                        token = strtok(NULL, " ");
                    }
                    exec_arg[i_t] = NULL;

                    if (execvp(name, exec_arg)< 0) {
                        perror("erro a executar comando");
                        _exit(-1);
                    }

                    _exit(0);
                }

                close(fd[i - 1][0]);
                close(fd[i][1]);
            } else {

                if (fork() == 0) {

                    if (dup2(fd[i - 1][0], STDIN_FILENO) < 0) {
                        perror("[filho] erro dup2");
                        _exit(-1);
                    }

                    close(fd[i - 1][0]);

                    char *name = strdup(token);
                    name = strtok(name, " ");

                    char *exec_arg[10];
                    token = strtok(token, " ");
                    int i_t = 0;

                    for (; token != NULL; i_t++) {
                        exec_arg[i_t] = token;
                        token = strtok(NULL, " ");
                    }
                    exec_arg[i_t] = NULL;

                    if (execvp(name, exec_arg)< 0) {
                        perror("erro a executar comando");
                        _exit(-1);
                    }

                    _exit(0);
                }

                close(fd[i - 1][0]);
            }

            token = strtok(NULL, "|");
        }

        while (wait(NULL) > 0);
    }



    struct timeval time_end;
    gettimeofday(&time_end, NULL);

    long timestamp_end = time_end.tv_sec * 1000 + time_end.tv_usec / 1000;

    size_info =
            snprintf(NULL, 0, "executeo %d %ld", pid, timestamp_end) + 1;
    free(info);
    info = malloc(size_info * sizeof(char));

    snprintf(info, size_info, "executeo %d %ld", pid, timestamp_end);


    write(fd_fifo, info, size_info);

    free(info);
    long time_passed = timestamp_end - timestamp_start;

    printf("Ended in %ld ms\n", time_passed);

    close(fd_fifo);

    return 0;
}


int stats_time(int argc, char** argv){

    int fd_fifo;

    if (open_fifopub(&fd_fifo)==-1) return -1;

    pid_t pid = getpid();

    char *fifoPID;
    if (create_fifopriv(pid, &fifoPID)==-1) return -1;


    char pids[200];
    strcpy(pids,argv[2]);
    for (int i = 3; i<argc; i++){
        strcat(pids, " ");
        strcat(pids,argv[i]);
    }


    int info_size = snprintf(NULL, 0, "stats-time ../tmp/fifo%d %d %s", pid, argc - 2, pids) + 1;
    char info[info_size];
    snprintf(info, info_size, "stats-time ../tmp/fifo%d %d %s", pid, argc - 2, pids);

    write(fd_fifo, info, info_size);

    close(fd_fifo);

    int fd_fifoPID;
    if ((fd_fifoPID = open(fifoPID, O_RDONLY, 0660)) == -1) {
        perror("erro a abrir fifo");
        return -1;
    }

    int time;

    read(fd_fifoPID, &time, sizeof (int));

    printf("Total execution time is %d ms\n", time);


    close(fd_fifoPID);


    if (unlink(fifoPID) < 0) {
        perror("Unlinked failed");
        return -1;
    }

    free(fifoPID);

    return 0;
}


int stats_command(int argc, char** argv){

    int fd_fifo;
    open_fifopub(&fd_fifo);

    pid_t pid = getpid();

    char *fifoPID;
    create_fifopriv(pid,&fifoPID);


    char pids[200];
    strcpy(pids,argv[3]);
    for (int i = 4; i<argc; i++){
        strcat(pids, " ");
        strcat(pids,argv[i]);
    }


    int number_processes = 1;
    char *command = argv[2];
    for (int i = 0; i < strlen(command); i++) {
        if (command[i] == '|')
            number_processes++;
    }

    int info_size =
            snprintf(NULL, 0, "stats-command ../tmp/fifo%d %d %s %d %s", pid, number_processes, argv[2], argc - 3, pids) + 1;
    char info[info_size];
    snprintf(info, info_size, "stats-command ../tmp/fifo%d %d %s %d %s", pid, number_processes, argv[2], argc - 3, pids);

    write(fd_fifo, info, info_size);

    close(fd_fifo);


    int fd_fifoPID;
    if ((fd_fifoPID = open(fifoPID, O_RDONLY, 0660)) == -1) {
        perror("erro a abrir fifo");
        return -1;
    }

    int number_executions;

    read(fd_fifoPID, &number_executions, sizeof (int));

    printf("%s was executed %d times\n", argv[2], number_executions);


    close(fd_fifoPID);


    if (unlink(fifoPID) < 0) {
        perror("Unlinked failed");
        return -1;
    }

    free(fifoPID);

    return 0;
}


int stats_uniq(int argc, char** argv){

    int fd_fifo;
    open_fifopub(&fd_fifo);

    pid_t pid = getpid();

    char* fifoPID;
    create_fifopriv(pid,&fifoPID);

    char pids[200];
    strcpy(pids,argv[2]);
    for (int i = 3; i<argc; i++){
        strcat(pids, " ");
        strcat(pids,argv[i]);
    }

    int info_size =
            snprintf(NULL, 0, "stats-uniq ../tmp/fifo%d %d %s", pid, argc-2, pids) + 1;
    char info[info_size];
    snprintf(info, info_size, "stats-uniq ../tmp/fifo%d %d %s", pid, argc - 2, pids);

    write(fd_fifo, info, info_size);

    close(fd_fifo);

    int fd_fifoPID;
    if ((fd_fifoPID = open(fifoPID, O_RDONLY, 0660)) == -1) {
        perror("erro a abrir fifo");
        return -1;
    }

    int processes_size = 128;
    char processes[processes_size];

    read(fd_fifoPID, &processes, processes_size);


    printf("%s", processes);


    close(fd_fifoPID);


    if (unlink(fifoPID) < 0) {
        perror("Unlinked failed");
        return -1;
    }

    free(fifoPID);

    return 0;

}



int main(int argc, char **argv) {


    if (argc == 2) {

        if (strcmp(argv[1], "status") == 0)
            return status();

    }

    else if (argc > 2) {

        if (strcmp(argv[1], "execute") == 0) {
            return execute(argv);
        }

        else if (strcmp(argv[1], "stats-time") == 0) {
            return stats_time(argc,argv);
        }

        else if (strcmp(argv[1], "stats-command") == 0) {
            return stats_command(argc,argv);
        }


        else if (strcmp(argv[1], "stats-uniq") == 0) {
            return stats_uniq(argc,argv);
        }

    }


    return 0;
}