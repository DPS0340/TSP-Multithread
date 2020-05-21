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
// gettid 함수 implicit으로 불러오는 warning을 막기 위해 선언
#define gettid() syscall(SYS_gettid)
// struct sigaction 사용을 위해 선언
#define _XOPEN_SOURCE
// 유한 버퍼의 길이 선언
#define BUFFER_SIZE 200
// 생산자 스레드가 완료된 소비자 스레드를 종료시키는 코드 선언
#define THREAD_END_CODE -5555

// 생산자 스레드가 소비자 스레드에게 넘겨주는 버퍼의 원소 선언
typedef struct _Element {
    // 현재 방문하는 노드의 인덱스값
    int currentIndex;
    // 생산자 스레드가 찾은 합
    int sum;
    // 비트를 이용한 노드 방문 여부
    uint64_t visited;
    // 생산자 스레드가 가본 경로를 담는 배열
    int path[50];
} Element;

// 스레드 변수 전역변수로 선언
pthread_t *_childThreads;
// 소비자 스레드의 개수를 담을 정수형 변수 선언
int childsLength;
int isChildsWorking[12];
// 생산자가 찾은 경로의 개수
int searchCountmainThreadSum;
// 소비자가 찾은 경로의 개수
int searchCountchildsSum;
// 캐시 배열
// 사이즈가 크므로 추후에 heap 영역으로 동적 할당 받는다
// 메모리를 차지하는 공간 복잡도가
// n이 행 갯수일때 O((2^n))이 되지만
// 수행 속도가 수백배 빨라지는 효과를 보므로 캐시를 사용하게 했다
int **cache;
// 지도 배열
// 50은 최대 tsp 입력값의 row의 갯수와 같다
int map[50][50];
// 생산자가 다루는 버퍼의 현재 인덱스 선언
// 나머지 연산으로 값이 순환됨
int prodIndex = 0;
// 소비자가 다루는 버퍼의 현재 인덱스 선언
// 나머지 연산으로 값이 순환됨
int childsIndex = 0;
// 생산자 - 소비자 문제 해결을 위한 뮤텍스 선언
// 스레드간의 공유 객체의 쓰기, 읽기가 엉키는 것을 방지할 수 있다
pthread_mutex_t childsMutex;
pthread_cond_t childsCond;
// 유한 버퍼를 선언
Element buffer[BUFFER_SIZE];

// 거리의 최소값 변수 선언
// 여기에서는 초기값으로 충분히 큰 정수인 32767로 설정하였다
int bestResult = INT16_MAX;
// 거리의 최소값의 경로 배열 선언
// 길이는 row의 최대값인 50으로 설정하였다
int fastestWay[50];
// 파일의 행 개수 선언
int fileLength;

// 할당된 메모리를 해제하는 함수
void freeMemories(void);
// 최솟값을 찾는 함수
int min(int, int);
// 파일을 여는 함수
FILE *openFile(const char *);
// TSP 소비자 함수
// visited는 비트를 사용해 경로들을 표시한다
int TSP_child(int *, int, int, int, int, uint64_t);
// TSP 생산자 함수
// visited는 비트를 사용해 경로들을 표시한다
int TSP_mainThread(int *, int, int, uint64_t, int);
// map 배열을 파일포인터와 row의 길이를 통해 읽어들이는 함수
void initMap(FILE *, int);
// 캐시 메모리를 초기화하는 함수
void initCache(void);
// 프로그램 실행에 주어진 인자가 2개인지 확인하는 함수
// 2개가 아니라면 오류 메시지 출력 후 프로그램 종료
void checkArgcCorrentness(int);
// 소비자들의 배열을 초기화하는 함수
void initchildsPointer(void);

