# chatserver-moduo-mysql-nginx-redis
集群聊天服务器和客户端代码，服务器基于muduo库实现，使用mysql进行数据存储，nginx负载均衡，redis使服务器订阅消息和发布订阅

./autobuild.sh自动编译构建

安装nginx/redis/mysql
redis和mysql服务安装后自动开启
nginx编译时加--with-stream参数激活tcp负载均衡模块
/usr/local/nginx/conf$ cat nginx.conf

#user  nobody;
worker_processes  1;

#error_log  logs/error.log;
#error_log  logs/error.log  notice;
#error_log  logs/error.log  info;

#pid        logs/nginx.pid;


events {
    worker_connections  1024;
}

# nginx tcp loadbalance config
stream {
    upstream MyServer {
         server 127.0.0.1:6000 weight=1 max_fails=3 fail_timeout=30s;
         server 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;
    }

    server {
         proxy_connect_timeout 1s;
         #proxy_timeout 3s;
         listen 8000;
         proxy_pass MyServer;
         tcp_nodelay on;
    }
}

http {
    include       mime.types;
    default_type  application/octet-stream;

    #log_format  main  '$remote_addr - $remote_user [$time_local] "$request" '
    #                  '$status $body_bytes_sent "$http_referer" '
    #                  '"$http_user_agent" "$http_x_forwarded_for"';

    #access_log  logs/access.log  main;

    sendfile        on;
    #tcp_nopush     on;

    #keepalive_timeout  0;
    keepalive_timeout  65;

    #gzip  on;

    server {
        listen       80;
        server_name  localhost;

        #charset koi8-r;

        #access_log  logs/host.access.log  main;

        location / {
            root   html;
            index  index.html index.htm;
        }

        #error_page  404              /404.html;

        # redirect server error pages to the static page /50x.html
        #
        error_page   500 502 503 504  /50x.html;
        location = /50x.html {
            root   html;
        }

        # proxy the PHP scripts to Apache listening on 127.0.0.1:80
        #
        #location ~ \.php$ {
        #    proxy_pass   http://127.0.0.1;
        #}

        # pass the PHP scripts to FastCGI server listening on 127.0.0.1:9000
        #
        #location ~ \.php$ {
        #    root           html;
        #    fastcgi_pass   127.0.0.1:9000;
        #    fastcgi_index  index.php;
        #    fastcgi_param  SCRIPT_FILENAME  /scripts$fastcgi_script_name;
        #    include        fastcgi_params;
        #}

        # deny access to .htaccess files, if Apache's document root
        # concurs with nginx's one
        #
        #location ~ /\.ht {
        #    deny  all;
        #}
    }


    # another virtual host using mix of IP-, name-, and port-based configuration
    #
    #server {
    #    listen       8000;
    #    listen       somename:8080;
    #    server_name  somename  alias  another.alias;

    #    location / {
    #        root   html;
    #        index  index.html index.htm;
    #    }
    #}


    # HTTPS server
    #
    #server {
    #    listen       443 ssl;
    #    server_name  localhost;

    #    ssl_certificate      cert.pem;
    #    ssl_certificate_key  cert.key;

    #    ssl_session_cache    shared:SSL:1m;
    #    ssl_session_timeout  5m;

    #    ssl_ciphers  HIGH:!aNULL:!MD5;
    #    ssl_prefer_server_ciphers  on;

    #    location / {
    #        root   html;
    #        index  index.html index.htm;
    #    }
    #}

}

开启：crhf@crhf-VMware-Ubuntu:/usr/local/nginx/sbin$ sudo ./nginx
修改配置文件后平滑重启：sudo nginx -s reload
平滑关闭： sudo nginx -s stop

---

# 附录 A：WSL 环境配置与运行记录（2026-10-21 实测）

本附录记录在本机 WSL 里从零把项目跑起来的完整过程。**项目源码未做任何修改**，所有改动都是环境层面的。
下面每条命令都标明了执行位置：`[WSL]` = 在 WSL 的 Debian 里执行，`[Windows]` = 在 PowerShell/CMD 里执行。

## A.1 实测环境

| 项 | 值 |
| --- | --- |
| 发行版 | Debian GNU/Linux 11 (bullseye)，WSL2，网络模式 mirrored |
| 编译器 / 构建 | g++ 10.2.1、cmake 3.18.4、git 2.30.2 |
| apt 源 | `http://archive.debian.org/debian bullseye`（bullseye 的 LTS 已于 2026-08-31 结束，官方源已归档，`apt-get update` 实测 Hit 正常） |
| systemd | **无**（pid1 是 init），所以服务不会随开机自启，见 A.6 |

## A.2 安装依赖

`[WSL]`

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake \
    libboost-dev \
    default-libmysqlclient-dev libmariadb-dev-compat \
    libhiredis-dev \
    mariadb-server redis-server
