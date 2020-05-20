// 제곱근 사용을 위해 include
#include <math.h>
// multi threading
#include <pthread.h>
// 세마포어 header file
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
// struct sigaction 사용을 위해 선언
#define _XOPEN_SOURCE
#define BUFFER_SIZE 50

typedef struct _Element {
    int currentIndex;
    uint64_t visited;
} Element;

// 세마포어를 할당받을 키값 설정
const char *WRITESEMAPHORE_KEY = "/WRITESEMAPHORE";
const char *GETSEMAPHORE_KEY = "/GETSEMAPHORE";
// 스레드 변수 전역변수로 선언
pthread_t _producer, *_consumers;
// 소비자 스레드의 개수를 담을 정수형 변수 선언
int consumersLength;
// 소비자 스레드의 주어지는 id 역할을 할 정수형 변수 선언
int consumersCount;
// 스레드에서 전역변수 쓰기 관리를 위한 세마포어 선언
sem_t *writeSemaphore;
// 스레드에서 전역변수 읽기 관리를 위한 세마포어 선언
sem_t *getSemaphore;
Element buffer[BUFFER_SIZE];
int tid[8];
int searchCountSum;
int searchCount[8];
int record[8][50];
int recordOfProducer[11];
int **cache;
int map[50][50];
int isdisconnected[9];

// 유한 버퍼의 생산자의 현재 인덱스의 div값 변수 선언
int bufferIndexDivOfProducer;
// 유한 버퍼의 생산자의 현재 인덱스의 mod값 변수 선언
int bufferIndexModOfProducer;
// 유한 버퍼의 소비자의 현재 인덱스의 mod값 변수 선언
int bufferIndexDivOfConsumer;
// 유한 버퍼의 소비자의 현재 인덱스의 mod값 변수 선언
int bufferIndexModOfConsumer;
// 거리의 최소값 변수 선언
int bestResult = INT16_MAX;
int fastestWay[50];
// 파일의 행 갯수 선언
int fileLength;

int isEveryThreadDisconnected(void);
void freeMemories(void);
int min(int a, int b);
FILE *openFile(const char *);
int getConsumerNumber(void);
int TSP_consumer(int, int, int, uint64_t);
int TSP_producer(int, uint64_t, int);
void initMap(FILE *, int);
void initCache(void);
void getUserInput(void);
void checkArgcCorrentness(int);
void fileNotFoundError(void);
void noCommandLineArgumentError(void);
void noInitialNumberOfThreadsError(void);
void tooManyCommandLineArgumentsError(void);
void memoryAllocationError(void);
void threadCreateError(int);
void semaphoreCreateError(void);
void outOfBoundError(void);
void invalidThreadNumberError(void);
void noContextsinFileError(void);
void fscanfError(void);
void initConsumersPointer(void);
void createThreads(void);
void reallocConsumerThreads(int);
int isStat(const char *);
int isThreads(const char *);
int isNum(const char *);
int getNum(const char *);
int findFileLength(FILE *);
void closeThreads(void);
void closeSemaphore(void);
void *producer(void *);
void *consumer(void *);
void handleSigaction(struct sigaction *);
void showResult(void);
void showThread(void);
void showStat(void);
void onDisconnect(int);