// 파일의 row 개수를 찾아 반환하는 함수
// 공식상 파일 포인터의 원소들의 개수가 n*(n-1)이므로 - 제곱근을 이용하여 버림한
// 값에 1을 추가한다
int findFileLength(FILE *);
// 스레드들을 종료하는 함수
void closeThreads(void);
// 뮤텍스를 할당 해제하는 함수
void closeMutex(void);
// 메인 스레드에서 생산자로 쓰이는 함수
void produceByMainThread(void);
// 소비자 스레드로 쓰이는 함수
void *child(void *);
// 버퍼의 개별 원소의 path 배열을 할당하는 함수
void initBuffer(void);
// 뮤텍스를 할당하는 함수
void initMutex(void);
// Ctrl-C등의 인터럽트 동작을 관리하는 함수
// Ctrl-C를 눌렀을 시 onDisconnect(int)가 호출되게끔 하는 함수이다
void handleSigaction(struct sigaction *);
// 결과값을 보여주는 함수
// 완료되었을때 호출된다
void showResult(void);
// 최단거리값, 최단거리의 경로, 모든 스레드가 탐색한 경로들의 합 등을 보여주는
// 함수 완료되었을때나 사용자 입력을 받았을 때 호출된다
void showStat(void);
int showNextThreadNumber(void);
// 종료시나 Ctrl-C 인터럽트 등을 받았을 때 호출되는 함수
// 할당된 메모리들을 해제하고 프로그램 종료를 시키는 함수
void onDisconnect(int);

// 여러 오류들을 함수 형태로 저장
//
// 호출되면 오류 메시지를 출력하고 프로그램이 종료된다
// exit code 1로 정상적인 종료가 아니라는것을 알림
void fscanfError(void);
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

int showNextThreadNumber(void) {
    for (int i = 0; i < childsLength; i++) {
        if (isChildsWorking[i] == 0) {
            return i;
        }
    }
    return -1;
}

