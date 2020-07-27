/**
 * file    prop_body.c
 * brief   プロパティファイル読み出し
 * note    COPYRIGHT FUJITSU LIMITED 2010
 *         FCT)Yagi
 *
 * 版数   日付    変更者    リリースノート               
 * ---- -------- --------- --------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>

static SLIST_HEAD(, propertie_data)  prop_head;

/* local function prototype. */
static FILE *open_prop_file(void);
static void parse_header(void);
static int parse_file(FILE *);
static void parse_line(char *);
static void add_properties(const char *, const char *);

/**
 * @brief プロパティDBの初期化
 */
void anycast_prop_init(void)
{
    FILE *file = open_prop_file();

    SLIST_INIT(&prop_head);

    if (file == NULL) {
        mlog("property file not found");
        // ファイルがなかったらヘッダの中の固定ファイルから展開
        parse_header();
    } else {
        // ファイルの展開処理
        if (parse_file(file) < 0) {
            parse_header();
        }
    }
}

/**
 * @brief propertiesファイルをopenしてみる。
 * とりあえず今はカレントディレクトリにファイルがある前提。
 * @warning FIXME: ファイルパスがどうなるかは詰めが必要.
 */
static FILE *open_prop_file()
{
    char cwd[128];
    char path[256];
    
    /*ディレクトリパスを設定*/
    strncpy(cwd, PROP_DIRNAME, 128);
    sprintf(path, "%s/%s", cwd, PROP_FILENAME);

    return fopen(path, "r");
}

/**
 * @brief ファイルがなかったらとりあえずヘッダ内のヘッダから直接展開する。
 */
static void parse_header()
{
#ifdef FRONT_T
    prop_db_t *tb = prop_db_front;
#else
    prop_db_t *tb = prop_db_backend;
#endif
    for (; tb->key != NULL; tb++) {
        add_properties(tb->key, tb->value);
    }
}

/**
 * @brief ファイルからプロパティ情報を展開
 */
static int parse_file(FILE * file)
{
    while (1) {
        char buff[128];
        fgets(buff, 128, file);
        if (feof(file)) {
            break;
        }
        if ((strlen(buff) == 1) && (buff[0] == '\n')) {
            continue;
        }
        if (buff[0] == '#') {
            continue;
        }
        /* 行データ解析 */
        parse_line(buff);
    }
    fclose(file);
    return 0;
}

/**
 * @brief 行データ解析処理
 */
static void parse_line(char *line)
{
    int slen;
    char *p1 = line;
    char *p2 = strchr(line, '=');
    char buff[128];

    strncpy(buff, line, 128);
    if (p2 == NULL) {
        mlog("init anycast prop def error(%s)", buff); 
        return;
    }
    /* 行分割 */
    *p2 = '\0';
    if ((strlen(p1) == 0) || /*(strlen(p2 + 1) == 1) ||*/ (p1 == p2)) {
        mlog("anycast prop parse error(%s)", buff);
        return;
    }
    p2++;
    p2 = skip_space(p2);
    slen = strlen(p2);
    *(p2 + slen - 1) = '\0';
    add_properties(p1, p2);
}

/**
 * @brief プロパティ値を文字列で取得
 *
 * @param key  プロパティキー文字列
 * @return const char *     プロパティ値。見つからなかったらNULL
 */
const char *anycast_get_properties(const char *_key)
{
    struct propertie_data *prop;

    SLIST_FOREACH (prop, &prop_head, list)  {
        char *key, *value;
        
        key = (char *) prop->data;
        value = key + strlen(key) + 1;
        if (strcmp(key, _key) == 0) {
            return value;
        }
    }

    return NULL;
}

/**
 * @brief 数値記述のプロパティ値をlong intに数値変換して取得
 *
 * @param key  プロパティキー文字列
 * @return long int    取得できた符号付数値
 */
long int anycast_get_properties_int(const char *_key)
{
    const char *value = anycast_get_properties(_key);
    if (value == NULL) {
        return 0;
    }
    return strtol(value, NULL, 10);
}

/**
 * @brief 16進数記述(0xXXXX)のプロパティ値をlong intで取得
 *
 * @param key  プロパティキー文字列
 * @return long int    取得できた値
 * @warning 数値の範囲はLONG_MAXまでなので、16bitの最上位ビットをONにすると
 * 負数の扱いになる。
 */
long int anycast_get_properties_hex(const char *_key)
{
    const char *value = anycast_get_properties(_key);
    if (value == NULL) {
        return 0;
    }
    return strtol(value, NULL, 16);
}

/**
 * @brief プロパティデータベースに追加
 *
 * @param key   プロパティキー
 * @param value プロパティ値
 */
 // 以下のデータ形式でキューにつなぐ
 // +---------+-----------------------+-----------------------+
 // + q_head  | key                   | value                 |
 // +---------+-----------------------+-----------------------+

static void add_properties(const char *key, const char *value)
{
    int len0, len1, len2;
    char *p1, *p2;
    struct propertie_data *prop;

    len0 = sizeof(struct propertie_data);
    len1 = strlen(key) + 1;
    len2 = strlen(value) + 1;
    prop = (struct propertie_data*)malloc(len0 + len1 + len2);
    p1 = (char*)(prop + 1);
    p2 = p1 + len1;
    strcpy(p1, key);
    strcpy(p2, value);

    SLIST_INSERT_HEAD(&prop_head, prop, list);
}

/* end */
