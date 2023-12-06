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

#define READ_END 0                             // INPUT_END means where the pipe takes input
#define WRITE_END 1 

using namespace std;

mutex mtx;

struct boolGroup {
    bool isBackground;
    bool isRedirectedInp;
    bool isRedirectedOut;

    boolGroup(bool a, bool b, bool c) : isBackground(a), isRedirectedInp(b), isRedirectedOut(c) {}
};

void waitForBackgroundProcesses(const vector<pid_t>& bgProcesses) {
    for (pid_t pid : bgProcesses) {
        waitpid(pid, nullptr, 0);
    }
}

void parseAndOutputFile(const std::vector<vector<string>>& argsmtx, const string& fileName, vector<boolGroup> & bools) {
    std::ofstream file(fileName, std::ios::app);

    if (!file) {
        std::cerr << "Error opening file: " << fileName << std::endl;
        return;
    }

    for(size_t i = 0; i < argsmtx.size(); i++){
        bools.push_back(boolGroup(false,false, false));
        file << "------------\n";
        file << "Command: " << argsmtx[i][0] << "\n";
        string input = "";
        string options = "";
        string redirection = "-";
        string background = "n";

        // Start from 1 since 0 is the command
        for(size_t j = 1; j < argsmtx[i].size(); ++j) {
            if(argsmtx[i][j] == "&") {
                background = "y";
                bools.back().isBackground = true;
            } else if(argsmtx[i][j][0] == '-' && options == "") {
                options = argsmtx[i][j];
            } else if((argsmtx[i][j] == ">" || argsmtx[i][j] == "<") && redirection == "-") {
                if(argsmtx[i][j] == ">") {
                    bools.back().isRedirectedOut = true;
                }
                else {
                    bools.back().isRedirectedInp = true;
                }
                redirection = argsmtx[i][j];
                ++j; // Skip the next element as it's part of redirection
                
            } else if(input == "" && argsmtx[i][j][0] != '-') {
                input = argsmtx[i][j];
            }
        }
        file << "Inputs: " << input << "\n";
        file << "Options: " << options << "\n";
        file << "Redirection: " << redirection << "\n";
        file << "Background Job: " << background << "\n";
        file << "------------\n";
        //fflush(&file);
    }

    file.close();
}

void readIntoVector(ifstream& file ,vector<vector<string>> & argsmtx) {
    string str;
 
    while(getline(file, str)) {
        argsmtx.push_back(vector<string>());
        stringstream s(str);
        string word;
        while(s >> word){
            argsmtx.back().push_back(word);
        }
    }
}

void* outputThread(void* arg) {
    int pipe_read_end = *(int*)arg;
    std::lock_guard<std::mutex> lock(mtx);
    //thread::id threadId = this_thread::get_id();
    cout << "---- " << pthread_self() << endl;
    
     // Use fdopen to associate a file stream with the file descriptor
    FILE *stream = fdopen(pipe_read_end, "r");
    if (stream == NULL) {
        perror("fdopen");
        close(pipe_read_end);
        return NULL;
    }
    char buffer[256];
    // Use fgets to read from the stream
    while (fgets(buffer, sizeof(buffer), stream) != NULL) {
        cout << buffer;
        cout.flush(); // Explicitly flush the stream
    }
    fclose(stream); // Close the read end of the pipe
    cout << "---- " << pthread_self() << endl;
    cout.flush(); // Explicitly flush the stream
    return NULL;
}

int main() {
    
    ifstream file("command.txt");
    if(!file.is_open()) {
        cout<< "Error retrieving file"<<endl;
        return 1;
    }
    vector<vector<string> > argsmtx;
    vector<boolGroup> bools;
      //Read the file and save into a vector
    readIntoVector(file, argsmtx);
    file.close();
    parseAndOutputFile(argsmtx, "parse.txt", bools);

    // Fork for every line
    vector<pthread_t> threads;
    vector<pid_t> childPIDs(argsmtx.size());
    vector<pid_t> bgPIDs;
    vector<vector<int>> pipes(argsmtx.size(),vector<int>(2));
    for(int i = 0; i < argsmtx.size(); i++ ){
        //if wait statement 
        int p[2];
        
        if(!bools[i].isRedirectedOut) {
            if(pipe(p) < 0)
                exit(1);
            pipes[i][0] = p[0];
            pipes[i] [1] = p[1];
        }
        if(argsmtx[i][0] =="wait"){
            close(p[WRITE_END]);
            close(p[READ_END]);
            waitForBackgroundProcesses(bgPIDs);
            
            for (auto &t : threads) { // Assuming backgroundThreads holds the threads for background processes
                void* ret;
                pthread_join(t, &ret);
            }
            bgPIDs.clear();
            threads.clear(); // Clear the list of background threads
        }
        else if ((childPIDs[i] = fork()) == 0) {
            // Convert each std::string to char*
            vector<string> &args = argsmtx[i];
            vector<char*> c_args;
            for (auto &arg : args) 
                c_args.push_back(const_cast<char*>(arg.c_str()));             
            
            if (bools[i].isBackground) 
                c_args.pop_back(); // Remove the '&' symbol

            if(bools[i].isRedirectedInp) {
                close(STDIN_FILENO);
                int fd = open(c_args.back(), O_RDONLY);
                c_args.pop_back();
                c_args.pop_back();
            }
            else if(bools[i].isRedirectedOut){
                //cout<<"true;\n";
                close(p[READ_END]);
                close(p[WRITE_END]);
                close(STDOUT_FILENO);
                int fd = open(c_args.back(), O_CREAT | O_APPEND | O_WRONLY,  0644);
                c_args.pop_back();
                c_args.pop_back();
            }
            if(!bools[i].isRedirectedOut) {
                close(p[READ_END]);
                dup2(p[WRITE_END], STDOUT_FILENO);
                close(p[WRITE_END]);

            }
            
            c_args.push_back(nullptr);   
            const char* prog_name = c_args[0]; // The program name
            execvp(prog_name, c_args.data());
            exit(0); // Child process exits
        }
        else{
            //waitpid(childPIDs[i], NULL, 0);
            close(p[WRITE_END]);
            
            if(bools[i].isRedirectedOut || !bools[i].isBackground ){
                waitpid(childPIDs[i], NULL, 0);
            }
            else{
                bgPIDs.push_back(childPIDs[i]);
            }
            if(!bools[i].isRedirectedOut) {
                //close(p[READ_END]);
                pthread_t pThread;
                pthread_create(&pThread, NULL, outputThread, &pipes[i][READ_END]);
                threads.push_back(pThread);
            }
            else {
                close(p[READ_END]);
            }
        }
    }
    if(bgPIDs.size() != 0) {
        waitForBackgroundProcesses(bgPIDs);
        //std::cout << "yes I waited\n" ;
    }
    // Join all threads
    for (auto &t : threads) {
        void* ret;
        pthread_join(t, &ret);
    }
    return 0;
}