```

要点：

- **`libmariadb-dev-compat` 必须装**。只装 `default-libmysqlclient-dev` 时头文件在 `/usr/include/mariadb/`，而代码里写的是 `#include <mysql/mysql.h>`，会找不到头文件。compat 包提供 `/usr/include/mysql/mysql.h` 与 `libmysqlclient.so`（实测已确认存在）。
- **Debian 11 没有 `mysql-server` 包**（实测 `apt-cache policy mysql-server` → `Candidate: (none)`），只有 `mariadb-server 10.5.29`。MariaDB 与 mysqlclient API 兼容，本项目代码无需改动即可使用。
- 代码用到 `boost/bind.hpp`、`boost/lexical_cast.hpp`，都是 header-only，`libboost-dev`（1.74）即可。

## A.3 编译安装 muduo（apt 里没有这个包，必须源码编译）

`[WSL]`

```bash
mkdir -p ~/src && cd ~/src
# github.com 直连不通（实测 ECONNREFUSED），这里显式给 git 指定 HTTP 代理
git -c http.proxy=http://127.0.0.1:10809 -c https.proxy=http://127.0.0.1:10809 \
    clone --depth 1 https://github.com/chenshuo/muduo.git

cd muduo && mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=release -DMUDUO_BUILD_EXAMPLES=NO ..
make -j8
sudo make install
```

- `-DMUDUO_BUILD_EXAMPLES=NO` 是为了避开 examples 对 protobuf 的依赖，本项目只需要 `muduo_base` 和 `muduo_net`。
- 安装结果（实测）：静态库 `/usr/local/lib/libmuduo_base.a`、`libmuduo_net.a`；头文件 `/usr/local/include/muduo/{base,net}`。静态库不需要配 `ld.so.conf`。
- 代理端口 `127.0.0.1:10809` 是本机 v2rayN/xray 的 HTTP 代理。**只有 clone 这一步需要联网到 GitHub**，装完之后运行项目不再需要代理。

## A.4 数据库准备

`src/server/db/db.cpp` 里写死了连接参数（实测读取）：

| 参数 | 值 |
| --- | --- |
| host / port | `127.0.0.1` / `3306` |
| user / password | `root` / `12345678` |
| dbname | `chat` |

**仓库里没有 `.sql` 文件**，下面的表结构是按 `src/server/model/*.cpp` 与 `src/server/chatservice.cpp` 里实际执行的 SQL 反推出来的，已存为 `[WSL] ~/CppProgram/db/chat_schema.sql`（放在仓库外，不污染 git 工作区）。

关键约束：`friend`、`offlinemessage`、`groupuser` 三张表的**列数必须与代码里 `insert into xxx values(...)` 的占位符个数严格相等**（分别是 2、2、3 列），所以不能给它们加自增 id 列，否则插入直接报错。

```
user(id PK auto_increment, name, password, state)
friend(userid, friendid)                        -- 2 列
offlinemessage(userid, message)                 -- 2 列
allgroup(id PK auto_increment, groupname, groupdesc)
groupuser(groupid, userid, grouprole)           -- 3 列
```

授权与建库（`[WSL]`，两个 sql 文件都已在 `~/CppProgram/db/` 下）：

```bash
sudo service mariadb start
sudo mysql < ~/CppProgram/db/chat_auth.sql      # 让 root 同时支持 unix_socket 免密 和 密码经 TCP 登录
sudo mysql < ~/CppProgram/db/chat_schema.sql    # 建库建表
mysql -h 127.0.0.1 -u root -p12345678 chat -e "show tables;"   # 用与程序完全相同的连接方式验证
```

> 为什么 `chat_auth.sql` 里要单独处理：Debian 的 MariaDB 默认 `root@localhost` 只走 `unix_socket` 认证，程序是用 TCP + 密码连的，不改就会 `Access denied`。这里用 `IDENTIFIED VIA unix_socket OR mysql_native_password` 保留两种登录方式。

Redis 无需配置，代码里连的是 `127.0.0.1:6379`（实测 `src/server/redis/redis.cpp:24`）。

## A.5 编译本项目

**必须显式指定 C++17。** 实测原因：`src/client/main.cpp:304` 用了 `body.data()` 作为 `void*` 传入 `recv_all`，而 `std::string::data()` 只有在 C++17 起才返回非 const 的 `char*`。Debian 11 的 g++ 10.2.1 默认标准是 `gnu++14`，所以默认编译会失败：

```
error: invalid conversion from 'const void*' to 'void*' [-fpermissive]
```

`[WSL]`

```bash
mkdir -p ~/CppProgram/build && cd ~/CppProgram/build
cmake -DCMAKE_CXX_STANDARD=17 ../chatserver-moduo
make -j8
```

