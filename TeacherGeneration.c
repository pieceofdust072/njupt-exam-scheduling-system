#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// 内存中最多可加载的教师数量。
#define MAX_TEACHERS 1000

// 教师数据结构：与主系统里的教师结构保持一致（字段顺序与大小均须一致）。
typedef struct {
    char id[20];          // 教师编号
    char name[50];        // 教师姓名
    char dept[50];        // 所属院系
    char title[20];       // 职称分类（主系统将 "教授"/"Professor" 均视为教授）
    char specialReq[100]; // 特殊要求文本
    int taskCount;        // 已安排任务次数
} Teacher;

// 用于随机拼接姓名/部门/职称的词库。
char* firstNames[] = {"Zhang", "Wang", "Li", "Zhao", "Liu", "Chen", "Yang", "Zhou", "Wu", "Sun", "Xu", "Zhu", "Ma", "Hu", "Guo", "Lin", "He", "Gao", "Luo", "Zheng"};
char* middleNames[] = {"Wei", "Fang", "Na", "Min", "Jing", "Qiang", "Lei", "Jun", "Yang", "Yong", "Ming", "Hua", "Xin", "Yu", "Bo", "Kai", "Tian", "Zhi", "Guo", "Hai"};
char* lastNames[] = {"Wei", "Fang", "Na", "Min", "Jing", "Qiang", "Lei", "Jun", "Yang", "Yong", "Ming", "Hua", "Xin", "Yu", "Bo", "Kai", "Tian", "Zhi", "Guo", "Hai", "Ping", "An", "Sheng", "Cheng", "Tao", "Feng", "Qing", "Liang", "Dong", "Gang"};
char* depts[] = {"Computer", "Automation", "Electronic", "Mathematics", "Language"};
char* titleList[] = {"Professor", "Regular Teacher"};

// 生成 count 条教师数据并覆盖写入 teachers.dat。
void GenerateData(int count) {
    FILE* fp = fopen("teachers.dat", "wb");
    if (!fp) return;

    // 以当前时间做随机种子，避免每次生成相同数据。
    srand((unsigned int)time(NULL));
    for (int i = 0; i < count; i++) {
        Teacher t;
        sprintf(t.id, "T%04d", i + 1);
        sprintf(t.name, "%s %s %s",
                firstNames[rand() % (sizeof(firstNames) / sizeof(firstNames[0]))],
                middleNames[rand() % (sizeof(middleNames) / sizeof(middleNames[0]))],
                lastNames[rand() % (sizeof(lastNames) / sizeof(lastNames[0]))]);
        strcpy(t.dept, depts[rand() % (sizeof(depts) / sizeof(depts[0]))]);

        // 约 30% 概率为教授，70% 为普通教师。
        if ((rand() % 10) < 3) strcpy(t.title, titleList[0]);
        else strcpy(t.title, titleList[1]);

        strcpy(t.specialReq, "-"); // 默认无特殊要求
        t.taskCount = 0;           // 初始未安排考场
        fwrite(&t, sizeof(Teacher), 1, fp);
    }
    fclose(fp);
    printf("Successfully generated %d teachers.\n", count);
}

// 从 teachers.dat 读取教师数据到内存数组，返回实际读到的条数。
int LoadData(Teacher t[], int max) {
    FILE* fp = fopen("teachers.dat", "rb");
    if (!fp) return 0;
    int n = 0;
    while (n < max && fread(&t[n], sizeof(Teacher), 1, fp)) n++;
    fclose(fp);
    return n;
}

// 按表格格式打印教师信息，便于在命令行快速检查数据。
void Display(Teacher t[], int n) {
    printf("\n%-10s %-20s %-12s %-16s %-10s %-20s\n", "ID", "Name", "Department", "Title", "TaskCount", "SpecialReq");
    printf("------------------------------------------------------------------------------------------------\n");
    for (int i = 0; i < n; i++) {
        printf("%-10s %-20s %-12s %-16s %-10d %-20s\n", t[i].id, t[i].name, t[i].dept, t[i].title, t[i].taskCount, t[i].specialReq);
    }
}

// qsort 比较函数：按任务次数升序。
static int CompareByTaskCount(const void* a, const void* b) {
    const Teacher* ta = (const Teacher*)a;
    const Teacher* tb = (const Teacher*)b;
    return ta->taskCount - tb->taskCount;
}

// 按任务次数升序排序。
void SortByCount(Teacher t[], int n) {
    qsort(t, n, sizeof(Teacher), CompareByTaskCount);
}

// 将当前内存中的教师数组整体保存回文件。
void Save(Teacher t[], int n) {
    FILE* fp = fopen("teachers.dat", "wb");
    if (!fp) return;
    fwrite(t, sizeof(Teacher), n, fp);
    fclose(fp);
    printf("Data saved successfully.\n");
}

int main() {
    Teacher t[MAX_TEACHERS];
    int n = LoadData(t, MAX_TEACHERS);
    int choice;

    while (1) {
        printf("\n1.Generate 2.Display 3.Sort(Count) 8.Save 0.Exit\nChoose: ");
        if (scanf("%d", &choice) != 1) break;

        switch (choice) {
            case 1: {
                int num;
                printf("Enter count: ");
                scanf("%d", &num);
                GenerateData(num);
                n = LoadData(t, MAX_TEACHERS);
                break;
            }
            case 2: Display(t, n); break;
            case 3: SortByCount(t, n); Display(t, n); break;
            case 8: Save(t, n); break;
            case 0: return 0;
        }
    }
    return 0;
}
