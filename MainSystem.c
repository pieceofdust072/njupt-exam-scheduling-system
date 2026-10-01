#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>//画窗口和按钮
#include <commctrl.h>//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#ifdef _MSC_VER
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#endif

// --- 0. 常量定义 ---
#define MAX_TEACHERS       500   // 教师及按教师维度数组的上限
#define MAX_ROOMS          100   // 考场数组上限
#define MAX_SCHEDULES     1000   // 排班记录数组上限
#define MAX_DATES          120   // 日期池上限
#define MAX_SLOTS          300   // 时段池上限
#define MAX_ROOM_ISSUES     20   // 未排满考场问题清单上限
#define TOP_REPORT_COUNT     8   // 统计报告展示的前 N 名教师
#define LARGE_ROOM_THRESHOLD 50  // 容量达到该值视为大教室
#define LARGE_ROOM_NEED      6   // 大教室所需监考人数
#define SMALL_ROOM_NEED      3   // 小教室所需监考人数

// 判断职称是否为“教授”（兼容中文“教授”与英文“Professor”两种数据来源）。
int IsProfessor(const char* title) {
    return strcmp(title, "教授") == 0 || strcmp(title, "Professor") == 0;
}

// --- 1. 结构体定义 ---
typedef struct {
    char id[20];          // 教师编号
    char name[50];        // 教师姓名
    char subject[50];     // 所带专业/科目
    char title[20];       // 职称
    char specialReq[100]; // 特殊要求（日期/请假等）
    char classes[40];     // 所带班级（分号分隔，如 "1班;2班"；"-" 表示未指定）
    int taskCount;        // 已安排监考次数
} Teacher;

typedef struct {
    char id[20];      // 旧版教师编号
    char name[50];    // 旧版教师姓名
    char subject[50];    // 旧版专业/科目
    char title[20];   // 旧版职称
    int taskCount;    // 旧版任务次数
} TeacherLegacy;

typedef struct {
    char id[20];           // 教师编号
    char lastModTime[30];  // 最近修改时间
    char operatorName[50]; // 最近操作人
} TeacherExt;

typedef struct {
    char id[30];         // 考场编号
    char name[50];       // 考场名称
    int capacity;        // 考场容量（大小）
    int usedSeats;       // 已使用座位数
    char status[20];     // 状态（可用/满额/维护）
    char date[9];        // 考试日期 YYYYMMDD
    char session[5];     // 场次（时段 HHMM）
    char createTime[20]; // 创建时间
    char subject[50];    // 对应考试科目
} ExamRoom;

typedef struct {
    char teacherName[50]; // 监考教师姓名
    char roomDetail[100]; // 考场详情
    char matchDetail[100];// 匹配信息
    
    // 【关键】新增两字段，永久保存在 schedules.dat 中
    char lastModTime[30]; // 最近修改时间
    char operatorName[50];// 最近操作人
} ScheduleEntry;//排班记录

typedef struct {
    char username[50]; // 用户名
    char password[50]; // 密码
} User;

// --- 2. 全局变量与句柄 ---
HWND hTeacherList, hScheduleList; // 教师列表与排班列表句柄
HWND hLogUser, hLogPass;          // 登录窗口输入框句柄
HWND hRegUser, hRegPass, hRegConf;// 注册窗口输入框句柄
HWND hEditID, hEditName, hEditSubject, hEditTitle; // 教师编辑窗口输入框句柄
HWND hEditSpecialReq; // 特殊要求输入框句柄
HWND hReqDatePicker, hReqReasonCombo; // 日期选择与原因下拉控件句柄
HWND hSearchTeacher; // 教师查询输入框句柄
HWND hSearchLabel, hHintStatic; // 查询标签与提示文本句柄（用于自适应布局）

HWND hEditSchedTeacher, hEditSchedRoomDetail, hEditSchedMatchDetail; // 排班编辑窗口输入框句柄
ScheduleEntry editingSchedule; // 当前正在编辑的排班记录
int editingScheduleIndex = -1; // 当前编辑记录在数据中的下标

BOOL isLoggedIn = FALSE; // 登录状态标识
char currentUser[50] = ""; // 当前登录用户名
Teacher editingTeacher;   // 当前正在编辑的教师记录
char* gReportText = NULL; // 报告文本缓存指针

// --- 3. 业务逻辑函数 ---

