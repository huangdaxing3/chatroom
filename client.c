#include <stdio.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "my_pack.h"

void getch(char *buf);
void *serv_cli(void *clifd);
void *cli_serv(void *clifd);
void *recv_box(void *fd);

PACK *pack_write;
PACK *pack_read;
BOX *box;
FRIEND *friend_list;
MESSAGE *mess;
GROUP_LIST *group_list;
GROUP_MEMBER *group_member_list;
FILE_RE *file;
int mark;

int main(int argc, char **argv)
{
    int i;
    int ret;
    int serv_port;
    struct sockaddr_in serv_addr;
    char recv_buf[BUFSIZE];
    
    int clifd;
    pthread_t pid_send;
    pthread_t pid_recv;
    // char *a = *argv;

    mark = 0;
    pthread_mutex_init(&n_mutex, NULL);
    pthread_cond_init(&n_cond, NULL);
    //检查参数个数
    if ( argc != 5 ) {
        printf("Usage: [-p] [serv_port] [-a] [serv_address]\n");
        exit(1);
    }
    //初始化服务器端地址结构
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    //从命令行获取服务器端的端口与地址
    for (i=1; i<argc; i++) 
    {
        if ( strcmp("-p", argv[i]) == 0 ) 
        {
            serv_port = atoi(argv[i+1]);      //atoi()把字符串转换成整型数
            if ( serv_port < 0 || serv_port > 65535 ) 
            {
                printf("invalid serv_addr.sin_port\n");
                exit(1);
            } else 
            {
                serv_addr.sin_port = htons(serv_port);     //htons()主机字节顺序转换为网络字节顺序
            }
            continue;
        }

        if ( strcmp("-a", argv[i]) == 0 ) 
        {
            if ( inet_aton(argv[i+1], &serv_addr.sin_addr) == 0 ) 
            {  //inet_aton()将IPv4的字符串地址（xxx.xxx.xxx.xxx）转换成网络地址结构体 struct in_addr。 
                printf("invalid server ip address\n");
                exit(1);
            }
            continue;
        }
    }
    //检查是否少输入了某项参数
    if ( serv_addr.sin_port == 0 || serv_addr.sin_addr.s_addr == 0 ) 
    {
        printf("Usage: [-p] [serv_addr.sin_port] [-a][serv_address]\n");
        exit(1);
    }

    //创建一个TCP套接字
    clifd = socket(AF_INET, SOCK_STREAM, 0);
    if ( clifd < 0 ) 
        perror("socket");

    // //向服务器端发送连接请求
    if ( connect(clifd,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr)) < 0 ) 
        perror("connect");

    pthread_create(&pid_send, NULL, cli_serv, (void *)&clifd);
    pthread_create(&pid_recv, NULL, serv_cli, (void *)&clifd);
    pthread_join(pid_send, NULL);
    pthread_join(pid_recv, NULL);
    
	return 0;
}

//linux下没有getch()
void getch(char *buf)
{
    struct termios tm, n_tm;
    char ch;
    tcgetattr(0, &tm);
    n_tm = tm;
    n_tm.c_lflag &= ~(ICANON | ECHO);
    int  i = 0;
    while(1)
    {
        tcsetattr(0, TCSANOW, &n_tm);
        ch = getchar();
        tcsetattr(0, TCSANOW, &tm);
        if(ch=='\n')
        {
            buf[i]='\0';
            break;
        }
        buf[i]=ch;
        i++;
        printf("*");
    }
    printf("\n");
}

