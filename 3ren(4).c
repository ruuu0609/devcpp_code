#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <windows.h>


// ====================== 宏定义 ======================
#define MAX_CONTACT 1000
#define MAX_SMS 2000
#define MAX_NUM 100
#define MAX_NAME_LEN    30
#define MAX_PHONE_LEN   15
#define MAX_GROUP_LEN   15
#define MAX_STORE_NUM   1000
#define ENCRYPT_KEY     0x68
#define MAIN_BIN_FILE   "contact_main.bin"
#define TEMP_BUF_LEN    1024
#define BACKUP_BUFF_LEN 10240
#define VCF_LINE_LIMIT  512
#define EXPORT_CSV_NAME "脱敏通讯录导出.csv"
#define EXPORT_TXT_NAME "脱敏通讯录导出.txt"

// ====================== 结构体定义 ======================
// 主通讯录联系人
typedef struct {
    int id;
    char name[20];
    char phone[12];
    char group[15];
    int privacy;
    int isBlack;
    int is_temp;
    long long valid_ts;
    int group_id;
    char label[60];
    char remark[80];
    long long anniv_ts;
    long long last_contact_ts;
    int priority;
    long long create_ts;
    char source[30];
    char custom_tag[60];
} Contact;

// 操作日志
typedef struct {
    char opName[20];
    char opPhone[12];
    char msg[50];
} OpLog;

// 用户
typedef struct {
    char username[20];
    char pwd[20];
} User;

// 短信
typedef struct Sms {
    char phone[12];
    char content[100];
    long long send_time;
    int send_state;
    char sender[20];
    int sms_type;
} Sms;

// 工具模块联系人（VCF/备份专用）
typedef struct ContactInfo
{
    char name[MAX_NAME_LEN];
    char phone[MAX_PHONE_LEN];
    char group[MAX_GROUP_LEN];
    int isBlackList;
    int msgSendHour;
} ContactInfo;

// ====================== 函数声明 ======================
// 通用工具
void clearBuf();
long long getNowTimeStamp();
int verifyPhoneText(char *str);
int isSameMD(long long t1, long long t2);
int checkRepeat(Contact con[], int count, char phone[]);
void getMonthDay(long long ts, char buf[]);
int checkPhoneValid(char *phone);
void mobileDesensitize(char *phoneNum);

// 文件读写(主通讯录)
int readContactBin(Contact con[], char *path);
void writeContactBin(Contact con[], int num, char *path);
void appendLogBin(OpLog log, char *path);
int loadSmsBin(Sms arr[], int *cnt);
void saveSmsBin(Sms arr[], int cnt);
void appendSmsBin(Sms arr[], int cnt);
void saveContactBin(Contact arr[], int cnt);

// 用户&系统初始化
void systemInit();
void registerUser();
int loginCheck();

// 主通讯录CRUD
void addContact();
void showAllContact();
void searchContact();
void modifyContact();
void delContact();
void addBlack();

// 联系人扩展功能
void checkTempContact();
void groupSendSms(int gid);
void autoCreateLabel(Contact arr[], int len);
void remindAnniversary();
void printContactInfo(Contact c);
void initNewContact(Contact *c);
void filterByGroup();
void filterByGroupID(int gid);
void sortContactByName();
void showLog();

// 短信功能
void addReceiveSms();
void sendOneSms();
void replySms();
void showReceivedSms();

// 工具模块（无全局数组，全部传参）
void loadBinaryDataToMemory(ContactInfo data[], int *total, int groupCnt[], int hourCnt[]);
void batchImportVCF();
void exportDesensitizeData(int exportType);
void showContactGroupChart(ContactInfo data[], int total, int groupCnt[]);
void showBlackWhiteChart(ContactInfo data[], int total);
void showMsgTimeDistributeChart(int hourCnt[]);
void showAllVisualAnalysis();
void binaryDataEncrypt(unsigned char *data, int len);
void createVersionBackup();
void restoreHistoryBackup();

// 菜单
void loginMenu();
void mainMenu();

// ====================== 通用工具实现 ======================
void clearBuf() {
    while (getchar() != '\n');
}

long long getNowTimeStamp() {
    return time(NULL);
}

int verifyPhoneText(char *str) {
    int i;
    for (i = 0; str[i] != '\0'; i++) {
        if (!((str[i] >= '0' && str[i] <= '9') ||
              (str[i] >= 'a' && str[i] <= 'z') ||
              (str[i] >= 'A' && str[i] <= 'Z')))
            return 0;
    }
    return 1;
}

int isSameMD(long long t1, long long t2)
{
    time_t time1 = (time_t)t1;
    time_t time2 = (time_t)t2;
    struct tm tm1 = *localtime(&time1);
    struct tm tm2 = *localtime(&time2);
    return (tm1.tm_mon == tm2.tm_mon && tm1.tm_mday == tm2.tm_mday);
}

void getMonthDay(long long ts, char buf[])
{
    int tmMon, tmDay;
    if (ts == 0)
    {
        strcpy(buf, "无");
        return;
    }
    time_t t = (time_t)ts;
    struct tm tm = *localtime(&t);
    tmMon = tm.tm_mon + 1;
    tmDay = tm.tm_mday;
    sprintf(buf, "%d月%d日", tmMon, tmDay);
}

int checkRepeat(Contact con[], int count, char phone[]) {
    int i;
    for (i = 0; i < count; i++) {
        if (strcmp(con[i].phone, phone) == 0) {
            return 1;
        }
    }
    return 0;
}

int checkPhoneValid(char *phone)
{
    int i, len = strlen(phone);
    if(len != 11)
        return 0;
    for(i = 0; i < 11; i++)
    {
        if(!isdigit(phone[i]))
            return 0;
    }
    return 1;
}

void mobileDesensitize(char *phoneNum)
{
    int len = strlen(phoneNum);
    if(len < 11)
        return;
    if(len > 11)
        phoneNum[11] = '\0';
    // 全部改为 phoneNum，不是 phone
    phoneNum[3] = '*';
    phoneNum[4] = '*';
    phoneNum[5] = '*';
    phoneNum[6] = '*';
}

// ====================== 主通讯录文件操作 ======================
int readContactBin(Contact con[], char *path) {
    FILE *fp = fopen(path, "rb");
    int cnt = 0;
    if (fp == NULL) return 0;
    while (fread(&con[cnt], sizeof(Contact), 1, fp)) {
        cnt++;
    }
    fclose(fp);
    return cnt;
}

void writeContactBin(Contact con[], int num, char *path) {
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) return;
    fwrite(con, sizeof(Contact), num, fp);
    fclose(fp);
}

void appendLogBin(OpLog log, char *path) {
    FILE *fp = fopen(path, "ab");
    if (fp == NULL) return;
    fwrite(&log, sizeof(OpLog), 1, fp);
    fclose(fp);
}