产物：`~/CppProgram/build/bin/ChatServer`（约 8.8 MB）、`~/CppProgram/build/bin/ChatClient`（约 3.1 MB）。

两个踩过的坑（都是实测）：

1. **不要用 `-DCMAKE_CXX_FLAGS="-std=c++17"`**。项目顶层 `CMakeLists.txt` 写的是 `set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)`，当该变量已有值时 `set()` 会收到两个参数并拼成 `-std=c++17;-g`，最终传给编译器导致 `c++: fatal error: no input files`。用 `-DCMAKE_CXX_STANDARD=17` 绕开这个变量，实测编译参数为 `-g -std=gnu++17`，通过。
2. 构建目录放在仓库**外面**（`~/CppProgram/build`），这样 `git status` 不会被编译产物污染。项目自带的 `autobuild.sh` 也是往 `../build` 里出的。

## A.6 启动与运行

手动起服务（`service` 走 SysV 脚本，实测可用）。**2026-10-22 更新：WSL 已启用 systemd 并配好开机自启，见附录 B.7；本节命令现在只用于调试和应急，也是首次配置时的操作步骤。** 单实例用 6000 端口，集群用 6001/6002（见附录 B）：

```bash
[WSL] sudo service mariadb start
[WSL] sudo service redis-server start
[WSL] cd ~/CppProgram/build/bin && ./ChatServer 127.0.0.1 6000
```

`ChatServer` 前台运行，`Ctrl+C` 会触发 `resetHandler`，把库里所有 `online` 用户改回 `offline` 后退出。

另开一个终端跑客户端（`[WSL]`）：

```bash
cd ~/CppProgram/build/bin && ./ChatClient 127.0.0.1 6000
```

交互命令（实测自 `src/client/main.cpp`）：首页是 `1.login / 2.register / 3.quit`；**登录用的是 id 而不是用户名**（这个变体改过原版），注册成功后会回显 userid。登录后支持 `help`、`chat:friendid:message`、`addfriend:friendid`、`creategroup:groupname:groupdesc`、`addgroup:groupid`、`groupchat:groupid:message`、`loginout`。

## A.7 实测验收结果

按上面的步骤执行后，实测到的证据：

| 检查项 | 结果 |
| --- | --- |
| `ss -tlnp` | `127.0.0.1:3306`(mariadbd)、`127.0.0.1:6379`(redis-server)、`127.0.0.1:6000`(ChatServer) 全部 LISTEN |
| 注册 alice | 客户端回显 `name register sucess, userid is 1`，退出码 0 |
| MySQL 落库 | `select * from user;` → `1 alice 666666 offline` |
| 登录 + 建群 | 客户端显示 `current login user => id:1 name:alice` 与命令列表 |
| 群组建落库 | `allgroup` → `1 testgroup testdesc`；`groupuser` → `1 1 creator` |
| Redis | ChatServer 日志出现 `connect redis-server success!` 与 muduo 的 `TcpServer::newConnection` |

## A.8 已知问题与未做项

- **`autobuild.sh` 不能直接用**：脚本里 `cmake ../chatserver-moduo-mysql-nginx-redis`，而实际目录名是 `chatserver-moduo`，路径不匹配会失败。属于项目内文件，未修改；用 A.5 的命令替代即可。
- **`test/testmuduo/bin/muduo_server` 在本机跑不了**：`ldd` 实测它要求 `GLIBCXX_3.4.32`、`GLIBC_2.32/2.34`，而本机是 g++ 10.2.1 + glibc 2.31。这是从别的机器（GCC 12+）拷进来的二进制，需要的话在 `test/testmuduo` 目录里重新编译。
- **【待确认】nginx 集群层未安装**。代码本身不依赖 nginx，单实例 `ChatServer` 已完整跑通；上面正文里那份 `stream{}` 配置是"多服务器负载均衡"才需要的。需要的话再补：Debian 11 可用 `apt-get install nginx`（自带 stream 模块），或按正文说明从源码编译加 `--with-stream`。
- **【待确认】A.7 的测试数据（用户 alice、群组 testgroup）仍留在 `chat` 库里**，需要清库的话执行 `mysql -h 127.0.0.1 -u root -p12345678 chat -e "delete from user; delete from allgroup; delete from groupuser;"`。

---

# 附录 B：nginx 集群层（2026-10-21 实测）

正文那份 `nginx.conf` 是原作者的 Ubuntu 环境（源码编译到 `/usr/local/nginx`）。本机改用发行版包，结论如下。

## B.1 安装：不需要源码编译

`[WSL]`

```bash
sudo apt-get install -y nginx
```

实测 `nginx/1.18.0`。**关键结论：Debian 的 nginx 包把 stream 做成了动态模块，不需要 `--with-stream` 重新编译**：

