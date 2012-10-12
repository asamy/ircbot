/*
 * simple irc database bot, based on debian's dpkg bot.
 * Copyright (c) 2012 fallen <f.fallen45@gmail.com>
 *
 * "THE BEER-WARE LICENSE" (Revision 42):
 * fallen wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.
 *
 * Hash map implementation (c) 2009, 2011 Per Ola Kristensson.
 * GNU GPL v3.  See also:
 * http://pokristensson.com/code/strmap/strmap.c
 */

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <time.h>

char *g_chan = "#test_kef_bot";
char *m_nick = "testfulguy";
char *g_owner = "fallen";
char *auth = "auth";
char *auth_pw = "auth_pw";
bool verbose=true;

static void xfree(void *ptr) {
    if (ptr)
        free(ptr);
}

struct pair {
    char *key;
    char *value;
};

struct bucket {
    unsigned int count;
    struct pair *pairs;
};

struct map {
    unsigned int count;
    struct bucket *buckets;
};

static struct map g_database = {
    .count = 1,
    .buckets = NULL
};

static struct map g_shitlist = {
    .count = 1,
    .buckets = NULL
};

static struct pair *get_pair(struct bucket *bucket, const char *key);
static unsigned long hash(const char *str);

static void map_init(struct map* map) {
    map->buckets = malloc(map->count * sizeof(struct bucket));
    if (map->buckets == NULL) {
        fprintf(stderr,
                "fatal: failed to allocate buckets of count %u for map\n",
                map->count);
        exit(EXIT_FAILURE);
    }

    memset(map->buckets, 0, map->count * sizeof(struct bucket));
}

/* Might be handy when clearing database etc. */
static void map_free(struct map *map) {
    unsigned i, j, n, m;
    struct bucket *bucket;
    struct pair *pair;

    if (map == NULL)
        return;

    n = map->count;
    bucket = map->buckets;
    i = 0;

    while (i < n) {
        m = bucket->count;
        pair = bucket->pairs;

        j= 0;
        while (j < m) {
            xfree(pair->key);
            xfree(pair->value);
            pair++;
            j++;
        }

        xfree(bucket->pairs);
        bucket++;
        i++;
    }

    xfree(map->buckets);
}

static int map_get(const struct map *map, const char *key, char *out_buf,
        unsigned int n_out) {
    unsigned int index;
    struct bucket *bucket;
    struct pair *pair;

    if (!map)
        return 0;
    if (!key)
        return 0;

    index = hash(key) % map->count;
    bucket = &map->buckets[index];
    pair = get_pair(bucket, key);

    if (!pair)
        return 0;

    if (out_buf == NULL && n_out == 0)
        return strlen(pair->value) + 1;

    if (out_buf == NULL)
        return 0;

    if (strlen(pair->value) >= n_out)
        return 0;

    strcpy(out_buf, pair->value);
    return 1;
}

static bool map_exists(const struct map *map, const char *key) {
    unsigned int index;
    struct bucket *bucket;

    if (map == NULL)
        return false;

    if (key == NULL)
        return false;

    index = hash(key) % map->count;
    bucket = &map->buckets[index];

    return get_pair(bucket, key);
}

static bool map_unset(const struct map *map, const char *key) {
    unsigned int index;
    struct bucket *bucket;
    struct pair *pair;

    if (map == NULL)
        return false;
    if (key == NULL)
        return false;

    index = hash(key) % map->count;
    bucket = &map->buckets[index];

    pair = get_pair(bucket, key);
    if (pair) {
        free(pair->key);
        free(pair->value);
        return true;
    }
    return false;
}

