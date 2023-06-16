#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>

#define FIFO_PUB "../tmp/fifoSC"



typedef struct programa {
    pid_t pid;
    char name[100];
    long timestamp;
} Programa;


void delete_programa(void *data) {
    free(data);
}


typedef struct programas_executing {
    GHashTable *p_hash_table;
} ProgramasExec;


int main(int argc, char **argv) {

    if (argc != 2) return 0;

    struct stat stats;

    if (stat(FIFO_PUB, &stats) == 0)
        if (unlink(FIFO_PUB) < 0) {
            perror("Unlinked failed");
            return -1;
        }


    if (mkfifo(FIFO_PUB, 0660) == -1) {
        perror("erro a criar fifo");
        return -1;
    }


    int fd_fifo;
    if ((fd_fifo = open(FIFO_PUB, O_RDONLY, 0660)) == -1) {
        perror("erro a abrir fifo");
        return -1;
    }

    ProgramasExec *proexec = malloc(sizeof(struct programas_executing));
    proexec->p_hash_table = g_hash_table_new_full(g_int_hash, g_int_equal, NULL, delete_programa);

    int buffer_size = 200;
    char buffer[buffer_size];


    while (1) {

        while (read(fd_fifo, buffer, buffer_size) > 0) {

            printf("mensagem recebida: %s\n",buffer);

            char *token = strtok(buffer, " ");

            if (strcmp(token, "execute") == 0) {

                token = strtok(NULL, " ");

                Programa *p = malloc(sizeof(struct programa));

                p->pid = (pid_t) strtol(token, NULL, 10);
                token = strtok(NULL, " ");
                int number_processes = (int) strtol(token, NULL, 10);
                token = strtok(NULL, " ");
                char processes[100] = "";
                strcat(processes, token);
                for (int i = 1; i < number_processes; i++) {
                    strcat(processes, " | ");
                    token = strtok(NULL, " ");
                    strcat(processes, token);
                }
                strcpy(p->name, processes);
                token = strtok(NULL, " ");
                p->timestamp = strtol(token, NULL, 10);

                g_hash_table_insert(proexec->p_hash_table, &p->pid, p);

            }

            else if (strcmp(token, "executeo") == 0) {

                token = strtok(NULL, " ");

                int pid = (int) strtol(token, NULL, 10);

                Programa *p = g_hash_table_lookup(proexec->p_hash_table, &pid);

                long timestamp_end;

                token = strtok(NULL, " ");
                timestamp_end = strtol(token, NULL, 10);


                if (fork() == 0) {

                    p->timestamp = timestamp_end - p->timestamp;


                    int savename_size = snprintf(NULL, 0, "../%s/%d.bin", argv[1], p->pid) + 1;
                    char savename[savename_size];
                    snprintf(savename, savename_size, "../%s/%d.bin", argv[1], p->pid);

                    int fd_save;
                    fd_save = open(savename, O_WRONLY | O_CREAT | O_TRUNC, 0660);

                    if (fd_save < 0) {
                        perror("erro a abrir file");
                        _exit(-1);
                    }

                    write(fd_save, p, sizeof(struct programa));

                    close(fd_save);

                    _exit(0);
                }


                g_hash_table_remove(proexec->p_hash_table, &pid);

            }

            else if (strcmp(token, "status") == 0) {

                if (fork() == 0) {

                    token = strtok(NULL, " ");

                    int fd_fifopid;
                    if ((fd_fifopid = open(token, O_WRONLY, 0660)) == -1) {
                        perror("erro a abrir fifo");
                        _exit(-1);
                    }


                    struct timeval time_end;
                    gettimeofday(&time_end, NULL);

                    long timestamp_end = time_end.tv_sec * 1000 + time_end.tv_usec / 1000;

                    GHashTableIter iter;
                    gpointer key, value;
                    g_hash_table_iter_init(&iter, proexec->p_hash_table);

                    while (g_hash_table_iter_next(&iter, &key, &value)) {

                        Programa *prog = (Programa *) value;

                        long time_passed = timestamp_end - prog->timestamp;

                        int info_size = 128;
                        char *info = malloc(info_size * sizeof(char));
                        snprintf(info, info_size, "%d %s %ld ms", prog->pid, prog->name, time_passed);

                        write(fd_fifopid, info, info_size);

                        free(info);
                    }

                    close(fd_fifopid);

                    _exit(0);
                }
            }

            else if (strcmp(token, "stats-time") == 0) {

                if (fork() == 0) {

                    token = strtok(NULL, " ");

                    int fd_fifopid;
                    if ((fd_fifopid = open(token, O_WRONLY, 0660)) == -1) {
                        perror("erro a abrir fifo");
                        _exit(-1);
                    }

                    token = strtok(NULL, " ");

                    int number_pids;
                    number_pids = (int) strtol(token, NULL, 10);

                    for (int i = 0; i < number_pids; i++) {

                        token = strtok(NULL, " ");

                        if (fork() == 0) {

                            int pathPIDFile_size =
                                    snprintf(NULL, 0, "../%s/%s.bin", argv[1], token) + 1;
                            char pathPIDFile[pathPIDFile_size];
                            snprintf(pathPIDFile, pathPIDFile_size, "../%s/%s.bin", argv[1], token);
                            int pid_file;
                            if ((pid_file = open(pathPIDFile, O_RDONLY, 0660)) == -1) {
                                perror("erro a abrir fifo");
                                _exit(0);
                            }

                            Programa p;

                            int time_counter = 0;

                            while (read(pid_file,&p,sizeof (struct programa))>0){
                                time_counter += (int) p.timestamp;
                            }

                            close(pid_file);

                            _exit(time_counter);
                        }

                    }

                    int time_counter = 0;
                    int status;

                    while (wait(&status)>0){
                        if (WIFEXITED(status))
                            if (WEXITSTATUS(status)>0) time_counter += WEXITSTATUS(status);
                    }

                    write(fd_fifopid,&time_counter,sizeof (int));

                    close(fd_fifopid);
                    _exit(0);
                }

            }


            else if (strcmp(token, "stats-command") == 0) {

                if (fork() == 0) {

                    token = strtok(NULL, " ");

                    int fd_fifopid;
                    if ((fd_fifopid = open(token, O_WRONLY, 0660)) == -1) {
                        perror("erro a abrir fifo");
                        _exit(-1);
                    }

                    token = strtok(NULL, " ");
                    int number_commands = (int) strtol(token, NULL, 10);

                    char command[100] = "";

                    token = strtok(NULL, " ");
                    strcat(command,token);
                    for (int i = 1; i<number_commands; i++){
                        strcat(command, " ");
                        token = strtok(NULL, " ");
                        strcat(command,token);
                        strcat(command, " ");
                        token = strtok(NULL, " ");
                        strcat(command,token);
                    }

                    token = strtok(NULL, " ");

                    int number_pids;
                    number_pids = (int) strtol(token, NULL, 10);

                    for (int i = 0; i < number_pids; i++) {

                        token = strtok(NULL, " ");

                        if (fork() == 0) {

                            int pathPIDFile_size =
                                    snprintf(NULL, 0, "../%s/%s.bin", argv[1], token) + 1;
                            char pathPIDFile[pathPIDFile_size];
                            snprintf(pathPIDFile, pathPIDFile_size, "../%s/%s.bin", argv[1], token);
                            int pid_file;
                            if ((pid_file = open(pathPIDFile, O_RDONLY, 0660)) == -1) {
                                perror("erro a abrir fifo");
                                _exit(0);
                            }

                            Programa p;

                            int time_counter = 0;

                            while (read(pid_file,&p,sizeof (struct programa))>0){
                                if (strcmp(p.name,command)==0)
                                    time_counter++;
                            }

                            close(pid_file);

                            _exit(time_counter);
                        }

                    }

                    int time_counter = 0;
                    int status;

                    while (wait(&status)>0){
                        if (WIFEXITED(status))
                            if (WEXITSTATUS(status)>0) time_counter += WEXITSTATUS(status);
                    }

                    write(fd_fifopid,&time_counter,sizeof (int));

                    close(fd_fifopid);
                    _exit(0);
                }


            }


            else if (strcmp(token, "stats-uniq") == 0) {

                if (fork() == 0) {

                    token = strtok(NULL, " ");

                    int fd_fifopid;
                    if ((fd_fifopid = open(token, O_WRONLY, 0660)) == -1) {
                        perror("erro a abrir fifo");
                        _exit(-1);
                    }

                    token = strtok(NULL, " ");

                    int number_pids;
                    number_pids = (int) strtol(token, NULL, 10);


                    int pp[2];

                    pipe(pp);

                    for (int i = 0; i < number_pids; i++) {

                        token = strtok(NULL, " ");

                        if (fork() == 0) {

                            close(pp[0]);

                            int pathPIDFile_size =
                                    snprintf(NULL, 0, "../%s/%s.bin", argv[1], token) + 1;
                            char pathPIDFile[pathPIDFile_size];
                            snprintf(pathPIDFile, pathPIDFile_size, "../%s/%s.bin", argv[1], token);
                            int pid_file;
                            if ((pid_file = open(pathPIDFile, O_RDONLY, 0660)) == -1) {
                                perror("erro a abrir fifo");
                                _exit(0);
                            }

                            Programa p;

                            char names[100] = "";

                            while (read(pid_file,&p,sizeof (struct programa))>0){
                                strcat(names,p.name);
                                strcat(names, "\n");
                            }

                            write(pp[1],names,100);

                            close(pid_file);

                            _exit(0);
                        }

                    }

                    char programs[200] = "";

                    close(pp[1]);

                    int status;

                    char buffer_names[100];
                    while (wait(&status)>0){
                        if (WIFEXITED(status))
                            if (WEXITSTATUS(status)==0){
                                read(pp[0],buffer_names,100);
                                strcat(programs, buffer_names);
                            }
                    }

                    close(pp[0]);

                    int msg_pp[2];

                    pipe(pp);
                    pipe(msg_pp);

                    dup2(pp[0],STDIN_FILENO);

                    write(pp[1], programs, 200);

                    if (fork() == 0){

                        close(pp[1]);
                        close(msg_pp[0]);

                        dup2(msg_pp[1],STDOUT_FILENO);

                        if (execlp("uniq", "uniq",NULL)< 0) {
                            perror("erro a executar comando");
                            _exit(-1);
                        }

                        _exit(0);
                    }

                    close(pp[1]);
                    close(pp[0]);
                    close(msg_pp[1]);


                    char info[200];

                    read(msg_pp[0], info, 200);

                    close(msg_pp[0]);

                    write(fd_fifopid, info, 200);

                    close(fd_fifopid);
                    _exit(0);
                }

            }



        }


    }


    if (unlink(FIFO_PUB) < 0) {
        perror("Unlinked failed");
        return -1;
    }

    return 0;


}