- 模块文件：`/usr/lib/nginx/modules/ngx_stream_module.so`
- 加载入口：`/etc/nginx/modules-enabled/50-mod-stream.conf`，由 `nginx.conf` 顶部的 `include /etc/nginx/modules-enabled/*.conf;` 引入

所以正文里 `nginx -V | grep stream` 是空的（没有编译期参数），但 `stream{}` 指令可用——以 `sudo nginx -t` 的结果为准。

### 安装路径对照（重要）

本机是发行版包安装，**不存在 `/usr/local/nginx`**（实测 `ls -d /usr/local/nginx` → `No such file or directory`）。所以正文第 9 行 `/usr/local/nginx/conf$ cat nginx.conf` 和第 143 行 `cd /usr/local/nginx/sbin && sudo ./nginx` 这两条命令在本机**会直接失败**，请按下表换路径：

| 用途 | 本机（apt / `nginx-core` 包） | 正文中原作者的路径（本机不存在） |
| --- | --- | --- |
| 可执行文件 | `/usr/sbin/nginx`（`dpkg -S /usr/sbin/nginx` → `nginx-core`） | `/usr/local/nginx/sbin/nginx` |
| 主配置文件 | `/etc/nginx/nginx.conf`（本附录的 `stream{}` 块追加在其末尾） | `/usr/local/nginx/conf/nginx.conf` |
| stream 模块 | `/usr/lib/nginx/modules/ngx_stream_module.so`，由 `/etc/nginx/modules-enabled/50-mod-stream.conf` 加载 | 编译期 `--with-stream` 静态编入 |
| 日志目录 | `/var/log/nginx/` | `/usr/local/nginx/logs/` |
| 站点配置目录 | `/etc/nginx/sites-enabled/`、`/etc/nginx/conf.d/` | 无此拆分 |
| 启动 | `sudo service nginx start`（等价于直接执行 `sudo /usr/sbin/nginx`） | `cd /usr/local/nginx/sbin && sudo ./nginx` |
| 重载 / 停止 | `sudo nginx -s reload` / `sudo nginx -s stop` | 同左（这两条命令与安装方式无关，可直接用） |

自查命令（`[WSL]`）：

```bash
command -v nginx                 # -> /usr/sbin/nginx
dpkg -S /usr/sbin/nginx          # -> nginx-core: /usr/sbin/nginx
ls -d /usr/local/nginx           # -> No such file or directory（本机没有源码安装）
sudo nginx -t                    # 校验当前生效的是哪份配置
```

## B.2 配置

这个包**没有** `streams-enabled/available` 目录，所以 `stream{}` 块必须写在 `nginx.conf` 顶层（与 `http{}` 平级）。已追加到 `/etc/nginx/nginx.conf` 文件末尾：

```nginx
stream {
    upstream MyServer {
        # 两台后端权重相同(weight=1)，nginx 按新连接轮询分发
        server 127.0.0.1:6001 weight=1 max_fails=3 fail_timeout=30s;
        server 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;
    }

    server {
        listen 8000;
        proxy_pass MyServer;
        proxy_connect_timeout 1s;
        tcp_nodelay on;
    }
}
```

- 入口端口：**8000**（`listen 8000;` 默认绑 `0.0.0.0`）
- 后端：**6001 与 6002，`weight=1` 优先级相同**
- 校验：`[WSL] sudo nginx -t` → `syntax is ok / test is successful`
  （注意必须带 `sudo`，否则会以 `open() "/run/nginx.pid" failed (13: Permission denied)` 失败，这不是配置错误）

## B.3 启动与自启

分工（2026-10-22 定稿）：**mariadb / redis-server / nginx 开机自启；两个 ChatServer 实例不自启，需要时手动起**（开发时经常要停掉重编，自启反而碍事）。配置过程见 B.7。

要用集群时启动（`[WSL]`）：

```bash
sudo systemctl start "chatserver@6001" "chatserver@6002"
```

其余管理命令（`[WSL]`）：

```bash
sudo systemctl start mariadb redis-server nginx chatdb-reset "chatserver@6001" "chatserver@6002"
sudo systemctl status "chatserver@6001"          # 看单个后端
sudo journalctl -u "chatserver@6002" -n 50       # 看它的日志
sudo systemctl restart "chatserver@6001"         # 重启单个后端
```

改 nginx 配置后：`sudo nginx -t && sudo systemctl reload nginx`（`sudo nginx -s reload` 同样可用）。

两个 `ChatServer` 实例**共用同一个 `chat` 库和同一个 redis**，这是集群能工作的前提：用户在哪台机器上由 `_userConnMap`（进程内）决定，跨机转发靠 redis 的发布订阅（`ChatService::oneChat` 里查库发现 `state=online` 就 `_redis.publish`）。

## B.4 实测验收