int loadSmsBin(Sms arr[], int *cnt) {
    FILE *fp = fopen("sms.bin", "rb");
    if (fp == NULL) return -1;
    *cnt = fread(arr, sizeof(Sms), MAX_SMS, fp);
    fclose(fp);
    return 0;
}

void saveSmsBin(Sms arr[], int cnt) {
    FILE *fp = fopen("sms.bin", "wb");
    if (fp == NULL) return;
    fwrite(arr, sizeof(Sms), cnt, fp);
    fclose(fp);
}

void appendSmsBin(Sms arr[], int cnt) {
    FILE *fp = fopen("sms.bin", "ab");
    if (fp == NULL) return;
    fwrite(arr, sizeof(Sms), cnt, fp);
    fclose(fp);
}

void saveContactBin(Contact arr[], int cnt) {
    writeContactBin(arr, cnt, "contact.bin");
}

// ====================== 系统初始化&用户 ======================
void systemInit() {
    const char *fileList[] = {"contact.bin", "log.bin", "user.bin", "sms.bin", MAIN_BIN_FILE};
    int i, total = sizeof(fileList)/sizeof(fileList[0]);
    FILE *fp;
    for(i = 0; i < total; i++)
    {
        fp = fopen(fileList[i], "ab");
        if(fp != NULL) fclose(fp);
    }
}

void registerUser() {
    User u;
    FILE *fp = fopen("user.bin", "ab");
    if (fp == NULL) {
        printf("文件打开失败，注册中断！\n");
        return;
    }
    printf("请输入用户名：");
    scanf("%s", u.username);
    clearBuf();
    printf("请输入密码：");
    scanf("%s", u.pwd);
    fwrite(&u, sizeof(User), 1, fp);
    fclose(fp);
    printf("注册成功！\n");
}

int loginCheck() {
    char un[20], pw[20];
    User u;
    FILE *fp = fopen("user.bin", "rb");
    int flag = 0;
    if (fp == NULL) {
        printf("无用户账号，请先注册！\n");
        registerUser();
        return 0;
    }
    printf("====系统登录====\n");
    printf("用户名：");
    scanf("%s", un);
    clearBuf();
    printf("密码：");
    scanf("%s", pw);
    while (fread(&u, sizeof(User), 1, fp)) {
        if (strcmp(u.username, un) == 0 && strcmp(u.pwd, pw) == 0) {
            flag = 1;
            break;
        }
    }
    fclose(fp);
    if(flag)
    {
        printf("登录成功！\n");
        return 1;
    }
    printf("账号或密码错误！\n");
    return 0;
}

// ====================== 联系人CRUD ======================
void addContact() {
    Contact allCon[MAX_CONTACT], newCon;
    int count = readContactBin(allCon, "contact.bin");
    int days, i;
    char birthStr[10];
    printf("====添加联系人====\n");
    initNewContact(&newCon);
    printf("姓名：");
    scanf("%s", newCon.name);
    clearBuf();
    printf("手机号(11位)：");
    scanf("%s", newCon.phone);
    clearBuf();
    if (strlen(newCon.phone) != 11) {
        printf("手机号必须为11位数字！\n");
        return;
    }
    if (checkRepeat(allCon, count, newCon.phone)) {
        printf("该手机号已存在！\n");
        return;
    }
    printf("分组名称：");
    scanf("%s", newCon.group);
    clearBuf();
    printf("分组ID(数字)：");
    scanf("%d", &newCon.group_id);
    clearBuf();
    printf("隐私等级(1公开/2私密/3隐藏)：");
    scanf("%d", &newCon.privacy);
    clearBuf();
    printf("是否临时联系人(0否/1是)：");
    scanf("%d", &newCon.is_temp);
    clearBuf();
    if (newCon.is_temp == 1) {
        printf("有效期(天)：");
        scanf("%d", &days);
        clearBuf();
        newCon.valid_ts = getNowTimeStamp() + days * 86400;
    }
    printf("备注：");
    scanf("%s", newCon.remark);
    clearBuf();
    printf("纪念日(输入MMDD，如0627，0表示无): ");
	scanf("%s", birthStr);
	clearBuf();
	if (strlen(birthStr) == 4 && strcmp(birthStr, "0") != 0) {
    	struct tm t = {0};
    	time_t now = time(NULL);
    	struct tm *today = localtime(&now);
    	t.tm_year = today->tm_year;
   		t.tm_mon = (birthStr[0]-'0')*10 + (birthStr[1]-'0') - 1;
    	t.tm_mday = (birthStr[2]-'0')*10 + (birthStr[3]-'0');
    	newCon.anniv_ts = mktime(&t);
	} else {
    	newCon.anniv_ts = 0;
	}
    newCon.id = count + 1;
    newCon.isBlack = 0;
    newCon.create_ts = getNowTimeStamp();
    allCon[count] = newCon;
    count++;
    autoCreateLabel(allCon, count);
    writeContactBin(allCon, count, "contact.bin");
    printf("联系人添加成功！\n");
    OpLog log;
    strcpy(log.opName, newCon.name);
    strcpy(log.opPhone, newCon.phone);
    strcpy(log.msg, "添加联系人");
    appendLogBin(log, "log.bin");
}

void showAllContact() {
    Contact allCon[MAX_CONTACT];
    int count = readContactBin(allCon, "contact.bin");
    int i;
    char md[20];
    if (count == 0) {
        printf("通讯录无数据！\n");
        return;
    }
    printf("\n===================所有联系人===================\n");
    printf("ID\t姓名\t手机号\t\t分组\t隐私等级\t黑名单\t标签\t\t纪念日\n");
    for (i = 0; i < count; i++) {
        getMonthDay(allCon[i].anniv_ts, md);
		printf("%d\t%s\t%s\t%s\t%d\t\t%s\t%-16s%s\n",
       		allCon[i].id,
       		allCon[i].name,
       		allCon[i].phone,
       		allCon[i].group,
       		allCon[i].privacy,
       		allCon[i].isBlack == 1 ? "是" : "否",
       		allCon[i].label,
       		md);
    }
    printf("================================================\n");
}

void searchContact() {
    Contact allCon[MAX_CONTACT];
    char key[12];
    int count = readContactBin(allCon, "contact.bin");
    int i, flag = 0;
    printf("请输入要查询的手机号：");
    scanf("%s", key);
    clearBuf();
    for (i = 0; i < count; i++) {
        if (strcmp(allCon[i].phone, key) == 0) {
            printf("\n找到联系人：\n");
            printContactInfo(allCon[i]);
            flag = 1;
            break;
        }
    }
    if (!flag) printf("未查询到该联系人！\n");
}

