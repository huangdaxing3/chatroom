#include <mysql/mysql.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
# include <sys/epoll.h>
# include <pthread.h>
# include <fcntl.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <stdlib.h>
# include <time.h>

# include "my_pack.h"

#define MAXEPOLL 1000

int close_mysql(MYSQL mysql);

int reg(PACK *pack, MYSQL n_mysql);
int create_group(PACK *pack, MYSQL n_mysql);
FRIEND *look_friend_list(PACK *pack, MYSQL n_mysql);
void *solve(void *recv_pack);

int main() {
	int                        i;
	int                        sock_fd;
	int                        conn_fd;
	int                        socklen;
	int                        epfd;
	int                        curfds;
	int                        nfds;
    int                        optval;
	char                       handle[300];
	MYSQL                      mysql;
	PACK                       recv_pack;
    PACK                       *pack;
    PACK                       *pack1;
    struct epoll_event         events[MAXEPOLL];
	pthread_t                  pid;
    MYSQL_RES                  *result;
	MYSQL_ROW                  row;
    socklen_t                  cli_len;
    struct sockaddr_in         serv_addr;
    struct sockaddr_in         cli;
	struct epoll_event         ev;
	
	BOX *mes = head;
    
    pthread_mutex_init(&mutex, NULL);
	socklen = sizeof(struct sockaddr_in);
	if (NULL == mysql_init(&mysql)) 
    {
        perror("mysql_init");
	}

    // 初始化数据库
    if (mysql_library_init(0, NULL, NULL) != 0) 
    {
        perror("mysqllibrary_init");
    }

    //连接数据库
    if (NULL == mysql_real_connect(&mysql, "127.0.0.1", "root", "196981", "chat", 0, NULL, 0)) 
    {
        perror("mysql_connect");
    }

    //设置中文字符集
    if (mysql_set_character_set(&mysql, "utf8") < 0) 
    {
        perror("mysql_set");
    }
    printf("服务器正在启动--**--\n");

    //创建一个TCP套接字
    sock_fd = socket(AF_INET,SOCK_STREAM,0);
    if ( sock_fd < 0 ) 
    {
        perror("socket");
    }

    //设置该套接字使之可以重新绑定端口
    optval = 1;
    if ( setsockopt(sock_fd,SOL_SOCKET,SO_REUSEADDR,(void*)&optval,sizeof(int)) < 0 ) 
    {
        perror("setsockopt");
    }

    //初始化服务器端地址结构
    memset(&serv_addr,0,sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //将套接字绑定到本地端口
    if ( bind(sock_fd,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr_in)) < 0 ) 
    {
        perror("bind");
    }

    //将套接字转化为监听套接字
    if ( listen(sock_fd,LISTENQ) < 0 ) 
    {
        perror("listen");
    }

	epfd = epoll_create(MAXEPOLL);

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = sock_fd;

	if(epoll_ctl(epfd, EPOLL_CTL_ADD, sock_fd, &ev) < 0) 
    {
	    perror("epoll_ctl");
	}
    printf("服务器启动成功--**--\n");

    while(1) 
    {
	 	if((nfds = epoll_wait(epfd, events, MAXEPOLL, -1)) < 0)
        {
	 		perror("epoll_wait");
	    }

        for (i = 0; i < nfds; i++) 
        { 
            if (events[i].data.fd == sock_fd)                  
            //如果是主socket的事件的话，则表示
            //有新连接进入了，进行新连接的处理。
            {
                if ((conn_fd = accept(sock_fd, (struct sockaddr*)&cli, &socklen)) < 0) 
                {
                    perror("accept");
                }
                printf("connect success!\n套接字编号：%d\n", conn_fd);
                
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_fd;

                if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &ev) < 0) 
                {
                    perror("epoll_ctl");
                }
                continue;
            } 
            else if(events[i].events & EPOLLERR) 
            {
                epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,0);    
                close(events[i].data.fd);
				continue;
            } else if (events[i].events & EPOLLHUP) {
				epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,0);    
                close(events[i].data.fd);
				continue;
			} else if (events[i].events & EPOLLIN) {     //表示对应的文件描述符可以读
                //如果是已经连接的用户，并且收到数据，
                //那么进行读入
                    memset(&recv_pack, 0, sizeof(PACK));
                    if (recv(events[i].data.fd, &recv_pack, sizeof(PACK), MSG_WAITALL) < 0) 
                    {
                        close(events[i].data.fd);
                        perror("recv");
                        continue;
                    }
                    printf("\n\e[1;34m*************PACK*************\e[0m\n");
                    printf("\e[1;34m*\e[0m type           : %d\n",recv_pack.type);
                    printf("\e[1;34m*\e[0m send_fd        : %d\n", recv_pack.data.send_fd);
                    printf("\e[1;34m*\e[0m send_account   : %d\n",recv_pack.data.send_id);
                    printf("\e[1;34m*\e[0m recv_fd        : %d\n",recv_pack.data.recv_fd);
                    printf("\e[1;34m*\e[0m recv_account   : %d\n",recv_pack.data.recv_id);
                    printf("\e[1;34m*\e[0m content_buff   : %s\n",recv_pack.data.content_buff);
                    // printf("\e[1;34m*\e[0m mess_buff      : %s\n",recv_pack.data.mess_buff);
                    printf("\e[1;34m*\e[0m recv_user      : %s\n",recv_pack.data.recv_user);
                    printf("\e[1;34m*\e[0m send_name      : %s\n",recv_pack.data.send_user);
                    printf("\e[1;34m*******************************\e[0m\n");
                    if (recv_pack.type == EXIT) 
                    {
                        if (send(events[i].data.fd, &recv_pack, sizeof(PACK), 0) < 0) 
                        {
                            perror("send");
                        }
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "update user set status = 0 where status = 1 and socket = %d", events[i].data.fd);
                        mysql_query(&mysql, handle);
                        printf("套接字为%d的用户已断开连接\n",events[i].data.fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, &ev);
                        continue;
                    }
                    
                    if (recv_pack.type == LOGIN) 
                    {       
                        //能在数据库中查到账号和密码就登录成功
                        printf("eee");
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "select *from user where account = %d and password = '%s'", recv_pack.data.send_id, recv_pack.data.content_buff);
                        pthread_mutex_lock(&mutex);
						printf("\e[1;34m*\e[0m content_buff   : %s\n",recv_pack.data.content_buff);
						printf("\e[1;34m*\e[0m send_id   : %d\n",recv_pack.data.send_id);
                        mysql_query(&mysql, handle);
                        result = mysql_store_result(&mysql);
						memset(handle, 0, sizeof(handle)); 
						sprintf(handle, "update user set socket = %d where account = %d", events[i].data.fd, recv_pack.data.send_id);
						mysql_query(&mysql, handle); 
						pthread_mutex_unlock(&mutex);
						row = mysql_fetch_row(result);
                        if (!row) 
                        {
                            recv_pack.type = ERROR_LOGIN;
                            memset(recv_pack.data.mess_buff, 0, sizeof(recv_pack.data.mess_buff));
                            strcpy(recv_pack.data.mess_buff, "password error");
                            if (send(events[i].data.fd, &recv_pack, sizeof(PACK), 0) < 0) 
                            {
                                perror("send");
                            }
                            pthread_mutex_unlock(&mutex);
                            continue;
                        } else {
							strcpy(recv_pack.data.send_user, row[1]);
							memset(handle, 0, sizeof(handle));
							recv_pack.data.recv_fd = events[i].data.fd;
							sprintf(handle, "update user set status = 1 where account = %d", recv_pack.data.send_id);
							mysql_query(&mysql, handle);
							mysql_free_result(result);
							memset(recv_pack.data.mess_buff, 0, sizeof(recv_pack.data.mess_buff));
							strcpy(recv_pack.data.mess_buff, "success!");
							
							if (send(recv_pack.data.recv_fd, &recv_pack, sizeof(PACK), 0) < 0) 
                            {
                        		perror("send");
                    		}

							pack1 = (PACK *)malloc(sizeof(PACK));
							memcpy(pack1, &recv_pack, sizeof(PACK));
							while (mes != NULL) 
                            {
								if (mes->recv_id == pack1->data.send_id) 
                                {
									break;
								}
								mes = mes->next;
							}
							if (mes == NULL) 
                            {
								mes = (BOX *)malloc(sizeof(BOX));
								mes->recv_id = pack1->data.send_id;
								mes->mes_count = 0;
                                mes->friend_count = 0;
								mes->count = 0;
                                mes->file_num =0;
								mes->next = NULL;
								if (head == NULL) 
                                {
									head = tail = mes;
								} else {
									tail->next = mes;
									tail = mes;
								}
								if (send(pack1->data.recv_fd, mes, sizeof(BOX), 0) < 0) 
                                {
									perror("send");
								}
							} else {
								if (send(pack1->data.recv_fd, mes, sizeof(BOX), 0) < 0) 
                                {
									perror("send");
								}
								mes->friend_count = 0;
								mes->mes_count = 0;
                                mes->file_num = 0;
                                mes->count = 0;
							}
						}
                    }
                    recv_pack.data.recv_fd = events[i].data.fd;
                    pack = (PACK*)malloc(sizeof(PACK));
                    memcpy(pack, &recv_pack, sizeof(PACK));
                    pthread_create(&pid, NULL, solve, (void*)pack);
                }
        }
    } 
    close(sock_fd);
}