int isEveryThreadDisconnected(void) {
    for (int i = 0; i < consumersLength + 1; i++) {
        if (isdisconnected[i] == 0) {
            return 0;
        }
    }
    return 1;
}
void freeMemories(void) {
    for (int i = 0; i < fileLength; i++) {
        free(cache[i]);
    }
    free(cache);
}
int min(int a, int b) { return a < b ? a : b; }
int TSP_consumer(int threadNumber, int count, int currentIndex,
                 uint64_t visited) {
    record[threadNumber][count + 11] = currentIndex;
    if (visited == (1 << fileLength) - 1) {
        searchCount[threadNumber]++;
        sem_wait(writeSemaphore);
        searchCountSum++;
        sem_post(writeSemaphore);
        return map[currentIndex][0];
    }
    int *ptr = &cache[currentIndex][visited];
    if (*ptr) {
        return *ptr;
    }
    *ptr = INT16_MAX;
    for (int next = 0; next < fileLength; next++) {
        if (visited & (1 << next))
            continue;
        if (map[currentIndex][next] == 0)
            continue;
        *ptr = min(*ptr, TSP_consumer(threadNumber, count + 1, next,
                                      visited | (1 << next)) +
                             map[currentIndex][next]);
    }
    return *ptr;
}
int TSP_producer(int currentIndex, uint64_t visited, int count) {
    recordOfProducer[count] = currentIndex;
    if (count == 10) {
        sem_wait(writeSemaphore);
        buffer[bufferIndexModOfProducer].currentIndex = currentIndex;
        buffer[bufferIndexModOfProducer].visited = visited;
        bufferIndexModOfProducer++;
        bufferIndexDivOfProducer += bufferIndexModOfProducer / BUFFER_SIZE;
        bufferIndexModOfProducer %= BUFFER_SIZE;
        sem_post(writeSemaphore);
        return 0;
    }
    if (visited == (1 << fileLength) - 1) {
        return map[currentIndex][0];
    }
    int *ptr = &cache[currentIndex][visited];
    if (*ptr) {
        return *ptr;
    }
    *ptr = INT16_MAX;
    for (int next = 0; next < fileLength; next++) {
        if (visited & (1 << next))
            continue;
        if (map[currentIndex][next] == 0)
            continue;
        *ptr = min(*ptr, TSP_producer(next, visited | (1 << next), count + 1) +
                             map[currentIndex][next]);
    }
    return *ptr;
}

void initCache(void) {
    cache = (int **)calloc(fileLength, sizeof(int *));
    if (cache == NULL) {
        return memoryAllocationError();
    }
    for (int i = 0; i < fileLength; i++) {
        cache[i] = (int *)calloc(1 << fileLength, sizeof(int));
        if (cache[i] == NULL) {
            return memoryAllocationError();
        }
    }
}

int getConsumerNumber(void) { return consumersCount++; }

FILE *openFile(const char *filename) {
    FILE *res = fopen(filename, "r");
    if (res == NULL) {
        fileNotFoundError();
    }
    return res;
}
void initMap(FILE *fp, int fileLength) {
    int resCode;
    for (int i = 0; i < fileLength; i++) {
        for (int j = 0; j < fileLength; j++) {
            if (i == j) {
                continue;
            }
            resCode = fscanf(fp, "%d", &map[i][j]);
            if (resCode != 1) {
                fscanfError();
            }
        }
    }
    return;
}

