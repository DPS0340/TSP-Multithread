// 제곱근 사용을 위해 include
#include <math.h>
// multi threading
#include <pthread.h>
// 세마포어 header file
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
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

// 메인 스레드가 자식 스레드에게 넘겨주는 버퍼의 원소 선언
typedef struct _Element
{
    // 현재 방문하는 노드의 인덱스값
    int currentIndex;
    // 생산자 스레드가 찾은 합
    int sum;
    // 비트를 이용한 노드 방문 여부
    uint64_t visited;
    // 생산자 스레드가 가본 경로를 담는 배열
    int path[51];
} Element;
typedef struct _Packet
{
    // 지역 최솟값
    int value;
    // 지역 최솟값의 경로
    int path[50];
} Packet;

// 자식 스레드의 배열을 전역변수로 선언
pthread_t *_childThreads;
// 자식 스레드의 갯수를 담을 정수형 변수 선언
int childsLength;
// 자식 스레드들이 현재 동작하고 있는지 보는 함수
// 12개는 자식 스레드의 최대 갯수
int isChildsWorking[12];
// 자식 스레드의 파이프의 배열
// 12개는 자식 스레드의 최대 갯수
int fd[12][2];
// 생산자가 찾은 경로의 개수
int searchCountmainThreadSum;
// 자식 스레드들이 찾은 경로의 개수
int searchCountchildsSum;
// 캐시 배열
// 사이즈가 크므로 추후에 heap 영역으로 동적 할당 받는다
// 메모리를 차지하는 공간 복잡도가
// n이 행 갯수일때 O(2^n)이 되지만
// 수행 속도가 수백배 빨라지는 효과를 보므로 캐시를 사용하게 했다
int **cache;
// 지도 배열
// 50은 최대 tsp 입력값의 row의 갯수와 같다
int map[50][50];
// 생산자가 다루는 버퍼의 현재 인덱스 선언
// 나머지 연산으로 값이 순환됨
int prodIndex = 0;
// 자식들이 다루는 버퍼의 현재 인덱스 선언
// 나머지 연산으로 값이 순환됨
int childsIndex = 0;
// 유한 버퍼를 선언
Element buffer[BUFFER_SIZE];

// 공유 객체의 수정을 관리하기 위한 뮤텍스 선언
pthread_mutex_t childsMutex;
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
// 파이프를 여는 함수
void initPipe(void);
// TSP 자식 함수
// visited는 비트를 사용해 경로들을 표시한다
int TSP_child(int *, int *, int *, int, int, int, int, uint64_t);
// TSP 메인 함수
// visited는 비트를 사용해 경로들을 표시한다
int TSP_mainThread(int *, int, int, uint64_t, int);
// map 배열을 파일포인터와 row의 길이를 통해 읽어들이는 함수
void initMap(FILE *, int);
// 캐시 메모리를 초기화하는 함수
void initCache(void);
// 프로그램 실행에 주어진 인자가 2개인지 확인하는 함수
// 2개가 아니라면 오류 메시지 출력 후 프로그램 종료
void checkArgcCorrentness(int);
// 자식 스레드들의 배열을 초기화하는 함수
void initchildsPointer(void);
// 메인 스레드에서 파이프를 읽는 함수
// 모두 읽어서 최솟값과 비교함
void listenPipeMessages(void);

// 파일의 row 개수를 찾아 반환하는 함수
// 공식상 파일 포인터의 원소들의 개수가 n*(n-1)이므로 - 제곱근을 이용하여 버림한
// 값에 1을 추가한다
int findFileLength(FILE *);
// 스레드들을 종료하는 함수
void closeThreads(void);
// 메인 스레드에서 생산자 역할을 하는 함수
void produceByMainThread(void);
// 자식 스레드로 쓰이는 함수
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
// 다음에 할당될 자식 스레드 번호를 정해주는 함수
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
void initPipeError(void);

int showNextThreadNumber(void)
{
    for (int i = 0; i < childsLength; i++)
    {
        if (isChildsWorking[i] == 0)
        {
            return i;
        }
    }
    return -1;
}

