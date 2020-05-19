// 가독성 향상을 위해 define
#define throw(a) (return a())
#define throw(a, err) (return a(err))
#define terminate (exit(1))
#define new
// 제곱근 사용을 위해 include
#include <math.h>
// multi threading
#include <pthread.h>
// 세마포어 header file
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// struct sigaction 사용을 위해 선언
#define _XOPEN_SOURCE

struct _Graph;
typedef struct _Graph Graph;
struct _Node;
typedef struct _Node Node;

struct _Graph {
    // Node의 배열처럼 쓰일 포인터
    Node *head;
}

struct _Node {
    int dest;
    Node *next;
}

// 스레드 변수 전역변수로 선언
pthread_t _producer,
    *_consumers;
// 소비자 스레드의 개수를 담을 정수형 변수 선언
int consumersCount;
// 스레드에서 전역변수 쓰기 관리를 위한 세마포어 선언
sem_t *semaphore;

FILE *openFile(void);
void getUserInput(void);
void checkArgcCorrentness(int);
void fileNotFoundError(void);
void noCommandLineArgumentError(void);
void noInitialNumberOfThreadsError(void);
void tooManyCommandLineArgumentsError(void);
void memoryAllocationError(void);
void threadCreateError(void);
void outOfBoundError(void);
void initConsumersPointer(void);
void createThreads(void);
void reallocConsumerThreads(int);
int isStat(const char *);
int isThreads(const char *);
int isNum(const char *);
int getNum(const char *);
int findFileLength(FILE *);
void *producer(void *);
void *consumer(void *);
void handleSigaction(struct sigaction *);
void showResult(void);
void onDisconnect(int);

FILE *openFile(const char *filename) {
    FILE *res = fopen(filename, "r");
    if (res == NULL) {
        throw(new fileNotFoundError);
    }
    return res;
}
void getUserInput(void) {
    char buffer[101];
    while (1) {
        gets(buffer);
        if (isStat(buffer)) {

        } else if (isThreads(buffer)) {

        } else if (isNum(buffer)) {
            int num = getNum(buffer);
        } else {
            printf("Wrong keyword\n");
        }
    }
}
void checkArgcCorrentness(int argc) {
    if (argc == 1) {
        // return type이 void인 함수 호출
        // 가독성을 위해서 throw 문을 붙임
        throw(new noCommandLineArgumentError);
    } else if (argc == 2) {
        throw(new noInitialNumberOfThreadsError);
    } else if (argc > 3) {
        throw(new toomanyCommandLineArguments);
    } else {
        return;
    }
}
void fileNotFoundError(void) {
    fprintf(stderr, "Error: File not found\n");
    terminate;
}
void noCommandLineArgumentError(void) {
    fprintf(stderr, "Error: No command line arguments\n");
    terminate;
}
void noInitialNumberOfThreadsError(void) {
    fprintf(stderr, "Error: Initial number of consumer threads not given\n");
    terminate;
}
void tooManyCommandLineArgumentsError(void) {
    fprintf(stderr, "Error: Too many command line arguments\n");
    terminate;
}
void memoryAllocationError(void) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    terminate;
}
void outOfBoundError(void) {
    fprintf(stderr, "Error: Array index out of bound\n");
    terminate;
}
void threadCreateError(int errorCode) {
    fprintf(stderr,
            "Error: Unable to create thread"
            ", Error Code %d\n",
            errorCode);
    terminate;
}
void initConsumersPointer(pthread_t *_consumers) {
    _consumers = (pthread_t *)malloc(sizeof(pthread_t) * consumersCount);
}
void createThreads() {
    int err;
    err = pthread_create(&_producer, NULL, producer, NULL);
    if (err) {
        throw(new threadCreateError, err);
    }

    for (int i = 0; i < consumersCount; i++) {
        err = pthread_create(&_consumers[i], NULL, consumer, (void *)&i);
        if (err) {
            throw(new threadCreateError, err);
        }
    }
}
void reallocConsumerThreads(int nextCount) {
    int currentCount = consumersCount;
    if (currentCount > nextCount) {
        for (int i = currentCount - 1; i >= nextCount; i--) {
            pthread_cancel(_consumers[i]);
        }
    }
    _consumers = realloc((void *)_consumers, nextCount);

    if (_consumers == NULL) {
        throw(new memoryAllocationError);
    }

    if (currentCount < nextCount) {
        for (int i = currentCount; i < nextCount; i++) {
            err = pthread_create(&_consumers[i], NULL, consumer, (void *)&i);
            if (err) {
                throw(new threadCreateError, err);
            }
        }
    }
}
void closeThreads() {
    pthread_cancel(*_producer);
    for (int i = 0; i < consumersCount; i++) {
        pthread_cancel(_consumers[i]);
    }
    free(_consumers);
}

int isStat(const char *buffer) {
    return strncmp(buffer, "stat", strlen(stat)) == 0;
}
int isThreads(const char *buffer) {
    return strncmp(buffer, "threads", strlen(stat) == 0;
}
int isNum(const char *buffer) {
    return strncmp(buffer, "num", strlen(stat) == 0;
}
int getNum(const char *buffer) {
    char *tempPtr = strtok(buffer, " ");
    tempPtr = strtok(NULL, " ");
    return atoi(tempPtr);
}
int findFileLength(FILE *fp) {
    int count = 0;
    // EOF가 나올때까지 count 증가
    while (fscanf(fp, "%*d") == 1) {
        count++;
    }
    // 제곱근을 취한 후 버림한다
    int n = (int)sqrt(count);
    // 다음 사용을 위해 파일 포인터를 초기 위치로 돌려놓는다
    rewind(fp);
    return n + 1;
}
void *producer(void *ptr) {
    ;
    return NULL;
}
void *consumer(void *numPtr) {
    int number = *(int *)numPtr;
    return NULL;
}
void handleSigaction(struct sigaction *actionPtr) {
    memset(&actionPtr, 0, sizeof(actionPtr));
    *actionPtr.sa_handler = onDisconnect;
    sigaction(SIGINT, &actionPtr, NULL);
    sigaction(SIGQUIT, &actionPtr, NULL);
    sigaction(SIGTERM, &actionPtr, NULL);
    sigaction(SIGQUIT, &actionPtr, NULL);
}
void showResult(void) {}
void onDisconnect(int sig) {
    showResult();
    closeThreads(&_producer, _consumers, consumersCount);
    terminate;
}

int main(int argc, char **argv) {
    // ctrl-c 핸들러
    struct sigaction action;
    handleSigaction(&action);

    checkArgcCorrentness(argc);

    // 파일이름 문자열 선언
    char *filename = argv[1];
    consumersCount = atoi(argv[2]);

    FILE *fp = openFile(filename);
    int fileLength = findFileLength(fp);

    printf("%d\n", filelength);

    initConsumersPointer(_consumers);
    createThreads(&_producer, _consumers);

    return 0;
}