void getUserInput(void) {
    char buffer[101];
    while (1) {
        fgets(buffer, 101, stdin);
        if (isStat(buffer)) {
            showStat();
        } else if (isThreads(buffer)) {
            showThread();
            showStat();
        } else if (isNum(buffer)) {
            int num = getNum(buffer);
            if (1 < num || num > 8) {
                return invalidThreadNumberError();
            }

            reallocConsumerThreads(num);
        } else {
            printf("Wrong keyword\n");
        }
    }
}
void checkArgcCorrentness(int argc) {
    if (argc == 1) {
        // return type이 void인 함수 호출
        // 가독성을 위해서 throw 문을 붙임
        return noCommandLineArgumentError();
    } else if (argc == 2) {
        return noInitialNumberOfThreadsError();
    } else if (argc > 3) {
        return tooManyCommandLineArgumentsError();
    } else {
        return;
    }
}
void fileNotFoundError(void) {
    fprintf(stderr, "Error: File not found\n");
    exit(1);
}
void noCommandLineArgumentError(void) {
    fprintf(stderr, "Error: No command line arguments\n");
    exit(1);
}
void noInitialNumberOfThreadsError(void) {
    fprintf(stderr, "Error: Initial number of consumer threads not given\n");
    exit(1);
}
void tooManyCommandLineArgumentsError(void) {
    fprintf(stderr, "Error: Too many command line arguments\n");
    exit(1);
}
void memoryAllocationError(void) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    exit(1);
}
void outOfBoundError(void) {
    fprintf(stderr, "Error: Array index out of bound\n");
    exit(1);
}
void invalidThreadNumberError(void) {
    fprintf(stderr, "Error: Invalid thread number\n");
    exit(1);
}
void noContextsinFileError(void) {
    fprintf(stderr, "Error: No contexts in text file\n");
    exit(1);
}
void fscanfError(void) {
    fprintf(stderr, "Error: fscanf failed\n");
    exit(1);
}
void threadCreateError(int errorCode) {
    fprintf(stderr,
            "Error: Unable to create thread"
            ", Error Code %d\n",
            errorCode);
    exit(1);
}
void semaphoreCreateError(void) {
    fprintf(stderr, "Error: Unable to create semaphore\n");
    exit(1);
}
void initConsumersPointer() {
    _consumers = (pthread_t *)calloc(consumersLength, sizeof(pthread_t));
}
void createThreads() {
    int err;
    err = pthread_create(&_producer, NULL, producer, NULL);
    if (err) {
        return threadCreateError(err);
    }

    for (int i = 0; i < consumersLength; i++) {
        err = pthread_create(&_consumers[i], NULL, consumer, NULL);
        if (err) {
            return threadCreateError(err);
        }
    }
}
void reallocConsumerThreads(int nextCount) {
    int currentCount = consumersLength;
    int err;
    if (currentCount > nextCount) {
        for (int i = currentCount - 1; i >= nextCount; i--) {
            pthread_cancel(_consumers[i]);
            consumersLength--;
        }
    }
    _consumers = realloc((void *)_consumers, nextCount);

    if (_consumers == NULL) {
        return memoryAllocationError();
    }

    if (currentCount < nextCount) {
        for (int i = currentCount; i < nextCount; i++) {
            err = pthread_create(&_consumers[i], NULL, consumer, (void *)&i);
            if (err) {
                return threadCreateError(err);
            }
        }
    }
}
void closeThreads(void) {
    pthread_cancel(_producer);
    for (int i = 0; i < consumersLength; i++) {
        pthread_cancel(_consumers[i]);
    }
    free(_consumers);
}
void closeSemaphore(void) {
    sem_unlink(GETSEMAPHORE_KEY);
    sem_unlink(WRITESEMAPHORE_KEY);
}