int close_mysql(MYSQL mysql) {
    mysql_close(&mysql);
    mysql_library_end();
    
    return 0;
}

void my_err(char *err_string , int line) {
    fprintf(stdout,"line:%d",line);
    perror(err_string);
    exit(1);
}

void *solve(void *recv_pack) {
    pthread_detach(pthread_self());
    int                i;
	PACK               *pack;
	MYSQL              mysql;
    BOX                *addfr;
    BOX                *real;
    BOX                *file_content;
    MESSAGE            *mess;
    GROUP_LIST         *list;
    GROUP_MEMBER       *member;
    BOX                *mes = head;
    int                ret;
    char               handle[200];
    MYSQL_RES          *result, *n_result;
    MYSQL_ROW          row, n_row;
    time_t             now ;
    struct tm          *tm_now ;
    time(&now) ;
    tm_now = localtime(&now) ;

	if (NULL == mysql_init(&mysql)) {
        my_err("init", __LINE__);
	}

    // 初始化数据库
    if (mysql_library_init(0, NULL, NULL) != 0) {
        my_err("library_init", __LINE__);
    }
    //连接数据库
    if (NULL == mysql_real_connect(&mysql, "127.0.0.1", "root", "196981", "chat", 0, NULL, 0)) {
        my_err("real_connect", __LINE__);
    }
    //设置中文字符集
    if (mysql_set_character_set(&mysql, "utf8") < 0) {
        my_err("set_character_set", __LINE__);
    }

	pack = (PACK *)recv_pack;
	switch(pack->type)
	{
        case REG:
            {
                reg(pack, mysql);
                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                strcpy(pack->data.mess_buff, "registered success!!");
                if (send(pack->data.recv_fd, pack, sizeof(PACK), 0) < 0) {
                    my_err("send", __LINE__);
                }
                
                break;
            }
        case ADD_FRIEND:
            {
                if (pack->data.send_id == pack->data.recv_id) {
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "not");
                    if (send(pack->data.recv_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);        
                    }
                } else {
                    sprintf(handle, "select *from user where account = %d", pack->data.recv_id);
                    mysql_query(&mysql, handle);
                    result = mysql_store_result(&mysql);
                    row = mysql_fetch_row(result);
                    if (!row) {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "fail");
                        if (send(pack->data.recv_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);        
                        }
                    } else {
                        memset(handle, 0 ,sizeof(handle));
                        printf("%d\n", pack->data.recv_id);
                        sprintf(handle, "select *from friend where user_account = %d and user_friend_account = %d", pack->data.send_id, pack->data.recv_id);
                        mysql_query(&mysql, handle);
                        n_result = mysql_store_result(&mysql);
                        n_row = mysql_fetch_row(n_result);
                        if (!n_row) {
                            memset(handle, 0, sizeof(handle));
                            sprintf(handle, "用户%s[%d]发来加好友请求\n", pack->data.send_user, pack->data.send_id);
                            if (atoi(row[3]) == 1) {
                                pack->data.send_fd = atoi(row[4]);
                                strcpy(pack->data.recv_user, row[1]);
                                strcpy(pack->data.content_buff, handle);
                                pack->type = FRIEND_PL;
                                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                    my_err("send", __LINE__);
                                }
                            } else {
                                while (addfr) {
                                    if (addfr->recv_id == pack->data.recv_id) {
                                        break;
                                    }
                                    addfr = addfr->next;
                                }
                                if (addfr != NULL) {
                                    addfr->plz_id[addfr->friend_count] = pack->data.send_id;
                                    strcpy(addfr->mess_buff[addfr->friend_count], handle);
                                    addfr->friend_count++;
                                } else {
                                    addfr = (BOX *)malloc(sizeof(BOX));
                                    addfr->recv_id = pack->data.recv_id;
                                    addfr->friend_count = 0;
                                    addfr->mes_count = 0;
                                    strcpy(addfr->mess_buff[addfr->friend_count], handle);
                                    addfr->plz_id[addfr->friend_count++] = pack->data.send_id;
                                    printf("%d\n", addfr->friend_count);    //判断
                                    if (head == NULL) {
                                        head = tail = addfr;
                                        tail->next = NULL;
                                    } else {
                                        tail->next = addfr;
                                        tail = addfr;
                                        tail->next = NULL;
                                    }
                                }
                            }
                            pack->type = ADD_FRIEND;
                            memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                            strcpy(pack->data.mess_buff, "success");
                            if (send(pack->data.recv_fd, pack, sizeof(PACK) , 0) < 0) {
                                my_err("send", __LINE__);
                            }
                        } else {
                            memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                            strcpy(pack->data.mess_buff, "no");
                            if (send(pack->data.recv_fd, pack, sizeof(PACK) , 0) < 0) {
                                my_err("send", __LINE__);
                            }
                        }
                    }
                }
                break;
            }
        case DELETE_FRIEND:
            {
                sprintf(handle, "select *from friend where user_account = %d and user_friend_account = %d", pack->data.send_id, pack->data.recv_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                if (row == NULL) {
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "fail");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                } else {
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "delete from friend where user_account = %d and user_friend_account = %d", pack->data.send_id, pack->data.recv_id);
                    mysql_query(&mysql, handle);
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "delete from friend where user_account = %d and user_friend_account = %d", pack->data.recv_id, pack->data.send_id);
                    mysql_query(&mysql, handle);
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "select *from friend where user_account = %d and user_friend _account = %d",pack->data.send_id, pack->data.recv_id);
                    mysql_query(&mysql, handle);
                    result = mysql_store_result(&mysql);
                    row = mysql_fetch_row(result);
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "select *from friend where user_account = %d and user_friend _account = %d",pack->data.recv_id, pack->data.send_id);
                    mysql_query(&mysql, handle);
                    result = mysql_store_result(&mysql);
                    n_row = mysql_fetch_row(result);
                    if(!(row && n_row)) {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "qqq");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    } else {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "fail");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    }
                }
                break;
            }
        case FRIEND_PL:
            {
                sprintf(handle, "insert into friend values(%d,%d,1)", pack->data.send_id, pack->data.recv_id);
                mysql_query(&mysql, handle);
                memset(handle, 0, sizeof(handle));
                sprintf(handle, "insert into friend values(%d,%d,1)", pack->data.recv_id, pack->data.send_id);
                mysql_query(&mysql, handle);
                break;
            }
        case LOOK_FRIEND_LIST:
            {
                FRIEND *list = look_friend_list(pack, mysql);
                if (list->cont != 0) {
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "success");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                    if (send(pack->data.send_fd, list, sizeof(FRIEND), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                } else {
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "fail");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                    if (send(pack->data.send_fd, list, sizeof(FRIEND), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                }
                break;
            }
        case BLACK_LIST:
            {
                sprintf(handle, "select *from friend where user_account = %d and user_friend_account = %d and relation = 1", pack->data.send_id, pack->data.recv_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                if (!row) {
                    printf("```");
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "fail");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                } else {
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "update friend set relation = -1 where user_account = %d and user_friend_account = %d", pack->data.send_id, pack->data.recv_id);
                    mysql_query(&mysql, handle);
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "update friend set relation = -1 where user_account = %d and user_friend_account = %d", pack->data.recv_id, pack->data.send_id);
                    mysql_query(&mysql, handle);
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "select *from friend where user_account = %d and user_friend_account = %d and relation = 1", pack->data.send_id, pack->data.recv_id);
                    mysql_query(&mysql, handle);
                    result = mysql_store_result(&mysql);
                    row = mysql_fetch_row(result);
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "select *from friend where user_account = %d and user_friend_account = %d and relation = 1", pack->data.recv_id, pack->data.send_id);
                    mysql_query(&mysql, handle);
                    result = mysql_store_result(&mysql);
                    n_row = mysql_fetch_row(result);
                    printf("到了");
                    if (!(row && n_row)) {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "success");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    } else {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "fail");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    }
                }
                break;
            }
        case OUT_BLACK_LIST:
            {
                sprintf(handle, "select *from friend where user_account = %d and user_friend_account = %d and relation = -1", pack->data.send_id, pack->data.recv_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                if (!row) {
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "fail");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                } else {
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "update friend set relation = 1 where user_account = %d and user_friend_account = %d", pack->data.send_id, pack->data.recv_id);
                    mysql_query(&mysql, handle);
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "update friend set relation = 1 where user_account = %d and user_friend_account = %d", pack->data.recv_id, pack->data.send_id);
                    mysql_query(&mysql, handle);
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "select *from friend where user_account = %d and user_friend_account = %d and relation = -1", pack->data.send_id, pack->data.recv_id);
                    mysql_query(&mysql, handle);
                    result = mysql_store_result(&mysql);
                    row = mysql_fetch_row(result);
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "select *from friend where user_account = %d and user_friend_account = %d and relation = -1", pack->data.recv_id, pack->data.send_id);
                    mysql_query(&mysql, handle);
                    result = mysql_store_result(&mysql);
                    n_row = mysql_fetch_row(result);
                    if (!(row && n_row)) {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "success");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    } else {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "fail");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    }
                }
                break;
            }
        case PRIVATE_CHAT:
            {
                pthread_mutex_lock(&mutex);
                sprintf(handle, "select *from user where account = %d",pack->data.recv_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                if (!row) {
                    printf("bbbb");
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "#quit");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                    pthread_mutex_unlock(&mutex);
                } else {
                    strcpy(pack->data.recv_user, row[1]);
                    if (atoi(row[3]) == 1) {
                        pack->type = RECV_FRIEND;
                        pack->data.recv_fd = atoi(row[4]);
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "select *from friend where user_account = %d and user_friend_account = %d and relation = 1",pack->data.recv_id, pack->data.send_id);
                        mysql_query(&mysql, handle);
                        result = mysql_store_result(&mysql);
                        row = mysql_fetch_row(result);
                        if (!row) {
                            printf("aaa");
                            pack->type = PRIVATE_CHAT;
                            memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                            strcpy(pack->data.mess_buff, "#quit");
                            if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                my_err("send", __LINE__);
                            }
                            pthread_mutex_unlock(&mutex);
                            break;
                        } else {
                            if (send(pack->data.recv_fd, pack, sizeof(PACK), 0) < 0) {
                                my_err("send", __LINE__);
                            }
                            memset(handle, 0, sizeof(handle));
                            sprintf(handle, "insert into message values(%d,%d,%d,%d,%d,%d,%d,%d,\"%s\")",pack->data.send_id,pack->data.recv_id,tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,pack->data.content_buff);
                            mysql_query(&mysql, handle);
                        }
                    } else {
                        real = head;
                        while(real != NULL) {
                            if (real->recv_id == pack->data.recv_id) {
                                real->send_id[real->mes_count] = pack->data.send_id;
                                strcpy(real->content_buff[real->mes_count++], pack->data.content_buff);
                                memset(handle, 0, sizeof(handle));
                                sprintf(handle, "insert into message values(%d,%d,%d,%d,%d,%d,%d,%d,\"%s\")",pack->data.send_id,pack->data.recv_id,tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,pack->data.content_buff);
                                mysql_query(&mysql, handle);
                                break;
                            }
                            real = real->next;
                        }
                    }
                    
                    pthread_mutex_unlock(&mutex);
                    pack->type = PRIVATE_CHAT;
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "#enter");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                }
                break;
            }
        case FRIEND_CHAT_RECORDS:
            {
                mess = (MESSAGE *)malloc(sizeof(MESSAGE));
                mess->count = 0;
                memset(handle, 0, sizeof(handle));
                sprintf(handle, "select *from message where send_account = %d and recv_account = %d union select *from message where send_account = %d and recv_account = %d", pack->data.send_id, pack->data.recv_id, pack->data.recv_id, pack->data.send_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                while (row = mysql_fetch_row(result)) {
                    mess->send_user[mess->count] = atoi(row[0]);
                    mess->recv_user[mess->count] = atoi(row[1]);
                    mess->year[mess->count] = atoi(row[2]);
                    mess->mon[mess->count] = atoi(row[3]);
                    mess->day[mess->count] = atoi(row[4]);
                    mess->hour[mess->count] = atoi(row[5]);
                    mess->min[mess->count] = atoi(row[6]);
                    mess->sec[mess->count] = atoi(row[7]);
                    strcpy(mess->message[mess->count], row[8]);
                    mess->count++;
                }
                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                    my_err("send", __LINE__);
                }
                if (send(pack->data.send_fd, mess, sizeof(MESSAGE), 0) < 0) {
                    my_err("send", __LINE__);
                }

                break;
            }
        case CREATE_GROUP:
            {
                create_group(pack, mysql);
                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                strcpy(pack->data.mess_buff, "found");
                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                    my_err("send", __LINE__);
                }

                break;
            }
        case ADD_GROUP:
            {
                sprintf(handle, "select *from groups where group_account = %d", pack->data.recv_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                
                if (!row) {
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "fail");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                } else {
                    strcpy(pack->data.recv_user, row[1]);
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "select *from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.send_id);
                    mysql_query(&mysql, handle);
                    result = mysql_store_result(&mysql);
                    n_row = mysql_fetch_row(result);
                    if (n_row) {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "enter");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    } else {
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "insert into group_mess values(%d,\"%s\",%d,\"%s\",0)", pack->data.recv_id, pack->data.recv_user, pack->data.send_id, pack->data.send_user);
                        mysql_query(&mysql, handle);
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "update groups set amount = %d where group_account = %d", atoi(row[2])+1, pack->data.recv_id);
                        mysql_query(&mysql, handle);
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "success");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    }
                }
                break;
            }
        case DELETE_GROUP:
            {
                sprintf(handle, "select *from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.send_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                if (!row) {
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "not");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                } else {
                    if (atoi(row[4]) == 1) {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "manager");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    } else {
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "select *from groups where group_account = %d", pack->data.recv_id);
                        mysql_query(&mysql, handle);
                        result = mysql_store_result(&mysql);
                        row = mysql_fetch_row(result);
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "delete from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.send_id);
                        mysql_query(&mysql, handle);
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "update groups set amount = %d where group_account = %d", atoi(row[2])-1, pack->data.recv_id);
                        mysql_query(&mysql, handle);
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "select *from group_mess where group_account = %d and person_account = %d",pack->data.recv_id, pack->data.send_id);
                        mysql_query(&mysql, handle);
                        result = mysql_store_result(&mysql);
                        n_row = mysql_fetch_row(result);
                        if (n_row) {
                            memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                            strcpy(pack->data.mess_buff, "fail");
                            if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                my_err("send", __LINE__);
                            }
                        } else {
                            memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                            strcpy(pack->data.mess_buff, "success");
                            if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                my_err("send", __LINE__);
                            }
                        }
                    }
                }
                break;
            }
        case DISBAND_GROUP:
            {
                sprintf(handle, "select *from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.send_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                if (!row) {
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "not");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                } else {
                    if (atoi(row[4]) == 1) {
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "delete from groups where group_account = %d", pack->data.recv_id);
                        mysql_query(&mysql, handle);
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "delete from group_mess where group_account = %d", pack->data.recv_id);
                        mysql_query(&mysql, handle);
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "success");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    } else {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "fail");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    }
                }
                break;
            }
        case LOOK_GROUP_LIST:
            {
                list = (GROUP_LIST *)malloc(sizeof(GROUP_LIST));
                list->count = 0;
                sprintf(handle, "select *from group_mess where person_account = %d", pack->data.send_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                while (row = mysql_fetch_row(result)) {
                    list->account[list->count] = atoi(row[0]);
                    strcpy(list->name[list->count], row[1]);
                    list->position[list->count] = atoi(row[4]);
                    list->count++;
                }
                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                    my_err("send", __LINE__);
                }
                if (send(pack->data.send_fd, list, sizeof(GROUP_LIST), 0) < 0) {
                    my_err("send", __LINE__);
                }
                break;
            }
        case LOOK_GROUP_MEMBER:
            {
                member = (GROUP_MEMBER *)malloc(sizeof(GROUP_MEMBER));
                member->count = 0;
                sprintf(handle, "select *from group_mess where group_account = %d", pack->data.recv_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                while(row = mysql_fetch_row(result)) {
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "select *from user where account = %d", atoi(row[2]));
                    mysql_query(&mysql, handle);
                    n_result = mysql_store_result(&mysql);
                    n_row = mysql_fetch_row(n_result);
                    member->status[member->count] = atoi(n_row[3]);
                    member->id[member->count] = atoi(row[2]);
                    strcpy(member->name[member->count], row[3]);
                    member->position[member->count] = atoi(row[4]);
                    member->count++;
                }
                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                    my_err("send", __LINE__);
                }
                if (send(pack->data.send_fd, member, sizeof(GROUP_MEMBER), 0) < 0) {
                    my_err("send", __LINE__);
                }
                break;
            }
        case SET_ADMINISTRATOR:
            {
                sprintf(handle, "select *from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.send_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                if (!row) {
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "not");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                } else {
                    if (atoi(row[4]) == 1) {
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "select *from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.group_adm);
                        mysql_query(&mysql, handle);
                        n_result = mysql_store_result(&mysql);
                        n_row = mysql_fetch_row(n_result);
                        if (!n_row) {
                            memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                            strcpy(pack->data.mess_buff, "fail");
                            if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                my_err("send", __LINE__);
                            }
                        } else {
                            if (atoi(n_row[4]) == 1) {
                                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                                strcpy(pack->data.mess_buff, "no");
                                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                    my_err("send", __LINE__);
                                }
                            } else if (atoi(n_row[4]) == 0) {
                                memset(handle, 0, sizeof(handle));
                                sprintf(handle, "update group_mess set position = -1 where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.group_adm);
                                mysql_query(&mysql, handle);
                                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                                strcpy(pack->data.mess_buff, "success");
                                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                    my_err("send", __LINE__);
                                }
                            } else if (atoi(n_row[4]) == -1){
                                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                                strcpy(pack->data.mess_buff, "real");
                                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                    my_err("send", __LINE__);
                                }
                            }
                        }
                    } else {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "notadm");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    }
                }
                break;
            }
        case CANCEL_ADMINI:
            {
                sprintf(handle, "select *from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.send_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                if (!row) {
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "not");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                } else {
                    if (atoi(row[4]) == 1) {
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "select *from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.group_adm);
                        mysql_query(&mysql, handle);
                        n_result = mysql_store_result(&mysql);
                        n_row = mysql_fetch_row(n_result);
                        if (!n_row) {
                            memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                            strcpy(pack->data.mess_buff, "fail");
                            if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                my_err("send", __LINE__);
                            }
                        } else {
                            if (atoi(n_row[4]) == 1) {
                                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                                strcpy(pack->data.mess_buff, "no");
                                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                    my_err("send", __LINE__);
                                }
                            } else if (atoi(n_row[4]) == -1) {
                                memset(handle, 0, sizeof(handle));
                                sprintf(handle, "update group_mess set position = 0 where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.group_adm);
                                mysql_query(&mysql, handle);
                                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                                strcpy(pack->data.mess_buff, "success");
                                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                    my_err("send", __LINE__);
                                }
                            } else if (atoi(n_row[4]) == 0){
                                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                                strcpy(pack->data.mess_buff, "real");
                                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                    my_err("send", __LINE__);
                                }
                            }
                        }
                    } else {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "notadm");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    }
                }
                break;
            }
        case KICK_PERSON:
            {
                MYSQL_RES *m_result;
                MYSQL_ROW m_row;
                sprintf(handle, "select *from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.send_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                if (!row) {
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "not");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                } else {
                    if (atoi(row[4]) == 1) {
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "select *from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.group_adm);
                        mysql_query(&mysql, handle);
                        n_result = mysql_store_result(&mysql);
                        n_row = mysql_fetch_row(n_result);
                        if (!n_row) {
                            memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                            strcpy(pack->data.mess_buff, "fail");
                            if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                my_err("send", __LINE__);
                            }
                        } else {
                            if (atoi(n_row[4]) == 1) {
                                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                                strcpy(pack->data.mess_buff, "no");
                                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                    my_err("send", __LINE__);
                                }
                            } else if ((atoi(n_row[4]) == 0) || (atoi(n_row[4]) == -1)){
                                memset(handle, 0, sizeof(handle));
                                sprintf(handle, "delete from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.group_adm);
                                mysql_query(&mysql, handle);
                                memset(handle, 0, sizeof(handle));
                                sprintf(handle, "select *from groups where group_account = %d", pack->data.recv_id);
                                mysql_query(&mysql, handle);
                                m_result = mysql_store_result(&mysql);
                                m_row = mysql_fetch_row(m_result);
                                memset(handle, 0, sizeof(handle));
                                sprintf(handle, "update groups set amount = %d where group_account = %d", atoi(m_row[2])-1, pack->data.recv_id);
                                mysql_query(&mysql, handle);
                                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                                strcpy(pack->data.mess_buff, "success");
                                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                    my_err("send", __LINE__);
                                }
                            }
                        }
                    } else if (atoi(row[4]) == -1) {
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "select *from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.group_adm);
                        mysql_query(&mysql, handle);
                        n_result = mysql_store_result(&mysql);
                        n_row = mysql_fetch_row(n_result);
                        if (!n_row) {
                            memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                            strcpy(pack->data.mess_buff, "fail");
                            if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                my_err("send", __LINE__);
                            }
                        } else {
                            if ((atoi(n_row[4]) == 1) || (atoi(n_row[4]) == -1)) {
                                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                                strcpy(pack->data.mess_buff, "nocontrol");
                                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                    my_err("send", __LINE__);
                                }
                            } else if (atoi(n_row[4]) == 0){
                                memset(handle, 0, sizeof(handle));
                                sprintf(handle, "delete from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.group_adm);
                                mysql_query(&mysql, handle);
                                memset(handle, 0, sizeof(handle));
                                sprintf(handle, "select *from groups where group_account = %d", pack->data.recv_id);
                                mysql_query(&mysql, handle);
                                m_result = mysql_store_result(&mysql);
                                m_row = mysql_fetch_row(m_result);
                                memset(handle, 0, sizeof(handle));
                                sprintf(handle, "update groups set amount = %d where group_account = %d", atoi(m_row[2])-1, pack->data.recv_id);
                                mysql_query(&mysql, handle);
                                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                                strcpy(pack->data.mess_buff, "success");
                                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                                    my_err("send", __LINE__);
                                }
                            }
                        }
                    } else if (atoi(row[4]) == 0) {
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "nocontrol");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    }
                }
                break;
            }
        case GROUP_CHAT:
            {
                MYSQL_RES *m_result;
                MYSQL_ROW m_row;
                pack->type = RECV_GROUP;
                sprintf(handle, "select *from groups where group_account = %d", pack->data.recv_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                if (!row) {
                    printf("aaa、\n");
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "fail");
                    pack->type = GROUP_CHAT;
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                } else {
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "select *from group_mess where group_account = %d and person_account = %d", pack->data.recv_id, pack->data.send_id);
                    mysql_query(&mysql, handle);
                    m_result = mysql_store_result(&mysql);
                    m_row = mysql_fetch_row(m_result);
                    if (!m_row) {
                        printf("vvv、\n");
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "exit");
                        pack->type = GROUP_CHAT;
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }
                    } else {
                        memset(handle, 0, sizeof(handle));
                        sprintf(handle, "select *from group_mess where group_account = %d", pack->data.recv_id);
                        mysql_query(&mysql, handle);
                        result = mysql_store_result(&mysql);
                        while (row = mysql_fetch_row(result)) {
                            printf("ccc\n");
                            printf("%d\n", atoi(row[2]));
                            if (atoi(row[2]) == pack->data.send_id)  continue;
                            memset(handle, 0, sizeof(handle));
                            sprintf(handle, "select *from user where account = %d", atoi(row[2]));
                            mysql_query(&mysql, handle);
                            n_result = mysql_store_result(&mysql);
                            n_row = mysql_fetch_row(n_result);
                            if (!n_row) my_err("row", __LINE__);
                            printf("%d", atoi(n_row[3]));
                            if (atoi(n_row[3]) == 1) {
                                printf("进入");
                                printf("%d\n", atoi(n_row[4]));
                                if (send(atoi(n_row[4]), pack, sizeof(PACK), 0) < 0) {
                                    my_err("send", __LINE__);
                                }
                                continue;
                            } else {
                                mes = head;
                                while(mes != NULL) {
                                    if (mes->recv_id == atoi(n_row[0])) {
                                        mes->group_send_account[mes->count] = pack->data.send_id;
                                        mes->group_account[mes->count] = pack->data.recv_id;
                                        strcpy(mes->message[mes->count++], pack->data.content_buff);
                                        break;
                                    }
                                    mes = mes->next;
                                }
                            }
                        }
                        pack->type = GROUP_CHAT;
                        memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                        strcpy(pack->data.mess_buff, "#success");
                        if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                            my_err("send", __LINE__);
                        }  
                    }
                }
                
                break;
            }
        case SEND_FILE:
            {
                printf("a\n");
                int fd = open(pack->data.fil_buff, O_CREAT|O_WRONLY|O_APPEND, 0777);
                write(fd, pack->data.content_buff, MAX_CHAR-1);
                close(fd);
                printf("%s\n", pack->data.fil_buff);
                printf("%s\n", pack->data.fil1_buff);
                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                    my_err("send", __LINE__);
                }
                break;
            }
        case FILE_MESS:
            {
                sprintf(handle, "select *from user where account = %d", pack->data.recv_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                printf("b\n");
                printf("%s\n", pack->data.fil_buff);
                printf("%s\n", pack->data.fil1_buff);
                if (row && (atoi(row[3]) == 1)) {
                    printf("c\n");
                    pack->data.recv_fd = atoi(row[4]);
                    pack->type = FILE_PL;
                    if (send(pack->data.recv_fd, recv_pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                    printf("%s\n", pack->data.fil_buff);
                    printf("%s\n", pack->data.fil1_buff);
                    pack->type = FILE_MESS;
                    if (send(pack->data.send_fd, recv_pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                    printf("%s\n", pack->data.fil_buff);
                    printf("%s\n", pack->data.fil1_buff);
                } else if (atoi(row[3]) == 0) {
                    printf("%s\n", pack->data.fil_buff);
                    printf("%s\n", pack->data.fil1_buff);
                    while (file_content) {
                        if (file_content->recv_id == pack->data.recv_id) {
                            break;
                        }
                        file_content = file_content->next;
                    }
                    if (file_content != NULL) {
                        file_content->fil_id[file_content->file_num] = pack->data.send_id;
                        strcpy(file_content->mess_buff[file_content->file_num], pack->data.fil_buff);
                        strcpy(file_content->mess1_buff[file_content->file_num], pack->data.fil1_buff);
                        file_content->file_num++;
                    } else {
                        file_content = (BOX *)malloc(sizeof(BOX));
                        file_content->recv_id = pack->data.recv_id;
                        file_content->file_num = 0;
                        strcpy(file_content->mess_buff[file_content->file_num], pack->data.fil_buff);
                        strcpy(file_content->mess1_buff[file_content->file_num], pack->data.fil1_buff);
                        file_content->fil_id[file_content->file_num++] = pack->data.send_id;
                        if (head == NULL) {
                            head = tail = file_content;
                            tail->next = NULL;
                        } else {
                            tail->next = file_content;
                            tail = file_content;
                            tail->next = NULL;
                        }
                    }
                    pack->type = FILE_MESS;
                    if (send(pack->data.send_fd, recv_pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                }
                break;
            }
        case RECV_FILE:
            {
                printf("e\n");
                pthread_mutex_lock(&mutex);
                printf("%s\n", pack->data.fil_buff);
                int fd = open(pack->data.fil_buff, O_RDONLY);
                lseek(fd, (MAX_CHAR-1)*pack->data.count, SEEK_SET);
                memset(pack->data.content_buff, 0, sizeof(pack->data.content_buff));
                memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                if (read(fd, pack->data.content_buff, MAX_CHAR-1) == 0) {
                    strcpy(pack->data.mess_buff, "success");
                }
                printf("%s", pack->data.mess_buff);
                if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                    my_err("send", __LINE__);
                }
                close(fd);
                pthread_mutex_unlock(&mutex);
                break;
            }
        case MODIFY_PASS:
            {
                time_t now;
                now = time(NULL);
                memset(handle, 0, sizeof(handle));
                sprintf(handle, "select *from password where use_account = %d", pack->data.send_id);
                mysql_query(&mysql, handle);
                result = mysql_store_result(&mysql);
                row = mysql_fetch_row(result);
                int differ = now - atoi(row[1]);
                if (differ < 7776000) {                      //３个月不允许修改密码
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "fail");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                } else {
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "update user set password = %d where account = %d", atoi(pack->data.mess_buff), pack->data.send_id);
                    mysql_query(&mysql, handle);
                    memset(handle, 0, sizeof(handle));
                    sprintf(handle, "update password set sec = %d where use_account = %d", now, pack->data.send_id);
                    mysql_query(&mysql, handle);
                    memset(pack->data.mess_buff, 0, sizeof(pack->data.mess_buff));
                    strcpy(pack->data.mess_buff, "success");
                    if (send(pack->data.send_fd, pack, sizeof(PACK), 0) < 0) {
                        my_err("send", __LINE__);
                    }
                }
                break;
            }
    }
    close_mysql(mysql);
}

int reg(PACK *pack, MYSQL n_mysql) {
    MYSQL mysql = n_mysql;
    char handle[100];
    PACK *recv_pack = pack;
    int account;
    time_t t = time(NULL);
    
    srand(t);
    account = rand()%1000000;
    sprintf(handle, "insert into user values(%d,\"%s\",\"%s\",%d,%d)", account, recv_pack->data.send_user, recv_pack->data.content_buff, 0, recv_pack->data.recv_fd);
    recv_pack->data.send_id = account;

    time_t now;
    now = time(NULL);
    mysql_query(&mysql, handle);
    memset(handle, 0, sizeof(handle));
    sprintf(handle, "insert into password values(%d,%d)",account, now);
    mysql_query(&mysql, handle);

    return 0;
}

FRIEND *look_friend_list(PACK *pack, MYSQL n_mysql) {
    MYSQL_ROW row, n_row;
    MYSQL_RES *result, *n_result;
    FRIEND *list = (FRIEND *)malloc(sizeof(FRIEND));
    char handle[150];
    PACK *recv_t  = pack;
    MYSQL mysql = n_mysql;

    list->cont = 0;
    sprintf(handle, "select *from friend where user_account = %d", recv_t->data.send_id);
    mysql_query(&mysql, handle);
    result = mysql_store_result(&mysql);
    while (row = mysql_fetch_row(result)) {
        if (atoi(row[2]) == 1) {
            list->id[list->cont] = atoi(row[1]);
            memset(handle, 0, sizeof(handle));
            printf("%d\n", atoi(row[1]));
            sprintf(handle, "select *from user where account = %d", atoi(row[1]));
            mysql_query(&mysql, handle);
            n_result = mysql_store_result(&mysql);
            n_row = mysql_fetch_row(n_result);
            printf("%s",n_row[1]);
            strcpy(list->name[list->cont], n_row[1]);
            list->status[list->cont++] = atoi(n_row[3]);
        } else {
            list->id[list->cont] = atoi(row[1]);
            memset(handle, 0, sizeof(handle));
            sprintf(handle, "select *from user where account = %d", atoi(row[1]));
            mysql_query(&mysql, handle);
            n_result = mysql_store_result(&mysql);
            n_row = mysql_fetch_row(n_result);
            strcpy(list->name[list->cont], n_row[1]);
            list->status[list->cont++] = atoi(row[2]);
        }
    }
    if (list->cont == 0)   return list;
    else return list;
}

int create_group(PACK *pack, MYSQL n_mysql) {
    MYSQL mysql = n_mysql;
    char handle[100];
    PACK *recv_pack = pack;
    int account;
    time_t t = time(NULL);

    srand(t);
    account = rand()%100000;
    sprintf(handle, "insert into groups values(%d,\"%s\",1)", account, recv_pack->data.recv_user);
    recv_pack->data.recv_id = account;
    mysql_query(&mysql, handle);
    memset(handle, 0, sizeof(handle));
    sprintf(handle, "insert into group_mess values(%d,\"%s\",%d,\"%s\",1)",recv_pack->data.recv_id, recv_pack->data.recv_user, recv_pack->data.send_id, recv_pack->data.send_user);
    mysql_query(&mysql, handle);

    return 0;
}