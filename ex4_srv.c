#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>

#define SECONDS_UNTIL_TIMEOUT 60
#define SIZE 100
#define TO_SRV "to_srv"


/*
 * This function is used to convert a given string to a given size of integers seperated by space.
 * In case all the conversions are valid, the function returns 0.
 * In case of any failure in one of the conversions, the function returns -1.
 * The conversion is done with strtol function.
 * A conversion is considered failure if the number is bigger than int max, smaller than int min
 * or can't be converted to a number.
 */
int fromBufferToNumbers(char* buffer, int* toSrvNumbers, int size) {
    char *token = strtok(buffer, " ");
    long temp =  strtol(token, NULL, 10);
    if(errno == EINVAL || temp > INT_MAX || temp < INT_MIN) {
        return -1;
    }
    int i;
    toSrvNumbers[0] = temp;
    for(i = 1; i < size; i++) {
        token = strtok(NULL, " ");
        temp = strtol(token, NULL, 10);
        if(errno == EINVAL || temp > INT_MAX || temp < INT_MIN) {
            return -1;
        }
        toSrvNumbers[i] = temp;
    }
    return 0;
}

/*
 * This function is used to calculate a binary operation between two numbers.
 * when operation is 1: +
 * when operation is 2: -
 * when operation is 3: *
 * when operation is 4: /
 * The result of the operation will be written to result. If the operation is / and secondNum is zero,
 * or operation is not between 1 and 4, NULL will be returned.
 */
int* calculate(int firstNum, int operation, int secondNum, int* result) {
    switch(operation) {
        case 1:
            *result = firstNum + secondNum;
            break;
        case 2:
            *result = firstNum - secondNum;
            break;
        case 3:
            *result = firstNum * secondNum;
            break;
        case 4:
            if(secondNum == 0) {
                return NULL;
            }
            *result = firstNum / secondNum;
            break;
        default:
            return NULL;
    }
    return result;
}

void handleClient() {

    // array to save the given number from the client file.
    int toSrvNumbers[4];

    // char buffer to save the result from calculation as string.
    char resultBuffer[10];

    // char buffer to save the client's PID as string.
    char clientPidToString[10];

    // integer to save the result of the operation.
    int result;

    // char buffer to save the content of "to_srv" file.
    char buffer[100];

    // char buffer to save the name of the file where the result of the operation will be written to.
    char fileName[100] = "to_client_";

    // open "to_srv" file. if fails, raise SIGUSR2 in parent and exit.
    int fd = open(TO_SRV, R_OK, 0777);
    if(fd < 0) {
        kill(SIGUSR2, getppid());
        exit(-1);
    }

    // read the input from to_srv file, check it's validity, close to_srv file and remove it.
    // if one of these operation above fails, raise SIGUSR2 in parent and exit.
    if(read(fd, buffer, SIZE) < 0 || fromBufferToNumbers(buffer, toSrvNumbers, 4) == -1 ||
            close(fd) == -1 || remove(TO_SRV) == -1) {
        kill(SIGUSR2, getppid());
        exit(-1);
    }

    // convert the client's PID to string with sprintf.
    sprintf(clientPidToString, "%d", toSrvNumbers[0]);

    //
    strcat(fileName, clientPidToString);
    int clientFile = open(fileName, W_OK | O_CREAT, 0777);
    if(clientFile < 0) {
        kill(SIGUSR2, getppid());
        exit(-1);
    }
    if(calculate(toSrvNumbers[1], toSrvNumbers[2], toSrvNumbers[3], &result) != NULL) {
        sprintf(resultBuffer, "%d", result);
        write(clientFile, resultBuffer, strlen(resultBuffer));
        write(clientFile, "\n\0", 2);
    }
    else {
        write(clientFile, "CANNOT_DIVIDE_BY_ZERO\n", strlen("CANNOT_DIVIDE_BY_ZERO\n"));
    }
    close(clientFile);
    kill(toSrvNumbers[0], SIGUSR1);
    kill(getppid(), SIGUSR2);
    exit(0);
}

/*
 * this signal will be sent if any error occurred from one on the child processes, or in this process.
 *  errors can be in fork, opening file or the input from client is not valid.
 */
void sigUsr2Handler(int sigNum, siginfo_t* info, void* v) {
    int status;
    if(info->si_pid != getpid()) {
        waitpid(info->si_pid, &status, 0);
        if(WIFEXITED(status)) {
            if(WEXITSTATUS(status) == -1) {
                printf("ERROR_FROM_EX4\n");
                exit(-1);
            }
        }
    }
}


void sigUsr1Handler(int sigNum) {
    alarm(0);
    int child = fork();
    if(child == -1) {
        raise(SIGUSR2);
    }
    if(child == 0) {
        handleClient();
    }
    else {
        alarm(SECONDS_UNTIL_TIMEOUT);
    }
}


void alarmHandler(int sigNum) {
    printf("The server was closed because no service request was received for the last 60 seconds\n");
    exit(-1);
}


int main() {
    printf("%d\n", getpid());
    struct sigaction sig;
    sigset_t mask;
    sigfillset(&mask);
    sig.sa_handler = sigUsr1Handler;
    sig.sa_flags = 0;
    sig.sa_mask = mask;
    sigaction(SIGUSR1, &sig, NULL);
    sig.sa_handler = alarmHandler;
    sigaction(SIGALRM, &sig, NULL);
    sig.sa_sigaction = sigUsr2Handler;
    sig.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR2, &sig, NULL);
    if(access("to_srv", F_OK) == 0) {
        if(remove("to_srv") == -1) {
            raise(SIGUSR2);
        }
    }
    alarm(SECONDS_UNTIL_TIMEOUT);
    while(1) {
        pause();
    }
    return 0;
}
