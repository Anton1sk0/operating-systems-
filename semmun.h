
#ifndef SEMMUN_H
#define SEMMUN_H

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

#define FULL 0
#define EMPTY 1
#define UP (+1)
#define DOWN (-1)

#define upEmpty(sem_id) up(sem_id, EMPTY)
#define upFULL(sem_id) up(sem_id, FULL)
#define upChild(sem_id, i) up(sem_id, i+2-1)

#define downEmpty(sem_id) down(sem_id, EMPTY)
#define downFULL(sem_id) down(sem_id, FULL)
#define downChild(sem_id, i) down(sem_id, i+2-1)

#endif /* SEMMUN_H */