void modifyContact() {
    Contact allCon[MAX_CONTACT];
    char phone[12];
    int count = readContactBin(allCon, "contact.bin");
    int i, idx = -1;
    printf("请输入要修改的手机号：");
    scanf("%s", phone);
    clearBuf();
    for (i = 0; i < count; i++) {
        if (strcmp(allCon[i].phone, phone) == 0) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        printf("联系人不存在！\n");
        return;
    }
    printf("修改姓名：");
    scanf("%s", allCon[idx].name);
    clearBuf();
    printf("修改分组名称：");
    scanf("%s", allCon[idx].group);
    clearBuf();
    printf("修改分组ID：");
    scanf("%d", &allCon[idx].group_id);
    clearBuf();
    printf("修改隐私等级(1/2/3)：");
    scanf("%d", &allCon[idx].privacy);
    clearBuf();
    printf("修改备注：");
    scanf("%s", allCon[idx].remark);
    clearBuf();
    allCon[idx].last_contact_ts = getNowTimeStamp();
    autoCreateLabel(allCon, count);
    writeContactBin(allCon, count, "contact.bin");
    OpLog log;
    strcpy(log.opName, allCon[idx].name);
    strcpy(log.opPhone, allCon[idx].phone);
    strcpy(log.msg, "修改联系人信息");
    appendLogBin(log, "log.bin");
    printf("修改成功！\n");
}

void delContact() {
    Contact allCon[MAX_CONTACT];
    char phone[12];
    int count = readContactBin(allCon, "contact.bin");
    int i, idx = -1;
    char opt;
    printf("请输入要删除的手机号：");
    scanf("%s", phone);
    clearBuf();
    for (i = 0; i < count; i++) {
        if (strcmp(allCon[i].phone, phone) == 0) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        printf("无该联系人！\n");
        return;
    }
    printf("确认删除？(y/n)：");
    scanf("%c", &opt);
    clearBuf();
    if (opt != 'y' && opt != 'Y') {
        printf("取消删除\n");
        return;
    }
    OpLog tempLog;
    strcpy(tempLog.opName, allCon[idx].name);
    strcpy(tempLog.opPhone, allCon[idx].phone);
    strcpy(tempLog.msg, "删除联系人");
    appendLogBin(tempLog, "log.bin");
    for (i = idx; i < count - 1; i++) {
        allCon[i] = allCon[i + 1];
    }
    count--;
    writeContactBin(allCon, count, "contact.bin");
    printf("删除成功！\n");
}

void addBlack() {
    Contact allCon[MAX_CONTACT];
    char phone[12];
    int count = readContactBin(allCon, "contact.bin");
    int i, idx = -1;
    printf("请输入要拉黑的手机号：");
    scanf("%s", phone);
    clearBuf();
    for (i = 0; i < count; i++) {
        if (strcmp(allCon[i].phone, phone) == 0) idx = i;
    }
    if (idx == -1) {
        printf("联系人不存在！\n");
        return;
    }
    if (allCon[idx].isBlack == 1) {
        printf("该联系人已在黑名单！\n");
        return;
    }
    allCon[idx].isBlack = 1;
    autoCreateLabel(allCon, count);
    writeContactBin(allCon, count, "contact.bin");
    OpLog log;
    strcpy(log.opName, allCon[idx].name);
    strcpy(log.opPhone, allCon[idx].phone);
    strcpy(log.msg, "拉黑联系人");
    appendLogBin(log, "log.bin");
    printf("拉黑操作成功！\n");
}

// ====================== 联系人扩展功能 ======================
void checkTempContact() {
    Contact contactBuf[MAX_CONTACT];
    int contactCnt = readContactBin(contactBuf, "contact.bin");
    Sms smsBuf[MAX_SMS];
    int smsCnt = 0;
    long long nowTs = getNowTimeStamp();
    char delPhoneList[MAX_CONTACT][12];
    int delPhoneNum = 0;
    int i, s, p;
    if (contactCnt == 0) {
        printf("无有效联系人数据，跳过临时联系人清理！\n");
        return;
    }
    loadSmsBin(smsBuf, &smsCnt);
    int newContactCnt = 0;
    for (i = 0; i < contactCnt; i++) {
        if (contactBuf[i].is_temp == 1 && nowTs > contactBuf[i].valid_ts) {
            strcpy(delPhoneList[delPhoneNum], contactBuf[i].phone);
            delPhoneNum++;
            continue;
        }
        contactBuf[newContactCnt++] = contactBuf[i];
    }
    int newSmsCnt = 0;
    for (s = 0; s < smsCnt; s++) {
        int skip = 0;
        for (p = 0; p < delPhoneNum; p++) {
            if (strcmp(smsBuf[s].phone, delPhoneList[p])==0)
                skip = 1;
        }
        if (!skip)
            smsBuf[newSmsCnt++] = smsBuf[s];
    }
    saveContactBin(contactBuf, newContactCnt);
    saveSmsBin(smsBuf, newSmsCnt);
    printf("自动清理完成！共删除%d个过期临时联系人及关联短信\n", delPhoneNum);
    OpLog log;
    strcpy(log.opName, "系统");
    strcpy(log.opPhone, "system");
    char msg[50];
    sprintf(msg, "清理%d个过期临时联系人", delPhoneNum);
    strcpy(log.msg, msg);
    appendLogBin(log, "log.bin");
}

void groupSendSms(int gid) {
    Contact contactBuf[MAX_CONTACT];
    int contactCnt = readContactBin(contactBuf, "contact.bin");
    Sms sendBatch[MAX_CONTACT];
    int sendNum = 0;
    char inputText[120];
    int i;
    for (i = 0; i < contactCnt; i++) {
        if (contactBuf[i].group_id == gid && contactBuf[i].isBlack == 0) {
            strcpy(sendBatch[sendNum].phone, contactBuf[i].phone);
            sendBatch[sendNum].send_time = getNowTimeStamp();
            sendBatch[sendNum].send_state = 1;
            strcpy(sendBatch[sendNum].sender, "system");
            sendBatch[sendNum].sms_type = 1;
            sendNum++;
        }
    }
    if (sendNum == 0) {
        printf("该分组无有效联系人，群发短信终止！\n");
        return;
    }
    printf("请输入群发短信内容：");
    scanf("%s", inputText);
    clearBuf();
    if (!verifyPhoneText(inputText)) {
        printf("内容包含非法字符，群发短信终止！\n");
        return;
    }
    for (i = 0; i < sendNum; i++) {
        strcpy(sendBatch[i].content, inputText);
    }
    appendSmsBin(sendBatch, sendNum);
    printf("群发短信成功！共发送%d条\n", sendNum);
    OpLog log;
    strcpy(log.opName, "系统");
    strcpy(log.opPhone, "system");
    char msg[50];
    sprintf(msg, "向分组%d发送%d条短信", gid, sendNum);
    strcpy(log.msg, msg);
    appendLogBin(log, "log.bin");
}