| 检查项 | 实测结果 |
| --- | --- |
| 端口监听 | `0.0.0.0:8000`(nginx)、`127.0.0.1:6001`、`127.0.0.1:6002`(两个 ChatServer) 全部 LISTEN |
| Windows 侧可达 | `[Windows] Test-NetConnection 127.0.0.1 -Port 8000` → `True`（mirrored 网络下 WSL 的监听端口在 Windows 本机直接可见） |
| 轮询分发 | 经 8000 接入两个客户端后，`sudo ss -tnp` 显示 nginx 分别到 6001、6002 各一条 ESTAB，两个 ChatServer 各持有一条对端连接 → 两客户端落在不同后端 |
| 跨服务器消息 | A(6001) 执行 `chat:2:hello-from-alice-cross-server` → B(6002) 输出 `2026-10-21 23:24:38 [1]alice said: hello-from-alice-cross-server`，证明走了 redis 发布订阅 |
| 故障转移 | `kill -INT` 掉 6002 后，经 8000 接入的客户端仍能正常登录（落到 6001），无连接失败 |
| redis 连接 | 两个后端日志均出现 `connect redis-server success!` |

## B.5 集群下的注册与登录

客户端只要把地址写成 nginx 入口即可：

```bash
[WSL] cd ~/CppProgram/build/bin && ./ChatClient 127.0.0.1 8000
```

实测：注册 `bob` 得到 `userid is 2`（自增 id 由 MySQL 统一分配，所以**两台后端共用一个库时 id 不会冲突**）。登录后 `select id,name,state from user;` 可见 `1 alice offline` / `2 bob offline`；用户在线期间 state 为 `online`，`loginout` 或 `Ctrl+C` 后回到 `offline`。

## B.6 已解决与仍未做

- ~~【待确认】不随 WSL 开机自启~~ → **已解决，见 B.7**。最终策略：mariadb / redis-server / nginx 自启，`chatserver@6001` / `chatserver@6002` **不自启**，需要时 `sudo systemctl start` 手动拉起。
- **附录 A 里那个跑在 6000 端口的单实例已停用**，自启的是 6001/6002 两个集群实例。若要按正文那份 `nginx.conf`（upstream 写的是 6000 和 6002）跑，需要改 B.2 的配置或调整 unit 实例。
- **未做**：`proxy_timeout` 未设置（长连接空闲不会被 nginx 主动断开）；未压测分发比例；`max_fails=3` 的实际摘除行为只做了单点 kill 验证。
- **【待确认】启用 systemd 对 WSL 其他行为的影响未系统检查**。本次只验证了聊天相关的服务与端口；网络/时钟/Windows 互操作等是否有变化没有逐项对比。回退方法见 B.7 最后一节。
- 测试数据现在多了 `bob(id=2)` 和一条 `offlinemessage`，清理语句见 A.8。

## B.7 开机自启（systemd，2026-10-22 实测）

### 第一步：启用 systemd

`[WSL] /etc/wsl.conf`（改动前的原文件已备份为 `/etc/wsl.conf.bak-20261022`）：

```ini
[boot]
systemd=true

[interop]
appendWindowsPath=false

[user]
default=bchao
```

**必须 `[Windows] wsl --shutdown` 后重新进入 WSL 才生效**（会终止 WSL 内所有进程，请先关掉 VS Code 远程窗口和正在跑的东西）。生效判据（实测）：

```bash
[WSL] ps -p 1 -o comm=            # systemd   （启用前是 init(Debian)）
[WSL] systemctl is-system-running # running
```

### 第二步：两个 unit 文件

`/etc/systemd/system/chatserver@.service` —— 模板 unit，`%i` 就是监听端口，一个文件服务多个实例：

```ini
[Unit]
Description=ChatServer (muduo) instance on port %i
After=network-online.target mariadb.service redis-server.service chatdb-reset.service
Requires=chatdb-reset.service

[Service]
Type=simple
User=bchao
WorkingDirectory=/home/bchao/CppProgram/build/bin
ExecStart=/home/bchao/CppProgram/build/bin/ChatServer 127.0.0.1 %i
# main.cpp 只注册了 SIGINT 的 handler(resetHandler 会复位 online 用户并 exit)，
# systemd 默认发 SIGTERM 会绕过它，所以显式改成 SIGINT。
KillSignal=SIGINT
TimeoutStopSec=15
Restart=on-failure
RestartSec=3

[Install]
WantedBy=multi-user.target
```

`/etc/systemd/system/chatdb-reset.service` —— 开机清理数据库残留状态：

```ini
[Unit]
Description=Reset stale online state in chat DB before ChatServer boot
After=mariadb.service
Requires=mariadb.service

[Service]
Type=oneshot
RemainAfterExit=yes
# 以 root 通过 unix_socket 认证登录 MariaDB，不需要把密码写进本文件。
ExecStart=/usr/bin/mysql chat -e "update user set state='offline' where state='online';"
```

