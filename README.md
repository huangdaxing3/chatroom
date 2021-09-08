# chatroom
####  一.　引言

 1. `聊天室基于C/S模型设计`
 2. `开发工具`
 
> Deepin GNU/Linux 20.2.1  　
> gcc编译器

 3. `项目参考`
   LinuxC编程实战
   CSDN

####   二. **功能：**
 1. 登录、注册
 2.  好友的增、删、查、屏蔽、私聊、查找聊天记录
 3. 群的增、删、查、设置管理、群聊、踢人
 4.  文件传输
 5. 支持部分离线

####  三. **概述：**
　聊天室基于C/S模型设计，所用数据库为MYSQL

服务端用到epoll+多线程实现
客户端分两个线程－一读一写

   **1 服务端**
   
   　运用了epoll+单线程
    
    
   ![在这里插入图片描述](https://img-blog.csdnimg.cn/81fe1eb0ca014f7794bfaa385c6f9d91.jpg?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3dlaXhpbl81MTI1OTgzNA==,size_16,color_FFFFFF,t_70#pic_center)
  
  1 第一先处理连接请求，事件请求等待
  
  2 若文件描述符可读则处理此事件


   **2 客户端**
   
   	　开了两个线程，一个接收服务器发的包，一个向服务器发包
     
   ![在这里插入图片描述](https://img-blog.csdnimg.cn/4c619a580482470fa19fc300898a49e3.jpg#pic_center)
   
   1.第一目录：注册登录  
   
   2.第二目录：细分为－朋友、群、修改密码、好友请求、退出

   3.第三目录：实现聊天室的各项功能


####  四. 编译运行

`服务端：`
>  gcc -I/usr/include/mariadb/mysql server.c -lmysqlclient -ldl -lpthread -o server

>  ./server

`客户端：`

> gcc client.c -o client -lpthread

> ./client -a 127.0.0.1 -p 4507     【**-a后加IP，-p后加设置端口号**】