void *cli_serv(void *clifd) 
{
    int choice;
    int i = 0;
    pack_write = (PACK *)malloc(sizeof(PACK));
    
    while (1) 
    {
        printf("\033c");
        printf("      ********************************************************************       \n");
        printf("      ********************************************************************       \n");
        printf("      ***                         1.   注册                             ***       \n");
        printf("      ***                         2.   登录                             ***       \n");
        printf("      ********************************************************************       \n");
        printf("      ********************************************************************       \n");

        printf("请选择你需要的功能－－\n");
        scanf("%d", &choice);
        switch (choice) 
        {            
            case 1:
                {
                    pack_write->type = REG;
                    printf("请输入昵称---");
                    scanf("%s", pack_write->data.send_user);
                    getchar();
                    printf("请输入密码---");
                    getch(pack_write->data.content_buff);

                    if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                        perror("send");
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_wait(&n_cond, &n_mutex);
                    pthread_mutex_unlock(&n_mutex);

                    break;
                }
            case 2:
                {
                    pack_write->type = LOGIN;
                    printf("请输入账号---");
                    scanf("%d", &pack_write->data.send_id);
                    getchar();
                    printf("请输入密码---");
                    getch(pack_write->data.content_buff);
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                        perror("send");
                    pthread_mutex_lock(&n_mutex);
                    while (mark == 0) 
                    {
                        pthread_cond_wait(&n_cond, &n_mutex);
                    }
                    pthread_mutex_unlock(&n_mutex);
                    mark = 0;
                    break;
                }
            case 3:
                {
                    pack_write->type = EXIT;
                    if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                    {
                        perror("send");
                    }
                    pthread_exit(0);
                }
            default:
                {
                    printf("请输入正确选项！\n");
                    printf("输入回车继续......");
                    getchar();
                    getchar();
                    break;
                }
        }
        if (choice > 2 || choice < 1) 
        {
            continue;   
        } else if (choice == 2) {
            if (strcmp(pack_write->data.mess_buff, "password error") == 0) 
            {
                printf("account or password error!!\n按下回车继续.....");
                getchar();
                continue;
            } else {
                printf("\nlogin success!\n");
                printf("按下回车继续......\n");
                getchar();
                break;
            }
        } else if (choice == 1) {
            printf("registered success!\n");
            printf("您的账号为－－－%d\n", pack_write->data.send_id);
            printf("按下回车继续.......");
            getchar();
            continue;
        }
    }
    while (1) 
    {
        printf("\033c");
        printf("      ********************************************************************       \n");
        printf("      ********************************************************************       \n");
        printf("      ***                         1.   朋友管理                         ***      \n");
        printf("      ***                         2.   群管理                           ***      \n");
        printf("      ***                         3.   请求                             ***      \n");
        printf("      ***                         4.   修改密码                          ***       \n");
        printf("      ***                         5.   退出                             ***      \n");
        printf("      ********************************************************************       \n");
        printf("      ********************************************************************       \n");
        printf("\n");

        printf("请选择您将进行的操作－－－");
        scanf("%d", &choice);
        getchar();
        switch (choice) 
        {
            case 1:
                {
                    printf("\033c");
                    printf("      ********************************************************************       \n");
                    printf("      ********************************************************************       \n");
                    printf("      ***                     1.   加好友                               ***       \n");
                    printf("      ***                     2.   删好友                               ***       \n");
                    printf("      ***                     3.   显示好友列表                          ***       \n");
                    printf("      ***                     4.   拉黑好友                             ***       \n");
                    printf("      ***                     5.   拉出黑名单                            ***      \n");
                    printf("      ***                     6.   私聊【退出私聊按#exit】                ***      \n");
                    printf("      ***                     7.   查看好友消息                          ***      \n");
                    printf("      ***                     8.   查看聊天记录                          ***      \n");
                    printf("      ***                     9.   传输文件                             ***      \n");
                    printf("      ***                     10.  接收文件                             ***       \n");
                    printf("      ********************************************************************       \n");
                    printf("      ********************************************************************       \n");
                    printf("\n");

                    printf("请选择您将进行的操作－－－");
                    scanf("%d", &choice);
                    getchar();
                    switch(choice) 
                    {
                        case 1:
                            {
                                pack_write->type = ADD_FRIEND;
                                printf("输入添加好友账号－－－");
                                scanf("%d", &pack_write->data.recv_id);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                    perror("send");
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (strcmp(pack_write->data.mess_buff, "success") == 0) 
                                {
                                    printf("发送请求成功－－等待对方同意\n");
                                    printf("按下回车键继续～");
                                    getchar();
                                } else if (strcmp(pack_write->data.mess_buff, "fail") == 0){
                                    printf("此账号不存在!!!\n");
                                    printf("按下回车键继续～");
                                    getchar();
                                } else if (strcmp(pack_write->data.mess_buff, "no") == 0){
                                    printf("此人已是您的好友\n");
                                    printf("按下回车键继续～");
                                    getchar();
                                } else if (strcmp(pack_write->data.mess_buff, "not") == 0){
                                    printf("自己不能添加自己啊！\n");
                                    printf("按下回车键继续～");
                                    getchar();
                                }
                                memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                                break;
                            }
                        case 2:
                            {
                                pack_write->type = DELETE_FRIEND;
                                printf("请输入删除好友的账号－－－");
                                scanf("%d", &pack_write->data.recv_id);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                    perror("send");
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (strcmp(pack_write->data.mess_buff, "qqq") == 0) 
                                {
                                    printf("删除好友成功－－恭喜恭喜\n");
                                    getchar();
                                } else {
                                    printf("没有此好友－－");
                                    getchar();
                                }
                                memset(pack_write->data.mess_buff ,0, sizeof(pack_write->data.mess_buff));
                                break;
                            }
                        case 3:
                            {
                                pack_write->type = LOOK_FRIEND_LIST;
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                    perror("send");
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                printf("%s\n",pack_write->data.mess_buff);
                                if (strcmp(pack_write->data.mess_buff, "success") == 0) 
                                {
                                    printf("====＝＝＝＝＝＝＝好友列表＝＝＝＝＝＝＝====\n");
                                    printf("　　账号　　　　 昵称　　　　状态\n");
                                    for(int i = 0; i < friend_list->cont; i++) 
                                    {
                                        printf("   %d－－－－%s－－",friend_list->id[i],friend_list->name[i]);
                                        if (friend_list->status[i] == 1) printf("－－*上线中*－－\n");
                                        else if(friend_list->status[i] == 0) printf("－－*离线中*－－\n");
                                        else if(friend_list->status[i] == -1) printf("－*【已拉黑】*－－\n");
                                    }
                                } else {
                                    printf("－－您的好友列表为空－－\n");
                                }
                                getchar();
                                break;
                            }
                        case 4:
                            {
                                pack_write->type = BLACK_LIST;
                                printf("请输入想拉黑好友的账号－－");
                                scanf("%d",&pack_write->data.recv_id);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                    perror("send");
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (strcmp(pack_write->data.mess_buff, "success") == 0) 
                                {
                                    printf("您不喜欢的人已帮您拉黑　^^\n");
                                    getchar();
                                } else {
                                    printf("您的好友列表中无此人或已经被您屏蔽　^^\n");
                                    getchar();
                                }
                                break;
                            }
                        case 5:
                            {
                                pack_write->type = OUT_BLACK_LIST;
                                printf("请输入您想要拉出黑名单人的账号－－");
                                scanf("%d",&pack_write->data.recv_id);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                {
                                    perror("send");
                                }
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (strcmp(pack_write->data.mess_buff, "success") == 0) 
                                {
                                    printf("已从黑名单中拉出　^^\n");
                                    getchar();
                                } else {
                                    printf("您的黑名单中无此人或并非您好友　^^\n");
                                    getchar();
                                }
                                break;
                            }
                        case 6:
                            {
                                pack_write->type = PRIVATE_CHAT;
                                memset(pack_write->data.content_buff, 0, sizeof(pack_write->data.content_buff));
                                printf("输入你想要私聊对象的账号－－");
                                scanf("%d", &pack_write->data.recv_id);
                                getchar();
                                printf("曰: ");
                                while (1) 
                                {
                                    scanf("%[^\n]", pack_write->data.content_buff);
                                    getchar();
                                    if (strcmp(pack_write->data.content_buff, "#exit") == 0) break;
                                    if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                    {
                                        perror("send");
                                    }
                                    pthread_mutex_lock(&n_mutex);
                                    pthread_cond_wait(&n_cond, &n_mutex);
                                    pthread_mutex_unlock(&n_mutex);
                                    if (strcmp(pack_write->data.mess_buff, "#quit") == 0) 
                                    {
                                        printf("您没有此好友，或已拉黑，无法私聊！\n");
                                        break;
                                    }
                                    
                                }
                                printf("按下回车键继续－－－");
                                getchar();
                                break;
                            }
                        case 7:
                            {
                                if (box->mes_count == 0) 
                                {
                                    printf("你没有未看的好友消息!\n");
                                } else {
                                    for (int i = 0; i < box->mes_count; ++i) 
                                    {
                                        printf("账号%d:%s\n", box->send_id[i], box->content_buff[i]);
                                    }
                                    box->mes_count = 0;
                                }
                                printf("按下回车继续......");
                                getchar();
                                break;
                            }
                        case 8:
                            {
                                pack_write->type = FRIEND_CHAT_RECORDS;
                                printf("输入您想查看的好友账号－－");
                                scanf("%d",&pack_write->data.recv_id);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                {
                                    perror("send");
                                }
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (mess->count == 0) printf("你们还没有聊过天呢~\n");
                                else 
                                {
                                    for (int i = 0; i < mess->count; i++)
                                        printf("%d-%d-%d %d:%d:%d－－%d:%s\n", mess->year[i],mess->mon[i],mess->day[i],mess->hour[i],mess->min[i],mess->sec[i],mess->send_user[i],mess->message[i]);
                                }
                                getchar();
                                break;
                            }
                        case 9:
                            {
                                int len;
                                int i;
                                int fp = 0;
                                struct stat buffer;
                                char fil[1000];
                                bzero(&buffer, sizeof(struct stat));
                                pack_write->type = SEND_FILE;
                                printf("请输入对方账号－－");
                                scanf("%d", &pack_write->data.recv_id);
                                getchar();
                                printf("输入文件的绝对路径－－");
                                scanf("%s", pack_write->data.fil_buff);
                                getchar();
                                len = strlen(pack_write->data.fil_buff);
                                char nam[len];
                                int coun;
                                int sign;
                                for (i = 0; i < len; i++) 
                                    fil[i] = pack_write->data.fil_buff[i];
                                printf("%s\n", fil);
                                if (lstat(pack_write->data.fil_buff, &buffer) < 0) 
                                {
                                    printf("File does not exist!\n");
                                    printf("按回车继续～");
                                    getchar();
                                    break;
                                }
                                if(S_ISDIR(buffer.st_mode)) 
                                {
                                    printf("不能传输目录！\n");
                                }
                                printf("%s\n", pack_write->data.fil_buff);
                                if ((fp = open(pack_write->data.fil_buff, O_RDONLY)) == -1) 
                                {
                                    perror("open file error！\n");
                                    printf("按回车继续～");
                                    getchar();
                                    break;
                                }
                                for (i = len-1; i >= 0; i--) {
                                    if (pack_write->data.fil_buff[i] == '/') {
                                        sign = i;
                                        i = -1;
                                    }
                                }
                                printf("%d %d\n", sign, i);
                                memset(pack_write->data.fil_buff, 0, sizeof(pack_write->data.fil_buff));
                                coun = 0;
                                for (i = sign+1; i < len; i++) {
                                    pack_write->data.fil_buff[coun] = fil[i];
                                    coun++;
                                }
                                printf("%s\n", pack_write->data.fil_buff);
                                printf("%d %d\n", sign, i);
                                memset(pack_write->data.fil1_buff, 0, sizeof(pack_write->data.fil1_buff));
                                pack_write->data.fil1_buff[0] = 'n';
                                coun = 1;
                                len = strlen(pack_write->data.fil_buff);
                                for (i = 0; i < len; i++) {
                                    pack_write->data.fil1_buff[coun] = pack_write->data.fil_buff[i];
                                    if (pack_write->data.fil_buff[i] == '\0') {
                                        pack_write->data.fil1_buff[coun] = '\0';
                                    }
                                    coun++;
                                }
                                printf("%s\n", pack_write->data.fil1_buff);
                                printf("给%d发送文件\n", pack_write->data.recv_id);
                                mark = 0;
                                pack_write->data.count = 0;
                                memset(pack_write->data.content_buff, 0, sizeof(pack_write->data.content_buff));
                                int sum = read(fp, pack_write->data.content_buff, MAX_CHAR-1);
                                printf("正在发送...\n");
                                while (sum != 0) 
                                {
                                    if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                    {
                                        perror("send");
                                    }
                                    printf("发送中...\n");
                                    pthread_mutex_lock(&n_mutex);
                                    pthread_cond_wait(&n_cond, &n_mutex);
                                    pthread_mutex_unlock(&n_mutex);
                                    pack_write->data.count++;
                                    memset(pack_write->data.content_buff, 0, sizeof(pack_write->data.content_buff));                    
                                    sum = read(fp, pack_write->data.content_buff, MAX_CHAR-1);
                                    if (sum < 0)  break;
                                }
                                close(fp);
                                pack_write->type = FILE_MESS;
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                {
                                    perror("send");
                                }
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                printf("发送成功\n");
                                printf("按回车继续~");
                                getchar();
                                break;
                            }
                        case 10:
                            {
                                pack_write->data.count = 0;
                                if (box->file_num == 0) 
                                {
                                    printf("没有人给你发文件!!\n");
                                    printf("按回车键继续.....");
                                    getchar();
                                    break;
                                } else {
                                    for (int i = 0; i < box->file_num; i++) {
                                        printf("[%d]好友发来%s文件\n", box->fil_id[i], box->mess_buff[i]);
                                        printf("*****************************\n");
                                        printf("          1.接收            \n");
                                        printf("          2.拒绝            \n");
                                        printf("*****************************\n");
                                        printf("请选择－－－");
                                        scanf("%d", &choice);
                                        getchar();
                                        if (choice == 1) 
                                        {
                                            while (1) 
                                            {
                                                pack_write->type = RECV_FILE;
                                                // pack_write->data.recv_account = bo;
                                                memset(pack_write->data.fil_buff, 0, sizeof(pack_write->data.mess_buff));
                                                strcpy(pack_write->data.fil_buff, box->mess_buff[i]);
                                                memset(pack_write->data.fil1_buff, 0, sizeof(pack_write->data.mess_buff));
                                                strcpy(pack_write->data.fil1_buff, box->mess1_buff[i]);
                                                printf("%s\n", box->mess_buff[i]);
                                                printf("%s\n", pack_write->data.fil_buff);
                                                printf("%s\n", pack_write->data.fil1_buff);  //有问题
                                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                                {
                                                    perror("send");
                                                }
                                                printf("接收文件中...");
                                                pthread_mutex_lock(&n_mutex);
                                                pthread_cond_wait(&n_cond, &n_mutex);
                                                pthread_mutex_unlock(&n_mutex);
                                                pack_write->data.count++;
                                                if (strcmp(pack_write->data.mess_buff, "success") == 0) {
                                                    printf("文件接收成功!!!\n");
                                                    printf("按下回车键继续......");
                                                    getchar();
                                                    break;
                                                }
                                            }
                                        } else {
                                            printf("你已拒绝接收此文件!!\n");
                                            printf("按下回车继续......\n");
                                            getchar();
                                        }
                                    }
                                    box->file_num = 0;
                                }
                                break;
                            }
                        default:
                            {
                                printf("请输入正确选项！\n");
                                printf("按回车继续\n");
                                getchar();
                                break;
                            }
                    }
                    break;
                }
            case 2:
                {
                    printf("\033c");
                    printf("      ********************************************************************       \n");
                    printf("      ********************************************************************       \n");
                    printf("      ***                         1.   创建群                           ***       \n");
                    printf("      ***                         2.   加群                             ***       \n");
                    printf("      ***                         3.   退群                             ***       \n");
                    printf("      ***                         4.   解散群                           ***        \n");
                    printf("      ***                         5.   群聊 【退出群聊按#exit】           ***        \n");
                    printf("      ***                         6.   查看已加入群                      ***        \n");
                    printf("      ***                         7.   查看群成员                        ***       \n");
                    printf("      ***                         8.   查看群消息                        ***       \n");
                    printf("      ***                         9.   设置群管理                        ***       \n");
                    printf("      ***                         10.  取消群管理                        ***        \n");
                    printf("      ***                         11.  群踢人                           ***       \n");
                    printf("      ********************************************************************       \n");
                    printf("      ********************************************************************       \n");
                    printf("\n");

                    printf("请选择您将进行的操作－－－");
                    scanf("%d", &choice);
                    getchar();
                    switch(choice) 
                    {
                        case 1:
                            {
                                pack_write->type = CREATE_GROUP;
                                printf("输入一个你喜欢的群名称－－");
                                scanf("%s", pack_write->data.recv_user);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                {
                                    perror("send");
                                }
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (strcmp(pack_write->data.mess_buff, "found") == 0) printf("群名:%s\n群号:%d\nCreation succeeded !\n",pack_write->data.recv_user, pack_write->data.recv_id);
                                else printf("创群失败!\n");
                                getchar();
                                break;
                            }
                        case 2:
                            {
                                pack_write->type = ADD_GROUP;
                                printf("输入你想加入群的群号－－");
                                scanf("%d",&pack_write->data.recv_id);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                {
                                    perror("send");
                                }
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (strcmp(pack_write->data.mess_buff, "success") == 0)    printf("您已成功加入该群!");
                                else if (strcmp(pack_write->data.mess_buff, "enter") == 0) printf("您在该群内!!");
                                else if (strcmp(pack_write->data.mess_buff, "fail") == 0)  printf("没有该群聊!");
                                getchar();
                                break;
                            }
                        case 3:
                            {
                                pack_write->type = DELETE_GROUP;
                                printf("请输入您想退出的群的群号－－");
                                scanf("%d", &pack_write->data.recv_id);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                {
                                    perror("send");
                                }
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (strcmp(pack_write->data.mess_buff, "success") == 0)  printf("退群成功!\n");
                                else if (strcmp(pack_write->data.mess_buff, "not") == 0) printf("您并未在此群中!\n");
                                else if (strcmp(pack_write->data.mess_buff, "manager") == 0) printf("您是该群群主，无法退群！\n");
                                else if (strcmp(pack_write->data.mess_buff, "fail") == 0) printf("退群失败!");
                                getchar();
                                break;
                            }
                        case 4:
                            {
                                pack_write->type = DISBAND_GROUP;
                                printf("请输入您想解散的群的群号－－");
                                scanf("%d", &pack_write->data.recv_id);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                {
                                    perror("send");
                                }
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (strcmp(pack_write->data.mess_buff, "success") == 0)  printf("解散群成功!\n");
                                else if (strcmp(pack_write->data.mess_buff, "not") == 0) printf("您并未在此群中!\n");
                                else if (strcmp(pack_write->data.mess_buff, "fail") == 0) printf("您权限不够，无法解散群!\n");
                                getchar();
                                break;
                            }
                        case 5:
                            {
                                pack_write->type = GROUP_CHAT;
                                printf("请输入群聊号－－");
                                scanf("%d", &pack_write->data.recv_id);
                                getchar();
                                printf("曰：");
                                while (1) 
                                {
                                    scanf("%[^\n]", pack_write->data.content_buff);
                                    getchar();
                                    if (strcmp(pack_write->data.content_buff, "#exit") == 0) 
                                    {
                                        printf("end~\n");
                                        break;
                                    }
                                    if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                    {
                                        perror("send");
                                    }
                                    pthread_mutex_lock(&n_mutex);
                                    pthread_cond_wait(&n_cond, &n_mutex);
                                    pthread_mutex_unlock(&n_mutex);
                                    if (strcmp(pack_write->data.mess_buff, "fail") == 0) 
                                    {
                                        printf("没有群号为%d的群\n", pack_write->data.recv_id);
                                        break;
                                    } else if (strcmp(pack_write->data.mess_buff, "exit") == 0) {
                                        printf("您不在该群内!\n");
                                        break;
                                    }
                                }
                                pack_write->data.recv_id = 0;
                                printf("按下回车键个继续...");
                                getchar();
                                break;
                            }
                        case 6:
                            {
                                pack_write->type = LOOK_GROUP_LIST;
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                {
                                    perror("send");
                                }
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (group_list->count == 0) printf("您还未加入任何群!\n");
                                else 
                                {
                                    printf("=============您加入的群=============\n");
                                    printf("    群号　　　　群名　　　　职位　　　\n");
                                    for(int i = 0; i < group_list->count; i++) 
                                    {
                                        printf("   %d       %s       ", group_list->account[i], group_list->name[i]);
                                        if (group_list->position[i] == 1) printf("群主\n");
                                        else if (group_list->position[i] == -1) printf("管理员\n");
                                        else if (group_list->position[i] == 0) printf("人员\n");
                                    }
                                }
                                printf("按回车继续～\n");
                                getchar();
                                break;
                            }
                        case 7:
                            {
                                pack_write->type = LOOK_GROUP_MEMBER;
                                printf("请输入你想查看的群－－");
                                scanf("%d", &pack_write->data.recv_id);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) {
                                    perror("send");
                                }
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (group_member_list->count == 0) printf("没有此群!");
                                else 
                                {
                                    printf("==========群成员信息==========\n");
                                    printf("    账号    昵称    职位    状态\n");
                                    for (int i = 0; i < group_member_list->count; i++) 
                                    {
                                        printf("   %d   %s    ", group_member_list->id[i], group_member_list->name[i]);
                                        if (group_member_list->position[i] == 1) printf(" 群主    ");
                                        else if (group_member_list->position[i] == -1) printf(" 管理    ");
                                        else if (group_member_list->position[i] == 0) printf(" 人员    ");
                                        if (group_member_list->status[i] == 1) printf("在线\n");
                                        else if (group_member_list->status[i] == 0) printf("离线\n");
                                    }
                                }
                                printf("按回车继续～");
                                getchar();
                                break;
                            }
                        case 8:
                            {
                                if (box->count == 0) 
                                {
                                    printf("没有群消息!!");
                                } else {
                                    for (int i = 0; i < box->count; i++) 
                                    {
                                        printf("群号%d 发送人账号%d 消息内容:\t%s\n", box->group_account[i], box->group_send_account[i], box->message[i]);    
                                    }
                                    box->count = 0;
                                }
                                printf("按下回车键继续......");
                                getchar();
                                break;
                            }
                        case 9:
                            {
                                pack_write->type = SET_ADMINISTRATOR;
                                printf("输入群号－－");
                                scanf("%d", &pack_write->data.recv_id);
                                getchar();
                                printf("请输入您想设置管理员账号－－");
                                scanf("%d", &pack_write->data.group_adm);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) {
                                    perror("send");
                                }
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (strcmp(pack_write->data.mess_buff, "success") == 0)  printf("设置管理成功!\n");
                                else if (strcmp(pack_write->data.mess_buff, "not") == 0) printf("您并未在此群中!\n");
                                else if (strcmp(pack_write->data.mess_buff, "notadm") == 0) printf("您不是群主，无权限!\n");
                                else if (strcmp(pack_write->data.mess_buff, "fail") == 0) printf("此账号所有者并不在本群中!\n");
                                else if (strcmp(pack_write->data.mess_buff, "no") == 0) printf("您已是群主!不能自降身份！！\n");
                                else if (strcmp(pack_write->data.mess_buff, "real") == 0) printf("此账号已经是管理!不用重复设置\n");
                                printf("按下回车继续～");
                                getchar();
                                break;
                            }
                        case 10:
                            {
                                pack_write->type = CANCEL_ADMINI;
                                printf("输入群号－－");
                                scanf("%d", &pack_write->data.recv_id);
                                getchar();
                                printf("请输入您想取消管理员账号－－");
                                scanf("%d", &pack_write->data.group_adm);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                {
                                    perror("send");
                                }
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (strcmp(pack_write->data.mess_buff, "success") == 0)  printf("取消管理成功!\n");
                                else if (strcmp(pack_write->data.mess_buff, "not") == 0) printf("您并未在此群中!\n");
                                else if (strcmp(pack_write->data.mess_buff, "notadm") == 0) printf("您不是群主，无权限!\n");
                                else if (strcmp(pack_write->data.mess_buff, "fail") == 0) printf("此账号所有者并不在本群中!\n");
                                else if (strcmp(pack_write->data.mess_buff, "no") == 0) printf("您已是群主!不能自降身份！！\n");
                                else if (strcmp(pack_write->data.mess_buff, "real") == 0) printf("此账号本不是管理!不用重复设置\n");
                                printf("按下回车继续～");
                                getchar();
                                break;
                            }
                        case 11:
                            {
                                pack_write->type = KICK_PERSON;
                                printf("输入群号－－");
                                scanf("%d", &pack_write->data.recv_id);
                                getchar();
                                printf("请输入您想踢出群人的账号－－");
                                scanf("%d", &pack_write->data.group_adm);
                                getchar();
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                {
                                    perror("send");
                                }
                                pthread_mutex_lock(&n_mutex);
                                pthread_cond_wait(&n_cond, &n_mutex);
                                pthread_mutex_unlock(&n_mutex);
                                if (strcmp(pack_write->data.mess_buff, "success") == 0)  printf("您不喜欢的人已踢出群聊!\n");
                                else if (strcmp(pack_write->data.mess_buff, "not") == 0) printf("您并未在此群中!\n");
                                else if (strcmp(pack_write->data.mess_buff, "nocontrol") == 0) printf("您权限不够!\n");
                                else if (strcmp(pack_write->data.mess_buff, "fail") == 0) printf("此账号所有者并不在本群中!\n");
                                else if (strcmp(pack_write->data.mess_buff, "no") == 0) printf("您是群主!不能踢自己！！\n");
                                printf("按下回车继续～");
                                getchar();
                                break;
                            }
                        default:
                            {
                                printf("请输入正确选项！\n");
                                printf("按回车继续\n");
                                getchar();
                                break;
                            }
                    }
                    break;
                }
            case 3:
                {
                    pthread_mutex_lock(&n_mutex);
                    pack_write->type = FRIEND_PL;
                    if (box->friend_count == 0) 
                    {
                        printf("没有好友请求－－");
                        printf("输入回车继续......");
                        pthread_mutex_unlock(&mutex);
                        getchar();
                    } else {
                        for (int i = 0; i < box->friend_count; ++i) 
                        {
                            printf("%s\n",box->mess_buff[i]);
                            pack_write->data.recv_id = box->plz_id[i];
                            printf("*****************************\n");
                            printf("          1.同意            \n");
                            printf("          2.拒绝            \n");
                            printf("*****************************\n");
                            printf("你的选择－－"); 
                            scanf("%d",&choice);
                            getchar();
                            if (choice == 1) 
                            {
                                strcpy(pack_write->data.content_buff, "agree");
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                {
                                    perror("send");
                                }
                            } else if (choice == 2) {
                                strcpy(pack_write->data.content_buff, "disagree");
                                if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                                {
                                    perror("send");
                                }
                            }
                        }
                        box->friend_count = 0;
                        printf("完成!****\n");
                        pthread_mutex_unlock(&n_mutex);
                        memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                        memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.content_buff));   //???
                        getchar();
                    }
                    break;
                }
            case 4:
                {
                    pack_write->type = MODIFY_PASS;
                    printf("输入原始密码－");
                    getch(pack_write->data.content_buff);
                    printf("输入修改后的密码－");
                    getch(pack_write->data.mess_buff);
                    if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                    {
                        perror("send");
                    }
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_wait(&n_cond, &n_mutex);
                    pthread_mutex_unlock(&n_mutex);
                    if (strcmp(pack_write->data.mess_buff, "fail") == 0) 
                    {
                        printf("自上次修改密码三个月内不允许修改!\n");
                    } else if (strcmp(pack_write->data.mess_buff, "success") == 0) {
                        printf("修改成功!\n");
                    }
                    printf("按回车键继续～");
                    getchar();
                    break;
                }
            case 5:
                {
                    pack_write->type = EXIT;
                    if (send(*(int *)clifd, pack_write, sizeof(PACK), 0) < 0) 
                    {
                        perror("send");
                    }
                    pthread_exit(0);
                    break;
                }
            default:
                {
                    printf("请输入正确选项！\n");
                    printf("按回车继续\n");
                    getchar();
                    break;
                }
        }
    }
}
void *recv_box(void *fd)
{
    if (recv(*(int *)fd, box, sizeof(BOX), MSG_WAITALL) < 0) 
    {
        perror("recv");
    }
    pthread_exit(0);
}
//客户端向服务端发包
void *serv_cli(void *clifd) 
{
    pthread_t pid;
    box = (BOX *)malloc(sizeof(BOX));
    friend_list = (FRIEND *)malloc(sizeof(FRIEND));
    mess = (MESSAGE *)malloc(sizeof(MESSAGE));
    group_list = (GROUP_LIST *)malloc(sizeof(GROUP_LIST));
    group_member_list = (GROUP_MEMBER *)malloc(sizeof(GROUP_MEMBER));
    file = (FILE_RE *)malloc(sizeof(FILE_RE));
    file->sign = 0;
    pack_read = (PACK *)malloc(sizeof(PACK));

    while (1) {
        memset(pack_read, 0, sizeof(PACK));
        if (recv(*(int *)clifd, pack_read, sizeof(PACK), MSG_WAITALL) < 0) 
        {
            perror("recv");
        }
        switch(pack_read->type) {
            case REG:
                {
                    pack_write->data.send_id = pack_read->data.send_id;
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case LOGIN:
                {
                    strcpy(pack_write->data.send_user, pack_read->data.send_user);
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pack_write->data.send_fd = pack_read->data.recv_fd;
					pthread_create(&pid, NULL, recv_box, clifd);
                    pthread_join(pid, NULL);
                    printf("离线期间---\n");
                    printf("有%d条未读私聊－－",box->mes_count);
                    printf("有%d条未读好友请求－－",box->friend_count);
                    printf("有%d条未读群聊－－",box->count);
                    printf("有%d条未接收文件－－\n",box->file_num);
                    printf("---待处理");
                    pthread_mutex_lock(&n_mutex);
                    mark = 1;
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case ERROR_LOGIN:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    mark = 1;
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case EXIT:
                {
                    printf("退出－－");
                    pthread_exit(0);
                    break;
                }
            case ADD_FRIEND:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case DELETE_FRIEND:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case FRIEND_PL:
                {
                    pthread_mutex_lock(&n_mutex);
                    box->plz_id[box->friend_count] = pack_read->data.send_id; 
                    strcpy(box->mess_buff[box->friend_count], pack_read->data.content_buff);
                    box->friend_count++;
                    printf("您有一条好友请求!!!\n");
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case LOOK_FRIEND_LIST:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    memset(friend_list, 0, sizeof(FRIEND));
                    if (recv(*(int *)clifd, friend_list, sizeof(FRIEND), MSG_WAITALL) < 0) 
                    {
                        perror("recv");
                    }
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case BLACK_LIST:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case OUT_BLACK_LIST:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case PRIVATE_CHAT:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    strcpy(pack_write->data.content_buff, pack_read->data.content_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case RECV_FRIEND:
                {
                    if (pack_read->data.send_id == pack_write->data.recv_id) {
                        printf("私聊：%s[%d]:\t%s\n", pack_read->data.send_user, pack_read->data.send_id, pack_read->data.content_buff);
                    } else {
                        box->send_id[box->mes_count] = pack_read->data.send_id;
                        strcpy(box->content_buff[box->mes_count++], pack_read->data.content_buff);
                        printf("您有一条好友消息!\n");
                    }
                    break;
                }
            case FRIEND_CHAT_RECORDS:
                {
                    pthread_mutex_lock(&n_mutex);
                    if (recv(*(int *)clifd, mess, sizeof(MESSAGE), MSG_WAITALL) < 0) 
                    {
                        perror("recv");
                    }
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case CREATE_GROUP:
                {
                    strcpy(pack_write->data.recv_user, pack_read->data.recv_user);
                    pack_write->data.recv_id = pack_read->data.recv_id;
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case ADD_GROUP:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case DELETE_GROUP:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case DISBAND_GROUP:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case LOOK_GROUP_LIST:
                {
                    pthread_mutex_lock(&n_mutex);
                    if (recv(*(int *)clifd, group_list, sizeof(GROUP_LIST), MSG_WAITALL) < 0) 
                    {
                        perror("recv");
                    }
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case LOOK_GROUP_MEMBER:
                {
                    pthread_mutex_lock(&n_mutex);
                    if (recv(*(int *)clifd, group_member_list, sizeof(GROUP_MEMBER), MSG_WAITALL) < 0) 
                    {
                        perror("recv");
                    }
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case SET_ADMINISTRATOR:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case CANCEL_ADMINI:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case KICK_PERSON:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case GROUP_CHAT:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case RECV_GROUP:
                {
                    if (pack_read->data.recv_id == pack_write->data.recv_id) {
                        printf("群号%d－%s[%d]:  %s\n", pack_read->data.recv_id, pack_read->data.send_user, pack_read->data.send_id, pack_read->data.content_buff);
                    } else {
                        printf("您有一条群消息!!\n");
                        box->group_account[box->count] = pack_read->data.recv_id;
                        box->group_send_account[box->count] = pack_read->data.send_id;
                        strcpy(box->message[box->count++], pack_read->data.content_buff);
                    }                            
                    break;
                }
            case SEND_FILE:
                {
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case FILE_PL:
                {
                    printf("%s\n", pack_read->data.fil_buff);
                    printf("%s\n", pack_read->data.fil1_buff);
                    box->fil_id[box->file_num] = pack_read->data.send_id; 
                    strcpy(box->mess_buff[box->file_num], pack_read->data.fil_buff);
                    strcpy(box->mess1_buff[box->file_num], pack_read->data.fil1_buff);
                    printf("[%d]好友给你发送了文件\n", box->fil_id[box->file_num]);
                    box->file_num++;
                    
                    break; 
                }
            case FILE_MESS:
                {
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case RECV_FILE:
                {
                    printf("%s\n", pack_write->data.fil_buff);
                    printf("%s\n", pack_write->data.fil1_buff);
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    int fd = open(pack_write->data.fil1_buff, O_CREAT|O_WRONLY|O_APPEND, 0777);
                    write(fd, pack_read->data.content_buff, MAX_CHAR-1);
                    close(fd);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
            case MODIFY_PASS:
                {
                    memset(pack_write->data.mess_buff, 0, sizeof(pack_write->data.mess_buff));
                    strcpy(pack_write->data.mess_buff, pack_read->data.mess_buff);
                    pthread_mutex_lock(&n_mutex);
                    pthread_cond_signal(&n_cond);
                    pthread_mutex_unlock(&n_mutex);
                    break;
                }
        }
    }
}