int isStat(const char *buffer) {
    return strncmp(buffer, "stat", strlen("stat")) == 0;
}
int isThreads(const char *buffer) {
    return strncmp(buffer, "threads", strlen("threads")) == 0;
}
int isNum(const char *buffer) {
    return strncmp(buffer, "num", strlen("num")) == 0;
}
int getNum(const char *buffer) {
    int result;
    char *tempPtr = (char *)calloc(strlen(buffer), sizeof(char));
    strcpy(tempPtr, buffer);
    tempPtr = strtok(tempPtr, " ");
    tempPtr = strtok(NULL, " ");
    result = atoi(tempPtr);
    free(tempPtr);
    return result;
}
int findFileLength(FILE *fp) {
    int count = 0, temp;
    // EOF가 나올때까지 count 증가
    while (fscanf(fp, "%d", &temp) == 1) {
        count++;
    }
    // 내용이 없으면
    if (count == 0) {
        noContextsinFileError();
    }
    // 제곱근을 취한 후 버림한다
    int n = (int)sqrt(count);
    // 다음 사용을 위해 파일 포인터를 초기 위치로 돌려놓는다
    rewind(fp);
    return n + 1;
}
void *producer(void *ptr) {
    for (int i = 0; i < fileLength; i++) {
        TSP_producer(i, (uint64_t)1 << i, 0);
        sem_post(getSemaphore);
    }
    isdisconnected[0] = 1;
    return NULL;
}
void *consumer(void *ptr) {
    int consumerNumber = getConsumerNumber();
    tid[consumerNumber] = gettid();
    while (searchCountSum < fileLength) {
        sem_wait(getSemaphore);
        Element *elem_ptr = &buffer[bufferIndexModOfConsumer];
        int *currentRecord = record[consumerNumber];
        for (int i = 0; i < 11; i++) {
            currentRecord[i] = recordOfProducer[i];
        }
        sem_wait(writeSemaphore);
        bufferIndexModOfConsumer++;
        bufferIndexDivOfConsumer += bufferIndexModOfConsumer / BUFFER_SIZE;
        bufferIndexModOfConsumer %= BUFFER_SIZE;
        sem_post(writeSemaphore);
        int resultOfConsumerTSP = TSP_consumer(
            consumerNumber, 0, elem_ptr->currentIndex, elem_ptr->visited);
        int firstVisited = currentRecord[0];
        int secondVisited = currentRecord[1];
        int result = resultOfConsumerTSP +
                     cache[firstVisited][(uint64_t)(1 << firstVisited) +
                                         (uint64_t)(1 << secondVisited)];
        printf("%d\n", result);
        if (resultOfConsumerTSP < bestResult) {
            bestResult = result;
            for (int i = 0; i < fileLength; i++) {
                fastestWay[i] = currentRecord[i];
            }
        }
    }
    isdisconnected[consumerNumber + 1] = 1;
    if (isEveryThreadDisconnected()) {
        onDisconnect(0);
    }

    return NULL;
}
void handleSigaction(struct sigaction *actionPtr) {
    memset(actionPtr, 0, sizeof(*actionPtr));
    actionPtr->sa_handler = onDisconnect;
    sigaction(SIGINT, actionPtr, NULL);
    sigaction(SIGQUIT, actionPtr, NULL);
    sigaction(SIGTERM, actionPtr, NULL);
    sigaction(SIGQUIT, actionPtr, NULL);
}
void showResult(void) {
    showStat();
    showThread();
}
void showThread(void) {
    for (int i = 0; i < consumersLength; i++) {
        printf("Thread Number : %d\n  TID: %d\n   searched routes of subtask: "
               "%d\n",
               i + 1, tid[i], searchCount[i]);
    }
}
void showStat(void) {
    int sum = 0;
    for (int i = 0; i < consumersLength; i++) {
        sum += searchCount[i];
    }
    printf("\nSum of searched routes: %d\n", sum);
    if (bestResult == INT16_MAX) {
        return;
    }
    printf("Current Lowest Sum of Weights: %d\n", bestResult);
    printf("Way: ");
    for (int i = 0; i < fileLength; i++) {
        printf("%d->", fastestWay[fileLength]);
    }
    // 마지막 화살표 지우기
    printf("\b\b");
    printf("  \n");
}
void initSemaphore(void) {
    getSemaphore = sem_open(GETSEMAPHORE_KEY, O_CREAT, 0700, 0);
    writeSemaphore = sem_open(WRITESEMAPHORE_KEY, O_CREAT, 0700, 1);
    // 세마포어가 제대로 할당되지 못했을 시
    if (getSemaphore <= 0 || writeSemaphore <= 0) {
        return semaphoreCreateError();
    }
}
void onDisconnect(int sig) {
    showResult();
    freeMemories();
    closeThreads();
    closeSemaphore();
    exit(1);
}

int main(int argc, char **argv) {
    printf("TSP Program\n");
    // ctrl-c 핸들러
    struct sigaction action;
    handleSigaction(&action);

    checkArgcCorrentness(argc);

    // 파일이름 문자열 선언
    char *filename = argv[1];
    consumersLength = atoi(argv[2]);
    if (1 > consumersLength || consumersLength > 8) {
        invalidThreadNumberError();
    }

    FILE *fp = openFile(filename);
    fileLength = findFileLength(fp);

    printf("%d\n", fileLength);
    initMap(fp, fileLength);
    initCache();

    initSemaphore();

    initConsumersPointer();
    createThreads();

    getUserInput();

    return 0;
}