void freeMemories(void) { free(cache); }
int min(int a, int b) { return a < b ? a : b; }
int TSP_child(int *resultPath, int *pathRecord, int *localSmallest, int sum,
              int threadNumber, int count, int currentIndex, uint64_t visited)
{
    // 현재 간 노드를 임시로 기록한다
    pathRecord[count] = currentIndex;
    pthread_mutex_lock(&childsMutex);
    searchCountchildsSum++;
    pthread_mutex_unlock(&childsMutex);
    // 다 가본 경우
    if (visited == (uint64_t)(1 << fileLength) - 1)
    {
        // 현재값이 스레드 최소값이면
        if (sum < *localSmallest)
        {
            *localSmallest = sum;
            for (int i = 0; i < fileLength; i++)
            {
                resultPath[i] = pathRecord[i];
            }
        }
        return sum + map[currentIndex][pathRecord[0]];
    }
    int *ptr = &cache[currentIndex][visited];
    // 이미 계산된 값이 있을경우
    // 캐싱이 되었다는걸 의미한다
    if (*ptr && *ptr != INT16_MAX)
    {
        // 캐싱된 값을 돌려준다
        return sum + (*ptr);
    }
    // 충분히 큰 값을 초기값으로 대입
    // 최소값을 찾기 위해서이다
    *ptr = INT16_MAX;
    // 여러 노드를 방문하려고 시도한다
    for (int next = 0; next < fileLength; next++)
    {
        // 똑같은 경로를 가려고 하는 경우
        if (currentIndex == next)
        {
            continue;
        }
        // next값이 이미 가본 노드일 경우
        if (visited & (1 << next))
            continue;
        // 재귀적으로 호출하면서 최소값을 찾음
        *ptr = min(*ptr, TSP_child(resultPath, pathRecord, localSmallest,
                                   sum + map[currentIndex][next], threadNumber,
                                   count + 1, next, visited | (1 << next)));
    }
    return *ptr;
}
int TSP_mainThread(int *pathRecord, int sum, int currentIndex, uint64_t visited,
                   int count)
{
    // 현재 간 노드를 기록한다
    pathRecord[count] = currentIndex;
    searchCountmainThreadSum++;
    if (count == fileLength - 13)
    {
        int childNumber, err;
        // 현재 버퍼가 비어있지 않을 경우 블로킹
        while (buffer[prodIndex].visited)
            ;
        buffer[prodIndex].currentIndex = currentIndex;
        buffer[prodIndex].sum = sum;
        buffer[prodIndex].visited = visited;

        for (int i = 0; i <= count; i++)
        {
            // 경로 복사
            buffer[prodIndex].path[i] = pathRecord[i];
        }

        while (showNextThreadNumber() == -1)
            ;
        childNumber = showNextThreadNumber();

        // 번호 넘겨주기
        buffer[prodIndex].path[50] = childNumber;
        // 스레드 만들기
        err = pthread_create(&_childThreads[childNumber], NULL, child,
                             (void *)&buffer[prodIndex]);
        // 1을 추가하고 사이즈보다 클시에는 나머지 연산을 통해서 0으로 돌린다
        prodIndex = (prodIndex + 1) % BUFFER_SIZE;
        if (err)
        {
            threadCreateError(err);
        }

        return sum;
    }
    int *ptr = &cache[currentIndex][visited];
    // 이미 계산된 값이 있을경우
    // 캐싱이 되었다는걸 의미한다
    if (*ptr && *ptr != INT16_MAX)
    {
        // 캐싱된 값을 돌려준다
        return sum + (*ptr);
    }
    // 충분히 큰 값을 초기값으로 대입
    // 최소값을 찾기 위해서이다
    *ptr = INT16_MAX;
    // 여러 노드를 방문하려고 시도한다
    for (int next = 0; next < fileLength; next++)
    {
        // 똑같은 경로를 가려고 하는 경우
        if (currentIndex == next)
        {
            continue;
        }
        // next값이 이미 가본 노드일 경우
        if (visited & (1 << next))
            continue;
        // 재귀적으로 호출하면서 최소값을 찾음
        *ptr =
            min(*ptr, TSP_mainThread(pathRecord, sum + map[currentIndex][next],
                                     next, visited | (1 << next), count + 1) +
                          map[currentIndex][next]);
    }
    return *ptr;
}

void initCache(void)
{
    cache = (int **)calloc(fileLength, sizeof(int *));
    if (cache == NULL)
    {
        return memoryAllocationError();
    }
    for (int i = 0; i < fileLength; i++)
    {
        cache[i] = (int *)calloc(1 << fileLength, sizeof(int));
        if (cache[i] == NULL)
        {
            return memoryAllocationError();
        }
    }
}

