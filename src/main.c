#include <windows.h>
#include <stdio.h>
#include <stdlib.h>  
#include <time.h>

typedef struct {
    long long count;
    unsigned int seed;
} ThreadData;

static long long g_K, g_p1, g_p2;
static long long g_wins1 = 0, g_wins2 = 0, g_draws = 0;
static CRITICAL_SECTION g_lock, g_rand_lock;

int dice_roll() {
    return 1 + rand() % 6;
}

long long player_points(long long rounds) {
    long long sum = 0;
    for (long long i = 0; i < rounds; i++) {
        sum += dice_roll();
        sum += dice_roll();
    }
    return sum;
}

DWORD WINAPI worker(LPVOID arg) {
    ThreadData *data = (ThreadData *)arg;
    long long w1 = 0, w2 = 0, dr = 0;
    srand(data->seed);

    for (long long i = 0; i < data->count; i++) {
        long long s1 = g_p1 + player_points(g_K);
        long long s2 = g_p2 + player_points(g_K);

        if (s1 > s2) w1++;
        else if (s2 > s1) w2++;
        else dr++;
    }
    EnterCriticalSection(&g_lock);
    g_wins1 += w1;
    g_wins2 += w2;
    g_draws += dr;
    LeaveCriticalSection(&g_lock);
    return 0;
}

int main(int argc, char *argv[]) {
    int threads;
    if (argc < 2) threads = 1;
    else {
        char *end;
        threads = strtol(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || threads < 1) {
            fprintf(stderr, "Threads count must be a positive int: %d", threads);
            return 1;
        }
    }
    long long n;
    printf("Enter K, p1 score, p2 score, n: ");
    /* Не уверен насчет "туров" как еще один аргумент */
    if (scanf("%lld %lld %lld %lld", &g_K, &g_p1, &g_p2, &n) != 4) {
        fprintf(stderr, "Error: expected 4 integers\n");
        return 1;
    }
    if (g_K < 1 || g_p1 < 0 || g_p2 < 0 || n < 1) {
        fprintf(stderr, "Error: need K >= 1, p1 >= 0, p2 >= 0, n >= 1\n");
        return 1;
    }

    HANDLE handles[threads];
    ThreadData data[threads];
    int created = 0;
    int error_code = 0;

    InitializeCriticalSection(&g_lock);

    long long base = n / threads;
    long long extra = n % threads;

    for (int i = 0; i < threads; i++) {
        data[i].count = base + (i < extra ? 1 : 0);
        data[i].seed = (unsigned int)time(NULL) ^ (i * 16777619U);

        handles[i] = CreateThread(NULL, 0, worker, &data[i], 0, NULL);
        if (handles[i] == NULL) {
            fprintf(stderr, "CreateThread failed on %d: error %lu\n", i+1, GetLastError());
            fprintf(stdout, "Were able to create only %d threads\n", i);
            error_code = 1;
            break;
        }
        created++;
    }

    for (int i = 0; i < created; i++) {
        if (WaitForSingleObject(handles[i], INFINITE) == WAIT_FAILED) {
            fprintf(stderr, "WaitForSingleObject failed: error %lu\n", GetLastError());
            error_code = 1;
        }
        CloseHandle(handles[i]);
    }
    DeleteCriticalSection(&g_lock);

    printf("Player 1 wins: %f%%\n", 100.0 * g_wins1 / n);
    printf("Player 2 wins: %f%%\n", 100.0 * g_wins2 / n);
    printf("Draws: %f%%\n", 100.0 * g_draws / n);

    return error_code;
}