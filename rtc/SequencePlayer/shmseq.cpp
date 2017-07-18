#include <iostream>
#include <pthread.h>
#include <cstring>

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#include <assert.h>

#define MAX_JOINT_NUM 64

namespace shmseq {
    struct STATE { enum NAME { READ, WRITE, IDLE}; };
    struct shmseq {
        double cur_rpy[3];
        double ref_angle_vector[MAX_JOINT_NUM];
        double tm;
        STATE::NAME state;
    };

    struct shmseq *shm;
    int semid;

    bool lock(){
        struct timespec ts100ms = {0, 100*1000};
        struct sembuf semlock = {0, -1, 0};
        return (semtimedop(semid,&semlock,1,&ts100ms) == 0); }
    bool unlock(){
        struct sembuf semunlock = {0, +1, 0};
        return (semtimedop(semid,&semunlock,1, NULL) == 0); }

    bool validq() { return (shm && semid != -1); }

    void *set_shared_memory(key_t key, size_t size) {
        int shm_id; void *ptr;
        shm_id=shmget(key, size*2, 0666|IPC_CREAT);
        if(shm_id==-1 && errno == EINVAL) shm_id=shmget(key, size, 0666|IPC_CREAT);
        if(shm_id==-1) return NULL;
        ptr=(struct shared_data *)shmat(shm_id, (void *)0, 0);
        if(ptr==(void *)-1) return NULL;
        return ptr; }

    void initialize(bool initq=true, int shmkey=4679, int semkey=4649) {
        shm = (struct shmseq *)set_shared_memory(shmkey, sizeof(struct shmseq));
        semid = semget(semkey, 1, 0666 | IPC_CREAT);
        if ( ! validq() ) {
            std::cerr << __func__ << " error occured"
                      << ", semid = " << semid
                      << ", shm   = " << shm << std::endl;
        } else if ( initq ) { shm->state = STATE::IDLE; unlock(); }}

    bool setJointAngles(double* q, double tm, int size, bool checkq=true) {
        if ( ! validq() || (checkq && ! lock()) ) return false;
        std::memcpy(shm->ref_angle_vector, q, size*sizeof(double));
        shm->tm = tm; shm->state = STATE::WRITE;
        if ( checkq ) assert(unlock()); // fatal error
        return true; }
    bool getJointAngles(double* q, double* tm, int size, bool checkq=true) {
        bool ret = false;
        if ( ! validq() || (checkq && ! lock()) ) return ret;
        if ( shm->state == STATE::WRITE ) {
            ret = true; tm[0] = shm->tm;
            std::memcpy(q, shm->ref_angle_vector, size*sizeof(double));
            shm->state = STATE::READ; }
        if ( checkq ) assert(unlock()); // fatal error
        return ret; }

    bool setCurRPY(double r, double p, double y, bool checkq=true) {
        if ( ! validq() || (checkq && ! lock()) ) return false;
        shm->cur_rpy[0] = r; shm->cur_rpy[1] = p; shm->cur_rpy[2] = y;
        if ( checkq ) assert(unlock());
        return true; }
    bool getCurRPY(double* rpy, bool checkq=true) {
        if ( ! validq() || (checkq && ! lock()) ) return false;
        std::memcpy(rpy, shm->cur_rpy, 3*sizeof(double));
        if ( checkq ) assert(unlock());
        return true; }
};
