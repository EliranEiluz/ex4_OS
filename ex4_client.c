#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <linux/random.h>
#define _GNU_SOURCE
#define TO_SRV "to_srv"



int fromStringsToNumbers(char** buffer, int* toSrvNumbers, int size) {
    long temp;
    for(int i = 0; i < size; i++) {;
        temp = strtol(buffer[i], NULL, 10);
        if(errno == EINVAL || temp > INT_MAX || temp < INT_MIN) {
            return -1;
        }
        toSrvNumbers[i] = temp;
    }
    return 0;
}

void sigUsr2Handle(int sigNum) {
    printf("ERROR_FROM_EX4\n");
    exit(-1);
}

void sigUsr1Handle(int sigNum) {
    alarm(0);
    char toClientFile[100] = "to_client_";
    int fd;
    char myPid[10];
    char buffer[100];
    sprintf(myPid, "%d", getpid());
    strcat(toClientFile, myPid);
    if((fd = open(toClientFile, R_OK)) == -1) {
        sigUsr2Handle(SIGUSR2);
    }
    if(read(fd, buffer, 100) == -1) {
        close(fd);
        remove(toClientFile);
        sigUsr2Handle(SIGUSR2);
    }

    if(close(fd) == -1 || remove(toClientFile) == -1) {
        sigUsr2Handle(SIGUSR2);
    }
    printf("%s", buffer);
}

void alarmHandle(int sigNum) {
    printf("Client closed because no response was received from the server for 30 seconds\n");
    if(access(TO_SRV, F_OK) != -1) {
        remove(TO_SRV);
    }
    exit(-1);
}

void initSignals() {
    struct sigaction sig;
    sigset_t mask;
    sigfillset(&mask);
    sig.sa_handler = sigUsr2Handle;
    sig.sa_flags = 0;
    sig.sa_mask = mask;
    sigaction(SIGUSR2, &sig, NULL);
    sig.sa_handler = sigUsr1Handle;
    sigaction(SIGUSR1, &sig, NULL);
    sig.sa_handler = alarmHandle;
    sigaction(SIGALRM, &sig, NULL);
}

int openToSrv() {
    int counter = 0;
    int fd = -1;
    int randBuffer[10];
    if(syscall(SYS_getrandom, randBuffer, 10*sizeof(int), 0) == -1) {
        raise(SIGUSR2);
    }
    while(1) {
        if(counter == 10) {
            raise(SIGUSR2);
        }
        fd = open(TO_SRV, W_OK | O_CREAT |O_EXCL, 0777);
        if(fd < 0) {
            sleep(randBuffer[counter] % 6);
            counter++;
        }
        else {
            break;
        }
    }
    return fd;
}

void writeToSrv(char** buf, int size, int fd, char* myPid) {
    char toWrite[100];
    strcpy(toWrite, myPid);
    for(int i = 0; i < size; i++) {
        strcat(toWrite, " ");
        strcat(toWrite, buf[i]);
    }
    if(write(fd, toWrite, 100) == -1) {
        raise(SIGUSR2);
    }
    close(fd);
}

int main(int argc, char** argv) {
    initSignals();
    if(argc != 5) {
        raise(SIGUSR2);
    }
    int toSrvParams[4];
    if(fromStringsToNumbers(argv + 1, toSrvParams, 4) == -1) {
        raise(SIGUSR2);
    }
    int fd = openToSrv();
    char myPid[10];
    sprintf(myPid, "%d", getpid());
    writeToSrv(argv + 2, 3, fd, myPid);
    kill(toSrvParams[0], SIGUSR1);
    alarm(30);
    pause();
}