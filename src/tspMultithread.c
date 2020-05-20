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
#define gettid() syscall(SYS_gettid)
// struct sigaction 사용을 위해 선언
#define _XOPEN_SOURCE
#define BUFFER_SIZE 200
#define THREAD_END_CODE -5555

typedef struct _Element {
    int currentIndex;
    int sum;
    uint64_t visited;
    int *path;
} Element;

// 스레드 변수 전역변수로 선언
pthread_t _producer, *_consumers;
// 소비자 스레드의 개수를 담을 정수형 변수 선언
int consumersLength;
// 소비자 스레드에게 주어지는 id 역할을 할 정수형 변수 선언
int consumersCount;
Element buffer[BUFFER_SIZE];
Element currentElem[8];
int tid[8];
int searchCountProducerSum;
int searchCountConsumersSum;
int searchCount[8];
int **cache;
int map[50][50];
int prodIndex = 0;
int consIndex = 0;
pthread_mutex_t consMutex;
pthread_mutex_t prodMutex;
pthread_cond_t consCond;
pthread_cond_t prodCond;

// 거리의 최소값 변수 선언
int bestResult = INT16_MAX;
int fastestWay[50];
// 파일의 행 갯수 선언
int fileLength;

void freeMemories(void);
int min(int a, int b);
FILE *openFile(const char *);
int getConsumerNumber(void);
int TSP_consumer(int *, int, int, int, int, uint64_t);
int TSP_producer(int *, int, int, uint64_t, int);
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
void closeMutex(void);
void *producer(void *);
void *consumer(void *);
void initBuffer(void);
void handleSigaction(struct sigaction *);
void showResult(void);
void showThread(void);
void showStat(void);
void onDisconnect(int);

