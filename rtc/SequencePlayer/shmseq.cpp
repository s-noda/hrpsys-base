#include <iostream>
#include <pthread.h>
#include <cstring>

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <errno.h>

#define MAX_JOINT_NUM 64

namespace shmseq {
    struct STATE { enum NAME { READ, WRITE, IDLE}; };
    struct shmseq {
        double cur_rpy[3];
        double ref_angle_vector[MAX_JOINT_NUM];
        double tm;
        STATE::NAME state;
        pthread_mutex_t cmd_lock;
    };

    static struct shmseq *shm;

    void *set_shared_memory(key_t _key, size_t _size) {
        int shm_id; void *ptr;
        size_t size = _size * 2; key_t key = _key;
        shm_id=shmget(key, size, 0666|IPC_CREAT);
        if(shm_id==-1 && errno == EINVAL) {
            size = _size; shm_id=shmget(key, size, 0666|IPC_CREAT); }
        if(shm_id==-1) {
            std::cerr << "shmget failed, "
                      << "key=" << key
                      << "size=" << size
                      << "errno=" << errno << std::endl;
            return NULL; }
        ptr=(struct shared_data *)shmat(shm_id, (void *)0, 0);
        if(ptr==(void *)-1) {
            std::cerr << "shmget failed, "
                      << "key=" << key
                      << "size=" << size
                      << "errno=" << errno << std::endl;
            return NULL; }
        return ptr; }

    void *initialize(bool initq=true, int addr=5000) {
        shm = (struct shmseq *)set_shared_memory(addr, sizeof(struct shmseq));
        if ( ! shm ) {
            std::cerr << __func__ << ", shmseq initialization error occured" << std::endl;
        } else if ( initq ) {
            shm->state = STATE::IDLE;
            pthread_mutex_init(&shm->cmd_lock, NULL); }}

    bool setJointAngles(double* q, double tm, int size) {
        pthread_mutex_lock(&shm->cmd_lock);
        std::memcpy(shm->ref_angle_vector, q, size*sizeof(double));
        shm->tm = tm;
        shm->state = STATE::WRITE;
        pthread_mutex_unlock(&shm->cmd_lock);
        std::cout << __func__;
        for (int i=0;i<size;i++) std::cout << " " << shm->ref_angle_vector[i];
        std::cout << std::endl;
        return true; }
    bool getJointAngles(double* q, double* tm, int size) {
        bool ret = false;
        pthread_mutex_lock(&shm->cmd_lock);
        if ( shm->state == STATE::WRITE ) {
            ret = true;
            std::memcpy(q, shm->ref_angle_vector, size*sizeof(double));
            tm[0] = shm->tm;
            shm->state = STATE::READ; }
        pthread_mutex_unlock(&shm->cmd_lock);
        if ( ret ) {
            std::cout << __func__ ;
            for (int i=0;i<size;i++) std::cout << " " << q[i];
            std::cout << std::endl; }
        return ret; }
};