### 为什么必须有 chatdb-reset

`wsl --shutdown` 是直接销毁虚拟机，ChatServer **收不到任何信号**，`main.cpp` 里 SIGINT handler 的 `reset()` 不会执行，于是库里残留 `state='online'`。而 `ChatService::login` 遇到 `online` 会直接拒绝登录并回 `this account is using, input another!`——表现为"重启后我的账号登不上"。这个 oneshot 在两个后端启动之前把 online 全部复位。

（`KillSignal=SIGINT` 只对 `systemctl stop/restart` 有效，对 `wsl --shutdown` 无效，所以两者都要。）

### 第三步：只让基础服务自启

```bash
[WSL] sudo systemctl enable --now mariadb redis-server nginx
```

ChatServer 两个实例**不 enable**，要用时手动起（命令见 B.3）。unit 文件照样装好可用——`enable` 与否只是 `/etc/systemd/system/multi-user.target.wants/` 里的软链，可以这样核对：

```bash
[WSL] ls -1 /etc/systemd/system/multi-user.target.wants/ | grep -E "chat|nginx|redis|maria"
# 输出 mariadb.service / nginx.service / redis-server.service —— 没有 chat 条目 = 重启后不会自启
```

`chatdb-reset` 的 `is-enabled` 显示 `static`——**这是正常的**，它没有 `[Install]` 段，靠 `chatserver@` 的 `Requires=` 在你 `systemctl start chatserver@6001` 时自动被拉起来，不需要也不能单独 enable。

### 冷启动实测验收

下表是 2026-10-22 上午那轮实测，**当时两个 ChatServer 实例也已 enable**（随后按要求改成不自启，见最后一行）。

| 检查项 | 实测结果 |
| --- | --- |
| 冷启动 | `[Windows] wsl --shutdown` → 等 8 秒 → 进入 WSL，`uptime -p` = `up 0 minutes` |
| 未执行任何启动命令 | `systemctl is-active` 六个全部 `active`：mariadb / redis-server / nginx / chatdb-reset / chatserver@6001 / chatserver@6002 |
| 端口 | `ss -tln` 同时出现 `3306 6379 8000 6001 6002` |
| 功能 | 经 nginx 入口 `./ChatClient 127.0.0.1 8000` 登录成功：`current login user => id:1 name:alice` |
| 写入链路 | 对方不在线时 `chat:2:auto-boot-smoke-test` 正确落库：`offlinemessage` 出现 `{"id":1,"msg":"auto-boot-smoke-test","msgid":6,"name":"alice","toid":2}` |
| 状态复位 | 重启后 `select id,name,state from user` → alice / bob 都是 `offline`（chatdb-reset 生效） |
| journal | `Started ChatServer (muduo) instance on port 6001.` / `... 6002.` 且各有一条 `connect redis-server success!` |
| **调整后（当前定稿）** | `systemctl disable "chatserver@6001" "chatserver@6002"` → 两者 `is-enabled=disabled`（进程仍 `active`，只是重启后不再自启）；`multi-user.target.wants/` 只剩 `mariadb.service`、`nginx.service`、`redis-server.service` |

### 停用与回退

```bash
# 停掉聊天服务（它们已经不自启，所以只需 stop）
[WSL] sudo systemctl stop "chatserver@6001" "chatserver@6002"

# 如果哪天想让它们重新自启
[WSL] sudo systemctl enable "chatserver@6001" "chatserver@6002"

# 完全回到启用 systemd 之前的状态
[WSL] sudo cp /etc/wsl.conf.bak-20261022 /etc/wsl.conf
[Windows] wsl --shutdown
```

---

# 附录 C：压测结果（2026-10-28 实测）

## C.1 测试环境与工具

| 项 | 值 |
| --- | --- |
| 机器 | 本机 WSL2（Debian 11），8 逻辑处理器，内存 3.8 GB |
| 被测 | `ChatServer`（`chatserver-moduo/build/bin`，`-O0 -g`，未加优化） |
| 依赖 | MariaDB 10.5.29、redis 6.0.16，均与 ChatServer **同机** |
| 服务端线程 | `src/server/chatserver.cpp:22` 的 `setThreadNum(4)` → 1 个 accept 线程 + 4 个 IO 线程，实测 `/proc/<pid>/task` = **5 线程** |
| 测试端口 | **7001 / 7002**（独立实例，未占用集群的 6001/6002，也没经过 nginx） |
| fd 上限 | `Max open files = 10240`（soft） |
| 压测客户端 | `~/CppProgram/loadtest/chatload.py`（Python 3.9 asyncio，**放在仓库外**），协议按 `src/client/main.cpp` 与 `src/server/chatservice.cpp:13` 实现：收 `4 字节网络序长度 + JSON`，发 `JSON + \0` |

