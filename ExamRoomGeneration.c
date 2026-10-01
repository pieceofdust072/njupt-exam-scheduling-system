#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// 考场容量配置：容量在 [MIN_CAPACITY, MIN_CAPACITY + CAPACITY_RANGE) 之间随机。
#define MIN_CAPACITY         30
#define CAPACITY_RANGE       71
// 容量达到该值视为大教室（需要更多监考），否则为小教室。
#define LARGE_ROOM_THRESHOLD 50
// 内存中最多可加载的考场数量。
#define MAX_ROOMS            100

// 考场数据结构：字段顺序与大小必须与主系统保持一致，避免二进制读写错位。
typedef struct {
    char id[30];        // 统一编号：ER-年月日-时分-序号
    char name[50];      // 考场名称
    int capacity;       // 教室容量（>=50 为大教室）
    int usedSeats;      // 当前已占用座位数
    char status[20];    // 考场状态
    char createTime[20];// 创建时间字符串
    char subject[50];   // 考试科目，用于专业对口排班
} ExamRoom;

// 预设词库：随机生成考场名、状态、科目时使用。
char* roomNames[] = {
    "Lecture Hall 101", "Multimedia Room 202", "Physics Lab A", "Chemistry Lab B",
    "Art Hall", "Conference Hall", "Computer Room 1", "Building 1 305", "Building 2 401",
    "Academic Center", "Lab Building 108", "Language Center 201", "Basic Class 502"
};
char* statusList[] = { "Available", "Full", "Maintenance" };
char* subjects[] = { "Computer", "Automation", "Electronic", "Mathematics", "Language" };

// 生成统一考场编号：ER-YYYYMMDD-HHMM-序号。
void GenerateID(char* id, int index) {
    // 按序号把考场分散到不同日期/时段，避免全部挤在同一时刻
    // （否则排班规则“同一时段最多一场”会因无可用时段而大面积排不满）。
    const int slotsPerDay = 3;             // 每天 3 个时段
    const int slotHours[] = { 9, 14, 19 }; // 上午 / 下午 / 晚上
    int slotIdx = index % slotsPerDay;
    int dayOffset = index / slotsPerDay;

    time_t now = time(NULL);
    struct tm t = *localtime(&now);        // 拷贝到本地结构，避免被后续调用覆盖
    t.tm_mday += dayOffset;                // 分散到不同天
    t.tm_hour = slotHours[slotIdx];        // 分散到不同时段
    t.tm_min = 0;
    t.tm_sec = 0;
    mktime(&t);                            // 归一化日期（处理跨月/跨年）

    sprintf(id, "ER-%04d%02d%02d-%02d%02d-%03d",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, index + 1);
}

// 生成 count 条考场数据并覆盖写入 exam_rooms.dat。
void GenerateData(int count) {
    FILE* fp = fopen("exam_rooms.dat", "wb");
    if (!fp) {
        printf("Error: Could not create file.\n");
        return;
    }

    // 以当前时间做随机种子，避免多次生成相同数据。
    srand((unsigned int)time(NULL));
    time_t now = time(NULL);
    char* timeStr = ctime(&now);
    timeStr[strlen(timeStr) - 1] = '\0'; // 去掉末尾换行符

    for (int i = 0; i < count; i++) {
        ExamRoom r;
        GenerateID(r.id, i);
        strcpy(r.name, roomNames[rand() % (sizeof(roomNames) / sizeof(roomNames[0]))]);
        r.capacity = MIN_CAPACITY + (rand() % CAPACITY_RANGE);
        r.usedSeats = 0;
        strcpy(r.status, statusList[0]);
        strcpy(r.subject, subjects[rand() % (sizeof(subjects) / sizeof(subjects[0]))]);
        strncpy(r.createTime, timeStr, sizeof(r.createTime) - 1);
        r.createTime[sizeof(r.createTime) - 1] = '\0';

        fwrite(&r, sizeof(ExamRoom), 1, fp);
    }
    fclose(fp);
    printf("Successfully generated %d exam rooms.\n", count);
}

// 从 exam_rooms.dat 读取考场数据到内存数组，返回实际读到的条数。
int LoadData(ExamRoom r[], int max) {
    FILE* fp = fopen("exam_rooms.dat", "rb");
    if (!fp) return 0;
    int n = 0;
    while (n < max && fread(&r[n], sizeof(ExamRoom), 1, fp)) n++;
    fclose(fp);
    return n;
}

// 按表格打印考场信息，Type 列根据容量自动判断大/小教室。
void Display(ExamRoom r[], int n) {
    printf("\n%-22s %-20s %-12s %-8s %-12s %-15s\n", "ID (Time-Based)", "Name", "Subject", "Capacity", "Type", "Status");
    printf("-----------------------------------------------------------------------------------------------\n");
    for (int i = 0; i < n; i++) {
        char* type = (r[i].capacity >= LARGE_ROOM_THRESHOLD) ? "Large" : "Small";
        printf("%-22s %-20s %-12s %-8d %-12s %-15s\n", r[i].id, r[i].name, r[i].subject, r[i].capacity, type, r[i].status);
    }
}

// 统计大/小教室数量，用于排班时估算监考需求。
void ShowStats(ExamRoom r[], int n) {
    int largeCount = 0, smallCount = 0;
    for (int i = 0; i < n; i++) {
        if (r[i].capacity >= LARGE_ROOM_THRESHOLD) largeCount++;
        else smallCount++;
    }
    printf("\n--- Exam Room Statistics ---\n");
    printf("Total Rooms: %d\n", n);
    printf("Large Rooms (>=50): %d (Needs 6 invigilators each)\n", largeCount);
    printf("Small Rooms (<50):  %d (Needs 3 invigilators each)\n", smallCount);
}

int main() {
    ExamRoom r[MAX_ROOMS];
    int n = LoadData(r, MAX_ROOMS);
    int choice;

    while (1) {
        printf("\n=== Exam Room Data Generator ===\n");
        printf("1. Generate New Data\n");
        printf("2. Display All Rooms\n");
        printf("3. Show Statistics\n");
        printf("0. Exit\n");
        printf("Choice: ");
        if (scanf("%d", &choice) != 1) break;

        switch (choice) {
            case 1: {
                int num;
                printf("Enter number of rooms to generate: ");
                scanf("%d", &num);
                GenerateData(num);
                n = LoadData(r, MAX_ROOMS);
                break;
            }
            case 2: Display(r, n); break;
            case 3: ShowStats(r, n); break;
            case 0: return 0;
            default: printf("Invalid choice.\n");
        }
    }
    return 0;
}