void autoCreateLabel(Contact arr[], int len) {
    int i;
    char prefix[4];
    for (i = 0; i < len; i++) {
        strncpy(prefix, arr[i].phone, 3);
        prefix[3] = '\0';
        if(
    	strcmp(prefix, "134")==0 || strcmp(prefix, "135")==0 || strcmp(prefix, "136")==0 ||
    	strcmp(prefix, "137")==0 || strcmp(prefix, "138")==0 || strcmp(prefix, "139")==0 ||
    	strcmp(prefix, "147")==0 || strcmp(prefix, "148")==0 || strcmp(prefix, "150")==0 ||
    	strcmp(prefix, "151")==0 || strcmp(prefix, "152")==0 || strcmp(prefix, "157")==0 ||
    	strcmp(prefix, "158")==0 || strcmp(prefix, "159")==0 || strcmp(prefix, "172")==0 ||
    	strcmp(prefix, "178")==0 || strcmp(prefix, "182")==0 || strcmp(prefix, "183")==0 ||
   		strcmp(prefix, "184")==0 || strcmp(prefix, "187")==0 || strcmp(prefix, "188")==0 ||
    	strcmp(prefix, "195")==0 || strcmp(prefix, "198")==0
	)
	{
   		strcpy(arr[i].label, "移动");
	}
	else if(
	    strcmp(prefix, "130")==0 || strcmp(prefix, "131")==0 || strcmp(prefix, "132")==0 ||
	    strcmp(prefix, "145")==0 || strcmp(prefix, "146")==0 || strcmp(prefix, "155")==0 ||
	    strcmp(prefix, "156")==0 || strcmp(prefix, "166")==0 || strcmp(prefix, "171")==0 ||
	    strcmp(prefix, "175")==0 || strcmp(prefix, "176")==0 || strcmp(prefix, "185")==0 ||
	    strcmp(prefix, "186")==0 || strcmp(prefix, "196")==0
	)
	{
   		strcpy(arr[i].label, "联通");
	}
	else if(
    	strcmp(prefix, "133")==0 || strcmp(prefix, "149")==0 || strcmp(prefix, "153")==0 ||
    	strcmp(prefix, "162")==0 || strcmp(prefix, "173")==0 || strcmp(prefix, "177")==0 ||
    	strcmp(prefix, "180")==0 || strcmp(prefix, "181")==0 || strcmp(prefix, "189")==0 ||
    	strcmp(prefix, "190")==0 || strcmp(prefix, "191")==0 || strcmp(prefix, "193")==0 ||
    	strcmp(prefix, "199")==0
	)
	{
    	strcpy(arr[i].label, "电信");
	}
	else
	{
   		strcpy(arr[i].label, "未知运营商");
	}
        if (strstr(arr[i].remark, "家人")) {
            strcat(arr[i].label, ",家人");
        }
        if (strstr(arr[i].remark, "同事")) {
            strcat(arr[i].label, ",同事");
        }
        if (arr[i].is_temp == 1) {
            strcat(arr[i].label, ",临时联系人");
        }
        if (arr[i].isBlack == 1) {
            strcat(arr[i].label, ",黑名单");
        }
    }
}

void remindAnniversary() {
    Contact contactBuf[MAX_CONTACT];
    int contactCnt = readContactBin(contactBuf, "contact.bin");
    int i;
    long long today = getNowTimeStamp();
    int hasRemind = 0;
    printf("\n===== 纪念日提醒 =====\n");
    for (i = 0; i < contactCnt; i++) {
        if (contactBuf[i].anniv_ts != 0 && isSameMD(today, contactBuf[i].anniv_ts)) {
            printf("今日纪念日：%s（%s）\n", contactBuf[i].name, contactBuf[i].phone);
            hasRemind = 1;
        }
        long long gap = today - contactBuf[i].last_contact_ts;
        if (gap > 3600 * 24 * 90) {
            printf("久未联系：%s（%s），已90天未联系\n", contactBuf[i].name, contactBuf[i].phone);
            hasRemind = 1;
        }
    }
    if (!hasRemind) {
        printf("暂无纪念日/久未联系提醒\n");
    }
}

void printContactInfo(Contact c) {
    char md[20];
	printf("\n-------------------------\n");
    printf("ID：%d\n", c.id);
    printf("姓名：%s\n", c.name);
    printf("手机号：%s\n", c.phone);
    printf("分组：%s（ID：%d）\n", c.group, c.group_id);
    printf("隐私等级：%d\n", c.privacy);
    printf("标签：%s\n", c.label);
    printf("备注：%s\n", c.remark);
    printf("黑名单：%s\n", c.isBlack ? "是" : "否");
    printf("是否临时：%s\n", c.is_temp ? "是" : "否");
    getMonthDay(c.anniv_ts, md);
	printf("纪念日：%s\n", md);
    printf("-------------------------\n");
}

void initNewContact(Contact *c) {
    c->id = 0;
    strcpy(c->name, "未命名");
    strcpy(c->phone, "");
    strcpy(c->group, "默认分组");
    c->privacy = 1;
    c->isBlack = 0;
    c->is_temp = 0;
    c->valid_ts = 0;
    c->group_id = 1;
    strcpy(c->label, "默认");
    strcpy(c->remark, "");
    c->anniv_ts = 0;
    c->last_contact_ts = getNowTimeStamp();
    c->priority = 5;
    c->create_ts = getNowTimeStamp();
    strcpy(c->source, "手动添加");
    strcpy(c->custom_tag, "");
}

void filterByGroup() {
    Contact allCon[MAX_CONTACT];
    char gName[15];
    int count = readContactBin(allCon, "contact.bin");
    int i, num = 0;
    printf("请输入要筛选的分组名称：");
    scanf("%s", gName);
    clearBuf();
    printf("\n【%s】分组的联系人：\n", gName);
    for (i = 0; i < count; i++) {
        if (strcmp(allCon[i].group, gName) == 0) {
            printf("%s\t%s\t标签：%s\n", allCon[i].name, allCon[i].phone, allCon[i].label);
            num++;
        }
    }
    if (num == 0) printf("该分组无联系人\n");
}

void filterByGroupID(int gid)
{
    Contact allCon[MAX_CONTACT];
    int count = readContactBin(allCon, "contact.bin");
    int i, num = 0;
    // printf单独一行，末尾加分号，再换行写for循环
    printf("\n【分组ID：%d】的联系人：\n", gid);
    for (i = 0; i < count; i++)
    {
        if (allCon[i].group_id == gid)
        {
            printf("%s\t%s\t标签：%s\n", allCon[i].name, allCon[i].phone, allCon[i].label);
            num++;
        }
    }
    if (num == 0)
        printf("该分组ID无联系人\n");
}
void sortContactByName() {
    Contact allCon[MAX_CONTACT];
    int count = readContactBin(allCon, "contact.bin");
    int i, j;
    Contact temp;
    if (count == 0) {
        printf("通讯录无数据，无需排序！\n");
        return;
    }
    for (i = 0; i < count - 1; i++) {
        for (j = 0; j < count - i - 1; j++) {
            if (strcmp(allCon[j].name, allCon[j + 1].name) > 0) {
                temp = allCon[j];
                allCon[j] = allCon[j + 1];
                allCon[j + 1] = temp;
            }
        }
    }
    writeContactBin(allCon, count, "contact.bin");
    printf("联系人已按姓名升序排序！\n");
    OpLog log;
    strcpy(log.opName, "系统");
    strcpy(log.opPhone, "system");
    strcpy(log.msg, "联系人按姓名排序");
    appendLogBin(log, "log.bin");
}