// 校验登录账号：在 users.dat 中逐条比对用户名和密码。
int ValidateUser(const char* username, const char* password) {
    FILE* fp = fopen("users.dat", "rb");
    if (!fp) return 0;

    User u; // 文件中读取的用户记录
    while (fread(&u, sizeof(User), 1, fp) == 1) {
        if (strcmp(u.username, username) == 0 && strcmp(u.password, password) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

int UserExists(const char* username) {
    // 检查账号是否已存在，避免注册重名用户。
    FILE* fp = fopen("users.dat", "rb");
    if (!fp) return 0;

    User u; // 文件中读取的用户记录
    while (fread(&u, sizeof(User), 1, fp) == 1) {
        if (strcmp(u.username, username) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

int AddUser(const char* username, const char* password) {
    // 追加写入新用户记录到 users.dat。
    FILE* fp = fopen("users.dat", "ab");
    if (!fp) return 0;

    User u; // 待写入的新用户记录
    memset(&u, 0, sizeof(u));
    strncpy(u.username, username, sizeof(u.username) - 1);
    strncpy(u.password, password, sizeof(u.password) - 1);

    int ok = (fwrite(&u, sizeof(User), 1, fp) == 1);
    fclose(fp);
    return ok;
}

int ValidatePasswordComplexity(const char* password, char* err, int errSize) {
    // 密码强度校验：长度、空白字符、大小写、数字、特殊符号。
    int hasUpper = 0, hasLower = 0, hasDigit = 0, hasSpecial = 0; // 四类字符命中标记
    int len; // 密码长度

    if (!password) {
        if (err && errSize > 0) snprintf(err, errSize, "密码不能为空。");
        return 0;
    }

    len = (int)strlen(password);
    if (len < 8) {
        if (err && errSize > 0) snprintf(err, errSize, "密码长度至少 8 位。");
        return 0;
    }

    for (int i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)password[i]; // 当前字符（按无符号处理）
        if (isspace(ch)) {
            if (err && errSize > 0) snprintf(err, errSize, "密码不能包含空格。\n请使用字母、数字和符号组合。");
            return 0;
        }
        if (isupper(ch)) hasUpper = 1;
        else if (islower(ch)) hasLower = 1;
        else if (isdigit(ch)) hasDigit = 1;
        else hasSpecial = 1;
    }

    if (!hasUpper || !hasLower || !hasDigit || !hasSpecial) {
        if (err && errSize > 0) {
            snprintf(err, errSize,
                     "密码复杂度不足。\n"
                     "需同时包含：\n"
                     "1. 大写字母\n"
                     "2. 小写字母\n"
                     "3. 数字\n"
                     "4. 特殊字符（如 !@#$%%）");
        }
        return 0;
    }

    return 1;
}

// 前置声明：将日期 token 规范为 YYYYMMDD（8 位）格式。
int NormalizeDateToken(const char* token, char out[9]);
// 前置声明：从 schedules.dat 读取全部排班记录到数组。
int LoadAllSchedules(ScheduleEntry* arr, int maxCount);

// 去掉字符串首尾空白字符，便于后续做稳定匹配。
void TrimInPlace(char* s) {
    int start = 0; // 首个非空白字符位置
    int end;       // 末尾非空白字符位置
    int len;       // 原字符串长度

    if (!s) return;
    len = (int)strlen(s);
    while (start < len && isspace((unsigned char)s[start])) start++;

    end = len - 1;
    while (end >= start && isspace((unsigned char)s[end])) end--;

    if (start > 0) memmove(s, s + start, end - start + 1);
    s[end - start + 1] = '\0';
}

int IsSpecialKeyword(const char* token) {
    // 判断是否为“不可监考”类关键字（中英文都支持）。
    return (strcmp(token, "请假") == 0 || strcmp(token, "出差") == 0 ||
            strcmp(token, "生病") == 0 || strcmp(token, "不可监考") == 0 ||
            strcmp(token, "leave") == 0 || strcmp(token, "trip") == 0 ||
            strcmp(token, "sick") == 0 || strcmp(token, "unavailable") == 0);
}

int ParseDateReasonToken(const char* token, char normalizedDate[9], char* reasonOut, int reasonOutSize) {
    // 解析形如“YYYY-MM-DD(请假)”的组合限制，并输出标准日期。
    const char* lp;            // 左括号位置
    const char* rp;            // 右括号位置
    char datePart[20] = {0};   // 日期片段
    char reasonPart[40] = {0}; // 原因片段

    if (!token) return 0;
    lp = strchr(token, '(');
    rp = strrchr(token, ')');
    if (!lp || !rp || lp >= rp || rp[1] != '\0') return 0;

    if ((int)(lp - token) >= (int)sizeof(datePart)) return 0;
    strncpy(datePart, token, lp - token);
    datePart[lp - token] = '\0';
    strncpy(reasonPart, lp + 1, rp - lp - 1);
    reasonPart[rp - lp - 1] = '\0';

    TrimInPlace(datePart);
    TrimInPlace(reasonPart);
    if (reasonPart[0] == '\0') return 0;

    if (!NormalizeDateToken(datePart, normalizedDate)) return 0;
    if (!(strcmp(reasonPart, "仅日期限制") == 0 || IsSpecialKeyword(reasonPart))) return 0;

    if (reasonOut && reasonOutSize > 0) {
        strncpy(reasonOut, reasonPart, reasonOutSize - 1);
        reasonOut[reasonOutSize - 1] = '\0';
    }
    return 1;
}

int HasSpecialReqToken(const char* req, const char* token) {
    // 在分号分隔的特殊要求列表中查重。
    char buf[220]; // 可修改副本（strtok 需要）
    char* p;       // 分词游标

    if (!req || !token || token[0] == '\0') return 0;
    if (strcmp(req, "-") == 0) return 0;

    strncpy(buf, req, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    p = strtok(buf, ";");
    while (p) {
        char part[120]; // 当前分词项
        strncpy(part, p, sizeof(part) - 1);
        part[sizeof(part) - 1] = '\0';
        TrimInPlace(part);
        if (strcmp(part, token) == 0) return 1;
        p = strtok(NULL, ";");
    }
    return 0;
}

void AppendSpecialReqToken(char* req, int reqSize, const char* token) {
    // 将新限制项追加到特殊要求字符串，自动去重并保护长度。
    if (!req || !token || token[0] == '\0' || reqSize <= 0) return;
    if (HasSpecialReqToken(req, token)) return;

    if (req[0] == '\0' || strcmp(req, "-") == 0) {
        strncpy(req, token, reqSize - 1);
        req[reqSize - 1] = '\0';
        return;
    }

    if ((int)strlen(req) + 1 + (int)strlen(token) >= reqSize) return;
    strcat(req, ";");
    strcat(req, token);
}

int NormalizeDateToken(const char* token, char out[9]) {
    // 统一日期格式：支持 YYYYMMDD / YYYY-MM-DD，输出为 YYYYMMDD。
    if (!token || !out) return 0;

    if (strlen(token) == 8) {
        for (int i = 0; i < 8; i++) {
            if (!isdigit((unsigned char)token[i])) return 0;
        }
        strcpy(out, token);
        return 1;
    }

    if (strlen(token) == 10 && token[4] == '-' && token[7] == '-') {
        if (!isdigit((unsigned char)token[0]) || !isdigit((unsigned char)token[1]) ||
            !isdigit((unsigned char)token[2]) || !isdigit((unsigned char)token[3]) ||
            !isdigit((unsigned char)token[5]) || !isdigit((unsigned char)token[6]) ||
            !isdigit((unsigned char)token[8]) || !isdigit((unsigned char)token[9])) {
            return 0;
        }
        sprintf(out, "%.4s%.2s%.2s", token, token + 5, token + 8);
        return 1;
    }

    return 0;
}

// req: 待校验的特殊要求字符串；errorMsg: 校验失败时输出错误信息；errorMsgSize: errorMsg 缓冲区大小。
int ValidateSpecialReqFormat(const char* req, char* errorMsg, int errorMsgSize) {
    // 完整校验特殊要求文本是否符合系统允许的格式。
    char buf[220]; // req 的可修改副本
    char* token;   // 分号分割后的当前项

    if (!req || req[0] == '\0' || strcmp(req, "-") == 0) return 1;

    strncpy(buf, req, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    token = strtok(buf, ";");
    while (token) {
        char part[120];    // 当前限制项
        char normalized[9];// 归一化日期缓存

        strncpy(part, token, sizeof(part) - 1);
        part[sizeof(part) - 1] = '\0';
        TrimInPlace(part);

        if (part[0] == '\0') {
            if (errorMsg && errorMsgSize > 0) {
                snprintf(errorMsg, errorMsgSize, "特殊要求格式错误：存在空项目（连续分号）。");
            }
            return 0;
        }

        if (!IsSpecialKeyword(part) && !NormalizeDateToken(part, normalized) &&
            !ParseDateReasonToken(part, normalized, NULL, 0)) {
            if (errorMsg && errorMsgSize > 0) {
                snprintf(errorMsg, errorMsgSize,
                         "特殊要求格式错误：%s。\n允许：-、YYYYMMDD、YYYY-MM-DD、请假/出差/生病/不可监考（可用分号分隔）",
                         part);
            }
            return 0;
        }

        token = strtok(NULL, ";");
    }

    return 1;
}

int LoadAllTeachers(Teacher* arr, int maxCount) {
    // 读取全部教师，兼容旧版数据结构并自动补默认值。
    FILE* fp = fopen("teachers.dat", "rb");
    if (!fp) return 0;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    long fileSize = ftell(fp); // 文件总字节数
    if (fileSize < 0) {
        fclose(fp);
        return 0;
    }
    rewind(fp);

    int cnt = 0; // 已加载记录数
    if (fileSize % (long)sizeof(Teacher) == 0) {
        while (cnt < maxCount && fread(&arr[cnt], sizeof(Teacher), 1, fp) == 1) {
            if (arr[cnt].specialReq[0] == '\0') strcpy(arr[cnt].specialReq, "-");
            cnt++;
        }
    } else if (fileSize % (long)sizeof(TeacherLegacy) == 0) {
        TeacherLegacy oldT; // 旧版记录临时变量
        while (cnt < maxCount && fread(&oldT, sizeof(TeacherLegacy), 1, fp) == 1) {
            memset(&arr[cnt], 0, sizeof(Teacher));
            strcpy(arr[cnt].id, oldT.id);
            strcpy(arr[cnt].name, oldT.name);
            strcpy(arr[cnt].subject, oldT.subject);
            strcpy(arr[cnt].title, oldT.title);
            strcpy(arr[cnt].specialReq, "-");
            arr[cnt].taskCount = oldT.taskCount;
            cnt++;
        }
    }

    fclose(fp);
    return cnt;
}

void SaveAllTeachers(Teacher* arr, int count) {
    // 将教师数组整体覆盖写回 teachers.dat。
    FILE* fp = fopen("teachers.dat", "wb");
    if (!fp) return;
    fwrite(arr, sizeof(Teacher), count, fp);
    fclose(fp);
}

int IsTeacherUnavailableForRoom(const Teacher* t, const ExamRoom* room) {
    // 根据教师特殊要求与考场日期判断该教师是否不可用。
    char compactDate[9] = {0};   // 从考场 ID 提取出的 YYYYMMDD
    char reqBuf[220];            // 特殊要求副本（用于 strtok）
    const char* req = t->specialReq; // 教师特殊要求原文
    char* token;                 // 当前分词项

    if (!req || req[0] == '\0' || strcmp(req, "-") == 0) return 0;
    strcpy(compactDate, room->date);
    if (compactDate[0] == '\0') return 0;

    strncpy(reqBuf, req, sizeof(reqBuf) - 1);
    reqBuf[sizeof(reqBuf) - 1] = '\0';

    token = strtok(reqBuf, ";");
    while (token) {
        char part[120];          // 当前限制项
        char normalized[9] = {0};// 归一化日期

        strncpy(part, token, sizeof(part) - 1);
        part[sizeof(part) - 1] = '\0';
        TrimInPlace(part);

        if (IsSpecialKeyword(part)) {
            return 1;
        }
        if (NormalizeDateToken(part, normalized) && strcmp(normalized, compactDate) == 0) {
            return 1;
        }
        if (ParseDateReasonToken(part, normalized, NULL, 0) && strcmp(normalized, compactDate) == 0) {
            return 1;
        }

        token = strtok(NULL, ";");
    }

    return 0;
}

int GetSlotIndex(char slots[][13], int* slotCount, const char* slotStr) {
    // 在时段池中查找或新增时段，返回对应下标。
    for (int i = 0; i < *slotCount; i++) {
        if (strcmp(slots[i], slotStr) == 0) return i;
    }
    if (*slotCount >= MAX_SLOTS) return -1;
    strcpy(slots[*slotCount], slotStr);
    (*slotCount)++;
    return *slotCount - 1;
}

int CountSchedulesForTeacher(const char* teacherName) {
    // 直接从文件流式统计某位教师的排班条数，避免一次性载入全部记录。
    FILE* fp = fopen("schedules.dat", "rb");
    ScheduleEntry entry;
    int count = 0;

    if (!fp) return 0;
    while (fread(&entry, sizeof(ScheduleEntry), 1, fp) == 1) {
        if (strcmp(entry.teacherName, teacherName) == 0) count++;
    }
    fclose(fp);
    return count;
}

void BuildNoScheduleReason(const char* keyword, char* out, int outSize) {
    // 当查询无排班结果时，分析并生成可读原因说明。
    Teacher* teachers = (Teacher*)malloc(MAX_TEACHERS * sizeof(Teacher)); // 教师缓存
    ExamRoom* rooms = (ExamRoom*)malloc(MAX_ROOMS * sizeof(ExamRoom)); // 考场缓存
    int tCount = 0;   // 教师总数
    int rmCount = 0;  // 考场总数
    int target = -1;  // 命中的教师下标

    if (!out || outSize <= 0) return;
    out[0] = '\0';

    if (!teachers || !rooms) {
        if (teachers) free(teachers);
        if (rooms) free(rooms);
        snprintf(out, outSize, "无法分析原因：内存不足。");
        return;
    }

    tCount = LoadAllTeachers(teachers, MAX_TEACHERS);
    for (int i = 0; i < tCount; i++) {
        if (strcmp(teachers[i].name, keyword) == 0) {
            target = i;
            break;
        }
    }
    if (target == -1) {
        for (int i = 0; i < tCount; i++) {
            if (strstr(teachers[i].name, keyword) != NULL) {
                target = i;
                break;
            }
        }
    }

    if (target == -1) {
        snprintf(out, outSize, "未找到该教师，可能姓名输入不完整或不存在。\n建议输入完整姓名后重试。");
        free(teachers);
        free(rooms);
        return;
    }

    {
        int assignedCount = CountSchedulesForTeacher(teachers[target].name); // 该教师历史安排数
        if (assignedCount > 0) {
            snprintf(out, outSize,
                     "教师 %s 已有 %d 条监考安排。\n当前查询无结果可能是姓名关键字不匹配，请输入完整姓名。",
                     teachers[target].name, assignedCount);
            free(teachers);
            free(rooms);
            return;
        }
    }

    {
        FILE* fr = fopen("exam_rooms.dat", "rb");
        int matchSubjectRooms = 0; // 科目匹配考场数
        int availableRooms = 0;    // 与特殊要求不冲突的考场数
        if (fr) {
            while (rmCount < MAX_ROOMS && fread(&rooms[rmCount], sizeof(ExamRoom), 1, fr) == 1) rmCount++;
            fclose(fr);
        }

        for (int i = 0; i < rmCount; i++) {
            if (strcmp(teachers[target].subject, rooms[i].subject) == 0) matchSubjectRooms++;
            if (!IsTeacherUnavailableForRoom(&teachers[target], &rooms[i])) availableRooms++;
        }

        if (strstr(teachers[target].specialReq, "请假") || strstr(teachers[target].specialReq, "出差") ||
            strstr(teachers[target].specialReq, "生病") || strstr(teachers[target].specialReq, "不可监考")) {
            snprintf(out, outSize,
                     "教师 %s 本轮未排班，主要原因：特殊要求为“%s”。",
                     teachers[target].name,
                     teachers[target].specialReq[0] ? teachers[target].specialReq : "-");
        } else if (availableRooms == 0 && rmCount > 0) {
            snprintf(out, outSize,
                     "教师 %s 本轮未排班，主要原因：特殊要求与所有考场时间冲突（%s）。",
                     teachers[target].name,
                     teachers[target].specialReq[0] ? teachers[target].specialReq : "-");
        } else if (IsProfessor(teachers[target].title) && matchSubjectRooms == 0) {
            snprintf(out, outSize,
                     "教师 %s 本轮未排班，主要原因：教授仅限本专业监考，但当前无本专业考场。",
                     teachers[target].name);
        } else {
            snprintf(out, outSize,
                     "教师 %s 本轮未排班，可能原因：人数限制下优先分配了更匹配或任务更均衡的教师。\n"
                     "可尝试放宽条件或增加考场/教师后重排。",
                     teachers[target].name);
        }
    }

    free(teachers);
    free(rooms);
}

int GetDateIndex(char dates[][9], int* dateCount, const char* dateStr) {
    // 在日期池中查找或新增日期，返回下标。
    for (int i = 0; i < *dateCount; i++) {
        if (strcmp(dates[i], dateStr) == 0) return i;
    }
    if (*dateCount >= MAX_DATES) return -1;
    strcpy(dates[*dateCount], dateStr);
    (*dateCount)++;
    return *dateCount - 1;
}

int ExtractDateFromRoomDetail(const char* roomDetail, char outDate[9]) {
    // 从“考场详情”文本里解析出 ER 编号中的日期。
    const char* p;
    if (!roomDetail || !outDate) return 0;

    p = strstr(roomDetail, "ER-");
    if (!p || strlen(p) < 11) return 0;

    p += 3;
    for (int i = 0; i < 8; i++) {
        if (!isdigit((unsigned char)p[i])) return 0;
        outDate[i] = p[i];
    }
    outDate[8] = '\0';
    return 1;
}

LRESULT CALLBACK ReportWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 报告窗口过程：负责创建列表、响应尺寸变化和资源释放。
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        char* text = (char*)cs->lpCreateParams;
        HWND hList = CreateWindowEx(WS_EX_CLIENTEDGE, "LISTBOX", "",
                                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                    LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
                                    10, 10, 760, 500, hWnd, NULL, cs->hInstance, NULL);
        HFONT hMono = CreateFont(
            -16, 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE,
            GB2312_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FIXED_PITCH | FF_MODERN,
            "Consolas");

        if (!hMono) {
            hMono = (HFONT)GetStockObject(ANSI_FIXED_FONT);
        }
        SendMessage(hList, WM_SETFONT, (WPARAM)hMono, TRUE);

        if (text && text[0]) {
            char* cursor = text;
            while (*cursor) {
                char line[1024] = {0};
                int i = 0;
                while (*cursor && *cursor != '\n' && i < (int)sizeof(line) - 1) {
                    if (*cursor != '\r') line[i++] = *cursor;
                    cursor++;
                }
                line[i] = '\0';
                SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)line);
                if (*cursor == '\n') cursor++;
            }
        }

        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)text);
        return 0;
    }
    case WM_SIZE: {
        HWND hList = GetWindow(hWnd, GW_CHILD);
        if (hList) {
            MoveWindow(hList, 10, 10, LOWORD(lParam) - 20, HIWORD(lParam) - 20, TRUE);
        }
        return 0;
    }
    case WM_DESTROY: {
        HWND hList = GetWindow(hWnd, GW_CHILD);
        HFONT hFont = NULL;
        if (hList) {
            hFont = (HFONT)SendMessage(hList, WM_GETFONT, 0, 0);
            if (hFont && hFont != (HFONT)GetStockObject(ANSI_FIXED_FONT)) {
                DeleteObject(hFont);
            }
        }
        char* text = (char*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        if (text) free(text);
        return 0;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void ShowReportWindow(HWND owner, const char* title, const char* text) {
    // 创建并显示报告窗口，展示多行统计/分析结果。
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(owner, GWLP_HINSTANCE);
    char* copy = NULL;
    HWND hWnd;

    if (!text) return;
    copy = (char*)malloc(strlen(text) + 1);
    if (!copy) {
        MessageBox(owner, "内存不足，无法打开报告窗口。", "错误", MB_OK | MB_ICONERROR);
        return;
    }
    strcpy(copy, text);

    hWnd = CreateWindow("ReportClass", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                        220, 140, 800, 600, owner, NULL, hInst, copy);
    if (!hWnd) {
        free(copy);
        MessageBox(owner, "报告窗口创建失败。", "错误", MB_OK | MB_ICONERROR);
    }
}

int DateToDayIndex(const char* yyyymmdd) {
    // 将日期字符串转换为“天序号”，用于连续性计算。
    struct tm t;          // 时间结构体
    char year[5] = {0};   // 年字符串
    char mon[3] = {0};    // 月字符串
    char day[3] = {0};    // 日字符串
    time_t ts;            // mktime 返回时间戳

    if (!yyyymmdd || strlen(yyyymmdd) != 8) return -1;
    memset(&t, 0, sizeof(t));

    strncpy(year, yyyymmdd, 4);
    strncpy(mon, yyyymmdd + 4, 2);
    strncpy(day, yyyymmdd + 6, 2);

    t.tm_year = atoi(year) - 1900;
    t.tm_mon = atoi(mon) - 1;
    t.tm_mday = atoi(day);
    t.tm_isdst = -1;

    ts = mktime(&t);
    if (ts == (time_t)-1) return -1;
    return (int)(ts / 86400);
}

void ShowScheduleQualityStats(HWND owner) {
    // 汇总排班质量：跨天次数、连续天数、单日峰值等统计。
    ScheduleEntry* arr = (ScheduleEntry*)malloc(MAX_SCHEDULES * sizeof(ScheduleEntry)); // 排班数据缓存
    char teacherNames[MAX_TEACHERS][50] = {{0}}; // 教师名池
    int dayKeys[MAX_TEACHERS][MAX_DATES] = {{0}};      // 每位教师的日期键集合
    int dayLoads[MAX_TEACHERS][MAX_DATES] = {{0}};     // 每位教师每日期的场次数
    int dayCnt[MAX_TEACHERS] = {0};              // 每位教师涉及日期数量
    int teacherCnt = 0;                 // 参与统计教师数
    int total = 0;                      // 排班总记录数
    int unknownDateRows = 0;            // 日期无法解析记录数

    if (!arr) {
        MessageBox(owner, "内存不足，无法统计。", "错误", MB_OK | MB_ICONERROR);
        return;
    }

    total = LoadAllSchedules(arr, MAX_SCHEDULES);
    if (total <= 0) {
        free(arr);
        MessageBox(owner, "当前没有排班数据可统计。", "提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    for (int i = 0; i < total; i++) {
        int ti = -1;
        int di = -1;
        char dateStr[9] = {0};
        int dayKey = -1;

        for (int t = 0; t < teacherCnt; t++) {
            if (strcmp(teacherNames[t], arr[i].teacherName) == 0) {
                ti = t;
                break;
            }
        }
        if (ti == -1 && teacherCnt < MAX_TEACHERS) {
            ti = teacherCnt;
            // 复制教师名并预留 1 字节给字符串结束符，避免越界。
            strncpy(teacherNames[teacherCnt], arr[i].teacherName, sizeof(teacherNames[teacherCnt]) - 1);
            teacherCnt++;
        }
        if (ti == -1) continue;

        if (!ExtractDateFromRoomDetail(arr[i].roomDetail, dateStr)) {
            unknownDateRows++;
            continue;
        }

        dayKey = DateToDayIndex(dateStr);
        if (dayKey < 0) {
            unknownDateRows++;
            continue;
        }

        for (int d = 0; d < dayCnt[ti]; d++) {
            if (dayKeys[ti][d] == dayKey) {
                di = d;
                break;
            }
        }
        if (di == -1 && dayCnt[ti] < MAX_DATES) {
            di = dayCnt[ti];
            dayKeys[ti][di] = dayKey;
            dayLoads[ti][di] = 0;
            dayCnt[ti]++;
        }
        if (di != -1) {
            dayLoads[ti][di]++;
        }
    }

    {
        int crossDayMax = 0;      // 最大跨天数
        int crossDayMaxIdx = -1;  // 最大跨天对应教师索引
        double avgDays = 0.0;     // 平均跨天数
        int rank[MAX_TEACHERS];            // 排序用教师索引数组
        int rankN = 0;            // 可参与排名教师数
        char msg[4096];           // 统计报告文本缓冲
        int used = 0;             // 已写入 msg 的长度

        msg[0] = '\0';
        for (int i = 0; i < teacherCnt; i++) {
            if (dayCnt[i] > 0) {
                avgDays += dayCnt[i];
                rank[rankN++] = i;
                if (dayCnt[i] > crossDayMax) {
                    crossDayMax = dayCnt[i];
                    crossDayMaxIdx = i;
                }
            }
        }
        if (rankN > 0) avgDays /= rankN;

        for (int i = 0; i < rankN - 1; i++) {
            for (int j = 0; j < rankN - i - 1; j++) {
                int a = rank[j], b = rank[j + 1];
                if (dayCnt[a] < dayCnt[b]) {
                    int t = rank[j];
                    rank[j] = rank[j + 1];
                    rank[j + 1] = t;
                }
            }
        }

        used += snprintf(msg + used, sizeof(msg) - used,
                         "排班质量统计\n\n总排班记录: %d\n参与教师数: %d\n平均跨天数: %.2f\n",
                         total, rankN, avgDays);
        if (crossDayMaxIdx >= 0) {
            used += snprintf(msg + used, sizeof(msg) - used,
                             "跨天最多教师: %s (%d 天)\n",
                             teacherNames[crossDayMaxIdx], crossDayMax);
        }
        used += snprintf(msg + used, sizeof(msg) - used,
                         "无法解析日期记录: %d\n\n",
                         unknownDateRows);

        used += snprintf(msg + used, sizeof(msg) - used,
                         "前 %d 位教师连续性详情:\n", TOP_REPORT_COUNT);

        for (int k = 0; k < rankN && k < TOP_REPORT_COUNT; k++) {
            int i = rank[k];
            int maxSingleDay = 0;   // 单日最大场次
            int maxConsecutive = 1; // 最大连续天数
            int curConsecutive = 1; // 当前连续天计数

            for (int d = 0; d < dayCnt[i] - 1; d++) {
                for (int e = 0; e < dayCnt[i] - d - 1; e++) {
                    if (dayKeys[i][e] > dayKeys[i][e + 1]) {
                        int tk = dayKeys[i][e];
                        int tl = dayLoads[i][e];
                        dayKeys[i][e] = dayKeys[i][e + 1];
                        dayLoads[i][e] = dayLoads[i][e + 1];
                        dayKeys[i][e + 1] = tk;
                        dayLoads[i][e + 1] = tl;
                    }
                }
            }

            for (int d = 0; d < dayCnt[i]; d++) {
                if (dayLoads[i][d] > maxSingleDay) maxSingleDay = dayLoads[i][d];
                if (d > 0) {
                    if (dayKeys[i][d] == dayKeys[i][d - 1] + 1) {
                        curConsecutive++;
                        if (curConsecutive > maxConsecutive) maxConsecutive = curConsecutive;
                    } else {
                        curConsecutive = 1;
                    }
                }
            }

            used += snprintf(msg + used, sizeof(msg) - used,
                             "%d. 教师: %s\n"
                             "   跨天数: %d\n"
                             "   最大连续天: %d\n"
                             "   最大单日场次: %d\n\n",
                             k + 1, teacherNames[i], dayCnt[i], maxConsecutive, maxSingleDay);
            if (used >= (int)sizeof(msg) - 200) break;
        }

        ShowReportWindow(owner, "排班质量统计报告", msg);
    }

    free(arr);
}

int HasOtherDateAssignments(const int loads[MAX_DATES], int dateCount, int curDateIdx) {
    // 判断教师在当前日期以外是否还有排班。
    for (int i = 0; i < dateCount; i++) {
        if (i != curDateIdx && loads[i] > 0) return 1;
    }
    return 0;
}

int CompareTeacherPriority(const Teacher* left, const Teacher* right,
                          int leftIdx, int rightIdx,
                          const ExamRoom* room,
                          int curDateIdx,
                          const int dateLoads[MAX_TEACHERS][MAX_DATES],
                          int dateCount,
                          const int selected[MAX_TEACHERS]) {
    // 候选教师优先级比较函数：返回值 < 0 表示 left 更优先。
    // 该函数用于同一考场内的“巡考位选择 + 普通监考补位排序”，核心目标：
    // 1) 保证同一考场不重复选同一教师；
    // 2) 尽量把教师任务聚合到同一天，减少跨日分散；
    // 3) 保留专业匹配倾向，同时兼顾任务总量均衡。
    //
    // 比较顺序（从高到低）：
    // A. selected：未被本考场选中的教师优先（避免同一考场重复）；
    // B. leftSame/rightSame：当前日期已有任务更多者优先（鼓励“同日连排”）；
    // C. leftOther/rightOther：跨其他日期更少者优先（降低跨天）；
    // D. leftMatch/rightMatch：专业匹配者优先；
    // E. leftEff/rightEff：有效负载更低者优先（均衡总量，教授带额外权重）；
    // F. leftIsProf/rightIsProf：非教授优先（在同等条件下尽量保留教授资源）；
    // G. id 字典序：最终稳定排序兜底，保证结果可重复。
    int leftSame = dateLoads[leftIdx][curDateIdx];  // left 在当前日期已排次数
    int rightSame = dateLoads[rightIdx][curDateIdx];// right 在当前日期已排次数
    int leftOther = HasOtherDateAssignments(dateLoads[leftIdx], dateCount, curDateIdx);   // left 是否跨其他日期
    int rightOther = HasOtherDateAssignments(dateLoads[rightIdx], dateCount, curDateIdx); // right 是否跨其他日期
    int leftMatch = (strcmp(left->subject, room->subject) == 0);   // left 是否专业匹配
    int rightMatch = (strcmp(right->subject, room->subject) == 0); // right 是否专业匹配
    int leftIsProf = IsProfessor(left->title);   // left 是否教授
    int rightIsProf = IsProfessor(right->title); // right 是否教授
    int leftEff = left->taskCount + (leftIsProf ? 1 : 0);       // left 有效负载
    int rightEff = right->taskCount + (rightIsProf ? 1 : 0);    // right 有效负载

    if (selected[leftIdx] != selected[rightIdx]) {
        return selected[leftIdx] - selected[rightIdx];
    }
    if (leftSame != rightSame) {
        return rightSame - leftSame;
    }
    if (leftOther != rightOther) {
        return leftOther - rightOther;
    }
    if (leftMatch != rightMatch) {
        return rightMatch - leftMatch;
    }
    if (leftEff != rightEff) {
        return leftEff - rightEff;
    }
    if (leftIsProf != rightIsProf) {
        return leftIsProf - rightIsProf;
    }
    return strcmp(left->id, right->id);
}

// 按考场编号排序，保证排班顺序稳定可复现。
static int CompareRoomById(const void* a, const void* b) {
    const ExamRoom* ra = (const ExamRoom*)a;
    const ExamRoom* rb = (const ExamRoom*)b;
    return strcmp(ra->id, rb->id);
}

// 按考场详情排序（ER-日期-时间前缀天然按时间先后排列）。
static int CompareScheduleByRoom(const void* a, const void* b) {
    const ScheduleEntry* sa = (const ScheduleEntry*)a;
    const ScheduleEntry* sb = (const ScheduleEntry*)b;
    return strcmp(sa->roomDetail, sb->roomDetail);
}

// qsort 上下文：候选教师优先级比较依赖当前考场、日期负载与已选标记。
static const Teacher* g_sortTeachers;
static const ExamRoom* g_sortRoom;
static int g_sortDateIdx;
static int g_sortDateCount;
static const int (*g_sortDateLoads)[MAX_DATES];
static const int* g_sortSelected;

// qsort 适配器：将候选下标比较委托给 CompareTeacherPriority。
static int CompareCandidateIndex(const void* a, const void* b) {
    int li = *(const int*)a;
    int ri = *(const int*)b;
    return CompareTeacherPriority(&g_sortTeachers[li], &g_sortTeachers[ri], li, ri,
                                  g_sortRoom, g_sortDateIdx, g_sortDateLoads, g_sortDateCount, g_sortSelected);
}

void RefreshTeacherList() {
    // 刷新教师列表视图（基础信息 + 扩展审计信息）。
    ListView_DeleteAllItems(hTeacherList);
    
    TeacherExt* exts = (TeacherExt*)malloc(MAX_TEACHERS * sizeof(TeacherExt)); // 教师扩展信息缓存
    int extCount = 0; // 扩展信息记录数
    if (exts) {
        FILE* fext = fopen("teacher_ext.dat", "rb");
        if (fext) {
            while (extCount < MAX_TEACHERS && fread(&exts[extCount], sizeof(TeacherExt), 1, fext)) extCount++;
            fclose(fext);
        }
    }

    Teacher* teachers = (Teacher*)malloc(MAX_TEACHERS * sizeof(Teacher));
    if (!teachers) {
        if (exts) free(exts);
        return;
    }

    int tCount = LoadAllTeachers(teachers, MAX_TEACHERS);
    for (int i = 0; i < tCount; i++) {
        Teacher* t = &teachers[i];
        LVITEM lvi = { 0 }; // 列表项结构
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = t->id;
        int pos = ListView_InsertItem(hTeacherList, &lvi);
        
        ListView_SetItemText(hTeacherList, pos, 1, t->name);
        ListView_SetItemText(hTeacherList, pos, 2, t->subject);  
        ListView_SetItemText(hTeacherList, pos, 3, t->title);
        char buf[10]; sprintf(buf, "%d", t->taskCount);
        ListView_SetItemText(hTeacherList, pos, 4, buf);
        ListView_SetItemText(hTeacherList, pos, 7, t->specialReq[0] ? t->specialReq : "-");
        ListView_SetItemText(hTeacherList, pos, 8, t->classes[0] ? t->classes : "-");

        char* modTime = "-";
        char* opName = "-";
        for (int j = 0; j < extCount; j++) {
            if (strcmp(exts[j].id, t->id) == 0) {
                modTime = exts[j].lastModTime;
                opName = exts[j].operatorName;
                break;
            }
        }
        ListView_SetItemText(hTeacherList, pos, 5, modTime);
        ListView_SetItemText(hTeacherList, pos, 6, opName);
    }
    free(teachers);
    if (exts) free(exts);
}

int LoadAllSchedules(ScheduleEntry* arr, int maxCount) {
    // 读取全部排班记录。
    FILE* fp = fopen("schedules.dat", "rb");
    if (!fp) return 0;
    int cnt = 0;
    while (cnt < maxCount && fread(&arr[cnt], sizeof(ScheduleEntry), 1, fp)) cnt++;
    fclose(fp);
    return cnt;
}

void SaveAllSchedules(ScheduleEntry* arr, int count) {
    // 保存全部排班记录到 schedules.dat。
    FILE* fp = fopen("schedules.dat", "wb");
    if (fp) {
        fwrite(arr, sizeof(ScheduleEntry), count, fp);
        fclose(fp);
    }
}

int GetScheduleSourceIndexFromListItem(int listIndex) {
    // 从列表项反查原始数据下标（用于过滤后定位）。
    LVITEM lvi; // 列表查询结构
    memset(&lvi, 0, sizeof(lvi));
    lvi.mask = LVIF_PARAM;
    lvi.iItem = listIndex;
    if (ListView_GetItem(hScheduleList, &lvi)) {
        return (int)lvi.lParam;
    }
    return listIndex;
}

void PopulateScheduleList(ScheduleEntry* arr, int cnt, const char* teacherKeyword) {
    // 用数组数据重绘排班列表，可按教师关键字过滤。
    ListView_DeleteAllItems(hScheduleList);

    for (int i = 0; i < cnt; i++) {
        if (teacherKeyword && teacherKeyword[0]) {
            if (strstr(arr[i].teacherName, teacherKeyword) == NULL) {
                continue;
            }
        }

        LVITEM lvi; // 待插入列表项
        memset(&lvi, 0, sizeof(lvi));
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = ListView_GetItemCount(hScheduleList);
        lvi.pszText = arr[i].teacherName;
        lvi.lParam = i;

        int pos = ListView_InsertItem(hScheduleList, &lvi);
        ListView_SetItemText(hScheduleList, pos, 1, arr[i].roomDetail);
        ListView_SetItemText(hScheduleList, pos, 2, arr[i].matchDetail);
        ListView_SetItemText(hScheduleList, pos, 3, arr[i].lastModTime[0] ? arr[i].lastModTime : "-");
        ListView_SetItemText(hScheduleList, pos, 4, arr[i].operatorName[0] ? arr[i].operatorName : "-");
    }
}

void FilterScheduleListByTeacher(const char* teacherKeyword) {
    // 按教师姓名关键字筛选排班列表。
    ScheduleEntry* arr = (ScheduleEntry*)malloc(MAX_SCHEDULES * sizeof(ScheduleEntry));
    if (!arr) return;

    int cnt = LoadAllSchedules(arr, MAX_SCHEDULES);
    PopulateScheduleList(arr, cnt, teacherKeyword);
    free(arr);
}

void RefreshScheduleList() {
    // 刷新排班列表为“全部数据”。
    ScheduleEntry* arr = (ScheduleEntry*)malloc(MAX_SCHEDULES * sizeof(ScheduleEntry));
    if (!arr) return;
    
    int cnt = LoadAllSchedules(arr, MAX_SCHEDULES);

    PopulateScheduleList(arr, cnt, NULL);
    free(arr);
}

// 按教师姓名关键字，输出该老师的全部监考安排报告（按时间排序）。
int ShowTeacherScheduleReport(HWND owner, const char* keyword) {
    ScheduleEntry* arr = (ScheduleEntry*)malloc(MAX_SCHEDULES * sizeof(ScheduleEntry));
    if (!arr) return 0;

    int total = LoadAllSchedules(arr, MAX_SCHEDULES);

    ScheduleEntry* matches = (ScheduleEntry*)malloc(MAX_SCHEDULES * sizeof(ScheduleEntry));
    if (!matches) { free(arr); return 0; }
    int m = 0;
    for (int i = 0; i < total; i++) {
        if (strstr(arr[i].teacherName, keyword) != NULL) {
            matches[m++] = arr[i];
        }
    }
    free(arr);

    if (m == 0) { free(matches); return 0; }

    // 按考场详情排序，ER-日期-时间前缀天然按时间先后排列。
    qsort(matches, m, sizeof(ScheduleEntry), CompareScheduleByRoom);

    char msg[8192];
    int used = 0;
    used += snprintf(msg + used, sizeof(msg) - used,
                     "教师监考安排查询\n\n教师: %s\n共 %d 场监考\n"
                     "----------------------------------------\n",
                     matches[0].teacherName, m);
    for (int i = 0; i < m; i++) {
        used += snprintf(msg + used, sizeof(msg) - used,
                         "%d. 考场: %s\n   匹配: %s\n",
                         i + 1, matches[i].roomDetail, matches[i].matchDetail);
        if (used >= (int)sizeof(msg) - 256) break;
    }

    ShowReportWindow(owner, "教师监考安排查询报告", msg);
    free(matches);
    return m;
}

void SaveCurrentSchedule() {
    // 将当前界面列表内容回写到 schedules.dat。
    int total = ListView_GetItemCount(hScheduleList); // 列表总行数
    if (total == 0) {
        FILE* fp = fopen("schedules.dat", "wb");
        if (fp) fclose(fp);
        return;
    }
    
    ScheduleEntry* arr = (ScheduleEntry*)malloc(MAX_SCHEDULES * sizeof(ScheduleEntry));
    if (!arr) return;
    
    int cnt = 0;
    for (int i = 0; i < total && i < MAX_SCHEDULES; i++) {
        ListView_GetItemText(hScheduleList, i, 0, arr[cnt].teacherName, 50);
        ListView_GetItemText(hScheduleList, i, 1, arr[cnt].roomDetail, 100);
        ListView_GetItemText(hScheduleList, i, 2, arr[cnt].matchDetail, 100);
        ListView_GetItemText(hScheduleList, i, 3, arr[cnt].lastModTime, 30);
        ListView_GetItemText(hScheduleList, i, 4, arr[cnt].operatorName, 50);
        cnt++;
    }
    SaveAllSchedules(arr, cnt);
    free(arr);
}

void ExportScheduleToTxt() {
    // 导出当前排班结果到文本文件，便于提交或打印。
    FILE* fp = fopen("考场安排导出.txt", "w");
    if (!fp) {
        MessageBox(NULL, "无法创建导出文件！", "导出失败", MB_OK | MB_ICONERROR);
        return;
    }

    char exportTime[30] = {0}; // 导出时间字符串
    time_t now = time(NULL);   // 当前时间戳
    struct tm* ti = localtime(&now); // 本地时间结构
    if (ti) {
        strftime(exportTime, sizeof(exportTime), "%Y-%m-%d %H:%M:%S", ti);
    } else {
        strcpy(exportTime, "未知时间");
    }

    fprintf(fp, "=== 高校考场排班安排导出 ===\n");
    fprintf(fp, "导出时间：%s\n\n", exportTime);

    int total = ListView_GetItemCount(hScheduleList); // 列表总行数
    if (total == 0) {
        fprintf(fp, "当前没有排班安排信息。\n");
    } else {
        for (int i = 0; i < total; i++) {
            char teacher[50] = {0}, room[100] = {0}, match[100] = {0}; // 每行基础字段
            char modTime[30] = {0}, opName[50] = {0}; // 审计字段
            ListView_GetItemText(hScheduleList, i, 0, teacher, 50);
            ListView_GetItemText(hScheduleList, i, 1, room, 100);
            ListView_GetItemText(hScheduleList, i, 2, match, 100);
            ListView_GetItemText(hScheduleList, i, 3, modTime, 30);
            ListView_GetItemText(hScheduleList, i, 4, opName, 50);

            fprintf(fp, "【%d】监考老师: %s\n", i + 1, teacher);
            fprintf(fp, "考场详情: %s\n", room);
            fprintf(fp, "匹配情况: %s\n", match);
            fprintf(fp, "修改时间: %s\n", modTime[0] ? modTime : "-");
            fprintf(fp, "操作人: %s\n", opName[0] ? opName : "-");
            fprintf(fp, "----------------------------------------\n\n");
        }
    }
    fclose(fp);
    MessageBox(NULL, "导出成功！\n文件：考场安排导出.txt", "提示", MB_OK | MB_ICONINFORMATION);
}

// 智能排班
void AutoSchedule() {
    Teacher* allT = (Teacher*)malloc(MAX_TEACHERS * sizeof(Teacher)); // 教师池
    ExamRoom* rms = (ExamRoom*)malloc(MAX_ROOMS * sizeof(ExamRoom)); // 考场池
    if (!allT || !rms) {
        if (allT) free(allT);
        if (rms) free(rms);
        MessageBox(NULL, "内存分配失败！", "错误", MB_OK);
        return;
    }

    int aCount = 0, rmCount = 0; // 教师数量、考场数量

    aCount = LoadAllTeachers(allT, MAX_TEACHERS);
    if (aCount <= 0) {
        free(allT); free(rms);
        MessageBox(NULL, "教师数据不存在！", "错误", MB_OK); 
        return; 
    }

    FILE* fr = fopen("exam_rooms.dat", "rb");
    if (!fr) { 
        free(allT); free(rms);
        MessageBox(NULL, "考场数据不存在！", "错误", MB_OK); 
        return; 
    }
    while (rmCount < MAX_ROOMS && fread(&rms[rmCount], sizeof(ExamRoom), 1, fr)) rmCount++;
    fclose(fr);

    if (aCount == 0 || rmCount == 0) {
        free(allT); free(rms);
        return;
    }

    ListView_DeleteAllItems(hScheduleList);

    // 先按考场编号排序，保证排班顺序稳定可复现（便于问题排查与结果对比）。
    qsort(rms, rmCount, sizeof(ExamRoom), CompareRoomById);

    char datePool[MAX_DATES][9] = {{0}};     // 日期池（YYYYMMDD）
    int dateCount = 0;                 // 已记录日期数
    int dateLoads[MAX_TEACHERS][MAX_DATES] = {{0}};   // 每位教师在每日期的负载
    char slotPool[MAX_SLOTS][13] = {{0}};    // 时段池（YYYYMMDDHHMM）
    int slotCount = 0;                 // 已记录时段数
    int slotLoads[MAX_TEACHERS][MAX_SLOTS] = {{0}};   // 每位教师在每时段的负载
    int patrolAssigned = 0;            // 已分配巡考人次
    int totalNeedCount = 0;            // 总需求场次
    int totalAssignedCount = 0;        // 实际完成场次
    int unfilledRoomCount = 0;         // 未排满考场数量
    char roomIssueList[MAX_ROOM_ISSUES][220] = {{0}}; // 未满足考场问题清单
    int roomIssueCount = 0;            // 问题条目数
    int mandatoryDone[MAX_TEACHERS] = {0}; // 每位教师是否已担任过任课监考（“带多个班只监考一个班”）

    char currentTimeStr[30] = {0};     // 自动排班写入的修改时间
    time_t now = time(NULL);           // 当前时间戳
    struct tm* ti = localtime(&now);   // 当前本地时间
    char autoOperatorName[] = "自动排班"; // 自动排班操作人标记
    if (ti) {
        strftime(currentTimeStr, sizeof(currentTimeStr), "%Y-%m-%d %H:%M:%S", ti);
    }

    // 主循环：按考场逐一构建“候选教师池”，并分阶段完成填充。
    for (int i = 0; i < rmCount; i++) {
        int need = (rms[i].capacity >= LARGE_ROOM_THRESHOLD) ? LARGE_ROOM_NEED : SMALL_ROOM_NEED; // 当前考场需求人数
        int roomAssigned = 0;      // 当前考场已分配人数
        int selected[MAX_TEACHERS] = {0};   // 本考场已选教师标记
        int assigned = 0;          // 本考场已安排计数
        int curDateIdx = -1;       // 当前考场日期索引
        int curSlotIdx = -1;       // 当前考场时段索引
        char dateStr[9] = {0};     // 当前考场日期串
        char slotStr[13] = {0};    // 当前考场时段串
        int candidateIdx[MAX_TEACHERS];     // 候选教师索引列表
        int cCount = 0;            // 候选教师数量

        // 解析考场所属“日期 + 时段”，用于冲突控制与连续性优化。
        // - dateStr：YYYYMMDD（用于同日聚合策略）
        // - slotStr：YYYYMMDDHHMM（用于同一时段不可重复）
        strcpy(dateStr, rms[i].date);
        snprintf(slotStr, sizeof(slotStr), "%s%s", rms[i].date, rms[i].session);
        if (dateStr[0]) {
            curDateIdx = GetDateIndex(datePool, &dateCount, dateStr);
        }
        if (slotStr[0]) {
            curSlotIdx = GetSlotIndex(slotPool, &slotCount, slotStr);
        }

        // 第一步：筛候选。
        // 约束说明：
        // 1) 教授仅允许监考本专业考场（教授不参与普通教师的均衡约束）；
        // 2) 任意教师都必须满足特殊要求（请假/出差/日期冲突等）；
        // 3) 同一教师在同一时段最多只安排一场（slotLoads == 0）；
        // 4) 普通教师硬均衡：优先只选“当前监考次数最少”的普通教师，保证彼此相差不超过 1 次。

        // 求普通教师当前最小监考次数，作为本轮均衡基准。
        int minNonProf = INT_MAX;
        for (int j = 0; j < aCount; j++) {
            if (!IsProfessor(allT[j].title) && allT[j].taskCount < minNonProf) {
                minNonProf = allT[j].taskCount;
            }
        }
        if (minNonProf == INT_MAX) minNonProf = 0;

        for (int j = 0; j < aCount; j++) {
            BOOL isProf = IsProfessor(allT[j].title);
            BOOL avail = !IsTeacherUnavailableForRoom(&allT[j], &rms[i]) &&
                         (curSlotIdx < 0 || slotLoads[j][curSlotIdx] == 0);
            if (!avail) continue;

            if (isProf) {
                if (strcmp(allT[j].subject, rms[i].subject) == 0) {
                    candidateIdx[cCount++] = j;
                }
            } else if (allT[j].taskCount <= minNonProf) {
                // 严格均衡：只纳入处于最低负载的普通教师。
                candidateIdx[cCount++] = j;
            }
        }

        // 若最低负载教师不足以填满本考场，放宽一级（允许 minNonProf+1），避免无谓空缺。
        if (cCount < need) {
            for (int j = 0; j < aCount; j++) {
                if (IsProfessor(allT[j].title)) continue;
                BOOL dup = FALSE;
                for (int k = 0; k < cCount; k++) {
                    if (candidateIdx[k] == j) { dup = TRUE; break; }
                }
                if (dup) continue;
                if (!IsTeacherUnavailableForRoom(&allT[j], &rms[i]) &&
                    (curSlotIdx < 0 || slotLoads[j][curSlotIdx] == 0) &&
                    allT[j].taskCount <= minNonProf + 1) {
                    candidateIdx[cCount++] = j;
                }
            }
        }

        if (cCount == 0 && roomIssueCount < MAX_ROOM_ISSUES) {
            sprintf(roomIssueList[roomIssueCount++],
                    "[%s] %s\n需求场次: %d\n原因: 无可用教师(专业限制或特殊要求冲突)",
                    rms[i].id, rms[i].name, need);
        }

        if (cCount == 0) continue;

        // 第二步：强制位（任课监考）
        // 规则：每个考场至少 1 名“专业匹配教师”。
        // “带多个班只监考一个班”：优先选尚未担任过任课监考的本专业教师，人人先轮一遍。
        int mandatory = -1; // 任课监考必选位
        for (int j = 0; j < cCount; j++) {
            int idx = candidateIdx[j];
            if (strcmp(allT[idx].subject, rms[i].subject) == 0 && !mandatoryDone[idx]) {
                if (mandatory == -1 || allT[idx].taskCount < allT[mandatory].taskCount) {
                    mandatory = idx;
                }
            }
        }
        // 若本专业教师都已被轮过，则允许重复（教师不足时的回退）。
        if (mandatory == -1) {
            for (int j = 0; j < cCount; j++) {
                int idx = candidateIdx[j];
                if (strcmp(allT[idx].subject, rms[i].subject) == 0) {
                    if (mandatory == -1 || allT[idx].taskCount < allT[mandatory].taskCount) {
                        mandatory = idx;
                    }
                }
            }
        }

        if (mandatory != -1 && assigned < need) {
            int pos = ListView_GetItemCount(hScheduleList);
            LVITEM lvi = {0};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = pos;
            lvi.pszText = allT[mandatory].name;
            ListView_InsertItem(hScheduleList, &lvi);

            char rmInfo[100], detail[200];
            sprintf(rmInfo, "[%s] %s", rms[i].id, rms[i].name);
            sprintf(detail, "专业匹配(%s) | 角色:任课监考", rms[i].subject);

            ListView_SetItemText(hScheduleList, pos, 1, rmInfo);
            ListView_SetItemText(hScheduleList, pos, 2, detail);
            ListView_SetItemText(hScheduleList, pos, 3, currentTimeStr[0] ? currentTimeStr : "-");
            ListView_SetItemText(hScheduleList, pos, 4, autoOperatorName);

            selected[mandatory] = 1;
            mandatoryDone[mandatory] = 1; // 该教师已完成其“一个班”的任课监考
            allT[mandatory].taskCount++;
            if (curDateIdx >= 0) dateLoads[mandatory][curDateIdx]++;
            if (curSlotIdx >= 0) slotLoads[mandatory][curSlotIdx]++;
            assigned++;
            roomAssigned++;
        }
        else if (mandatory == -1 && roomIssueCount < MAX_ROOM_ISSUES) {
            sprintf(roomIssueList[roomIssueCount++],
                "[%s] %s\n需求场次: %d\n原因: 缺少本专业任课老师(强制必考位无法满足)",
                rms[i].id, rms[i].name, need);
        }

        // 硬性同日聚合：当日已排班教师足够填满剩余位置时，只保留他们，实现“尽量一天完成”。
        if (curDateIdx >= 0) {
            int sameDayCount = 0;
            for (int j = 0; j < cCount; j++) {
                int idx = candidateIdx[j];
                if (idx != mandatory && dateLoads[idx][curDateIdx] > 0) sameDayCount++;
            }
            if (sameDayCount >= need - assigned) {
                int n = 0;
                for (int j = 0; j < cCount; j++) {
                    int idx = candidateIdx[j];
                    if (idx == mandatory || dateLoads[idx][curDateIdx] > 0) {
                        candidateIdx[n++] = idx;
                    }
                }
                cCount = n;
            }
        }

        // 第三步：巡考位
        // 在剩余候选中选 1 人作为巡考，并计入教师任务次数。
        // 巡考位也走统一优先级函数，确保“同日聚合 + 均衡 + 专业倾向”。
        int patrol = -1; // 巡考候选
        for (int j = 0; j < cCount; j++) {
            int idx = candidateIdx[j];
            if (selected[idx]) continue;
            if (patrol == -1 || CompareTeacherPriority(&allT[idx], &allT[patrol], idx, patrol,
                                                       &rms[i], curDateIdx >= 0 ? curDateIdx : 0,
                                                       dateLoads, dateCount, selected) < 0) {
                patrol = idx;
            }
        }

        if (patrol != -1 && assigned < need) {
            int pos = ListView_GetItemCount(hScheduleList);
            LVITEM lvi = {0};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = pos;
            lvi.pszText = allT[patrol].name;
            ListView_InsertItem(hScheduleList, &lvi);

            char rmInfo[100], detail[200];
            sprintf(rmInfo, "[%s] %s", rms[i].id, rms[i].name);
            sprintf(detail, "专业:%s | 角色:巡考(计入监考次数)", rms[i].subject);

            ListView_SetItemText(hScheduleList, pos, 1, rmInfo);
            ListView_SetItemText(hScheduleList, pos, 2, detail);
            ListView_SetItemText(hScheduleList, pos, 3, currentTimeStr[0] ? currentTimeStr : "-");
            ListView_SetItemText(hScheduleList, pos, 4, autoOperatorName);

            selected[patrol] = 1;
            allT[patrol].taskCount++;
            if (curDateIdx >= 0) dateLoads[patrol][curDateIdx]++;
            if (curSlotIdx >= 0) slotLoads[patrol][curSlotIdx]++;
            assigned++;
            roomAssigned++;
            patrolAssigned++;
        }

        // 第四步：普通监考位排序补齐。
        // 对候选按统一优先级排序，随后从前到后补足缺口。
        // 注意：selected 会阻止同一考场重复选择同一教师。
        g_sortTeachers = allT;
        g_sortRoom = &rms[i];
        g_sortDateIdx = curDateIdx >= 0 ? curDateIdx : 0;
        g_sortDateCount = dateCount;
        g_sortDateLoads = dateLoads;
        g_sortSelected = selected;
        qsort(candidateIdx, cCount, sizeof(int), CompareCandidateIndex);

        for (int j = 0; j < cCount && assigned < need; j++) {
            int idx = candidateIdx[j];
            Teacher* target = &allT[idx];
            if (selected[idx]) continue;

            int pos = ListView_GetItemCount(hScheduleList);
            LVITEM lvi = { 0 }; lvi.mask = LVIF_TEXT; lvi.iItem = pos; 
            lvi.pszText = target->name;
            ListView_InsertItem(hScheduleList, &lvi);
            
            char rmInfo[100], subjectInfo[200];
            sprintf(rmInfo, "[%s] %s", rms[i].id, rms[i].name);
            sprintf(subjectInfo, "%s (%s) | 角色:监考", target->subject, rms[i].subject);
            
            ListView_SetItemText(hScheduleList, pos, 1, rmInfo);
            ListView_SetItemText(hScheduleList, pos, 2, subjectInfo);
            ListView_SetItemText(hScheduleList, pos, 3, currentTimeStr[0] ? currentTimeStr : "-");
            ListView_SetItemText(hScheduleList, pos, 4, autoOperatorName);

            target->taskCount++;
            if (curDateIdx >= 0) dateLoads[idx][curDateIdx]++;
            if (curSlotIdx >= 0) slotLoads[idx][curSlotIdx]++;
            selected[idx] = 1;
            assigned++;
            roomAssigned++;
        }

        // 记录本考场完成度，供最终报告汇总。
        totalNeedCount += need;
        totalAssignedCount += roomAssigned;
        if (roomAssigned < need) {
            unfilledRoomCount++;
            if (roomIssueCount < MAX_ROOM_ISSUES) {
                sprintf(roomIssueList[roomIssueCount++],
                        "[%s] %s\n已排: %d\n需求: %d\n原因: 教师数量不足或可用性冲突",
                        rms[i].id, rms[i].name, roomAssigned, need);
            }
        }
    }

    // 将本次排班后更新过的 taskCount 持久化到教师文件，确保下轮排班沿用新负载。
    SaveAllTeachers(allT, aCount);

    // 将列表中的排班结果写回 schedules.dat，并刷新教师列表显示。
    SaveCurrentSchedule();
    RefreshTeacherList();
    free(allT);
    free(rms);

    {
        char msg[2048];
        // 结果分级反馈：
        // 1) 完全失败（0 场）
        // 2) 部分成功（有未排满）
        // 3) 全部完成
        // 并将未满足考场问题清单拼接到报告窗口，辅助用户调参。
        if (totalAssignedCount == 0) {
            sprintf(msg,
                    "本次未生成任何有效监考安排，系统无法作出安排。\n\n"
                    "建议：\n"
                    "1. 增加教师数量。\n"
                    "2. 放宽专业对口限制。\n"
                    "3. 检查教师特殊要求是否过多冲突。\n"
                    "4. 检查考场与科目数据是否完整。\n\n"
                    "当前考场总需求: %d 场次，实际安排: 0 场次。\n\n"
                    "未满足考场明细:\n",
                    totalNeedCount);
            for (int i = 0; i < roomIssueCount && strlen(msg) < sizeof(msg) - 256; i++) {
                strcat(msg, roomIssueList[i]);
                strcat(msg, "\n--------------------\n");
            }
            ShowReportWindow(GetActiveWindow(), "无法排班报告", msg);
            MessageBox(NULL, "系统无法作出安排，请根据打开的报告修改条件后重试。", "无法排班", MB_OK | MB_ICONERROR);
        } else if (unfilledRoomCount > 0) {
            sprintf(msg,
                    "排班已部分完成，但仍有考场未排满。\n\n"
                    "未排满考场数: %d\n"
                    "总需求场次: %d\n"
                    "实际完成场次: %d\n\n"
                    "建议：\n"
                    "1. 增加教师数量。\n"
                    "2. 放宽专业限制或允许更多教师跨专业监考。\n"
                    "3. 减少教师特殊要求冲突。\n\n"
                    "本次巡考分配：%d 人次\n\n"
                    "未排满考场明细:\n",
                    unfilledRoomCount, totalNeedCount, totalAssignedCount, patrolAssigned);
            for (int i = 0; i < roomIssueCount && strlen(msg) < sizeof(msg) - 256; i++) {
                strcat(msg, roomIssueList[i]);
                strcat(msg, "\n--------------------\n");
            }
            ShowReportWindow(GetActiveWindow(), "排班未排满报告", msg);
            MessageBox(NULL, "部分考场未排满，详情已打开报告窗口。", "排班未排满", MB_OK | MB_ICONWARNING);
        } else {
            sprintf(msg,
                    "排班已完成！\n"
                    "- 连续性优化：优先同日连排，减少跨日分散\n"
                    "- 任课老师必考：每个考场至少1名专业匹配教师\n"
                    "- 巡考任务已纳入计数，本次巡考分配：%d 人次\n"
                    "- 普通教师与教授仍按均衡策略分配",
                    patrolAssigned);
            MessageBox(NULL, msg, "提示", MB_OK | MB_ICONINFORMATION);
        }
    }
}

// --- 4. 修改教师窗口回调 ---
LRESULT CALLBACK EditTeacherProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 教师编辑窗口过程：负责界面创建与保存逻辑。
    switch (msg) {
    case WM_CREATE:
        CreateWindow("STATIC", "编号:", WS_CHILD | WS_VISIBLE, 20, 20, 50, 20, hWnd, NULL, NULL, NULL);
        hEditID = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", editingTeacher.id, WS_CHILD | WS_VISIBLE, 80, 18, 150, 25, hWnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "姓名:", WS_CHILD | WS_VISIBLE, 20, 60, 50, 20, hWnd, NULL, NULL, NULL);
        hEditName = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", editingTeacher.name, WS_CHILD | WS_VISIBLE, 80, 58, 150, 25, hWnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "专业:", WS_CHILD | WS_VISIBLE, 20, 100, 50, 20, hWnd, NULL, NULL, NULL);
        hEditSubject = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", editingTeacher.subject, WS_CHILD | WS_VISIBLE, 80, 98, 150, 25, hWnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "职称:", WS_CHILD | WS_VISIBLE, 20, 140, 50, 20, hWnd, NULL, NULL, NULL);
        hEditTitle = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", editingTeacher.title, WS_CHILD | WS_VISIBLE, 80, 138, 150, 25, hWnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "特殊要求:", WS_CHILD | WS_VISIBLE, 20, 180, 60, 20, hWnd, NULL, NULL, NULL);
        hEditSpecialReq = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", editingTeacher.specialReq, WS_CHILD | WS_VISIBLE, 80, 178, 220, 25, hWnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "日期:", WS_CHILD | WS_VISIBLE, 20, 212, 50, 20, hWnd, NULL, NULL, NULL);
        hReqDatePicker = CreateWindowEx(0, DATETIMEPICK_CLASS, "", WS_CHILD | WS_VISIBLE | DTS_SHORTDATEFORMAT, 80, 210, 130, 24, hWnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "原因:", WS_CHILD | WS_VISIBLE, 20, 242, 50, 20, hWnd, NULL, NULL, NULL);
        hReqReasonCombo = CreateWindow("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 80, 240, 130, 150, hWnd, NULL, NULL, NULL);
        SendMessage(hReqReasonCombo, CB_ADDSTRING, 0, (LPARAM)"仅日期限制");
        SendMessage(hReqReasonCombo, CB_ADDSTRING, 0, (LPARAM)"请假");
        SendMessage(hReqReasonCombo, CB_ADDSTRING, 0, (LPARAM)"出差");
        SendMessage(hReqReasonCombo, CB_ADDSTRING, 0, (LPARAM)"生病");
        SendMessage(hReqReasonCombo, CB_ADDSTRING, 0, (LPARAM)"不可监考");
        SendMessage(hReqReasonCombo, CB_SETCURSEL, 0, 0);
        CreateWindow("BUTTON", "添加限制", WS_CHILD | WS_VISIBLE, 220, 225, 80, 32, hWnd, (HMENU)402, NULL, NULL);
        CreateWindow("STATIC", "格式支持: - / 日期 / 日期(原因) / 分号分隔", WS_CHILD | WS_VISIBLE, 20, 272, 290, 16, hWnd, NULL, NULL, NULL);
        CreateWindow("BUTTON", "保存修改", WS_CHILD | WS_VISIBLE, 110, 295, 100, 35, hWnd, (HMENU)401, NULL, NULL);
        break;
        
    case WM_COMMAND:
        if (LOWORD(wParam) == 402) {
            // “添加限制”按钮：将日期选择器和原因组合成标准 token 并追加到文本框。
            SYSTEMTIME st;          // 日期控件选中的日期
            char reason[40] = {0};  // 下拉框选中的原因
            char token[80] = {0};   // 组合后的限制 token
            char currentReq[120] = {0}; // 当前特殊要求文本

            if (DateTime_GetSystemtime(hReqDatePicker, &st) != GDT_VALID) {
                MessageBox(hWnd, "请选择有效日期。", "提示", MB_OK | MB_ICONWARNING);
                return 0;
            }
            GetWindowText(hReqReasonCombo, reason, 40);
            if (reason[0] == '\0') strcpy(reason, "仅日期限制");

            if (strcmp(reason, "仅日期限制") == 0) {
                sprintf(token, "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
            } else {
                sprintf(token, "%04d-%02d-%02d(%s)", st.wYear, st.wMonth, st.wDay, reason);
            }

            GetWindowText(hEditSpecialReq, currentReq, 120);
            AppendSpecialReqToken(currentReq, sizeof(currentReq), token);
            SetWindowText(hEditSpecialReq, currentReq);
        } else if (LOWORD(wParam) == 401) {
            // “保存修改”按钮：先校验，再落盘 teachers.dat，最后更新扩展审计信息。
            Teacher newData; // 待保存的新教师数据
            GetWindowText(hEditID, newData.id, 20);
            GetWindowText(hEditName, newData.name, 50);
            GetWindowText(hEditSubject, newData.subject, 50);
            GetWindowText(hEditTitle, newData.title, 20);
            GetWindowText(hEditSpecialReq, newData.specialReq, 100);

            if (newData.specialReq[0] == '\0') {
                strcpy(newData.specialReq, "-");
            }

            {
                char errMsg[220] = {0};
                if (!ValidateSpecialReqFormat(newData.specialReq, errMsg, sizeof(errMsg))) {
                    MessageBox(hWnd, errMsg, "特殊要求校验失败", MB_OK | MB_ICONWARNING);
                    return 0;
                }
            }

            Teacher* cache = (Teacher*)malloc(MAX_TEACHERS * sizeof(Teacher));
            if (!cache) {
                MessageBox(NULL, "内存不足，无法保存修改！", "错误", MB_OK | MB_ICONERROR);
                return 0;
            }
            int cnt = LoadAllTeachers(cache, MAX_TEACHERS);
            for (int i = 0; i < cnt; i++) {
                if (strcmp(cache[i].id, editingTeacher.id) == 0) {
                    newData.taskCount = cache[i].taskCount;
                    strcpy(newData.classes, cache[i].classes); // 编辑不改动所带班级
                    cache[i] = newData;
                    break;
                }
            }
            SaveAllTeachers(cache, cnt);
            free(cache);

            TeacherExt* exts = (TeacherExt*)malloc(MAX_TEACHERS * sizeof(TeacherExt)); // 扩展审计数据缓存
            int extCount = 0; // 扩展记录数量
            FILE* fext = fopen("teacher_ext.dat", "rb");
            if (fext && exts) {
                while (extCount < MAX_TEACHERS && fread(&exts[extCount], sizeof(TeacherExt), 1, fext)) extCount++;
                fclose(fext);
            }
            
            time_t now = time(NULL);      // 当前时间戳
            struct tm *ti = localtime(&now); // 本地时间
            char timeStr[30]; strftime(timeStr, 30, "%Y-%m-%d %H:%M:%S", ti); // 格式化时间

            BOOL foundExt = FALSE;
            for (int i = 0; i < extCount; i++) {
                if (strcmp(exts[i].id, editingTeacher.id) == 0) {
                    strcpy(exts[i].id, newData.id);
                    strcpy(exts[i].lastModTime, timeStr);
                    strcpy(exts[i].operatorName, currentUser);
                    foundExt = TRUE; break;
                }
            }
            if (!foundExt && exts && extCount < MAX_TEACHERS) {
                strcpy(exts[extCount].id, newData.id);
                strcpy(exts[extCount].lastModTime, timeStr);
                strcpy(exts[extCount].operatorName, currentUser);
                extCount++;
            }
            fext = fopen("teacher_ext.dat", "wb");
            if (fext && exts) { fwrite(exts, sizeof(TeacherExt), extCount, fext); fclose(fext); }
            if (exts) free(exts);

            MessageBox(NULL, "修改成功，数据已立即刷新！", "提示", MB_OK);
            RefreshTeacherList(); 
            DestroyWindow(hWnd);  
        }
        break;
    default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// 考场安排编辑窗口
LRESULT CALLBACK ScheduleEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 排班编辑窗口过程：支持新增与修改排班条目。
    switch (msg) {
    case WM_CREATE:
        CreateWindow("STATIC", "监考老师:", WS_CHILD | WS_VISIBLE, 20, 20, 80, 20, hWnd, NULL, NULL, NULL);
        hEditSchedTeacher = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", editingSchedule.teacherName, WS_CHILD | WS_VISIBLE, 110, 18, 180, 25, hWnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "考场详情:", WS_CHILD | WS_VISIBLE, 20, 60, 80, 20, hWnd, NULL, NULL, NULL);
        hEditSchedRoomDetail = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", editingSchedule.roomDetail, WS_CHILD | WS_VISIBLE, 110, 58, 180, 25, hWnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "匹配情况:", WS_CHILD | WS_VISIBLE, 20, 100, 80, 20, hWnd, NULL, NULL, NULL);
        hEditSchedMatchDetail = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", editingSchedule.matchDetail, WS_CHILD | WS_VISIBLE, 110, 98, 180, 25, hWnd, NULL, NULL, NULL);

        CreateWindow("BUTTON", "保存安排", WS_CHILD | WS_VISIBLE, 110, 160, 100, 35, hWnd, (HMENU)601, NULL, NULL);
        break;
        
    case WM_COMMAND:
        if (LOWORD(wParam) == 601) {
            // 统一自动写入“修改时间 + 操作人”，减少人工遗漏。
            ScheduleEntry newEntry; // 新增/更新的排班记录
            GetWindowText(hEditSchedTeacher, newEntry.teacherName, 50);
            GetWindowText(hEditSchedRoomDetail, newEntry.roomDetail, 100);
            GetWindowText(hEditSchedMatchDetail, newEntry.matchDetail, 100);

            // 自动记录修改时间和操作人
            time_t now = time(NULL);       // 当前时间戳
            struct tm *ti = localtime(&now); // 本地时间
            strftime(newEntry.lastModTime, 30, "%Y-%m-%d %H:%M:%S", ti);
            strcpy(newEntry.operatorName, currentUser[0] ? currentUser : "系统");

            ScheduleEntry* arr = (ScheduleEntry*)malloc(MAX_SCHEDULES * sizeof(ScheduleEntry));
            if (!arr) {
                MessageBox(NULL, "内存不足！", "错误", MB_OK);
                DestroyWindow(hWnd);
                return 0;
            }
            int cnt = LoadAllSchedules(arr, MAX_SCHEDULES);

            if (editingScheduleIndex == -1) {
                if (cnt < MAX_SCHEDULES) arr[cnt++] = newEntry;
            } else if (editingScheduleIndex < cnt) {
                arr[editingScheduleIndex] = newEntry;
            }

            SaveAllSchedules(arr, cnt);
            free(arr);
            RefreshScheduleList();
            MessageBox(NULL, "考场安排信息保存成功！", "提示", MB_OK);
            DestroyWindow(hWnd);
        }
        break;
    default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// 登录/注册窗口
LRESULT CALLBACK LoginWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 登录窗口过程：处理登录校验与打开主窗口。
    switch (msg) {
    case WM_CREATE:
        CreateWindow("STATIC", "用户名:", WS_CHILD | WS_VISIBLE, 50, 40, 60, 20, hWnd, NULL, NULL, NULL);
        hLogUser = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE, 110, 38, 130, 25, hWnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "密  码:", WS_CHILD | WS_VISIBLE, 50, 80, 60, 20, hWnd, NULL, NULL, NULL);
        hLogPass = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_PASSWORD, 110, 78, 130, 25, hWnd, NULL, NULL, NULL);
        CreateWindow("BUTTON", "登 录", WS_CHILD | WS_VISIBLE, 50, 130, 80, 35, hWnd, (HMENU)101, NULL, NULL);
        CreateWindow("BUTTON", "注 册", WS_CHILD | WS_VISIBLE, 150, 130, 80, 35, hWnd, (HMENU)102, NULL, NULL);
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == 101) {
            char u[50] = {0}, p[50] = {0};
            GetWindowText(hLogUser, u, 50);
            GetWindowText(hLogPass, p, 50);

            if (strlen(u) == 0 || strlen(p) == 0) {
                MessageBox(hWnd, "请输入用户名和密码。", "登录失败", MB_OK | MB_ICONWARNING);
                break;
            }

            if (!ValidateUser(u, p)) {
                MessageBox(hWnd, "用户名或密码错误。", "登录失败", MB_OK | MB_ICONERROR);
                break;
            }

            isLoggedIn = TRUE;
            strcpy(currentUser, u);
            {
                int sw = GetSystemMetrics(SM_CXSCREEN);
                int sh = GetSystemMetrics(SM_CYSCREEN);
                int mw = 1300, mh = 580;
                if (!CreateWindow("MClass", "高校考勤排班管理系统", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  (sw - mw) / 2, (sh - mh) / 2, mw, mh, NULL, NULL,
                                  (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL)) {
                    MessageBox(hWnd, "主窗口创建失败，请重试。", "错误", MB_OK | MB_ICONERROR);
                    isLoggedIn = FALSE;
                    currentUser[0] = '\0';
                    break;
                }
            }
            DestroyWindow(hWnd);
        } else if (LOWORD(wParam) == 102) {
            CreateWindow("RegClass", "注册", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 500, 350, 300, 250, hWnd, NULL, NULL, NULL);
        }
        break;
    case WM_DESTROY:
        if (!isLoggedIn) PostQuitMessage(0);
        break;
    default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK RegisterWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 注册窗口过程：处理注册表单校验与用户落盘。
    switch (msg) {
    case WM_CREATE:
        CreateWindow("STATIC", "账号:", WS_CHILD | WS_VISIBLE, 30, 20, 60, 20, hWnd, NULL, NULL, NULL);
        hRegUser = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE, 100, 18, 140, 25, hWnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "密码:", WS_CHILD | WS_VISIBLE, 30, 60, 60, 20, hWnd, NULL, NULL, NULL);
        hRegPass = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_PASSWORD, 100, 58, 140, 25, hWnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "确认:", WS_CHILD | WS_VISIBLE, 30, 100, 60, 20, hWnd, NULL, NULL, NULL);
        hRegConf = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_PASSWORD, 100, 98, 140, 25, hWnd, NULL, NULL, NULL);
        CreateWindow("STATIC", "要求: >=8位,含大小写/数字/符号", WS_CHILD | WS_VISIBLE, 30, 130, 220, 16, hWnd, NULL, NULL, NULL);
        CreateWindow("BUTTON", "提交注册", WS_CHILD | WS_VISIBLE, 90, 145, 100, 35, hWnd, (HMENU)301, NULL, NULL);
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == 301) {
            // 注册流程：完整性校验 -> 密码强度校验 -> 重名校验 -> 写入文件。
            char u[50] = {0}, p[50] = {0}, c[50] = {0};
            char pwdErr[256] = {0};
            GetWindowText(hRegUser, u, 50);
            GetWindowText(hRegPass, p, 50);
            GetWindowText(hRegConf, c, 50);

            if (strlen(u) == 0 || strlen(p) == 0 || strlen(c) == 0) {
                MessageBox(hWnd, "请完整填写注册信息。", "注册失败", MB_OK | MB_ICONWARNING);
                break;
            }
            if (strcmp(p, c) != 0) {
                MessageBox(hWnd, "两次输入的密码不一致。", "注册失败", MB_OK | MB_ICONWARNING);
                break;
            }
            if (!ValidatePasswordComplexity(p, pwdErr, sizeof(pwdErr))) {
                MessageBox(hWnd, pwdErr, "注册失败", MB_OK | MB_ICONWARNING);
                break;
            }
            if (UserExists(u)) {
                MessageBox(hWnd, "该账号已存在，请更换账号。", "注册失败", MB_OK | MB_ICONWARNING);
                break;
            }
            if (!AddUser(u, p)) {
                MessageBox(hWnd, "无法写入用户数据文件 users.dat。", "注册失败", MB_OK | MB_ICONERROR);
                break;
            }

            MessageBox(hWnd, "注册成功，请返回登录。", "提示", MB_OK | MB_ICONINFORMATION);
            DestroyWindow(hWnd);
        }
        break;
    default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// --- 6. 主窗体逻辑 ---
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 主窗口过程：负责列表、按钮和业务命令分发。
    switch (msg) {
    case WM_CREATE:
        // 上半区：教师列表。
        hTeacherList = CreateWindowEx(0, WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 10, 10, 960, 150, hWnd, NULL, NULL, NULL);
        ListView_SetExtendedListViewStyle(hTeacherList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        char* tH[] = { "ID", "姓名", "专业", "职称", "次数", "修改时间", "操作人", "特殊要求", "班级" }; // 教师列表列标题
        int tW[] = { 60, 80, 100, 100, 50, 160, 100, 200, 100 }; // 教师列表列宽
        for (int i = 0; i < 9; i++) {
            LVCOLUMN lvc = {0}; lvc.mask = LVCF_TEXT | LVCF_WIDTH; lvc.pszText = tH[i]; lvc.cx = tW[i];
            ListView_InsertColumn(hTeacherList, i, &lvc);
        }

        hScheduleList = CreateWindowEx(0, WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE | LVS_REPORT, 10, 180, 1100, 250, hWnd, NULL, NULL, NULL);
        ListView_SetExtendedListViewStyle(hScheduleList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        char* sH[] = { "监考老师", "考场详情", "匹配情况", "修改时间", "操作人" }; // 排班列表列标题
        int sW[]   = { 150, 380, 250, 160, 100 }; // 排班列表列宽
        for (int i = 0; i < 5; i++) {
            LVCOLUMN lvc = {0};
            lvc.mask = LVCF_TEXT | LVCF_WIDTH;
            lvc.pszText = sH[i];
            lvc.cx = sW[i];
            ListView_InsertColumn(hScheduleList, i, &lvc);
        }

        // 按钮区
        CreateWindow("BUTTON", "开始专业对口排班", WS_CHILD | WS_VISIBLE, 10, 440, 180, 40, hWnd, (HMENU)501, NULL, NULL);
        CreateWindow("BUTTON", "修改选中教师", WS_CHILD | WS_VISIBLE, 200, 440, 150, 40, hWnd, (HMENU)502, NULL, NULL);
        CreateWindow("BUTTON", "增加考场安排信息", WS_CHILD | WS_VISIBLE, 360, 440, 160, 40, hWnd, (HMENU)503, NULL, NULL);
        CreateWindow("BUTTON", "导出考场安排为TXT", WS_CHILD | WS_VISIBLE, 530, 440, 140, 40, hWnd, (HMENU)506, NULL, NULL);
        CreateWindow("BUTTON", "连续性统计", WS_CHILD | WS_VISIBLE, 680, 440, 110, 40, hWnd, (HMENU)508, NULL, NULL);
        hHintStatic = CreateWindow("STATIC", "提示：右键排班列表可修改/删除", WS_CHILD | WS_VISIBLE, 800, 450, 220, 20, hWnd, NULL, NULL, NULL);

        hSearchLabel = CreateWindow("STATIC", "按老师姓名查询:", WS_CHILD | WS_VISIBLE, 10, 500, 110, 20, hWnd, NULL, NULL, NULL);
        hSearchTeacher = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE, 120, 495, 220, 28, hWnd, NULL, NULL, NULL);
        CreateWindow("BUTTON", "查询", WS_CHILD | WS_VISIBLE, 350, 494, 70, 30, hWnd, (HMENU)507, NULL, NULL);

        RefreshScheduleList();
        RefreshTeacherList();
        break;

    case WM_CONTEXTMENU:
        // 排班列表右键菜单：自动选中右键命中的行，弹出「修改 / 删除」。
        if ((HWND)wParam == hScheduleList) {
            int sx = (int)(short)LOWORD(lParam);
            int sy = (int)(short)HIWORD(lParam);
            POINT pt = { sx, sy };
            ScreenToClient(hScheduleList, &pt);

            LVHITTESTINFO ht = { 0 };
            ht.pt = pt;
            int idx = ListView_HitTest(hScheduleList, &ht);
            BOOL hasSel = (idx >= 0);

            if (hasSel) {
                ListView_SetItemState(hScheduleList, -1, 0, LVIS_SELECTED);
                ListView_SetItemState(hScheduleList, idx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            }

            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING | (hasSel ? MF_ENABLED : MF_GRAYED), 505, "修改选中安排");
            AppendMenu(hMenu, MF_STRING | (hasSel ? MF_ENABLED : MF_GRAYED), 504, "删除选中安排");
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN, sx, sy, 0, hWnd, NULL);
            DestroyMenu(hMenu);
            return 0;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);

    case WM_COMMAND:
        // 按钮命令分发：每个按钮 ID 对应一类业务动作。
        if (LOWORD(wParam) == 501) AutoSchedule();
        else if (LOWORD(wParam) == 502) {
            int sel = ListView_GetNextItem(hTeacherList, -1, LVNI_SELECTED);
            if (sel != -1) {
                ListView_GetItemText(hTeacherList, sel, 0, editingTeacher.id, 20);
                ListView_GetItemText(hTeacherList, sel, 1, editingTeacher.name, 50);
                ListView_GetItemText(hTeacherList, sel, 2, editingTeacher.subject, 50);
                ListView_GetItemText(hTeacherList, sel, 3, editingTeacher.title, 20);
                ListView_GetItemText(hTeacherList, sel, 7, editingTeacher.specialReq, 100);
                CreateWindow("EditClass", "修改教师", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 550, 300, 320, 330, hWnd, NULL, NULL, NULL);
            }
        }
        else if (LOWORD(wParam) == 503) {
            memset(&editingSchedule, 0, sizeof(editingSchedule));
            editingScheduleIndex = -1;
            CreateWindow("ScheduleEditClass", "增加考场安排信息", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 550, 300, 320, 240, hWnd, NULL, NULL, NULL);
        }
        else if (LOWORD(wParam) == 504) {
            int sel = ListView_GetNextItem(hScheduleList, -1, LVNI_SELECTED);
            if (sel != -1) {
                ScheduleEntry* arr = (ScheduleEntry*)malloc(MAX_SCHEDULES * sizeof(ScheduleEntry));
                if (arr) {
                    int cnt = LoadAllSchedules(arr, MAX_SCHEDULES);
                    int sourceIndex = GetScheduleSourceIndexFromListItem(sel);
                    if (sourceIndex >= 0 && sourceIndex < cnt) {
                        for (int k = sourceIndex; k < cnt - 1; k++) arr[k] = arr[k + 1];
                        SaveAllSchedules(arr, cnt - 1);
                        RefreshScheduleList();
                        MessageBox(NULL, "选中考场安排已删除！", "提示", MB_OK);
                    }
                    free(arr);
                }
            } else MessageBox(NULL, "请先选中一条考场安排！", "提示", MB_OK);
        }
        else if (LOWORD(wParam) == 505) {
            int sel = ListView_GetNextItem(hScheduleList, -1, LVNI_SELECTED);
            if (sel != -1) {
                ListView_GetItemText(hScheduleList, sel, 0, editingSchedule.teacherName, 50);
                ListView_GetItemText(hScheduleList, sel, 1, editingSchedule.roomDetail, 100);
                ListView_GetItemText(hScheduleList, sel, 2, editingSchedule.matchDetail, 100);
                editingScheduleIndex = GetScheduleSourceIndexFromListItem(sel);
                CreateWindow("ScheduleEditClass", "更改考场安排信息", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 550, 300, 320, 240, hWnd, NULL, NULL, NULL);
            } else MessageBox(NULL, "请先选中一条考场安排！", "提示", MB_OK);
        }
        else if (LOWORD(wParam) == 506) {
            ExportScheduleToTxt();
        }
        else if (LOWORD(wParam) == 507) {
            char keyword[50] = {0}; // 查询关键字
            char reason[512] = {0}; // 无结果原因报告
            GetWindowText(hSearchTeacher, keyword, 50);
            if (keyword[0] == '\0') {
                RefreshScheduleList();
                MessageBox(hWnd, "查询条件为空，已显示全部监考安排。", "提示", MB_OK | MB_ICONINFORMATION);
            } else {
                // 直接输出该老师的全部监考安排报告，并同步过滤列表。
                int found = ShowTeacherScheduleReport(hWnd, keyword);
                FilterScheduleListByTeacher(keyword);
                if (found == 0) {
                    BuildNoScheduleReason(keyword, reason, sizeof(reason));
                    ShowReportWindow(hWnd, "教师排班查询报告", reason[0] ? reason : "未找到该老师的监考安排。");
                    MessageBox(hWnd, "未查询到该老师的监考安排，详细原因已打开报告窗口。", "查询结果", MB_OK | MB_ICONINFORMATION);
                }
            }
        }
        else if (LOWORD(wParam) == 508) {
            ShowScheduleQualityStats(hWnd);
        }
        break;
    case WM_SIZE: {
        // 自适应布局：列表随窗口拉伸，按钮区与查询区贴底。
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        int btnY = h - 140;   // 按钮行 y
        int searchY = h - 85; // 查询行 y
        int schedH = btnY - 190;
        if (schedH < 100) schedH = 100;

        if (hTeacherList) MoveWindow(hTeacherList, 10, 10, w - 20, 150, TRUE);
        if (hScheduleList) MoveWindow(hScheduleList, 10, 180, w - 20, schedH, TRUE);

        MoveWindow(GetDlgItem(hWnd, 501), 10, btnY, 180, 40, TRUE);
        MoveWindow(GetDlgItem(hWnd, 502), 200, btnY, 150, 40, TRUE);
        MoveWindow(GetDlgItem(hWnd, 503), 360, btnY, 160, 40, TRUE);
        MoveWindow(GetDlgItem(hWnd, 506), 530, btnY, 140, 40, TRUE);
        MoveWindow(GetDlgItem(hWnd, 508), 680, btnY, 110, 40, TRUE);
        if (hHintStatic) MoveWindow(hHintStatic, 800, btnY + 10, 220, 20, TRUE);

        if (hSearchLabel) MoveWindow(hSearchLabel, 10, searchY, 110, 20, TRUE);
        if (hSearchTeacher) MoveWindow(hSearchTeacher, 120, searchY, 220, 28, TRUE);
        MoveWindow(GetDlgItem(hWnd, 507), 350, searchY, 70, 30, TRUE);
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// --- 7. 入口 ---
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    // 程序入口：注册窗口类并进入消息循环。
    (void)hPrev;
    (void)lpCmd;
    (void)nShow;

    InitCommonControls();
    WNDCLASS lc = { 0 }; lc.lpfnWndProc = LoginWndProc; lc.hInstance = hInst; lc.hbrBackground = (HBRUSH)(COLOR_3DFACE+1); lc.lpszClassName = "LClass"; RegisterClass(&lc);
    WNDCLASS rc = { 0 }; rc.lpfnWndProc = RegisterWndProc; rc.hInstance = hInst; rc.hbrBackground = (HBRUSH)(COLOR_3DFACE+1); rc.lpszClassName = "RegClass"; RegisterClass(&rc);
    WNDCLASS ec = { 0 }; ec.lpfnWndProc = EditTeacherProc; ec.hInstance = hInst; ec.hbrBackground = (HBRUSH)(COLOR_3DFACE+1); ec.lpszClassName = "EditClass"; RegisterClass(&ec);
    WNDCLASS sc = { 0 }; sc.lpfnWndProc = ScheduleEditProc; sc.hInstance = hInst; sc.hbrBackground = (HBRUSH)(COLOR_3DFACE+1); sc.lpszClassName = "ScheduleEditClass"; RegisterClass(&sc);
    WNDCLASS rp = { 0 }; rp.lpfnWndProc = ReportWndProc; rp.hInstance = hInst; rp.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); rp.lpszClassName = "ReportClass"; RegisterClass(&rp);
    WNDCLASS mc = { 0 }; mc.lpfnWndProc = MainWndProc; mc.hInstance = hInst; mc.hbrBackground = (HBRUSH)(COLOR_3DFACE+1); mc.lpszClassName = "MClass"; RegisterClass(&mc);

    {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        CreateWindow("LClass", "登录系统", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                     (sw - 310) / 2, (sh - 220) / 2, 310, 220, NULL, NULL, hInst, NULL);
    }
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}