FILE *openFile(const char *filename)
{
    FILE *res = fopen(filename, "r");
    if (res == NULL)
    {
        fileNotFoundError();
    }
    return res;
}
void initPipe(void)
{
    // 자식 스레드의 갯수만큼
    for (int i = 0; i < childsLength; i++)
    {
        // 파이프를 초기화하고
        // 파이프가 제대로 할당되지 않으면
        if (pipe(fd[i]) == -1)
        {
            // 오류 출력
            return initPipeError();
        }
    }
}
void initMap(FILE *fp, int fileLength)
{
    int resCode;
    for (int i = 0; i < fileLength; i++)
    {
        for (int j = 0; j < fileLength; j++)
        {
            // i == j일시 해당 입력파일에서는 값을 주지 않는다
            // 알고리즘에서 필터링하므로 해당 인덱스의 값은 아무 값이어도
            // 상관없지만 전역변수이므로 이미 0으로 초기화되어 추가적인 대입이
            // 필요하지 않다
            if (i == j)
            {
                continue;
            }
            // fscanf는 하나의 값을 읽어들인 경우 1을 반환한다(포인터 연산과는
            // 다른 순수 return값)
            resCode = fscanf(fp, "%d", &map[i][j]);
            // 결과값이 1이 아니면 읽기에 오류가 있다는 것을 의미하므로
            if (resCode != 1)
            {
                // 오류를 출력하고 프로그램을 종료한다
                fscanfError();
            }
        }
    }
    fclose(fp);
    return;
}
void checkArgcCorrentness(int argc)
{
    if (argc == 1)
    {
        return noCommandLineArgumentError();
    }
    else if (argc == 2)
    {
        return noInitialNumberOfThreadsError();
    }
    else if (argc > 3)
    {
        return tooManyCommandLineArgumentsError();
    }
    else
    {
        return;
    }
}
void fileNotFoundError(void)
{
    fprintf(stderr, "Error: File not found\n");
    exit(1);
}
void noCommandLineArgumentError(void)
{
    fprintf(stderr, "Error: No command line arguments\n");
    exit(1);
}
void noInitialNumberOfThreadsError(void)
{
    fprintf(stderr, "Error: Initial number of child threads not given\n");
    exit(1);
}
void tooManyCommandLineArgumentsError(void)
{
    fprintf(stderr, "Error: Too many command line arguments\n");
    exit(1);
}
void memoryAllocationError(void)
{
    fprintf(stderr, "Error: Memory allocation failed\n");
    exit(1);
}
void outOfBoundError(void)
{
    fprintf(stderr, "Error: Array index out of bound\n");
    exit(1);
}
void invalidThreadNumberError(void)
{
    fprintf(stderr, "Error: Invalid thread number\n");
    exit(1);
}
void noContextsinFileError(void)
{
    fprintf(stderr, "Error: No contexts in text file\n");
    exit(1);
}
void initPipeError(void)
{
    fprintf(stderr, "Error: Pipe initialization failed\n");
    exit(1);
}
void fscanfError(void)
{
    fprintf(stderr, "Error: fscanf failed\n");
    exit(1);
}
void threadCreateError(int errorCode)
{
    fprintf(stderr,
            "Error: Unable to create thread"
            ", Error Code %d\n",
            errorCode);
    exit(1);
}
void semaphoreCreateError(void)
{
    fprintf(stderr, "Error: Unable to create semaphore\n");
    exit(1);
}
void initchildsPointer()
{
    _childThreads = (pthread_t *)calloc(childsLength, sizeof(pthread_t));
}
void closeThreads(void)
{
    // 모든 자식 스레드 순회
    for (int i = 0; i < childsLength; i++)
    {
        // 자식 스레드가 작동중일 경우
        if (isChildsWorking[i])
        {
            // 스레드 종료
            pthread_cancel(_childThreads[i]);
        }
    }
    // 배열 동적 할당 해제
    free(_childThreads);
}
void closeMutex(void) { pthread_mutex_destroy(&childsMutex); }

