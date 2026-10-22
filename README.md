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