**最大的局限**：压测客户端和服务端在**同一台机器上抢 CPU**，所以吞吐数字只能用于「找瓶颈」和「比较相对变化」，不能当容量规划用。

## C.2 核心结果

### 吞吐与延迟（登录，40 个用户/轮，每轮换新进程）

| 并发 | QPS | p50 | p95 | 服务端是否存活 |
| --- | --- | --- | --- | --- |
| 1（串行） | 18.5/s | 50.9 ms | 56.7 ms | 存活 |
| 2 | 29.9/s | 58.9 ms | 75.9 ms | 存活 |
| 3 | 39.1/s | 62.8 ms | 73.5 ms | 存活 |
| 5 | 39.7/s | 70.2 ms | 119.6 ms | 存活 |
| 10 | 11.3 ~ 53.3/s（波动大） | 114 ~ 157 ms | 174 ~ 224 ms | **6 次中崩 2 次** |
| 20 | 52.9/s | 205.0 ms | 344.1 ms | 存活（侥幸） |
| 200 同时 | — | — | — | **必崩** |

注册（串行单连接）：**33.8 次/秒**，即约 30 ms/次。

### 并发容量（串行登录建立后保持连接，`HOLDC=1`）

| 保持的已登录连接数 | 结果 | 服务端 fd | 服务端 RSS | 线程数 | MariaDB `Threads_connected` |
| --- | --- | --- | --- | --- | --- |
| 200 | 全部成功，保持 8s 无异常 | 223 | 9.4 MB | 5 | 4 |
| 600 | 全部成功，保持 8s 无异常 | 623 | 10.5 MB | 5 | 5 |

按 `Max open files=10240` 推算，fd 层面还能撑到约 1 万连接；**连接数本身不是瓶颈**。

### 消息吞吐（同机转发，50 对 × 20 轮 = 1000 条）

`received=1000/1000`，`p50=25.9ms p95=37.0ms p99=39.0ms`，标称 `236.5 msg/s`。

> **这个数字不可信**：客户端每轮之间 `sleep(0.2)`，20 轮就是 4 秒，而 `send_wall` 总共才 4.23 秒——也就是说 236.5 msg/s 基本是被我自己的 sleep 除出来的，**是下限不是上限**。要测真实上限得去掉 pacing 重跑。延迟分位（p50 25.9ms）同样包含排队时间，只能当参考。

## C.3 压出来的真问题：redis 上下文被多线程共享，导致堆破坏崩溃

这是本次压测最重要的产出，**功能测试完全发现不了**（单客户端跑一天也不崩）。

### 现象

| 触发方式 | 结果 |
| --- | --- |
| 200 个连接同时登录 | `Aborted (core dumped)`，signal 6 |
| 600 个连接**同时断开** | 2/2 次必崩，日志 `double free` |
| 跨服务器发 4 对 × 10 条消息（走 `redis.publish`） | 服务端 core dump，客户端 `ConnectionResetError` |
| 10 个并发登录 | 6 次崩 2 次 |

### ASan 定位（不改源码，单独用 g++ 加 `-fsanitize=address` 编一份）

```
ERROR: AddressSanitizer: heap-use-after-free
READ of size 52, thread T3 (ChatServer2)
  #1 redisBufferWrite (libhiredis.so.0.14)
  #2 Redis::subscribe(int)      src/server/redis/redis.cpp:78
  #3 ChatService::login(...)    src/server/chatservice.cpp:98
  #12 ChatServer::onMessage(... src/server/chatserver.cpp:53

0x606000003bc3 is located 3 bytes inside of 56-byte region
freed by thread T4 (ChatServer3) here:      ← 一个 IO 线程释放
previously allocated by thread T3 (ChatServer2) here:  ← 另一个 IO 线程分配
```

### 成因（结论，不含修复代码）

`Redis` 是 `ChatService` 的单例成员，内部只有一对裸指针 `_publish_context` / `_subcribe_context`（`include/server/redis/redis.hpp:34,37`）。`Redis::subscribe()` / `unsubscribe()` 对 `_subcribe_context` 做 `redisAppendCommand` + `while(!done) redisBufferWrite(...)`，**全程没有任何锁**；`publish` 同理。

而 `setThreadNum(4)` 让每条 TCP 连接的回调在**不同的 IO 线程**上执行。于是两个用户同时登录（或同时断开）时，两个线程并发读写同一个 hiredis 上下文的内部缓冲区，把它自己的 `redisBuffer` 指针搞乱 → 重复释放 → glibc `free(): invalid pointer` / `double free` → abort。

ASan 那句「T3 分配、T4 释放」就是这件事的直接证据：同一块内存在两个 IO 线程手里被操作。

### 可以怎么往下查（不替你写）

