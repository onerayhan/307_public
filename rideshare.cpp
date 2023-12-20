#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>
#include <mutex>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <cassert>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>


using namespace std;

class Semaphore {
public:
    Semaphore(int initial_value, sem_t & mtx) : extmtx(mtx), s_value(initial_value){
        
        pthread_mutex_init(&s_lock, NULL);
        pthread_cond_init(&cond, NULL);
        s_value = initial_value;
    }

    ~Semaphore() {
        pthread_mutex_destroy(&s_lock);
        pthread_cond_destroy(&cond);
    }

    void wait() {
        pthread_mutex_lock(&s_lock);
        s_value--;
        //Release global mutex like a condition variable before entering the wait
        sem_post(&extmtx);
        if (s_value < 0) {
            pthread_cond_wait(&cond, &s_lock);
        }
        //s_value--;
        pthread_mutex_unlock(&s_lock);
    }

    void post() {
        pthread_mutex_lock(&s_lock);
        s_value++;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&s_lock);
    }

    void getValue(int *val) const {
        *val = s_value;
    }

    void postValue(int val) {
        pthread_mutex_lock(&s_lock);
        for(int i = 0; i< val; i++){
            ++s_value;
            pthread_cond_signal(&cond);
        }
        pthread_mutex_unlock(&s_lock);
    }
    void setExtMtx(sem_t & mtx) {
        extmtx = mtx;
    }

private:
    mutable pthread_mutex_t s_lock;
    //binary semaphore used for global mutex
    sem_t & extmtx;
    pthread_cond_t cond;
    int s_value;
};

bool isArgsCorrect(int, int);
void* fanThread( void* args);
int getRandomInt(int, int);
const int TEAM_A = 0;
const int TEAM_B = 1;
const int MAX_WAITING = 4;
int fanCount[2];
//binary semaphore for mutex
sem_t mtx;
int carID = 0;
Semaphore* sem_Teams[2];
pthread_barrier_t barrier;


int main(int argc, char * argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <numFansA> <numFansB>" << std::endl;
        return 1;
    }
    srand(time(0));
    sem_init(&mtx, 0, 1);
    sem_Teams[0] = new Semaphore(0 ,mtx);
    sem_Teams[1] = new Semaphore(0, mtx);
    pthread_barrier_init(&barrier, NULL, MAX_WAITING);
    int numFansA = atoi(argv[1]); 
    int numFansB = atoi(argv[2]); 
    vector<pthread_t> teamAThreads;
    vector<pthread_t> teamBThreads;
    if(!isArgsCorrect(numFansA, numFansB)) {
        cout <<  "Main Terminates\n";
        return 0;
    }
    int totalFans = numFansA + numFansB;
    int strideA = totalFans / numFansA;
    int strideB = totalFans / numFansB;
    int counterA = 0;
    int counterB = 0;

    // For total  number of fans pick random fan and create a thread for it
    for (int i = 0; i < totalFans ; i++) {
        if(counterA < strideA && numFansA > 0) {
            pthread_t pThread;
            pthread_create(&pThread,NULL, fanThread, (void*)&TEAM_A );
            teamAThreads.push_back(pThread);
            ++counterA;
            --numFansA;
        }
        else if(counterB < strideB && numFansB > 0) {
            pthread_t pThread;
            pthread_create(&pThread,NULL, fanThread, (void*)&TEAM_B );
            teamBThreads.push_back(pThread);
            ++counterB;
            --numFansB;

        }
        // Reset counters if they reach their stride values
        if (counterA == strideA) counterA = 0;
        if (counterB == strideB) counterB = 0;
    }

    for(auto thread: teamAThreads) {
        pthread_join(thread, NULL);
    }
    for(auto thread: teamBThreads){
        pthread_join(thread, NULL);
    }
    pthread_barrier_destroy(&barrier);
    delete sem_Teams[0];
    delete sem_Teams[1];
    sem_destroy(&mtx);
    cout <<  "Main Terminates\n";
}

void* fanThread(void* args){
    bool driver = false;
    int team = *((int*)args);
    sem_wait(&mtx);

    cout<< "Thread ID: " << pthread_self() << ", Team: " << (team == 0 ? "A" : "B") << ", I am looking for a car\n";
    cout.flush();
    sem_Teams[team]->getValue(&fanCount[team]);
    sem_Teams[1 - team]->getValue(&fanCount[1 - team]);
    if(fanCount[team] < -2){
        driver = true;
        sem_Teams[team]->postValue(3);
    } 
    else if(fanCount[team] < 0 && fanCount[1 - team] < -1 ) {
        driver = true;
        sem_Teams[team]->post();
        sem_Teams[1 - team]->postValue(2);
    }
    else {
        sem_Teams[team]->wait();
    }
    cout<< "Thread ID: " << pthread_self() << ", Team: " << (team == 0 ? "A" : "B") << ", I have found a spot in a car\n";
    cout.flush();
    pthread_barrier_wait(&barrier);
    if(driver) {
        cout.flush();
        cout<< "Thread ID: " << pthread_self() << ", Team: " << (team == 0 ? "A" : "B") << ", I am the captain and driving the car : "<< carID<<"\n";
        ++carID;
        cout.flush();
        pthread_barrier_destroy(&barrier); 
        pthread_barrier_init(&barrier, NULL, MAX_WAITING); 
        sem_post(&mtx);
    }
    return 0;
}

bool isArgsCorrect(int a, int b) {
    return (((a + b) % 4 == 0) && (a % 2 == 0));
}

int getRandomInt(int min, int max) {
    return min + rand() % (max - min + 1);
}