void showLog() {
    OpLog log;
    FILE *fp = fopen("log.bin", "rb");
    if (fp == NULL) {
        printf("暂无操作日志！\n");
        return;
    }
    printf("\n================操作日志================\n");
    while (fread(&log, sizeof(OpLog), 1, fp)) {
        printf("操作人：%s | 关联手机号：%s | 操作：%s\n", log.opName, log.opPhone, log.msg);
    }
    fclose(fp);
    printf("========================================\n");
}

// ====================== 短信功能 ======================
void sendOneSms()
{
    Sms s;
    char phone[12];
    Contact contacts[MAX_CONTACT];
    int cnt = readContactBin(contacts, "contact.bin");
    int i, found = 0;
    char c;
    printf("\n======= 单发短信 =======\n");
    printf("请输入对方手机号：");
    scanf("%s", phone);
    clearBuf();
    for(i=0; i<cnt; i++){
        if(strcmp(contacts[i].phone, phone) == 0){
            found = 1;
            if(contacts[i].isBlack == 1){
                printf("该联系人已拉黑，无法发送！\n");
                return;
            }
            break;
        }
    }
    if(!found){
        printf("该号码不在通讯录，是否继续发送？(y/n)：");
        scanf("%c",&c); clearBuf();
        if(c!='y' && c!='Y') return;
    }
    printf("请输入短信内容：");
    scanf("%[^\n]", s.content);
    clearBuf();
    strcpy(s.phone, phone);
    s.send_time = getNowTimeStamp();
    s.send_state = 1;
    strcpy(s.sender, "me");
    s.sms_type = 1;
    Sms list[MAX_SMS];
    int n=0;
    loadSmsBin(list, &n);
    list[n++] = s;
    saveSmsBin(list, n);
    printf("发送成功！\n");
}

void replySms()
{
    Sms sms[MAX_SMS];
    int n=0,i,idx,pos=-1,count=0;
    loadSmsBin(sms, &n);
    printf("\n======= 收到的短信 =======\n");
    int has = 0;
    for(i=0; i<n; i++){
        if(sms[i].sms_type == 2){
            printf("%d、来自：%s 内容：%s\n", ++has, sms[i].phone, sms[i].content);
        }
    }
    if(has ==0){
        printf("暂无收到的短信！\n");
        return;
    }
    printf("请选择要回复的序号：");
    scanf("%d",&idx); clearBuf();
    for(i=0; i<n; i++){
        if(sms[i].sms_type ==2){
            count++;
            if(count == idx){
                pos = i;
                break;
            }
        }
    }
    if(pos ==-1){
        printf("序号错误！\n");
        return;
    }
    Sms rep;
    printf("请输入回复内容：");
    scanf("%[^\n]", rep.content);
    clearBuf();
    strcpy(rep.phone, sms[pos].phone);
    rep.send_time = getNowTimeStamp();
    rep.send_state =1;
    strcpy(rep.sender, "me");
    rep.sms_type =1;
    sms[n++] = rep;
    saveSmsBin(sms,n);
    printf("回复成功！\n");
}

void showReceivedSms()
{
    Sms sms[MAX_SMS];
    int n=0,i;
    loadSmsBin(sms, &n);
    printf("\n======= 收到的短信 =======\n");
    int has=0;
    for(i=0; i<n; i++){
        if(sms[i].sms_type == 2){
            printf("来自：%s\n内容：%s\n", sms[i].phone, sms[i].content);
            printf("------------------------\n");
            has++;
        }
    }
    if(has ==0) printf("暂无收到的短信！\n");
}

void addReceiveSms()
{
    Sms s;
    char phone[12];
    printf("\n======= 模拟收到短信 =======\n");
    printf("请输入对方手机号：");
    scanf("%s", phone);
    clearBuf();
    printf("请输入短信内容：");
    scanf("%[^\n]", s.content);
    clearBuf();
    strcpy(s.phone, phone);
    s.send_time = getNowTimeStamp();
    s.send_state = 1;
    strcpy(s.sender, "other");
    s.sms_type = 2;
    Sms smsList[MAX_SMS];
    int n = 0;
    loadSmsBin(smsList, &n);
    smsList[n++] = s;
    saveSmsBin(smsList, n);
    printf("成功收到一条短信！\n");
}

// ====================== 工具模块（无全局数组，全部局部传参） ======================
void loadBinaryDataToMemory(ContactInfo data[], int *total, int groupCnt[], int hourCnt[])
{
    FILE *binFile = fopen(MAIN_BIN_FILE, "rb");
    int i;
    memset(data, 0, sizeof(ContactInfo)*MAX_STORE_NUM);
    memset(groupCnt, 0, sizeof(int)*4);
    memset(hourCnt, 0, sizeof(int)*24);
    *total = 0;
    if(binFile == NULL)
    {
        printf("【数据加载提示】本地不存在二进制通讯录存储文件\n");
        return;
    }
    *total = fread(data, sizeof(ContactInfo), MAX_STORE_NUM, binFile);
    fclose(binFile);
    for(i = 0; i < *total; i++)
    {
        if(strcmp(data[i].group, "默认分组") == 0) groupCnt[0]++;
        else if(strcmp(data[i].group, "家人") == 0) groupCnt[1]++;
        else if(strcmp(data[i].group, "朋友") == 0) groupCnt[2]++;
        else if(strcmp(data[i].group, "同事") == 0) groupCnt[3]++;
        int h = data[i].msgSendHour;
        if(h >=0 && h <=23) hourCnt[h]++;
    }
}