1. 用 `-fsanitize=thread` 再编一份，TSan 会直接把「哪两行在竞争同一个地址」报出来，比 ASan 更对症。
2. 关键判断点是：**业务回调到底跑在哪个 loop 上**。`setThreadNum(4)` 意味着跑在 4 个 IO 线程之一，而 `Redis`/`_userConnMap`/各个 model 都是全局单例——要么让这些状态只被一个线程访问（例如把业务处理投递回 base loop 串行执行），要么给每个用到共享 context 的地方加锁。
3. 验证修复的判据（可判定）：`600 个连接同时断开` 与 `200 个连接同时登录` 两个场景各跑 10 次，`pgrep -f "ChatServer 127.0.0.1 700" | wc -l` 必须 10 次都是 1，且日志里没有 `double free` / `invalid pointer`。

## C.4 瓶颈定位：慢在数据库访问方式，不是连接数

- 串行登录 **p50 ≈ 51ms**、注册 ≈ 30ms。对同机 localhost 来说这个量级不正常。
- 但 MariaDB 侧 `Threads_connected` 在 600 个在线用户时**只有 4~5**，`Max_used_connections=7`，`Aborted_connects=0` → **不是连接数耗尽**（`max_connections=151` 差得远）。我最初的这个猜测被实测否证了。
- 真正的原因有两条，都能对上数据：
  1. **每个请求新建一次数据库连接**：`db.cpp` 的用法是在每个模型方法里 `MySQL mysql;` 局部对象 + `connect()`，用完即析构关闭。ASan 那轮日志里崩溃前已经打出 **46 条 `connect mysql success!`**，而登录一次要走 `query(id)` + `updateState()` 两个方法 = 两次建连。
  2. **每条写事务都 fsync**：实测 `innodb_flush_log_at_trx_commit = 1`（默认），注册/登录都带 INSERT/UPDATE，在 WSL2 的 ext4 上每次 fsync 就是几十毫秒量级——正好对上 30~50ms 的单请求延迟。
- 因此提升方向是：**复用数据库连接**（单例/连接池，或把业务收敛到单线程后共用一个连接）与**降低 fsync 频率**（`innodb_flush_log_at_trx_commit=2` 或组提交，代价是崩溃时可能丢 1 秒事务）。前者是代码层面的，后者是配置层面的（`[WSL] sudo mysqld_safe --innodb-flush-log-at-trx-commit=2` 或写进 `/etc/mysql/mariadb.conf.d/` 后重启服务）。

## C.5 复现步骤

```bash
[WSL] cd ~/CppProgram/loadtest
# 准备 700 个压测用户（会往 chat 库插入 name like 'load%' 的行）
[WSL] python3 chatload.py prep 127.0.0.1 7001 700
# 登录阶梯（HOLDC 控制建立连接的并发；=1 为串行）
[WSL] HOLDC=1 python3 chatload.py login 127.0.0.1 7001 200 10
# 并发容量：串行建立 600 个登录态并保持 8 秒
[WSL] HOLDC=1 python3 chatload.py hold 127.0.0.1 7001 600 8 <ChatServer的pid>
# 断开风暴：进程退出时 600 个连接一起关（本用例必崩）
[WSL] HOLDC=1 python3 chatload.py hold 127.0.0.1 7001 600 3
# ASan 版本（不改源码，单独编）
[WSL] cd ~/CppProgram/chatserver-moduo && g++ -std=c++17 -g -O0 -fsanitize=address \
  -Iinclude -Iinclude/server -Iinclude/server/db -Iinclude/server/model -Iinclude/server/redis -Ithirdparty \
  src/server/*.cpp src/server/db/*.cpp src/server/model/*.cpp src/server/redis/*.cpp \
  -lmuduo_net -lmuduo_base -lmysqlclient -lhiredis -lpthread -o /tmp/ChatServer.asan
```

## C.6 遗留与说明

- **测试数据留在库里**：`chat` 库中现有 700 个 `load%` 用户和若干 `offlinemessage`。清理：
  `mysql -h 127.0.0.1 -u root -p12345678 chat -e "delete from offlinemessage; delete from user where name like 'load%';"`
- **压测用的 7001/7002 实例已停**，集群的 6001/6002 未参与本轮，也不经过 nginx。**「经 nginx 的入口压测」和「跨服务器在修复后的压测」尚未做**。
- **日期更正**：附录 A/B 里写的「2026-10-21 / 10-22」是按当时 WSL 的时钟记录的，而那次 WSL 时钟比 Windows 慢（本次实测两边已一致为 2026-10-28）。也就是说 A/B 的日期标注可能整体偏早，事件**先后顺序仍然正确**，但绝对日期请以本附录的 2026-10-28 为参照。
- 崩溃是**竞态**，不是确定性崩溃：并发≥10 时表现为概率性（实测 6 次崩 2 次），并发越高或断开越集中则越接近必现。