void freeMemories(void) {
    for (int i = 0; i < fileLength; i++) {
        free(cache[i]);
    }
    free(cache);
}
int min(int a, int b) { return a < b ? a : b; }
int TSP_consumer(int *pathRecord, int sum, int threadNumber, int count,
                 int currentIndex, uint64_t visited) {
    pathRecord[count] = currentIndex;
    pthread_mutex_lock(&consMutex);
    searchCountConsumersSum++;
    searchCount[threadNumber]++;
    pthread_mutex_unlock(&consMutex);
    if (visited == (1 << fileLength) - 1) {
        if (sum < bestResult) {
            pthread_mutex_lock(&consMutex);
            bestResult = sum;
            for (int i = 0; i < fileLength; i++) {
                fastestWay[i] = pathRecord[i];
            }
            pthread_mutex_unlock(&consMutex);
        }
        return map[currentIndex][0];
    }
    int *ptr = &cache[currentIndex][visited];
    if (*ptr) {
        return *ptr;
    }
    *ptr = INT16_MAX;
    for (int next = 0; next < fileLength; next++) {
        if (currentIndex == next) {
            continue;
        }
        if (visited & (1 << next))
            continue;
        if (map[currentIndex][next] == 0)
            continue;
        *ptr = min(*ptr, TSP_consumer(pathRecord, sum + map[currentIndex][next],
                                      threadNumber, count + 1, next,
                                      visited | (1 << next)) +
                             map[currentIndex][next]);
    }
    return *ptr;
}
int TSP_producer(int *pathRecord, int sum, int currentIndex, uint64_t visited,
                 int count) {
    pathRecord[count] = currentIndex;
    searchCountProducerSum++;
    if (count == fileLength - 12) {
        pthread_mutex_lock(&prodMutex);
        while (buffer[prodIndex].visited) {
            pthread_cond_wait(&prodCond, &prodMutex);
        }
        buffer[prodIndex].sum = sum;
        buffer[prodIndex].currentIndex = currentIndex;
        buffer[prodIndex].visited = visited;
        for (int i = 0; i < count + 1; i++) {
            buffer[prodIndex].path[i] = pathRecord[i];
        }
        prodIndex = (prodIndex + 1) % BUFFER_SIZE;
        pthread_mutex_unlock(&prodMutex);

        pthread_mutex_lock(&consMutex);
        pthread_cond_signal(&consCond);
        pthread_mutex_unlock(&consMutex);
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
        if (currentIndex == next) {
            continue;
        }
        if (visited & (1 << next))
            continue;
        *ptr = min(*ptr, TSP_producer(pathRecord, sum + map[currentIndex][next],
                                      next, visited | (1 << next), count + 1) +
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
    fclose(fp);
    return;
}

void getUserInput(void) {
    char buffer[101];
    while (1) {
        fgets(buffer, 101, stdin);
        if (consumersLength == 0) {
            return;
        }
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
    for (int i = 0; i < consumersLength; i++) {
        pthread_cancel(_consumers[i]);
    }
    free(_consumers);
    pthread_cancel(_producer);
}
void closeMutex(void) {
    pthread_mutex_destroy(&prodMutex);
    pthread_mutex_destroy(&consMutex);

    pthread_cond_destroy(&prodCond);
    pthread_cond_destroy(&consCond);
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
    int path[50] = {
        0,
    };
    for (int i = 0; i < fileLength; i++) {
        TSP_producer(path, 0, i, (uint64_t)1 << i, 0);
    }
    buffer[prodIndex].currentIndex = THREAD_END_CODE;
    return NULL;
}
void *consumer(void *ptr) {
    Element Elem;
    int consumerNumber = getConsumerNumber();
    tid[consumerNumber] = gettid();
    int currentRecord[50];
    if (currentElem[consumerNumber].visited) {
        Elem = currentElem[consumerNumber];
        for (int next = 0; next < fileLength; next++) {
            if (Elem.currentIndex == next) {
                continue;
            }
            if (Elem.visited & (1 << next))
                continue;
            if (map[Elem.currentIndex][next] == 0)
                continue;
            TSP_consumer(currentRecord, Elem.sum, consumerNumber,
                         fileLength - 11, next, Elem.visited | (1 << next));
        }
        currentElem[consumerNumber].sum = 0;
        currentElem[consumerNumber].currentIndex = 0;
        currentElem[consumerNumber].visited = 0;
    }
    while (1) {
        pthread_mutex_lock(&consMutex);
        if (buffer[consIndex].currentIndex == THREAD_END_CODE) {
            consumersLength--;
            pthread_mutex_unlock(&consMutex);
            pthread_cond_signal(&consCond);
            if (consumersLength == 0) {
                printf("\nTask Done!\n");
                onDisconnect(0);
            }
            break;
        }

        while (buffer[consIndex].visited == 0) {
            pthread_cond_wait(&consCond, &consMutex);
        }

        Elem = buffer[consIndex];
        for (int i = 0; i < fileLength - 11; i++) {
            currentRecord[i] = Elem.path[i];
        }
        buffer[consIndex].sum = 0;
        buffer[consIndex].currentIndex = 0;
        buffer[consIndex].visited = 0;
        currentElem[consumerNumber].sum = Elem.sum;
        currentElem[consumerNumber].currentIndex = Elem.currentIndex;
        currentElem[consumerNumber].visited = Elem.visited;

        consIndex = (consIndex + 1) % BUFFER_SIZE;
        pthread_mutex_unlock(&consMutex);
        pthread_mutex_lock(&prodMutex);
        pthread_cond_signal(&prodCond);
        pthread_mutex_unlock(&prodMutex);

        for (int next = 0; next < fileLength; next++) {
            if (Elem.currentIndex == next) {
                continue;
            }
            if (Elem.visited & (1 << next))
                continue;
            if (map[Elem.currentIndex][next] == 0)
                continue;
            TSP_consumer(currentRecord, Elem.sum, consumerNumber,
                         fileLength - 11, next, Elem.visited | (1 << next));
        }
        currentElem[consumerNumber].sum = 0;
        currentElem[consumerNumber].currentIndex = 0;
        currentElem[consumerNumber].visited = 0;
    }

    pthread_exit(NULL);

    return NULL;
}
void initBuffer(void) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer[i].path = (int *)calloc(50, sizeof(int));
    }
}
void closeBuffer(void) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        free(buffer[i].path);
    }
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
    printf("Press enter to continue...\n");
}
void showThread(void) {
    for (int i = 0; i < consumersLength; i++) {
        printf("Thread Number : %d\n  TID: %d\n   searched routes of subtask: "
               "%d\n",
               i + 1, tid[i], searchCount[i]);
    }
}
void showStat(void) {
    printf("Sum of searched routes (ignore cached routes): %d\n",
           searchCountProducerSum + searchCountConsumersSum);
    printf("Sum of searched routes by Producer Thread: %d\n",
           searchCountProducerSum);
    printf("Sum of searched routes by Consumer Thread: %d\n",
           searchCountConsumersSum);
    if (bestResult == INT16_MAX) {
        return;
    }
    printf("Current Lowest Sum of Weights: %d\n", bestResult);
    printf("Way: ");
    for (int i = 0; i < fileLength; i++) {
        printf("%d->", fastestWay[i]);
    }
    // 마지막 화살표 지우기
    printf("\b\b");
    printf("  \n");
}
void initSemaphore(void) {
    pthread_mutex_init(&consMutex, NULL);
    pthread_mutex_init(&prodMutex, NULL);

    pthread_cond_init(&consCond, NULL);
    pthread_cond_init(&prodCond, NULL);
}
void onDisconnect(int sig) {
    showResult();
    closeThreads();
    closeMutex();
    closeBuffer();
    freeMemories();
    pthread_exit(NULL);
    exit(0);
}
void destroyMutex(void) {
    pthread_mutex_destroy(&prodMutex);
    pthread_mutex_destroy(&consMutex);
}
void destroyCond(void) {
    pthread_cond_destroy(&prodCond);
    pthread_cond_destroy(&consCond);
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

    printf("number of file rows: %d\n", fileLength);
    printf("Calculating...\n");
    initMap(fp, fileLength);
    initBuffer();
    initCache();

    initSemaphore();

    initConsumersPointer();
    createThreads();

    getUserInput();
    pthread_join(_producer, NULL);
    for (int i = 0; i < consumersLength; i++) {
        pthread_join(_consumers[i], NULL);
    }

    return 0;
}