static int map_put(const struct map *map, const char *key,
            const char* value) {
   unsigned int key_len, value_len, index;
   struct bucket *bucket;
   struct pair *tmp_pairs, *pair;
   char *tmp_value;
   char *new_key, *new_value;

    if (map == NULL)
        return 0;

    if (key == NULL || value == NULL)
        return 0;

    key_len = strlen(key);
    value_len = strlen(value);

    index = hash(key) % map->count;
    bucket = &map->buckets[index];

    pair = get_pair(bucket, key);
    if (pair) {
        if (strlen(pair->value) < value_len) {
            tmp_value = realloc(pair->value, (value_len + 1) * sizeof(char));
            if (!tmp_value)
                return 0;

            pair->value = tmp_value;
        }
        strcpy(pair->value, value);
        return 1;
    }

    new_key = malloc((key_len + 1) * sizeof(char));
    if (!new_key)
        return 0;

    new_value = malloc((value_len + 1) * sizeof(char));
    if (!new_value)
        goto out;

    if (bucket->count == 0) {
        bucket->pairs = malloc(sizeof(struct pair));    /* initial pair */
        if (bucket->pairs == NULL)
            goto out;

        bucket->count = 1;
    } else {
        tmp_pairs = realloc(bucket->pairs, (bucket->count + 1) * sizeof(struct pair));
        if (tmp_pairs == NULL)
            goto out;

        bucket->pairs = tmp_pairs;
        bucket->count++;
    }

    pair = &bucket->pairs[bucket->count - 1];
    pair->key = new_key;
    pair->value = new_value;

    strcpy(pair->key, key);
    strcpy(pair->value, value);

    return 1;
out:
    xfree(new_key);
    xfree(new_value);
    return 0;
}

static int map_get_count(const struct map *map) {
    unsigned int i, j, n, m;
    unsigned int count;
    struct bucket *bucket;
    struct pair *pair;

    if (!map)
        return 0;

    bucket = map->buckets;
    n = map->count;
    i = 0;
    count = 0;

    while (i < n) {
        pair = bucket->pairs;
        m = bucket->count;
        j = 0;
        while (j < m) {
            count++;
            pair++;
            j++;
        }
        bucket++;
        i++;
    }
    return count;
}

static struct pair *get_pair(struct bucket *bucket, const char *key) {
    unsigned int i, n;
    struct pair *pair;

    n=bucket->count;
    if (n == 0)
        return NULL;

    pair = bucket->pairs;
    i = 0;
    while (i < n) {
        if (pair->key != NULL && pair->value != NULL) {
            if (strcmp(pair->key, key) == 0)
                return pair;
        }
        pair++;
        i++;
    }
    return NULL;
}

