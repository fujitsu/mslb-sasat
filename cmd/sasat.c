/*
    COPYRIGHT FUJITSU LIMITED 2010   
     
    sasatコマンド

    トランスレータの振分け設定更新(フロントのみ）とログ取得、イベントトレース
    のオン・オフコントロールを行う
    フロント、バックエンド共通で使用する

    使用法：
    sasat -p (振分け設定更新)
    sasat -l {all | stat | mlog | elog | cl | pol | svr} (ログ取得）
    sasat -t {0 | 1} (イベントトレース off/on)

    ※複数オプション同時指定可 
    例）sasat -p -l stat -l pol -l cl -t 0

    command completeと表示されたら正常終了
    
    Yagi    
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <linux/un.h>
#include <errno.h>

static unsigned char get_log_opt(char *arg);
static int send_cmd(unsigned char cmd, unsigned char opt);

#define USAGE "Usage: sasat (OPTION)\n\
  -p (振分け設定ファイルの再読み込み)\n\
  -l {all | stat | mlog | elog | cl | pol | svr} (ログ取得)\n\
  -t {0 | 1} (イベントトレースの停止/開始)\n"

#define SASAT_FILE  "/dev/shm/.sasat"

/*
    command code
*/
enum {
    UD_POLICY_UPD = 1,
    UD_DUMP_REQ,
    UD_EVTR,

    /* マジックナンバー */
    UD_MAGIC_NO = 0x46726e74
};

/*
    log種別
*/
enum {
    LOG_STAT    = 1,
    LOG_MLOG    = (1 << 1),
    LOG_EVTLOG  = (1 << 2),
    LOG_CLI     = (1 << 3),
    LOG_STAT2   = (1 << 4),
    LOG_SVR     = (1 << 5),
    LOG_ALL     = 0xff,
};

struct log_option {
    const char *name;
    unsigned char code;
};

/* 
    logオプションをコードに変換する情報
*/
const struct log_option log_option[] = {
    {"all",  LOG_ALL},
    {"stat", LOG_STAT},
    {"mlog", LOG_MLOG},
    {"elog", LOG_EVTLOG},
    {"cl",   LOG_CLI},
    {"pol",  LOG_STAT2},
    {"svr",  LOG_SVR},
    {NULL,   0}
};

/* 通信データフォーマット */
typedef struct ud_request_s {
    int magic_no;
    unsigned char req_id;
    unsigned char req_data;
    unsigned char _rsv[2];
} ud_request_t;

typedef struct ud_resp_s {
    int magic_no;
    unsigned char req_id;
    unsigned char req_data;
    unsigned char result;
    unsigned char reason_code;
} ud_resp_t;

/* getopt関数で使用する */
extern char *optarg;
extern int optind, opterr, optopt;

/* local valiables */
char ud_file[] = "/dev/shm/reqXXXXXX";

/*
    main
*/
int
main(int argc, char *argv[])
{
    int opt;
    unsigned char pol, log;
    unsigned char cmd;
    long evt;

    pol = log = evt = cmd = 0;

    while ((opt = getopt(argc, argv, "pl:t:")) != -1) {
        switch (opt) {
        case 'p':
            cmd++;
            pol = 1;
            break;
        case 'l':
            cmd++;
            log |= get_log_opt(optarg);
            break;
        case 't':
            cmd++;
            evt = strtol(optarg, NULL, 10);
            if (evt > 1) {
                fprintf(stderr, USAGE);
                exit(EXIT_FAILURE);
            }
            evt |= 0x80;
            break;
        default: /* aq?aq */
            fprintf(stderr, USAGE); 
            exit(EXIT_FAILURE);
        }
    }
    if (!cmd) {
        fprintf(stderr, USAGE);
        exit(EXIT_FAILURE);
    } 

    /* 振り分け設定更新 */
    if (pol == 1) {
        fprintf(stderr, "Update policy command\n");
        if (send_cmd(UD_POLICY_UPD, 0) < 0) {
            exit(EXIT_FAILURE);
        }
    }
    /* log */
    if (log) {
        fprintf(stderr, "Log command\n"); 
        if (send_cmd(UD_DUMP_REQ, log) < 0) {
            exit(EXIT_FAILURE);
        }
    }
    /* event trace */
    if (evt) {
        fprintf(stderr, "Event trace command\n");
        if (send_cmd(UD_EVTR, evt&~0x80) < 0) {
             exit(EXIT_FAILURE);
        }
    }
    exit(EXIT_SUCCESS);
}

/*
    コマンドオプション
*/
static unsigned char 
get_log_opt(char *arg)
{
    int i = 0;

    while (log_option[i].name != NULL) {
        if (strcmp(log_option[i].name, arg) == 0) {
            break;
        }
        i++;
    }

    return log_option[i].code;
}
    
/*
    timeout 5sec
*/
static int
read_timeo(int fd)
{
    fd_set  rset;
    struct timeval tv;

    FD_ZERO(&rset);
    FD_SET(fd, &rset);

    /* 5秒 */
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    return select(fd+1, &rset, NULL, NULL, &tv);
}

/*
    通信
*/
static int
send_cmd(unsigned char cmd, unsigned char opt)
{
    int sockfd, fd, addrlen, len, err;
    struct sockaddr_un cliaddr, servaddr;
    ud_request_t req;
    ud_resp_t resp;
    
    err = -1;

    sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket create error");
        return -1;
    }

    fd = mkstemp(ud_file);
    if (fd == -1) {
        perror("tempolary file error(mkstemp)");
        close(sockfd);
        return -1;
    }

    unlink(ud_file);

    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sun_family = AF_LOCAL;
    strncpy(cliaddr.sun_path, ud_file, sizeof(cliaddr.sun_path));
    if (bind(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0) {
        perror("bind error");
        goto cmd_end;
    }    
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strncpy(servaddr.sun_path, SASAT_FILE, sizeof(servaddr.sun_path));
    addrlen = sizeof(servaddr);

    req.magic_no = UD_MAGIC_NO;
    req.req_id = cmd;
    req.req_data = opt;

    len = sendto(sockfd, &req, sizeof(ud_request_t), 0, 
        (struct sockaddr *)&servaddr, addrlen);

    if ((len < 0) || (len != sizeof(ud_request_t))) {
        perror("sendto error");
        goto cmd_end;
    }

    if (read_timeo(sockfd) == 0) {
        fprintf(stderr, "response timeout\n");
        goto cmd_end;
    }

    len = recvfrom(sockfd, &resp,
            sizeof(ud_resp_t),
            MSG_DONTWAIT,
            (struct sockaddr *) &servaddr,
            (socklen_t *) & addrlen); 

    if ( (len < 0) || (len != sizeof(ud_resp_t))) {
        perror("recv error");
        goto cmd_end;
    }

    if (resp.result != 0) {
        fprintf(stderr, "error reported from translator, reason code = %d\n", 
            resp.reason_code);        
        goto cmd_end;
    }

    fprintf(stderr, "sasat command complete\n");

    /* no error */
    err = 0;

cmd_end:
    close(sockfd);
    close(fd);
    unlink(ud_file);

    return err;
}

/* end */
