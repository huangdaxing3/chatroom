# ifndef _MY_PACK_H
# define _MY_PACK_H
# include <pthread.h>

# define SERV_PORT 4507    //服务器的端口
# define LISTENQ 35        //连接请求队列的最大长度

# define REG 1   // 注册
# define LOGIN 2    // 登录
# define ADD_FRIEND  3 //添加好友
# define FRIEND_PL   4   //好友请求
# define DELETE_FRIEND 5 //删除好友
# define LOOK_FRIEND_LIST 6  //查看好友列表
# define BLACK_LIST 7  //拉黑
# define OUT_BLACK_LIST 8   //拉出黑名单
# define PRIVATE_CHAT 9  //私聊
# define FRIEND_CHAT_RECORDS 10  //好友消息
# define CREATE_GROUP 11  //创建群
# define ADD_GROUP 12  //加群
# define DELETE_GROUP 13  //退群
# define DISBAND_GROUP 14  //解散群
# define LOOK_GROUP_LIST 15 //查看群列表
# define LOOK_GROUP_MEMBER 16  //查看群成员
# define SET_ADMINISTRATOR 17  //设置管理员
# define KICK_PERSON 18  //群踢人
# define GROUP_CHAT 19   //群聊
# define RECV_GROUP 20   //接收群聊消息
# define RECV_FRIEND 21  //接收朋友消息
# define SEND_FILE 22
# define FILE_PL 23
# define MODIFY_PASS 27   //修改密码
# define CANCEL_ADMINI 28  //取消管理员

# define FILE_MESS 24
# define RECV_FILE 25

# define ERROR_LOGIN 26

# define MAXIN 1024
# define EXIT -1
# define ERROR -2

# define BUFSIZE 1024
# define MAX_CHAR 1024

typedef struct 
{
    int               count;
    int               send_fd;
    int               recv_fd;
    int               send_id;
    int               recv_id;
    int               group_adm;
    char              send_user[50];
    char              recv_user[50];
    char              content_buff[MAXIN];
    char              mess_buff[MAXIN];
    char              fil_buff[MAXIN];
    char              fil1_buff[MAXIN];
} DATA;

typedef struct 
{
    int               type;
    DATA              data;
} PACK;

typedef struct 
{
    int               cont;
    int               id[1000];
    char              name[1000][20];
    int               status[1000];
    int               black[1000];
} FRIEND;

// 消息盒子
typedef struct BOX 
{    
    int               recv_id;        // 接受消息者   
    int               send_id[500];   // 发送消息者 
    int               plz_id[500];    // 发送好友请求者
    int               fil_id[500];    // 发送文件者
    char              content_buff[500][MAXIN];    // 发送的消息
    char              mess_buff[500][100];    // 发送的请求
    char              mess1_buff[500][100];
    int               mes_count;     // 消息数量
    int               friend_count;    // 请求数量
    int               group_send_account[500];   // 群里发送消息者
    char              message[500][MAXIN];    // 消息内容
    int               count;     // 群消息数量
    int               group_account[500];    // 群号
    int               file_num;   //文件数量
    struct BOX        *next;
} BOX;

BOX *head;
BOX *tail;

typedef struct 
{
    int               count;
    char              message[500][MAXIN];
    int               send_user[500];
    int               recv_user[500];
    int               year[500];
    int               mon[500];
    int               day[500];
    int               hour[500];
    int               min[500];
    int               sec[500];
} MESSAGE;

typedef struct 
{
    int               group_account;
    int               message_number;
    int               send_account[500];
    char              message[500][MAXIN];
} GROUP_MESSAGE;

typedef struct 
{
    int               count;
    int               id[500];	
    char              name[500][20];
    int               position[500];
    int               status[500];
} GROUP_MEMBER;

typedef struct 
{
    int               account[100];
    char              name[100][20];
    int               count;
    int               position[100];
} GROUP_LIST;

typedef struct 
{
    char              filename[50];
    int               account; 	
    char              nickname[50];
    int               sign;
} FILE_RE;

pthread_mutex_t n_mutex;
pthread_cond_t n_cond;
pthread_mutex_t mutex;
pthread_cond_t cond;

#endif