void batchImportVCF()
{
    char vcfFileName[80] = {0};
    char tempLine[VCF_LINE_LIMIT] = {0};
    ContactInfo localData[MAX_STORE_NUM];
    int localTotal = 0;
    int localGroup[4] = {0};
    int localHour[24] = {0};
    ContactInfo tempContact;
    int successImportNum = 0;
    int repeatContact = 0;
    int i;
    
    memset(&tempContact, 0, sizeof(ContactInfo));
    loadBinaryDataToMemory(localData, &localTotal, localGroup, localHour);
    
    printf("\n===== VCF手机联系人批量导入功能 =====\n");
    printf("请输入本地VCF文件完整名称（必须包含后缀.vcf）：");
    scanf("%s", vcfFileName);
    clearBuf();
    
    FILE *vcfFile = fopen(vcfFileName, "rb");
    if(vcfFile == NULL)
    {
        printf("【导入失败】无法读取目标VCF文件！\n");
        return;
    }
    
    while(fgets(tempLine, VCF_LINE_LIMIT, vcfFile) != NULL)
    {
        // 读取姓名
        if(strstr(tempLine, "FN:") != NULL)
        {
            char nameBuf[MAX_NAME_LEN] = {0};
            sscanf(tempLine, "FN:%s", nameBuf);
            strcpy(tempContact.name, nameBuf);
        }
        
        // 读取手机号
        if(strstr(tempLine, "TEL:") != NULL)
        {
            char phoneBuf[MAX_PHONE_LEN] = {0};
            if(strstr(tempLine, "TEL;") != NULL)
            {
                char *p = strstr(tempLine, ":");
                if(p != NULL)
                {
                    strcpy(phoneBuf, p + 1);
                    int len = strlen(phoneBuf);
                    if(len > 0 && phoneBuf[len-1] == '\n')
                        phoneBuf[len-1] = '\0';
                    if(len > 1 && phoneBuf[len-2] == '\r')
                        phoneBuf[len-2] = '\0';
                }
            }
            else
            {
                sscanf(tempLine, "TEL:%s", phoneBuf);
            }
            strcpy(tempContact.phone, phoneBuf);
        }
        
        // 读取分组
        if(strstr(tempLine, "CATEGORIES:") != NULL)
        {
            char groupBuf[MAX_GROUP_LEN] = {0};
            sscanf(tempLine, "CATEGORIES:%s", groupBuf);
            int len = strlen(groupBuf);
            if(len > 0 && groupBuf[len-1] == '\n')
                groupBuf[len-1] = '\0';
            if(len > 1 && groupBuf[len-2] == '\r')
                groupBuf[len-2] = '\0';
            strcpy(tempContact.group, groupBuf);
        }
        
        // 读取黑名单标记
        if(strstr(tempLine, "X-BLACKLIST:") != NULL || strstr(tempLine, "X-BLACK:") != NULL)
        {
            int blackFlag = 0;
            if(strstr(tempLine, "X-BLACKLIST:") != NULL)
                sscanf(tempLine, "X-BLACKLIST:%d", &blackFlag);
            else
                sscanf(tempLine, "X-BLACK:%d", &blackFlag);
            tempContact.isBlackList = blackFlag;
        }
        
        // 遇到 END:VCARD 表示一个联系人读取完成
        if(strstr(tempLine, "END:VCARD") != NULL)
        {
            // 检查是否重复
            int repeatFlag = 0;
            for(i = 0; i < localTotal; i++)
            {
                if(strcmp(localData[i].name, tempContact.name) == 0 && 
                   strcmp(localData[i].phone, tempContact.phone) == 0)
                {
                    repeatFlag = 1;
                    break;
                }
            }
            if(repeatFlag)
            {
                repeatContact++;
                memset(&tempContact, 0, sizeof(ContactInfo));
                continue;
            }
            
            // 如果没有分组信息，让用户选择
            if(strlen(tempContact.group) == 0)
            {
                int groupChoice;
                printf("\n联系人：%s (%s) 未设置分组\n", tempContact.name, tempContact.phone);
                printf("请选择分组：1.默认分组  2.家人  3.朋友  4.同事  5.自定义\n");
                printf("请输入数字：");
                scanf("%d", &groupChoice);
                clearBuf();

                switch(groupChoice)
                {
                    case 2:
                        strcpy(tempContact.group, "家人");
                        break;
                    case 3:
                        strcpy(tempContact.group, "朋友");
                        break;
                    case 4:
                        strcpy(tempContact.group, "同事");
                        break;
                    case 5:
                        printf("请输入自定义分组名称：");
                        scanf("%s", tempContact.group);
                        clearBuf();
                        break;
                    default:
                        strcpy(tempContact.group, "默认分组");
                        break;
                }
            }
            
            tempContact.msgSendHour = rand() % 24;
            
            if(localTotal >= MAX_STORE_NUM)
            {
                printf("容量已满，停止导入\n");
                memset(&tempContact, 0, sizeof(ContactInfo));
                break;
            }
            
            printf("【导入】%s (%s) 分组：%s 黑名单：%s\n", 
                   tempContact.name, 
                   tempContact.phone, 
                   tempContact.group,
                   tempContact.isBlackList ? "是" : "否");
            
            localData[localTotal++] = tempContact;
            successImportNum++;
            memset(&tempContact, 0, sizeof(ContactInfo));
        }
    }
    
    fclose(vcfFile);
    
    FILE *binStore = fopen(MAIN_BIN_FILE, "wb");
    fwrite(localData, sizeof(ContactInfo), localTotal, binStore);
    fclose(binStore);
    
    printf("\n========== 导入完成 ==========\n");
    printf("成功导入：%d人\n", successImportNum);
    printf("重复跳过：%d人\n", repeatContact);
}

void exportDesensitizeData(int exportType)
{
    ContactInfo localData[MAX_STORE_NUM];
    int localTotal = 0;
    int localGroup[4] = {0};
    int localHour[24] = {0};
    loadBinaryDataToMemory(localData, &localTotal, localGroup, localHour);
    if(localTotal <= 0)
    {
        printf("【导出终止】当前通讯录无任何联系人数据，无法生成导出文件\n");
        return;
    }
    FILE *fp = NULL;
    ContactInfo tmp;
    int i;
    if(exportType == 1)
    {
        fp = fopen(EXPORT_CSV_NAME, "wb");
        fprintf(fp, "联系人姓名,手机号码,所属分组,是否黑名单标记\n");
    }
    else
    {
        fp = fopen(EXPORT_TXT_NAME, "wb");
        fprintf(fp,"================================================\n");
        fprintf(fp,"通讯录对外分发脱敏文件 禁止直接传播原始隐私数据\n");
        fprintf(fp,"================================================\n\n");
    }
    if(fp == NULL)
    {
        printf("【导出失败】目标文件创建失败，请检查磁盘剩余空间！\n");
        return;
    }
    for(i = 0; i < localTotal; i++)
    {
        tmp = localData[i];
        mobileDesensitize(tmp.phone);
        if(exportType == 1)
        {
            fprintf(fp,"%s,%s,%s,%d\n",tmp.name,tmp.phone,tmp.group,tmp.isBlackList);
        }
        else
        {
            // 修复三元表达式初始化数组报错，改用strcpy赋值
            char flag[4];
            if(tmp.isBlackList == 1)
                strcpy(flag, "是");
            else
                strcpy(flag, "否");
            fprintf(fp,"姓名:%s 手机:%s 分组:%s 黑名单:%s\n",tmp.name,tmp.phone,tmp.group,flag);
        }
    }
    fclose(fp);
    printf("===== 文件导出操作完成 =====\n");
    if(exportType == 1)
        printf("导出文件：%s\n", EXPORT_CSV_NAME);
    else
        printf("导出文件：%s\n", EXPORT_TXT_NAME);
    printf("所有敏感手机号仅在内存临时脱敏，二进制源文件数据保持原始完整\n");
}