int findFileLength(FILE *fp)
{
    int count = 0, temp;
    // EOF가 나올때까지 count 증가
    while (fscanf(fp, "%d", &temp) == 1)
    {
        count++;
    }
    // 내용이 없으면
    if (count == 0)
    {
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
void produceByMainThread(void)
{
    int path[50] = {
        0,
    };
    // 노드를 순회하면서
    for (int i = 0; i < fileLength; i++)
    {
        // TSP를 호출한다
        // 한번만 순회하면서 함수를 호출하면 함수 내에서 재귀적으로 모두 커버가
        // 된다
        TSP_mainThread(path, 0, i, (uint64_t)1 << i, 0);
    }
    return;
}
void *child(void *ptr)
{
    Element Elem = *(Element *)ptr;
    Element *Elem_ptr = (Element *)ptr;
    int threadNumber = Elem.path[50];
    Elem_ptr->currentIndex = 0;
    Elem_ptr->visited = 0;
    isChildsWorking[threadNumber] = 1;
    int currentRecord[50] = {
        0,
    };
    int resultPath[50] = {
        0,
    };

    // 경로 복사
    for (int i = 0; i < fileLength - 12; i++)
    {
        currentRecord[i] = Elem.path[i];
    }

    // 지역 최소값 변수 선언
    // 충분히 큰 값인 32767 대입
    int localSmallest = INT16_MAX;

    for (int next = 0; next < fileLength; next++)
    {
        // 원래 있던 곳으로 가려는 경우
        if (Elem.currentIndex == next)
        {
            continue;
        }
        // 갔던 곳을 다시 가려는 경우
        if (Elem.visited & (1 << next))
        {
            continue;
        }
        // for loop으로 순회하면서
        // 재귀적으로 TSP
        TSP_child(resultPath, currentRecord, &localSmallest,
                  Elem.sum + map[Elem.currentIndex][next], threadNumber,
                  fileLength - 12, next, Elem.visited | (1 << next));
    }

    // 패킷 구조체 지역변수 선언
    Packet pack;
    // value값에 지역 최솟값 대입
    pack.value = localSmallest;
    // 경로 복사
    for (int i = 0; i < fileLength; i++)
    {
        pack.path[i] = resultPath[i];
    }

    // 쓰기 pipe로 write
    write(fd[threadNumber][1], &pack, sizeof(pack));
    isChildsWorking[threadNumber] = 0;

    // 스레드 종료
    pthread_exit(NULL);

    return NULL;
}
void handleSigaction(struct sigaction *actionPtr)
{
    memset(actionPtr, 0, sizeof(*actionPtr));
    actionPtr->sa_handler = onDisconnect;
    sigaction(SIGINT, actionPtr, NULL);
    sigaction(SIGQUIT, actionPtr, NULL);
    sigaction(SIGTERM, actionPtr, NULL);
    sigaction(SIGQUIT, actionPtr, NULL);
}
void showResult(void)
{
    showStat();
    printf("Closing the program...\n");
}
void showStat(void)
{
    // 모든 스레드가 탐색된 경로들의 갯수들 (캐시되어서 두번 이상 탐색될
    // 경로들의 갯수는 제외)
    printf("Number of searched routes (except cached routes): %d\n",
           searchCountmainThreadSum + searchCountchildsSum);
    printf("Number of searched routes by main Thread: %d\n",
           searchCountmainThreadSum);
    printf("Number of searched routes by child Threads: %d\n",
           searchCountchildsSum);
    // 초기값 그대로인 경우
    if (bestResult == INT16_MAX)
    {
        // 출력하지 않는다
        return;
    }
    // 현재 최단거리 값
    printf("Current Lowest Sum of Weights: %d\n", bestResult);
    printf("Way: ");
    for (int i = 0; i < fileLength; i++)
    {
        printf("%d->", fastestWay[i]);
    }
    printf("%d\n", fastestWay[0]);
}
void onDisconnect(int sig)
{
    showResult();
    closeThreads();
    closeMutex();
    freeMemories();
    pthread_exit(NULL);
    exit(0);
}
int isAnyThreadExists(void)
{
    for (int i = 0; i < childsLength; i++)
    {
        if (isChildsWorking[i])
        {
            return 1;
        }
    }
    return 0;
}
void listenPipeMessages(void)
{
    // 패킷 구조체 변수 선언
    Packet pack;

    // 모든 자식 스레드의 파이프에
    for (int i = 0; i < childsLength; i++)
    {
        // 읽기 파이프를 non-blocking 모드로 설정
        fcntl(fd[i][0], F_SETFL, fcntl(fd[i][0], F_GETFL) | O_NONBLOCK);
    }
    while (isAnyThreadExists())
    {
        for (int i = 0; i < childsLength; i++)
        {
            // 파이프 읽기 대기열에 남은게 있으면 읽는다
            while ((read(fd[i][0], &pack, sizeof(pack)) > 0))
            {
                // 전역 최소값보다 더 작으면
                if (pack.value < bestResult)
                {
                    // 전역 최소값을 다시 설정
                    bestResult = pack.value;
                    // 경로 복사
                    for (int i = 0; i < fileLength; i++)
                    {
                        fastestWay[i] = pack.path[i];
                    }
                }
            }
        }
    }
}
int main(int argc, char **argv)
{
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
    if (1 > childsLength || childsLength > 12)
    {
        // 에러 출력후 프로그램 종료
        invalidThreadNumberError();
    }

    // filename을 이용해 파일 포인터 열기
    FILE *fp = openFile(filename);
    // 파일 포인터의 row값을 찾기
    fileLength = findFileLength(fp);

    printf("number of file rows: %d\n", fileLength);
    printf("Calculating...\n");
    initPipe();
    // map 배열 초기화
    initMap(fp, fileLength);
    // tsp 문제에 쓰일 캐시 메모리를 할당한다
    initCache();
    // 뮤텍스 할당
    pthread_mutex_init(&childsMutex, NULL);
    // 자식 스레드 배열 초기화
    initchildsPointer();

    // 생산자 함수 동작
    produceByMainThread();

    // 파이프에 있는 메시지들을 모두 읽는다
    listenPipeMessages();

    // 프로그램 종료
    onDisconnect(0);

    return 0;
}