static unsigned long hash(const char *str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

static bool is_upper_string(const char *str) {
    int i;
    int len;

    if (!str || *str=='\0')
        return false;
    len = strlen(str);
    for (i = 0; i < len; i++) {
        if (!isupper(str[i]))
            return false;
    }
    return true;
}

static void __attribute__((noreturn)) error(const char *err, ...) {
    va_list ap;

    va_start(ap, err);
    vfprintf(stderr, err, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static void setup_async(int fd) {
    int old_flags;

    old_flags = fcntl(fd, F_GETFL, 0);
    if (!(old_flags & O_NONBLOCK))
        old_flags |= O_NONBLOCK;

    fcntl(fd, F_SETFL, old_flags);
}

static int get_sock(const char *addr, int port) {
    int sockfd;
    struct sockaddr_in srv;
    struct hostent *hp;
    time_t start;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        goto out_fd;

    if (!(hp = gethostbyname(addr)))
        goto out;

    bcopy((char *)hp->h_addr, (char *)&srv.sin_addr, hp->h_length);

    srv.sin_family = AF_INET;
    srv.sin_port = htons(6667);

    setup_async(sockfd);

    start = time(NULL);
    while (time(NULL) - start < 10) {
        errno = 0;
        printf("Trying %s...\n", addr);
        if (connect(sockfd, (struct sockaddr *)&srv, sizeof(srv)) == -1) {
            switch (errno) {
                case EISCONN:
                case EALREADY:
                case EINPROGRESS:
                    goto job_done;
                    break;
                default:
                    printf("%s\n", strerror(errno));
                    sleep(1);
                    continue;
            }
        }
job_done:
        printf("%s seems OK\n", addr);
        setsockopt(sockfd, SOL_SOCKET, SO_LINGER, 0, 0);
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 0, 0);
        setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, 0, 0);
        return sockfd;
    }

out:
    error("failed to resolve %s: %s\n", addr, strerror(errno));
out_fd:
    if (sockfd != -1)
        close(sockfd);
    error("failed to connect to %s: %s\n", addr, strerror(errno));
}

static int sends(int fd, char *buf, ...) {
    char *buff;
    int len;
    int written;

    va_list va;

    va_start(va, buf);
    len = vasprintf(&buff, buf, va);
    va_end(va);

    written = write(fd, buff, len);
    free(buff);

    return written;
}

/* String misc */
static int strwildmatch(const char *pattern, const char *string) {
    switch (*pattern) {
        case '\0': return *string;
        case '*': return !(!strwildmatch(pattern+1, string) || (*string && !strwildmatch(pattern, string+1)));
        case '?': return !(*string && !strwildmatch(pattern+1, string+1));
        default: return !((toupper(*pattern) == toupper(*string)) && !strwildmatch(pattern+1, string+1));
    }
}

static void filter(char *a) {
    while (a[strlen(a)-1] == '\r' || a[strlen(a)-1] == '\n') a[strlen(a)-1] = 0;
}

static  void _376(int fd, char *sender, char *str) {
    sends(fd, "PRIVMSG Q@CServe.quakenet.org :AUTH %s %s\n",
            auth, auth_pw);
    sends(fd, "MODE %s +x\n", m_nick);
    sends(fd, "JOIN %s\n", g_chan);
}

static void _PING(int fd, char *sender, char *str) {
    printf("Ping pong\n");
    sends(fd, "PONG %s\n", str);
}

static void _433(int fd, char *sender, char *str) {
    if (*str == '*')
        sends(fd, "NICK %s\n", m_nick);
}

static void _recon(int fd, char *sender, char *str) {
    close(fd);
}

static void help_cmd(int fd, char *sender, int argc, char **argv);
static void set_cmd(int fd, char *sender, int argc, char **argv)
{
    char desc[2048], *to;
    int i;
    char *what;

    if (map_exists(&g_shitlist, sender))
        return;
    if (argc < 2) {
        sends(fd, "PRIVMSG %s :%s: usage set <what> <description>\n", g_chan, sender);
        return;
    }
    what = argv[0];
    to = desc;
    for (i = 1; i < argc; i++) {
        to = stpcpy(to, argv[i]);
        to = stpcpy(to, " ");
    }

    desc[2047] = 0;
    if (!map_exists(&g_database, what)) {
        map_put(&g_database, what, desc);
        sends(fd, "PRIVMSG %s :Ok, %s\n", g_chan, sender);
    } else {
        sends(fd, "PRIVMSG %s :Sorry, %s, the term %s has a description.  Try !what %s %s\n",
                g_chan, sender, what, sender, what);
    }
}

static void send_term(int fd, char *sender, char *term, char *out) {
    static const char *ok_responds[] = {
        "I heard", "I think",  NULL
    };
    int i = rand() % (sizeof(ok_responds) / sizeof(ok_responds[0]));
    if (ok_responds[i] == NULL)
        i=0;
    sends(fd, "PRIVMSG %s :%s %s was %s, %s\n", g_chan, ok_responds[i], term, out, sender);
}

static void give_cmd(int fd, char *sender, int argc, char **argv)
{
    int i;
    char out_buf[2048];
    static const char *nope_responds[] = {
        "I don't know",
        "Sorry, I can't find that term.",
        "What is it?",  "I'm done",
        "Why don't you RTFM for it?",
        "Have you tried reading the manual?",
        "I give up, what is it?", 
        NULL
    };

    if (argc < 2) {
        sends(fd, "PRIVMSG %s :%s: Usage !give <who> <what>\n", g_chan, sender);
        return;
    }

    if (!!map_get(&g_database, argv[1], out_buf, 2048)) {
        send_term(fd, argv[0], argv[1], out_buf);
    } else {
        i = rand() % (sizeof(nope_responds) / sizeof(nope_responds[0]));
        if (nope_responds[i] == NULL)
            i=0;
        sends(fd, "PRIVMSG %s :%s, %s\n", g_chan, nope_responds[i], sender);
    }
}

static void count_cmd(int fd, char *sender, int argc, char **argv) {
    if (map_exists(&g_shitlist, sender))
        return;
    sends(fd, "PRIVMSG %s :I have %d buckets in database, %s\n",
            g_chan, map_get_count(&g_database), sender);
}

static void rm_cmd(int fd, char *sender, int argc, char **argv) {
    int i, j;

    if (map_exists(&g_shitlist, sender))
        return;
    if (strncmp(sender, g_owner, strlen(g_owner)))
        return;
    if (argc < 1) {
        sends(fd, "PRIVMSG %s :%s: usage !rm <...>\n",
                g_chan, sender);
        return;
    }

    for (i = 0; i < argc; i++) {
        if (map_exists(&g_database, argv[i])) {
            map_unset(&g_database, argv[i]);
            j++;
        }
    }

    if (j > 0) {
        sends(fd, "PRIVMSG %s :Successfully, removed %d elements, %s\n",
            g_chan, j, sender);
    } else {
        sends(fd, "PRIVMSG %s :Can't find that term, %s\n",
                g_chan, sender);
    }
}

static void sl_cmd(int fd, char *sender, int argc, char **argv) {
    int i;
    if (strncmp(sender, g_owner, strlen(g_owner)))
        return;
    for (i = 0; i < argc; i++) {
        if (i+1 < argc)
            map_put(&g_shitlist, argv[i], argv[i+1]);
        else
            map_put(&g_shitlist, argv[i], "No reason was given.");
    }
}

static void unsl_cmd(int fd, char *sender, int argc, char **argv) {
    int i;
    if (strncmp(sender, g_owner, strlen(g_owner)))
        return;
    for (i = 0; i <argc; i++) {
        if (map_exists(&g_shitlist, argv[i]))
            map_unset(&g_shitlist, argv[i]);
    }
}

static void why_cmd(int fd, char *sender, int argc, char **argv) {
    char out_buf[1024];
    if (argc < 1) {
        sends(fd, "PRIVMSG %s :%s: usage !why <name>\n", g_chan, sender);
        return;
    }

    if (!!map_get(&g_shitlist, argv[0], out_buf, 1024))
        sends(fd, "PRIVMSG %s :%s was shitlisted for being %s\n",
                g_chan, argv[0], out_buf);
}

static void quit_cmd(int fd, char *sender, int argc, char **argv) {
    if (strncmp(sender, g_owner, strlen(g_owner)))
        return;
    if (argc == 1)
        sends(fd, "QUIT :shutting down\n");
    else
        sends(fd, "QUIT :%s\n", argv[0]);
    exit(EXIT_SUCCESS);
}

static void clear_cmd(int fd, char *sender, int argc, char **argv) {
    if (strncmp(sender, g_owner, strlen(g_owner)))
        return;
    map_free(&g_database);
    map_init(&g_database);
}

static void slc_cmd(int fd, char *sender, int argc, char **argv) {
    if (strncmp(sender, g_owner, strlen(g_owner)))
        return;
    map_free(&g_shitlist);
    map_init(&g_shitlist);
}

static const struct command {
    const char *name;
    void (*cmd_func) (int fd, char *sender, int argc, char **argv);
} commands[] = {
    { "help",  help_cmd    },
    { "set",   set_cmd     },
    { "add",   set_cmd     },
    { "give",  give_cmd    },
    { "count", count_cmd   },
    { "clear", clear_cmd   },
    { "slc",   slc_cmd     },
    { "rm",    rm_cmd      },
    { "sl",    sl_cmd      },
    { "unsl",  unsl_cmd    },
    { "why",   why_cmd     },
    { "quit",  quit_cmd    },
    { NULL,   NULL         }
};

static void help_cmd(int fd, char *sender, int argc, char **argv) {
    int i;
    char buffer[1024], *p;
    p=buffer;
    sends(fd, "PRIVMSG %s :Available commands:\n", g_chan);
    for (i = 0; commands[i].name != NULL; i++) {
        p = stpcpy(p, commands[i].name);
        p = stpcpy(p, " ");
    }
    sends(fd, "PRIVMSG %s :%s\n", g_chan, buffer);
}

static void _PRIVMSG(int fd, char *sender, char *str) {
    char *from, *message;
    int i;
    char *p, **argv;
    int j, argc;

    for (i=0;i<strlen(sender)&&sender[i]!='!';i++);
    sender[i]=0;
    sender++;   /* strip the : */

    for (i=0;i<strlen(str) && str[i] != ' ';i++);
    str[i]=0;
    from=str;
    message=str+i+2;

    if (from[0] == '#') {
        if (message[0] == '!') {
            message++;
            for (i=0; commands[i].name != NULL; i++) {
                int len = strlen(commands[i].name);
                if (!strncmp(message, commands[i].name, len)) {
                    message += len + 1;

                    argv = (char **)calloc(1024, sizeof(char *));
                    for (p = strtok(message, " "), j =0; p && *p && j < 1024; p=strtok((char *)NULL, " "),j++)
                        argv[j] = strdup(p);
                    argv[j + 1] = NULL;
                    argc = j;

                    commands[i].cmd_func(fd, sender, argc, argv);
                    for (i = 0; i < argc; i++)
                        free(argv[i]);
                    xfree(argv);
                    break;
                }    
            }
        } else {
            char out_buf[1024];
            if (message && !!map_get(&g_database, message, out_buf, 1024))
                send_term(fd, sender, message, out_buf);
        }
    }
}

static const struct messages {
    char *cmd;
    void (* func)(int,char *,char *); 
} msgs[] = {
    { "376",    _376   },
    { "433",    _433   },
    { "422",    _376   },
    { "PING",   _PING  },
    { "PRIVMSG", _PRIVMSG },
    { "467",    _recon },
    { "471",    _recon },
    { "473",    _recon },
    { "474",    _recon },
    { "475",    _recon },
    { NULL,    (void (*)(int,char *,char *))0 }
};

int main(int argc, char **argv) { 
    char c;
    int optidx = 0;
    char *host = "irc.quakenet.org";
    int port = 6667;
    int sockfd = -1;
    static const struct option opts[] = {
        {"channel", required_argument,    0, 'c'},
        {"host",    required_argument,    0, 'h'},
        {"port",    required_argument,    0, 'p'},
        {"nick",    required_argument,    0, 'n'},
        {"help",    no_argument,          0, 'v'},
        {"silent",  no_argument,          0, 's'},
        {0,         0,                    0,  0 }
    };

    srand(time(NULL));
    while ((c = getopt_long(argc, argv, "c:h:p:n:vs", opts, &optidx)) != -1) {
        switch (c) {
            case 'c': g_chan=optarg; break;
            case 'h': host=optarg; break;
            case 'p': port=atoi(optarg); break;
            case 'n': m_nick=optarg; break;
            case 's': verbose=false; break;
            case 'v':
                printf("Usage %s v0.01 <options...>\n", argv[0]);
                printf("Mandatory arguments to long options are mandatory for short options too.\n");
                printf("Options: --config --host --port --nick --help --silent --channel --init\n");
                printf("All of the above options take an extra argument which is the value'n");
                printf("Except -h|--help\n");
                exit(EXIT_SUCCESS);
                break;
            case '?': /* getopt already thrown an error. */ return 1;
            default:
                if (optopt == 'c')
                    fprintf(stderr, "Option -%c requires an argument.\n",
                        optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option -%c.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character '\\x%x'.\n",
                        optopt);
               return 1;
        }
    }

    map_init(&g_database);
    map_init(&g_shitlist);
derp:
    sockfd = get_sock(host, port);
    while (true) {
        fd_set f;
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

        FD_ZERO(&f);
        FD_SET(sockfd, &f);

        select(sockfd + 1, &f, NULL, NULL, &tv);
        if (!FD_ISSET(sockfd, &f))
            continue;

        char buff[4096], *str;
        unsigned int i;

        if ((i = read(sockfd, buff, 4096)) <= 0) goto derp;
        buff[i] = '\0';

        str = strtok(buff, "\n");
        while (str && *str) {
            char name[1024], sender[1024];
            filter(str);

            if (*str == ':') {
                for (i = 0; i < strlen(str) && str[i] != ' '; i++);
                str[i] = '\0';
                strcpy(sender, str);
                strcpy(str, str+i+1);
            } else
                strcpy(sender, "*");

            for (i = 0; i < strlen(str) && str[i] != ' '; i++);
            str[i]= 0;
            strcpy(name, str);
            strcpy(str, str+i+1);

            static bool sent = false;    /* pre-connect phase */
            if (!sent && strcmp(name, "NOTICE") == 0) {
                sends(sockfd, "NICK %s\nUSER %s localhost localhost :%s\n", m_nick, m_nick, m_nick);
                sent = true;
            }
            for (i = 0; msgs[i].cmd != NULL; i++)
                if (!strcasecmp(msgs[i].cmd, name))
                    msgs[i].func(sockfd, sender, str);

            if (!strcasecmp(name, "ERROR")) goto derp;
            str = strtok((char *)NULL, "\n");
        }
    }

    return 0;
}