void showContactGroupChart(ContactInfo data[], int total, int groupCnt[])
{
    int all = groupCnt[0] + groupCnt[1] + groupCnt[2] + groupCnt[3];
    float r0=0,r1=0,r2=0,r3=0;
    int i;
    if(all>0)
    {
        r0 = (float)groupCnt[0]/all*100;
        r1 = (float)groupCnt[1]/all*100;
        r2 = (float)groupCnt[2]/all*100;
        r3 = (float)groupCnt[3]/all*100;
    }
    printf("总人数:%d\n默认分组%d(%.2f%%):",groupCnt[0],r0);
    for(i=0;i<groupCnt[0];i++) printf("■");
    printf("\n家人%d(%.2f%%):",groupCnt[1],r1);
    for(i=0;i<groupCnt[1];i++) printf("■");
    printf("\n朋友%d(%.2f%%):",groupCnt[2],r2);
    for(i=0;i<groupCnt[2];i++) printf("■");
    printf("\n同事%d(%.2f%%):",groupCnt[3],r3);
    for(i=0;i<groupCnt[3];i++) printf("■\n");
}

void showBlackWhiteChart(ContactInfo data[], int total)
{
    int i, normal=0, black=0;
    float rn,rb;
    for(i=0;i<total;i++)
    {
        if(data[i].isBlackList) black++;
        else normal++;
    }
    int sum = normal+black;
    rn = sum?(float)normal/sum*100:0;
    rb = sum?(float)black/sum*100:0;
    printf("总记录%d\n正常%d(%.2f%%):",normal,rn);
    for(i=0;i<normal;i++) printf("▇");
    printf("\n黑名单%d(%.2f%%):",black,rb);
    for(i=0;i<black;i++) printf("▇\n");
}

void showMsgTimeDistributeChart(int hourCnt[])
{
    int h,i,max=0,peak=0;
    for(h=0;h<24;h++)
    {
        printf("%02d时:%d条 ",hourCnt[h]);
        for(i=0;i<hourCnt[h];i++) printf("?");
        printf("\n");
        if(hourCnt[h]>max)
        {
            max = hourCnt[h];
            peak = h;
        }
    }
    printf("高峰时段%d点\n",peak);
}
void showAllVisualAnalysis()
{
    Contact allCon[MAX_CONTACT];
    int count = readContactBin(allCon, "contact.bin");
    int i, j;
    int normal = 0, black = 0;

    // ===== 统计主通讯录 (contact.bin) =====
    printf("\n========================================\n");
    printf("          主通讯录统计\n");
    printf("========================================\n");
    
    if(count <= 0)
    {
        printf("  主通讯录无数据\n");
    }
    else
    {
        char groupNames[MAX_CONTACT][15];
        int groupCounts[MAX_CONTACT] = {0};
        int groupNum = 0;
        int found;

        for(i = 0; i < count; i++)
        {
            if(allCon[i].isBlack == 1)
                black++;
            else
                normal++;

            found = 0;
            for(j = 0; j < groupNum; j++)
            {
                if(strcmp(groupNames[j], allCon[i].group) == 0)
                {
                    groupCounts[j]++;
                    found = 1;
                    break;
                }
            }
            if(!found && strlen(allCon[i].group) > 0)
            {
                strcpy(groupNames[groupNum], allCon[i].group);
                groupCounts[groupNum] = 1;
                groupNum++;
            }
        }

        printf("  总人数: %d人\n", count);
        printf("  ------------------------------\n");
        for(i = 0; i < groupNum; i++)
        {
            float ratio = (float)groupCounts[i] / count * 100;
            printf("  %-10s %3d人 ( %5.1f%% ) ", groupNames[i], groupCounts[i], ratio);
            int barLen = (int)(groupCounts[i] * 30 / count);
            if(barLen < 1 && groupCounts[i] > 0) barLen = 1;
            for(j = 0; j < barLen && j < 30; j++) printf("■");
            printf("\n");
        }

        printf("  ------------------------------\n");
        int sum = normal + black;
        float rn = (float)normal / sum * 100;
        float rb = (float)black / sum * 100;
        printf("  正常   %3d人 ( %5.1f%% ) ", normal, rn);
        int barLenN = (int)(normal * 30 / sum);
        if(barLenN < 1 && normal > 0) barLenN = 1;
        for(i = 0; i < barLenN && i < 30; i++) printf("▇");
        printf("\n");
        printf("  黑名单 %3d人 ( %5.1f%% ) ", black, rb);
        int barLenB = (int)(black * 30 / sum);
        if(barLenB < 1 && black > 0) barLenB = 1;
        for(i = 0; i < barLenB && i < 30; i++) printf("▇");
        printf("\n");
    }

    // ===== 统计VCF工具数据 (MAIN_BIN_FILE) =====
    printf("\n========================================\n");
    printf("          VCF工具数据统计\n");
    printf("========================================\n");
    
    FILE *check = fopen(MAIN_BIN_FILE, "rb");
    if(check == NULL)
    {
        printf("  VCF工具无数据（请使用菜单18导入）\n");
        return;
    }
    fclose(check);
    
    ContactInfo vcfData[MAX_STORE_NUM];
    int vcfTotal = 0;
    int vcfGroup[4] = {0};
    int vcfHour[24] = {0};

    loadBinaryDataToMemory(vcfData, &vcfTotal, vcfGroup, vcfHour);

    if(vcfTotal <= 0)
    {
        printf("  VCF工具无数据（请使用菜单18导入）\n");
        return;
    }

    // VCF 动态统计所有分组
    char vcfGroupNames[MAX_STORE_NUM][15];
    int vcfGroupCounts[MAX_STORE_NUM] = {0};
    int vcfGroupNum = 0;
    int vcfNormal = 0, vcfBlack = 0;
    int found2;

    for(i = 0; i < vcfTotal; i++)
    {
        if(vcfData[i].isBlackList == 1)
            vcfBlack++;
        else
            vcfNormal++;

        found2 = 0;
        for(j = 0; j < vcfGroupNum; j++)
        {
            if(strcmp(vcfGroupNames[j], vcfData[i].group) == 0)
            {
                vcfGroupCounts[j]++;
                found2 = 1;
                break;
            }
        }
        if(!found2 && strlen(vcfData[i].group) > 0)
        {
            strcpy(vcfGroupNames[vcfGroupNum], vcfData[i].group);
            vcfGroupCounts[vcfGroupNum] = 1;
            vcfGroupNum++;
        }
    }

    printf("  总人数: %d人\n", vcfTotal);
    printf("  ------------------------------\n");
    for(i = 0; i < vcfGroupNum; i++)
    {
        float ratio = (float)vcfGroupCounts[i] / vcfTotal * 100;
        printf("  %-10s %3d人 ( %5.1f%% ) ", vcfGroupNames[i], vcfGroupCounts[i], ratio);
        int barLen = (int)(vcfGroupCounts[i] * 30 / vcfTotal);
        if(barLen < 1 && vcfGroupCounts[i] > 0) barLen = 1;
        for(j = 0; j < barLen && j < 30; j++) printf("■");
        printf("\n");
    }

    printf("  ------------------------------\n");
    int vcfSum = vcfNormal + vcfBlack;
    float vcfRn = (float)vcfNormal / vcfSum * 100;
    float vcfRb = (float)vcfBlack / vcfSum * 100;
    printf("  正常   %3d人 ( %5.1f%% ) ", vcfNormal, vcfRn);
    int vcfBarLenN = (int)(vcfNormal * 30 / vcfSum);
    if(vcfBarLenN < 1 && vcfNormal > 0) vcfBarLenN = 1;
    for(i = 0; i < vcfBarLenN && i < 30; i++) printf("▇");
    printf("\n");
    printf("  黑名单 %3d人 ( %5.1f%% ) ", vcfBlack, vcfRb);
    int vcfBarLenB = (int)(vcfBlack * 30 / vcfSum);
    if(vcfBarLenB < 1 && vcfBlack > 0) vcfBarLenB = 1;
    for(i = 0; i < vcfBarLenB && i < 30; i++) printf("▇");
    printf("\n");
    printf("========================================\n");
}

