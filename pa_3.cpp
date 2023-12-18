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


using namespace std;
bool isArgsCorrect(int, int);
void* fanThread( void* args);
int getRandomInt(int, int);
const int TEAM_A = 0;
const int TEAM_B = 1;
const int MAX_WAITING = 4;
int fanCount[2];
sem_t mtx;
sem_t sem_Teams[2];
pthread_barrier_t barrier;


int main(int argc, char * argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <numFansA> <numFansB>" << std::endl;
        return 1;
    }
    srand(time(0));
    sem_init(&mtx, 0, 1);
    sem_init(&sem_Teams[0], 0, 0);
    sem_init(&sem_Teams[1], 0, 0);
    pthread_barrier_init(&barrier, NULL, MAX_WAITING);
    int numFansA = atoi(argv[1]); 
    int numFansB = atoi(argv[2]); 
    vector<pthread_t> teamAThreads;
    vector<pthread_t> teamBThreads;
    
    if(!isArgsCorrect(numFansA, numFansB)) {
        //cout << (numFansA % 2) <<  "  " << (numFansA + numFansB) % 4 << "\n";
        cout <<  "Main Terminates\n";

        return 0;
    }
    cout<<"I run";
    int tmpA = numFansA;
    int tmpB = numFansB;
    // For total  number of fans pick random fan and create a thread for it
    for (int i = 0; i < numFansA + numFansB ; i++) {
        int stride = getRandomInt(0, tmpA + tmpB);
        if(stride < tmpA){
            pthread_t pThread;

            pthread_create(&pThread,NULL, fanThread, (void*)&TEAM_A );
            teamAThreads.push_back(pThread);
            --tmpA;
        }
        else {
            pthread_t pThread;
            pthread_create(&pThread,NULL, fanThread, (void*)&TEAM_B );
            teamBThreads.push_back(pThread);
            --tmpB;
        } 
    }

    for(auto thread: teamAThreads) {
        pthread_join(thread, NULL);
    }
    for(auto thread: teamBThreads){
        pthread_join(thread, NULL);
    }
    pthread_barrier_destroy(&barrier);
    cout <<  "Main Terminates\n";
}



void* fanThread(void* args){
    bool driver = false;
    int team = *((int*)args);
    sem_wait(&mtx);
    fanCount[team] = fanCount[team] + 1;
    //if((fanCount[0] >= 2  && fanCount[1] >= 2 ) || fanCount[0] >= 4 || fanCount[1] >= 4)
//    while( fanCount[0] + fanCount[1] < 4 )
//printf("Thread ID: %ld, Team: %c, I am looking for a car\n", pthread_self(), args->team);

    sem_getvalue(&sem_Teams[team], &fanCount[team]);
    sem_getvalue(&sem_Teams[1 - team], &fanCount[1 - team]);
    if(fanCount[team] < -2){
        driver = true;
        sem_post(&sem_Teams[team]);
        sem_post(&sem_Teams[team]);
        sem_post(&sem_Teams[team]);
    } 
    else if(fanCount[team] < 0 && fanCount[1 - team] < -1 ) {
        driver = true;
        sem_post(&sem_Teams[team]);
        sem_post(&sem_Teams[1 - team]);
        sem_post(&sem_Teams[team]);
    }
    else {
        sem_post(&mtx);
        sem_wait(&sem_Teams[team]);
        //sem_wait(&mutex)
    }
    //printf("Thread ID: %ld, Team: %c, I have found a spot in a car\n", pthread_self(), args->team);
    pthread_barrier_wait(&barrier);
    if(driver) {
      //  printf("Thread ID: %ld, Team: %c, I am the captain and driving the car\n", pthread_self(), args->team);
        pthread_barrier_destroy(&barrier); 
        pthread_barrier_init(&barrier, NULL, MAX_WAITING); 
        sem_post(&mtx);
    }
    else {

    }
    cout << "I'm A fan" << endl; 
}

bool isArgsCorrect(int a, int b) {
    return (((a + b) % 4 == 0) && (a % 2 == 0));
}

int getRandomInt(int min, int max) {
    return min + rand() % (max - min + 1);
}

