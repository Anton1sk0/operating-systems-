
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "limits.h"
#include <limits.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include "semmun.h"
#include "FileReader.h"

// ufos.txt 10 20 10

// ./a.out filename children segment_lines loops

void up(int sem_id, int sem_num) {
    struct sembuf sop = {0};
    sop.sem_num = sem_num;
    sop.sem_op = UP;
    sop.sem_flg = 0;
    int k = (semop(sem_id, &sop, 1) == 0);
    if (!k) {
        perror("Semop up failed:");
    }
    assert(k);
}

void down(int sem_id, int sem_num) {
    struct sembuf sop = {0};
    sop.sem_num = sem_num;
    sop.sem_op = DOWN;
    sop.sem_flg = 0;
    int k = (semop(sem_id, &sop, 1) == 0);
    if (!k) {
        perror("Semop down failed:");
    }
    assert(k);
}

int children_main(int id, int loops, int segments, int lines_per_segment, int sem_id, char * shared_memory_pointer) {
    srand(id);

    float submit_time = 0.0f;
    float answer_time = 0.0f;
    char buffer[LINE_LENGTH] = "monkey";
    char logfile[PATH_MAX] = "logfile_";
    int * process_id = (int *) (shared_memory_pointer + 0);
    int * segment_id = process_id + 1;
    int * line_id = process_id + 2;
    int * request_type = process_id + 3;
    char * segment_pointer = (char*) (process_id + 4);
    struct timeval start, end;
    long secs_used, micros_used;

    sprintf(buffer, "%d", id);
    strcat(logfile, buffer);
    strcat(logfile, "_");
    sprintf(buffer, "%d", getpid());
    strcat(logfile, buffer);
    strcat(logfile, ".txt");

    FILE * fp = fopen(logfile, "w+");

    if (fp == NULL) {
        perror("fopen failed");
        exit(1);
    }

    printf("  PID %d, child %d: created with logfile: %s \n", getpid(), id, logfile);

    for (int i = 0, l = 0, s = rand() % segments; i < loops; i++) {
        if (rand() % 100 > 70) {
            s = rand() % segments;
        }

        l = rand() % lines_per_segment;

        printf("  PID %d, child %d: executing step %d of %d:   [id:%d,s:%d,l:%d] \n", getpid(), id, i + 1, loops, id, s, l);

        gettimeofday(&start, NULL);

        downEmpty(sem_id);
        // produce:
        *request_type = REQUEST;
        *process_id = id;
        *segment_id = s;
        *line_id = l;
        upFULL(sem_id);

        gettimeofday(&end, NULL);

        secs_used = (end.tv_sec - start.tv_sec); //avoid overflow by subtracting first
        micros_used = ((secs_used * 1000000) + end.tv_usec) - (start.tv_usec);
        submit_time = micros_used / 1000;

        gettimeofday(&start, NULL);

        downChild(sem_id, id);

        gettimeofday(&end, NULL);

        secs_used = (end.tv_sec - start.tv_sec); //avoid overflow by subtracting first
        micros_used = ((secs_used * 1000000) + end.tv_usec) - (start.tv_usec);
        answer_time = micros_used / 1000;

        int M = sizeof (char)*lines_per_segment * (LINE_LENGTH + 1);
        char * merged = malloc(M);
        memcpy(merged, segment_pointer, M);

        char * ptr1 = merged;
        int counter = 0;

        while (true) {
            if (*ptr1 == '\n') {
                counter++;
            }
            if (*ptr1 == '\0') {
                break;
            }
            if (counter == l) {
                break;
            }
            ptr1++;
        }

        char * ptr2 = merged;
        int counter2 = 0;

        while (true) {
            if (*ptr2 == '\n') {
                counter2++;
            }
            if (*ptr2 == '\0') {
                break;
            }

            if (counter2 == l + 1) {
                break;
            }
            ptr2++;
        }

        int length = ptr2 - ptr1 - 1;

        //        printf("length = %d\n", length);

        memset(buffer, 0, sizeof (buffer));

        memcpy(buffer, ptr1 + 1, length);

        free(merged);

        char * t = buffer;
        while (*t == ' ') {
            t++;
        }

        fprintf(fp, "(%d/%d) %d %d %f %f '%s'\n", i + 1, loops, s, l, submit_time, answer_time, t);
        fprintf(stdout, "CHILD RECEIVED: (%d/%d) %d %d %f %f '%s'\n", i + 1, loops, s, l, submit_time, answer_time, t);

        downEmpty(sem_id);
        // produce:
        *request_type = NOTIFICATION;
        *process_id = id;
        *segment_id = s;
        *line_id = l;
        upFULL(sem_id);

        usleep(LOOP_DELAY);
    }


    fclose(fp);

    printf("  PID %d, child %d: destroyed \n", getpid(), id);

    exit(0);
}