void binaryDataEncrypt(unsigned char *data, int len)
{
    int i;
    for(i=0;i<len;i++) data[i]^=ENCRYPT_KEY;
}

void createVersionBackup()
{
    char name[100];
    sprintf(name,"backup_%lld.bin",getNowTimeStamp());
    FILE *in = fopen(MAIN_BIN_FILE,"rb");
    FILE *out = fopen(name,"wb");
    unsigned char buf[BACKUP_BUFF_LEN];
    int len;
    if(in==NULL||out==NULL)
    {
        printf("备份失败\n");
        if(in)fclose(in);
        if(out)fclose(out);
        return;
    }
    while((len = fread(buf,1,BACKUP_BUFF_LEN,in))>0)
    {
        binaryDataEncrypt(buf,len);
        fwrite(buf,1,len,out);
    }
    fclose(in);fclose(out);
    printf("备份文件:%s 创建成功\n",name);
}

void restoreHistoryBackup()
{
    char bakName[100],confirm[10];
    printf("警告恢复覆盖现有数据，输入yes确认：");
    scanf("%s",confirm);clearBuf();
    if(strcmp(confirm,"yes")!=0) return;
    printf("输入备份文件名：");
    scanf("%s",bakName);clearBuf();
    FILE *bak = fopen(bakName,"rb");
    FILE *main = fopen(MAIN_BIN_FILE,"wb");
    unsigned char buf[BACKUP_BUFF_LEN];
    int len;
    if(bak==NULL||main==NULL)
    {
        printf("文件错误\n");
        if(bak)fclose(bak);
        if(main)fclose(main);
        return;
    }
    while((len=fread(buf,1,BACKUP_BUFF_LEN,bak))>0)
    {
        binaryDataEncrypt(buf,len);
        fwrite(buf,1,len,main);
    }
    fclose(bak);fclose(main);
    printf("恢复完成\n");
}

// ====================== 菜单 ======================
void loginMenu() {
    int sel;
    printf("====欢迎使用通讯录管理系统====\n");
    printf("1.账号登录  2.注册新账号\n");
    printf("请选择：");
    scanf("%d", &sel);
    clearBuf();
    if (sel == 1) {
        if (loginCheck()) {
            checkTempContact();
            remindAnniversary();
            mainMenu();
        } else {
            loginMenu();
        }
    } else if (sel == 2) {
        registerUser();
        loginMenu();
    } else {
        printf("输入错误！\n");
        loginMenu();
    }
}

void mainMenu() {
    int sel, gid;
    while (1) {
        printf("\n=====通讯录管理系统主菜单=====\n");
        printf("1.添加联系人\t2.查看所有联系人\n");
        printf("3.查询联系人\t4.修改联系人\n");
        printf("5.删除联系人\t6.按名称筛选分组\n");
        printf("7.拉黑联系人\t8.查看操作日志\n");
        printf("9.清理临时联系人\n");
        printf("10.群发短信\t11.纪念日提醒\n");
        printf("12.按ID筛选分组\t13.按姓名排序\n");
        printf("14.单发短信\t15.回复短信\n");
        printf("16.查看收到短信\t17.模拟收短信\n");
        printf("18.VCF导入/脱敏导出工具\n");
        printf("19.数据可视化统计图表\n");
        printf("20.加密备份与恢复\n");
        printf("0.退出系统\n");
        printf("请输入操作选项：");
        scanf("%d", &sel);
        clearBuf();
        switch (sel) {
            case 1: addContact(); break;
            case 2: showAllContact(); break;
            case 3: searchContact(); break;
            case 4: modifyContact(); break;
            case 5: delContact(); break;
            case 6: filterByGroup(); break;
            case 7: addBlack(); break;
            case 8: showLog(); break;
            case 9: checkTempContact(); break;
            case 10:
                printf("分组ID：");
                scanf("%d",&gid);clearBuf();
                groupSendSms(gid);break;
            case 11: remindAnniversary(); break;
            case 12:
                printf("分组ID：");
                scanf("%d",&gid);clearBuf();
                filterByGroupID(gid);break;
            case 13: sortContactByName(); break;
            case 14: sendOneSms(); break;
            case 15: replySms(); break;
            case 16: showReceivedSms(); break;
            case 17: addReceiveSms(); break;
            case 18:
			{
    			int op;
    			printf("1.VCF批量导入 2.导出CSV 3.导出TXT\n选择：");
    			scanf("%d",&op);clearBuf();
    			if(op==1) 
    				{
        				batchImportVCF();
    				}
    			else if(op == 2) 
   					{
        				exportDesensitizeData(1); // 选2 CSV，传1
    				}
    			else if(op == 3)
   					{
        				exportDesensitizeData(2); // 选3 TXT，传2
    				}
    			else 
    				{
        				printf("无效\n");
    				}
    			break;
			}
            case 19: showAllVisualAnalysis(); break;
            case 20:
            {
                int op;
                printf("1.创建加密备份 2.恢复备份\n选择：");
                scanf("%d",&op);clearBuf();
                if(op==1) createVersionBackup();
                else if(op==2) restoreHistoryBackup();
                else printf("无效\n");
                break;
            }
            case 0: printf("系统退出\n");return;
            default: printf("输入错误\n");
        }
        printf("\n回车返回菜单...");
        getchar();
    }
}

int main() {
    srand((unsigned int)time(NULL));
    systemInit();
    SetConsoleOutputCP(936);
    loginMenu();
    return 0;
}