void freeMemories(void) { free(cache); }
int min(int a, int b) { return a < b ? a : b; }
int TSP_child(int *pathRecord, int sum, int threadNumber, int count,
              int currentIndex, uint64_t visited) {
    // 현재 간 노드를 기록한다
    pathRecord[count] = currentIndex;
    pthread_mutex_lock(&childsMutex);
    searchCountchildsSum++;
    pthread_mutex_unlock(&childsMutex);
    // 다 가본 경우
    if (visited == (1 << fileLength) - 1) {
        // 현재값이 전역 최소값이면
        if (sum < bestResult) {
            // 전역 변수 쓰기를 하므로 충돌을 막기 위해 lock을 건다
            pthread_mutex_lock(&childsMutex);
            bestResult = sum;
            for (int i = 0; i < fileLength; i++) {
                fastestWay[i] = pathRecord[i];
            }
            // 전역 변수 쓰기를 다했으므로 lock을 푼다
            pthread_mutex_unlock(&childsMutex);
        }
        return sum + map[currentIndex][pathRecord[0]];
    }
    int *ptr = &cache[currentIndex][visited];
    // 이미 계산된 값이 있을경우
    // 캐싱이 되었다는걸 의미한다
    if (*ptr && *ptr != INT16_MAX) {
        // 캐싱된 값을 돌려준다
        return sum + (*ptr);
    }
    // 충분히 큰 값을 초기값으로 대입
    // 최소값을 찾기 위해서이다
    *ptr = INT16_MAX;
    // 여러 노드를 방문하려고 시도한다
    for (int next = 0; next < fileLength; next++) {
        // 똑같은 경로를 가려고 하는 경우
        if (currentIndex == next) {
            continue;
        }
        // next값이 이미 가본 노드일 경우
        if (visited & (1 << next))
            continue;
        // 재귀적으로 호출하면서 최소값을 찾음
        *ptr = min(*ptr, TSP_child(pathRecord, sum + map[currentIndex][next],
                                   threadNumber, count + 1, next,
                                   visited | (1 << next)));
    }
    return *ptr;
}
int TSP_mainThread(int *pathRecord, int sum, int currentIndex, uint64_t visited,
                   int count) {
    // 현재 간 노드를 기록한다
    pathRecord[count] = currentIndex;
    searchCountmainThreadSum++;
    if (count == fileLength - 13) {
        int childNumber, err;
        buffer[prodIndex].currentIndex = currentIndex;
        buffer[prodIndex].sum = sum;
        buffer[prodIndex].visited = visited;

        for (int i = 0; i <= count; i++) {
            buffer[prodIndex].path[i] = pathRecord[i];
        }

        while ((childNumber = showNextThreadNumber()) == -1)
            ;
        // 1을 추가하고 사이즈보다 클시에는 나머지 연산을 통해서 0으로 돌린다
        err = pthread_create(&_childThreads[childNumber], NULL, child,
                             (void *)&buffer[prodIndex]);
        prodIndex = (prodIndex + 1) % BUFFER_SIZE;
        if (err) {
            threadCreateError(err);
        }

        return sum;
    }
    // 충분히 큰 값을 초기값으로 대입
    // 최소값을 찾기 위해서이다
    int res = INT16_MAX;
    // 여러 노드를 방문하려고 시도한다
    for (int next = 0; next < fileLength; next++) {
        // 똑같은 경로를 가려고 하는 경우
        if (currentIndex == next) {
            continue;
        }
        // next값이 이미 가본 노드일 경우
        if (visited & (1 << next))
            continue;
        // 재귀적으로 호출하면서 최소값을 찾음
        res = min(res, TSP_mainThread(pathRecord, sum + map[currentIndex][next],
                                      next, visited | (1 << next), count + 1) +
                           map[currentIndex][next]);
    }
    return res;
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
            // i == j일시 해당 입력파일에서는 값을 주지 않는다
            // 알고리즘에서 필터링하므로 해당 인덱스의 값은 아무 값이어도
            // 상관없지만 전역변수이므로 이미 0으로 초기화되어 추가적인 대입이
            // 필요하지 않다
            if (i == j) {
                continue;
            }
            // fscanf는 하나의 값을 읽어들인 경우 1을 반환한다(포인터 연산과는
            // 다른 순수 return값)
            resCode = fscanf(fp, "%d", &map[i][j]);
            // 결과값이 1이 아니면 읽기에 오류가 있다는 것을 의미하므로
            if (resCode != 1) {
                // 오류를 출력하고 프로그램을 종료한다
                fscanfError();
            }
        }
    }
    fclose(fp);
    return;
}
void checkArgcCorrentness(int argc) {
    if (argc == 1) {
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
    fprintf(stderr, "Error: Initial number of child threads not given\n");
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
void initchildsPointer() {
    _childThreads = (pthread_t *)calloc(childsLength, sizeof(pthread_t));
}
void closeThreads(void) {
    for (int i = 0; i < childsLength; i++) {
        if (isChildsWorking[i]) {
            pthread_cancel(_childThreads[i]);
        }
    }
    free(_childThreads);
}
void closeMutex(void) {
    pthread_mutex_destroy(&childsMutex);
    pthread_cond_destroy(&childsCond);
}

int findFileLength(FILE *fp) {
    int count = 0, temp;
    // EOF가 나올때까지 count 증가
    while (fscanf(fp, "%d", &temp) == 1) {
        count++;
    }
    // 내용이 없으면
    if (count == 0) {
        // 오류 메시지 출력 후 프로그램 종료
        noContextsinFileError();
    }
    // 제곱근을 취한 후 버림한다
    // 간단한 수식을 사용 - n-1 <= sqrt(n*n - n) < n
    int n = (int)sqrt(count);
    // 다음 사용을 위해 파일 포인터를 초기 위치로 돌려놓는다
    rewind(fp);
    // 수식상으로 1을 더한 n+1이 실제 row값이라고 볼 수 있음
    return n + 1;
}
void produceByMainThread(void) {
    int path[50] = {
        0,
    };
    // 노드를 순회하면서
    for (int i = 0; i < fileLength; i++) {
        // TSP를 호출한다
        // 한번만 순회하면서 함수를 호출하면 함수 내에서 재귀적으로 모두 커버가
        // 된다
        TSP_mainThread(path, 0, i, (uint64_t)1 << i, 0);
    }
    return;
}
void *child(void *ptr) {
    Element Elem = *(Element *)ptr;
    Element *Elem_ptr = (Element *)ptr;
    Elem_ptr->currentIndex = 0;
    Elem_ptr->visited = 0;
    pthread_mutex_lock(&childsMutex);
    int threadNumber = showNextThreadNumber();
    isChildsWorking[threadNumber] = 1;
    childsLength++;
    pthread_mutex_unlock(&childsMutex);
    int currentRecord[50];

    for (int i = 0; i < fileLength - 12; i++) {
        currentRecord[i] = Elem.path[i];
    }

    for (int next = 0; next < fileLength; next++) {
        if (Elem.currentIndex == next) {
            continue;
        }
        if (Elem.visited & (1 << next))
            continue;
        TSP_child(currentRecord, Elem.sum + map[Elem.currentIndex][next],
                  threadNumber, fileLength - 12, next,
                  Elem.visited | (1 << next));
    }

    isChildsWorking[threadNumber] = 0;

    pthread_exit(NULL);

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
    printf("Press enter to continue...\n");
}
void showStat(void) {
    // 모든 스레드가 탐색된 경로들의 갯수들 (캐시되어서 두번 이상 탐색될
    // 경로들의 갯수는 제외)
    printf("Number of searched routes (except cached routes): %d\n",
           searchCountmainThreadSum + searchCountchildsSum);
    printf("Number of searched routes by main Thread: %d\n",
           searchCountmainThreadSum);
    printf("Number of searched routes by child Threads: %d\n",
           searchCountchildsSum);
    // 초기값 그대로인 경우
    if (bestResult == INT16_MAX) {
        // 출력하지 않는다
        return;
    }
    // 현재 최단거리 값
    printf("Current Lowest Sum of Weights: %d\n", bestResult);
    printf("Way: ");
    for (int i = 0; i < fileLength; i++) {
        printf("%d->", fastestWay[i]);
    }
    printf("%d\n", fastestWay[0]);
}
void initMutex(void) {
    pthread_mutex_init(&childsMutex, NULL);
    pthread_cond_init(&childsCond, NULL);
}
void onDisconnect(int sig) {
    showResult();
    closeThreads();
    closeMutex();
    freeMemories();
    pthread_exit(NULL);
    exit(0);
}
void destroyMutex(void) { pthread_mutex_destroy(&childsMutex); }
void destroyCond(void) { pthread_cond_destroy(&childsCond); }

int main(int argc, char **argv) {
    printf("TSP Program\n");

    // ctrl-c 핸들러
    struct sigaction action;
    handleSigaction(&action);

    // argc가 2개여야 하므로 체크
    checkArgcCorrentness(argc);

    // 파일이름 문자열 선언
    char *filename = argv[1];
    childsLength = atoi(argv[2]);
    // 입력받은 스레드 개수가 정상적인 범위에 있지 않을 경우
    if (1 > childsLength || childsLength > 8) {
        // 에러 출력후 프로그램 종료
        invalidThreadNumberError();
    }

    // filename을 이용해 파일 포인터 열기
    FILE *fp = openFile(filename);
    // 파일 포인터의 row값을 찾기
    fileLength = findFileLength(fp);

    printf("number of file rows: %d\n", fileLength);
    printf("Calculating...\n");
    // map 배열 초기화
    initMap(fp, fileLength);
    // tsp 문제에 쓰일 캐시 메모리를 할당한다
    initCache();
    // 뮤텍스 할당
    initMutex();
    // 소비자 배열 초기화
    initchildsPointer();

    // 생산자 함수 동작
    produceByMainThread();

    onDisconnect(0);

    return 0;
}