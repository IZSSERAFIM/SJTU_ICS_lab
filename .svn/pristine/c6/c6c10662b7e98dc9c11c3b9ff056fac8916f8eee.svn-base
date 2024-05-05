// 李若彬 522031910747
#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

// 定义高速缓存行
typedef struct {
    int valid;     // 有效位
    int tag;       // 标记位
    int time_stamp; // 时间戳
} CacheLine;

// 定义高速缓存
typedef struct {
    int S;              // S=2^s是组的个数
    int E;              // 每组有E行
    int B;              // B个block
    CacheLine **lines;  // 高速缓存
} Cache;

int hit_count = 0, miss_count = 0, eviction_count = 0;  // 记录缓存命中、缓存未命中和缓存替换的次数
int verbose = 0;                                        // 是否打印详细信息
char trace_file[1000];                                  // 存储跟踪文件的路径
Cache *cache = NULL;

// 初始化高速缓存
void initCache(int s, int E, int b) {
    int S = 1 << s;  // 2^s
    int B = 1 << b;  // 2^b
    cache = (Cache *)malloc(sizeof(Cache));
    cache->S = S;
    cache->E = E;
    cache->B = B;
    cache->lines = (CacheLine **)malloc(sizeof(CacheLine *) * S);
    for (int i = 0; i < S; i++) {
        cache->lines[i] = (CacheLine *)malloc(sizeof(CacheLine) * E);
        for (int j = 0; j < E; j++) {
            cache->lines[i][j].valid = 0;
            cache->lines[i][j].tag = -1;
            cache->lines[i][j].time_stamp = 0;
        }
    }
}

// 释放高速缓存占用的内存
void freeCache() {
    int S = cache->S;
    for (int i = 0; i < S; i++) {
        free(cache->lines[i]);
    }
    free(cache->lines);
    free(cache);
}

// 更新高速缓存行信息
void update(int i, int op_s, int op_tag) {
    cache->lines[op_s][i].valid = 1;
    cache->lines[op_s][i].tag = op_tag;
    for (int k = 0; k < cache->E; k++) {
        if (cache->lines[op_s][k].valid == 1) {
            cache->lines[op_s][k].time_stamp++;
        }
    }
    cache->lines[op_s][i].time_stamp = 0;
}

// 获取高速缓存行的索引
int getIndex(int op_s, int op_tag) {
    for (int i = 0; i < cache->E; i++) {
        if (cache->lines[op_s][i].valid && cache->lines[op_s][i].tag == op_tag) {
            return i;
        }
    }
    return -1;
}

// 查找最近最少使用的高速缓存行
int findLRU(int op_s) {
    int max_index = 0;
    int max_stamp = 0;
    for (int i = 0; i < cache->E; i++) {
        if (cache->lines[op_s][i].time_stamp > max_stamp) {
            max_stamp = cache->lines[op_s][i].time_stamp;
            max_index = i;
        }
    }
    return max_index;
}

// 检查高速缓存是否已满
int isFull(int op_s) {
    for (int i = 0; i < cache->E; i++) {
        if (cache->lines[op_s][i].valid == 0) {
            return i;
        }
    }
    return -1;
}

// 更新高速缓存信息
void updateInfo(int op_tag, int op_s) {
    int index = getIndex(op_s, op_tag);
    if (index == -1) {
        miss_count++;
        if (verbose) {
            printf("miss ");
        }
        int i = isFull(op_s);
        if (i == -1) {
            eviction_count++;
            if (verbose) {
                printf("eviction");
            }
            i = findLRU(op_s);
        }
        update(i, op_s, op_tag);
    } else {
        hit_count++;
        if (verbose) {
            printf("hit");
        }
        update(index, op_s, op_tag);
    }
}

// 读取跟踪文件并更新高速缓存信息
void getTrace(int s, int E, int b) {
    FILE *pFile;
    pFile = fopen(trace_file, "r");
    if (pFile == NULL) {
        exit(-1);
    }
    char identifier;
    unsigned address;
    int size;
    while (fscanf(pFile, " %c %x,%d", &identifier, &address, &size) > 0) {
        int op_tag = address >> (s + b);
        int op_s = (address >> b) & ((unsigned)(-1) >> (8 * sizeof(unsigned) - s));
        switch (identifier) {
            case 'M':
                updateInfo(op_tag, op_s);
                updateInfo(op_tag, op_s);
                break;
            case 'L':
                updateInfo(op_tag, op_s);
                break;
            case 'S':
                updateInfo(op_tag, op_s);
                break;
        }
    }
    fclose(pFile);
}

// 打印帮助信息
void printHelp() {
    printf("** A Cache Simulator by Deconx\n");
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("-h         Print this help message.\n");
    printf("-v         Optional verbose flag.\n");
    printf("-s <num>   Number of set index bits.\n");
    printf("-E <num>   Number of lines per set.\n");
    printf("-b <num>   Number of block offset bits.\n");
    printf("-t <file>  Trace file.\n\n\n");
    printf("Examples:\n");
    printf("linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

int main(int argc, char *argv[]) {
    char opt;
    int s, E, b;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1 ) {
        switch (opt) {
            case 'h':
                printHelp();
                exit(0);
            case 'v':
                verbose = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                strcpy(trace_file, optarg);
                break;
            default:
                printHelp();
                exit(-1);
        }
    }
    initCache(s, E, b);
    getTrace(s, E, b);
    freeCache();
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}