int main(int argc, char** argv) {
    if (argc != 5) {
        printf("Invalid number of arguments \n");
        return 1;
    }

    char * filename = argv[1];
    int children = atoi(argv[2]);
    int segment_lines = atoi(argv[3]);
    int loops = atoi(argv[4]);
    int total_lines = 0;
    char buffer[4000];
    int segments;
    key_t key;
    int shmid;
    int nsems = 2 + children;
    int SHM_SIZE = 4 * sizeof (int) + (LINE_LENGTH + 1) * segment_lines;
    struct semid_ds buf;
    struct sembuf sb;
    int semid;
    int ZERO_VALUE = 0;
    int ** on_hold_ids = NULL;

    FILE * fp = fopen(filename, "rt");

    if (fp == NULL) {
        printf("Invalid filename. \n");
        exit(1);
    }

    while (fgets(buffer, sizeof (buffer), fp)) {
        total_lines++;
    }

    segments = total_lines / segment_lines;

    if (segments == 0) {
        printf("Segmens are zero, this is not an acceptable value \n");
        exit(1);
    }

    if (total_lines < 1000) {
        printf("total_lines are less than 1000, this is not an acceptable value \n");
        exit(1);
    }

    /* make the key: */
    if ((key = ftok(argv[0], 'R')) == -1) {
        perror("ftok");
        exit(1);
    }

    /* connect to (and possibly create) the segment: */
    if ((shmid = shmget(key, SHM_SIZE, 0644 | IPC_CREAT)) == -1) {
        perror("shmget");
        exit(1);
    }

    if ((semid = semget(IPC_PRIVATE, nsems, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
        perror("semget");
        exit(1);
    }

    printf("Sem ID: %d \n", semid);
    printf("SHM ID: %d \n", shmid);

    for (int i = 0; i < nsems; i++) {
        if (semctl(semid, i, SETVAL, ZERO_VALUE) < 0) {
            perror("Could not set value of semaphore");
            exit(4);
        }
    }

    upEmpty(semid);

    /* attach to the segment to get a pointer to it: */
    char * shared_memory_pointer = shmat(shmid, NULL, 0);
    if (shared_memory_pointer == (char *) (-1)) {
        perror("shmat");
        exit(1);
    }

    int * process_id = (int *) (shared_memory_pointer + 0);
    int * segment_id = process_id + 1;
    int * line_id = process_id + 2;
    int * request_type = process_id + 3;
    char * segment_pointer = (char*) (process_id + 4);

    on_hold_ids = malloc(sizeof (int*)*(children + 1));

    printf("Parent data: \n");
    printf("  PID of parent    : %d \n", getpid());
    printf("  Total lines      : %d \n", total_lines);
    printf("  Lines per segment: %d \n", segment_lines);
    printf("  Bytes per lines  : %d \n", LINE_LENGTH);
    printf("Parameters:\n");
    printf("  Filename         :  %s \n", filename);
    printf("  Children         :  %d \n", children);
    printf("  Segments (full)  :  %d \n", segments);
    printf("  Children loops   :  %d \n", loops);
    printf("Shared memory data:\n");
    printf("  ID               :  %d \n", shmid);
    printf("  Size             :  %ld \n", 3 * sizeof (int) + (LINE_LENGTH + 1) * segment_lines);

    for (int i = 0; i < children; i++) {
        int k = fork();

        if (k == -1) {
            perror("fork failed");
            exit(1);
        }

        if (k == 0) {
            children_main(i + 1, loops, segments, segment_lines, semid, shared_memory_pointer);
        } else {
            on_hold_ids[i + 1] = malloc(sizeof (int)*2);
            on_hold_ids[i + 1][0] = NOT_WAITING;
            on_hold_ids[i + 1][1] = INT_MAX;
        }
    }

    on_hold_ids[0] = NULL;

    int requests_processed = 0, notifications_processed = 0, readers = 0;

    while (requests_processed < loops * children || notifications_processed < loops * children) {
        // consume
        downFULL(semid);
        int rt = *request_type;
        int id = *process_id;
        int s = *segment_id;
        int l = *line_id;
        upEmpty(semid);

        if (rt == REQUEST) {
            requests_processed++;

            printf(">> Request: %d \n", requests_processed);

            if (requests_processed == 1 || segment_already_loaded(s) == true) { // first request or cache hit
                if (requests_processed == 1) {
                    char * merged = load_segment(fp, s, segment_lines);
                    int M = sizeof (char)*segment_lines * (LINE_LENGTH + 1);
                    memcpy(segment_pointer, merged, M);
                    free(merged);
                }

                readers++;
                upChild(semid, id);
            } else { // cache miss
                if (readers == 0) {
                    char * merged = load_segment(fp, s, segment_lines);
                    int M = sizeof (char)*segment_lines * (LINE_LENGTH + 1);
                    memcpy(segment_pointer, merged, M);
                    free(merged);

                    readers++;
                    upChild(semid, id);
                } else {
                    // do not wake, child, since it cannot be serviced
                    int timestamp = requests_processed;
                    on_hold_ids[id][0] = s;
                    on_hold_ids[id][1] = timestamp;
                }
            }
        } else if (rt == NOTIFICATION) {
            notifications_processed++;
            readers--;

            if (readers == 0) {
                int min_pos = -1;
                int timestamp = INT_MAX;

                for (int i = 1; i <= children; i++) {
                    if (on_hold_ids[i][0] != NOT_WAITING && on_hold_ids[i][1] < timestamp) {
                        min_pos = i;
                        timestamp = on_hold_ids[i][1];
                    }
                }

                if (min_pos != -1) {
                    char * merged = load_segment(fp, on_hold_ids[min_pos][0], segment_lines);
                    int M = sizeof (char)*segment_lines * (LINE_LENGTH + 1);
                    memcpy(segment_pointer, merged, M);
                    free(merged);

                    readers++;
                    upChild(semid, min_pos);

                    for (int i = 1; i <= children; i++) {
                        if (i != min_pos && on_hold_ids[i][0] != NOT_WAITING && on_hold_ids[i][0] == on_hold_ids[min_pos][0]) {
                            on_hold_ids[i][0] = NOT_WAITING;
                            on_hold_ids[i][1] = INT_MAX;

                            readers++;
                            upChild(semid, i);
                        }
                    }

                    on_hold_ids[min_pos][0] = NOT_WAITING;
                    on_hold_ids[min_pos][1] = INT_MAX;
                }
            }
        } else {
            exit(1);
        }
    }

    printf("MAIN: Total Requests: %d, Waiting table: \n", requests_processed);

    for (int i = 1; i <= children; i++) {
        if (on_hold_ids[i][1] != INT_MAX) {
            printf("MAIN: Process ID: %d - Segment:%d, Timestamp: %d \n", i, on_hold_ids[i][0], on_hold_ids[i][1]);
        }
    }

    printf("MAIN: waiting for children to exit \n");

    for (int i = 0; i < children; i++) {
        wait(NULL);
    }



    fclose(fp);

    if (shmdt(shared_memory_pointer) == -1) {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(shmid, IPC_RMID, 0) == -1) {
        fprintf(stderr, "shmctl(IPC_RMID) failed\n");
        exit(1);
    }

    if (semctl(semid, 0, IPC_RMID) < 0) {
        perror("Could not delete semaphore");
    }

    printf("MAIN: Exit \n");

    for (int i = 0; i < children; i++) {
        free(on_hold_ids[i + 1]);
    }
    free(on_hold_ids);

    return 0;
}

