/* ============================================================================
 *  MiniShell v2.1 - Modern, Powerful, Beautiful Shell in C
 *  Features: line editor, tab completion, history, pipes, redirection,
 *            globbing, background jobs, env expansion, signals
 *  ============================================================================ */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>
#include <dirent.h>
#include <glob.h>
#include <pwd.h>
#include <ctype.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

/* ─────────────────────────── Colors ─────────────────────────── */
#define RST   "\x1b[0m"
#define BLD   "\x1b[1m"
#define DIM   "\x1b[2m"

#define FG_R  "\x1b[31m"
#define FG_G  "\x1b[32m"
#define FG_Y  "\x1b[33m"
#define FG_B  "\x1b[34m"
#define FG_M  "\x1b[35m"
#define FG_C  "\x1b[36m"
#define FG_W  "\x1b[37m"

#define FG_BR "\x1b[91m"
#define FG_BG "\x1b[92m"
#define FG_BY "\x1b[93m"
#define FG_BB "\x1b[94m"
#define FG_BM "\x1b[95m"
#define FG_BC "\x1b[96m"

/* ─────────────────────────── Constants ─────────────────────────── */
#define LINE_MAX_LEN  4096
#define MAX_ARGS      256
#define MAX_PIPES     64
#define HISTORY_MAX   500
#define HIST_FILE     ".minishell_history"
#define PROMPT_BUF    2048

/* ─────────────────────────── Globals ─────────────────────────── */
static struct termios g_orig_termios;
static int            g_raw_mode    = 0;
static int            g_interactive = 0;

static char  *g_history[HISTORY_MAX];
static int    g_history_count = 0;

static pid_t  g_foreground_pid = -1;
static int    g_last_status    = 0;

/* Line editor state */
static char   g_line[LINE_MAX_LEN];
static int    g_line_len = 0;
static int    g_line_cur = 0;
static int    g_hist_pos = -1;
static char   g_saved_draft[LINE_MAX_LEN];
static int    g_prompt_visible_len = 0;

/* ─────────────────────────── Error helpers ─────────────────────────── */
static void err_msg(const char *fmt, ...) {
    fprintf(stderr, BLD FG_R "  x " RST FG_BR);
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, RST "\n");
}

/* ─────────────────────────── Terminal control ─────────────────────────── */
static void term_restore(void) {
    if (g_raw_mode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_raw_mode = 0;
    }
}

static int term_raw(void) {
    if (g_raw_mode) return 0;
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) == -1) return -1;
    struct termios raw = g_orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |=  (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return -1;
    g_raw_mode = 1;
    return 0;
}

/* Compute visible width (skip ANSI, count UTF-8 chars as 1) */
static int visible_strlen(const char *s) {
    int len = 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c == 0x1b) {
            s++;
            if (*s == '[') {
                s++;
                while (*s && !isalpha((unsigned char)*s)) s++;
                if (*s) s++;
            } else if (*s) {
                s++;
            }
        } else if (c < 0x80) {
            len++; s++;
        } else if ((c & 0xE0) == 0xC0) {
            len++; s += 2;
        } else if ((c & 0xF0) == 0xE0) {
            len++; s += 3;
        } else if ((c & 0xF8) == 0xF0) {
            len++; s += 4;
        } else {
            s++;
        }
    }
    return len;
}

/* ─────────────────────────── Banner ─────────────────────────── */
static void print_banner(void) {
    printf("\n");
    printf(BLD FG_BC
        "  +-------------------------------------------------------------+\n" RST);
    printf(BLD FG_BM
        "  |                                                             |\n" RST);
    printf(BLD FG_BM
        "  |   ##     ##  ##  ###    ##  ##  #######  ##    ##  #######  |\n" RST);
    printf(BLD FG_BM
        "  |   ###   ###  ##  ####   ##  ##  ##       ##    ##  ##       |\n" RST);
    printf(BLD FG_BM
        "  |   ## # # ##  ##  ## ##  ##  ##  #####    ########  #####    |\n" RST);
    printf(BLD FG_BM
        "  |   ##  #  ##  ##  ##  ## ##  ##  ##       ##    ##  ##       |\n" RST);
    printf(BLD FG_BM
        "  |   ##     ##  ##  ##   ####  ##  #######  ##    ##  #######  |\n" RST);
    printf(BLD FG_BM
        "  |                                                             |\n" RST);
    printf(BLD FG_BC
        "  +-------------------------------------------------------------+\n" RST);
    printf("\n");
    printf(FG_BY "    >> " RST BLD "v2.1" RST DIM " - Modern Shell in C" RST "\n");
    printf(FG_BG "    *  " RST DIM "Type " RST BLD FG_BY "help" RST DIM
           " to see all features" RST "\n\n");
}

/* ─────────────────────────── Prompt ─────────────────────────── */
static void build_prompt(char *out, size_t outsz) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, "?");

    const char *home = getenv("HOME");
    char shortcwd[PATH_MAX];
    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(shortcwd, sizeof(shortcwd), "~%s", cwd + strlen(home));
    } else {
        snprintf(shortcwd, sizeof(shortcwd), "%s", cwd);
    }

    const char *user = getenv("USER");
    if (!user) {
        struct passwd *pw = getpwuid(getuid());
        user = pw ? pw->pw_name : "user";
    }

    char host[128] = "minishell";
    if (gethostname(host, sizeof(host)) == 0) {
        host[sizeof(host) - 1] = '\0';
        char *dot = strchr(host, '.');
        if (dot) *dot = '\0';
    }

    snprintf(out, outsz,
        BLD FG_BG "┌─[" RST
        BLD FG_BC "%s" RST
        DIM FG_W "@" RST
        BLD FG_BG "%s" RST
        BLD FG_BG "]" RST
        BLD FG_BB " %s" RST "\n"
        BLD FG_BG "└─" RST BLD FG_BM "❯" RST " ",
        user, host, shortcwd);

    /* compute visible length of last line */
    const char *last = strrchr(out, '\n');
    g_prompt_visible_len = visible_strlen(last ? last + 1 : out);
}

/* ─────────────────────────── History ─────────────────────────── */
static char *hist_path(void) {
    static char path[PATH_MAX];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(path, sizeof(path), "%s/%s", home, HIST_FILE);
    return path;
}

static void history_load(void) {
    FILE *f = fopen(hist_path(), "r");
    if (!f) return;
    char line[LINE_MAX_LEN];
    while (fgets(line, sizeof(line), f) && g_history_count < HISTORY_MAX) {
        line[strcspn(line, "\n")] = '\0';
        if (*line) g_history[g_history_count++] = strdup(line);
    }
    fclose(f);
}

static void history_save(void) {
    FILE *f = fopen(hist_path(), "w");
    if (!f) return;
    for (int i = 0; i < g_history_count; i++)
        fprintf(f, "%s\n", g_history[i]);
    fclose(f);
}

static void history_add(const char *line) {
    if (!line || !*line) return;
    if (g_history_count > 0 &&
        strcmp(g_history[g_history_count - 1], line) == 0) return;

    if (g_history_count < HISTORY_MAX) {
        g_history[g_history_count++] = strdup(line);
    } else {
        free(g_history[0]);
        memmove(g_history, g_history + 1, (HISTORY_MAX - 1) * sizeof(char *));
        g_history[HISTORY_MAX - 1] = strdup(line);
    }
}

/* ─────────────────────────── Line editor ─────────────────────────── */
static void led_refresh(void) {
    char buf[LINE_MAX_LEN + 256];
    char mov[32] = "";
    if (g_prompt_visible_len > 0)
        snprintf(mov, sizeof(mov), "\x1b[%dC", g_prompt_visible_len);

    int n = snprintf(buf, sizeof(buf), "\r%s\x1b[K%s", mov, g_line);
    if (n < 0 || n >= (int)sizeof(buf)) return;
    write(STDOUT_FILENO, buf, n);

    int after = g_line_len - g_line_cur;
    if (after > 0) {
        char seq[32];
        int m = snprintf(seq, sizeof(seq), "\x1b[%dD", after);
        write(STDOUT_FILENO, seq, m);
    }
}

static void led_set_line(const char *s) {
    int n = (int)strlen(s);
    if (n >= LINE_MAX_LEN) n = LINE_MAX_LEN - 1;
    memcpy(g_line, s, n);
    g_line[n] = '\0';
    g_line_len = n;
    g_line_cur = n;
}

static void led_insert(int c) {
    if (g_line_len >= LINE_MAX_LEN - 1) return;
    memmove(&g_line[g_line_cur + 1], &g_line[g_line_cur],
            g_line_len - g_line_cur + 1);
    g_line[g_line_cur] = (char)c;
    g_line_len++;
    g_line_cur++;
}

static void led_delete_before(void) {
    if (g_line_cur <= 0) return;
    memmove(&g_line[g_line_cur - 1], &g_line[g_line_cur],
            g_line_len - g_line_cur + 1);
    g_line_len--;
    g_line_cur--;
}

static void led_delete_at(void) {
    if (g_line_cur >= g_line_len) return;
    memmove(&g_line[g_line_cur], &g_line[g_line_cur + 1],
            g_line_len - g_line_cur);
    g_line_len--;
}

static void led_kill_to_end(void) {
    g_line[g_line_cur] = '\0';
    g_line_len = g_line_cur;
}

static void led_kill_to_start(void) {
    memmove(g_line, &g_line[g_line_cur], g_line_len - g_line_cur + 1);
    g_line_len -= g_line_cur;
    g_line_cur = 0;
}

static void led_delete_word(void) {
    int i = g_line_cur;
    while (i > 0 && isspace((unsigned char)g_line[i - 1])) i--;
    while (i > 0 && !isspace((unsigned char)g_line[i - 1])) i--;
    int removed = g_line_cur - i;
    memmove(&g_line[i], &g_line[g_line_cur], g_line_len - g_line_cur + 1);
    g_line_len -= removed;
    g_line_cur = i;
}

/* ─────────────────────────── Completion ─────────────────────────── */
typedef struct { char **items; int count; int cap; } MatchList;

static void ml_init(MatchList *m) { m->items = NULL; m->count = 0; m->cap = 0; }

static void ml_add(MatchList *m, const char *s) {
    if (m->count == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 16;
        m->items = realloc(m->items, m->cap * sizeof(char *));
    }
    m->items[m->count++] = strdup(s);
}

static void ml_free(MatchList *m) {
    for (int i = 0; i < m->count; i++) free(m->items[i]);
    free(m->items);
    m->items = NULL; m->count = 0; m->cap = 0;
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static int command_ok(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (!S_ISREG(st.st_mode)) return 0;
    return access(path, X_OK) == 0;
}

static void complete_command(const char *prefix, MatchList *ml) {
    const char *path = getenv("PATH");
    if (!path) path = "/usr/bin:/bin";

    char *dup = strdup(path);
    if (!dup) return;
    char *save = NULL;
    for (char *dir = strtok_r(dup, ":", &save); dir;
         dir = strtok_r(NULL, ":", &save)) {
        DIR *d = opendir(dir);
        if (!d) continue;
        struct dirent *ent;
        size_t plen = strlen(prefix);
        while ((ent = readdir(d))) {
            if (ent->d_name[0] == '.') continue;
            if (strncmp(ent->d_name, prefix, plen) != 0) continue;
            char full[PATH_MAX];
            snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
            if (command_ok(full)) {
                int found = 0;
                for (int i = 0; i < ml->count; i++)
                    if (strcmp(ml->items[i], ent->d_name) == 0) { found = 1; break; }
                if (!found) ml_add(ml, ent->d_name);
            }
        }
        closedir(d);
    }
    free(dup);
}

static void complete_path(const char *prefix, MatchList *ml) {
    char dirbuf[PATH_MAX], basebuf[PATH_MAX];
    const char *slash = strrchr(prefix, '/');

    if (slash) {
        size_t dlen = slash - prefix + 1;
        if (dlen >= sizeof(dirbuf)) return;
        memcpy(dirbuf, prefix, dlen);
        dirbuf[dlen] = '\0';
        snprintf(basebuf, sizeof(basebuf), "%s", slash + 1);
    } else {
        strcpy(dirbuf, ".");
        snprintf(basebuf, sizeof(basebuf), "%s", prefix);
    }

    DIR *d = opendir(dirbuf);
    if (!d) return;
    struct dirent *ent;
    size_t blen = strlen(basebuf);
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.' && basebuf[0] != '.') continue;
        if (strncmp(ent->d_name, basebuf, blen) != 0) continue;

        char full[PATH_MAX];
        if (strcmp(dirbuf, ".") == 0)
            snprintf(full, sizeof(full), "%s", ent->d_name);
        else
            snprintf(full, sizeof(full), "%s%s", dirbuf, ent->d_name);

        struct stat st;
        int isdir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));

        char item[PATH_MAX + 4];
        if (isdir) snprintf(item, sizeof(item), "%s/", full);
        else       snprintf(item, sizeof(item), "%s", full);
        ml_add(ml, item);
    }
    closedir(d);
}

static int common_prefix(char **items, int n, char *out, size_t outsz) {
    if (n == 0) return 0;
    snprintf(out, outsz, "%s", items[0]);
    for (int i = 1; i < n; i++) {
        size_t j = 0;
        while (out[j] && items[i][j] && out[j] == items[i][j]) j++;
        out[j] = '\0';
    }
    return (int)strlen(out);
}

static int is_command_position(void) {
    int i = 0;
    while (i < g_line_cur && isspace((unsigned char)g_line[i])) i++;
    for (; i < g_line_cur; i++) {
        if (isspace((unsigned char)g_line[i])) return 0;
        if (g_line[i] == '|' || g_line[i] == ';' ||
            g_line[i] == '&' || g_line[i] == '>') return 1;
    }
    return 1;
}

static void handle_tab(void) {
    int start = g_line_cur;
    while (start > 0 && !isspace((unsigned char)g_line[start - 1])) start--;
    int wordlen = g_line_cur - start;
    if (wordlen >= LINE_MAX_LEN) return;

    char word[LINE_MAX_LEN];
    memcpy(word, &g_line[start], wordlen);
    word[wordlen] = '\0';

    MatchList ml; ml_init(&ml);

    if (is_command_position() && strchr(word, '/') == NULL)
        complete_command(word, &ml);
    else
        complete_path(word, &ml);

    if (ml.count == 0) { ml_free(&ml); return; }

    qsort(ml.items, ml.count, sizeof(char *), cmp_str);

    if (ml.count == 1) {
        const char *match = ml.items[0];
        int add_space = (match[strlen(match) - 1] != '/');
        int newlen = (int)strlen(match);
        if (start + newlen + (add_space ? 1 : 0) >= LINE_MAX_LEN - 1) {
            ml_free(&ml); return;
        }
        memmove(&g_line[start + newlen + (add_space ? 1 : 0)],
                &g_line[g_line_cur], g_line_len - g_line_cur + 1);
        memcpy(&g_line[start], match, newlen);
        g_line_len = g_line_len - wordlen + newlen + (add_space ? 1 : 0);
        if (add_space) g_line[start + newlen] = ' ';
        g_line_cur = start + newlen + (add_space ? 1 : 0);
        g_line[g_line_len] = '\0';
    } else {
        char prefix[LINE_MAX_LEN];
        if (common_prefix(ml.items, ml.count, prefix, sizeof(prefix)) > wordlen) {
            int newlen = (int)strlen(prefix);
            memmove(&g_line[start + newlen], &g_line[g_line_cur],
                    g_line_len - g_line_cur + 1);
            memcpy(&g_line[start], prefix, newlen);
            g_line_len = g_line_len - wordlen + newlen;
            g_line_cur = start + newlen;
            g_line[g_line_len] = '\0';
        } else {
            printf("\n");
            int cols = 80;
            struct winsize ws;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
                cols = ws.ws_col;
            int maxw = 0;
            for (int i = 0; i < ml.count; i++) {
                int w = (int)strlen(ml.items[i]);
                if (w > maxw) maxw = w;
            }
            int per_row = cols / (maxw + 2);
            if (per_row < 1) per_row = 1;
            for (int i = 0; i < ml.count; i++) {
                printf("%-*s", maxw + 2, ml.items[i]);
                if ((i + 1) % per_row == 0) printf("\n");
            }
            if (ml.count % per_row != 0) printf("\n");
        }
    }

    ml_free(&ml);
    led_refresh();
}

/* ─────────────────────────── Line reader ─────────────────────────── */
static int handle_escape_seq(void) {
    char seq[8];
    if (read(STDIN_FILENO, seq, 1) != 1) return 0;
    if (seq[0] != '[' && seq[0] != 'O') return 0;
    if (read(STDIN_FILENO, seq + 1, 1) != 1) return 0;

    switch (seq[1]) {
    case 'A':
        if (g_history_count == 0) break;
        if (g_hist_pos == -1) {
            strncpy(g_saved_draft, g_line, sizeof(g_saved_draft) - 1);
            g_saved_draft[sizeof(g_saved_draft) - 1] = '\0';
            g_hist_pos = g_history_count - 1;
        } else if (g_hist_pos > 0) {
            g_hist_pos--;
        }
        led_set_line(g_history[g_hist_pos]);
        led_refresh();
        break;
    case 'B':
        if (g_hist_pos == -1) break;
        if (g_hist_pos < g_history_count - 1) {
            g_hist_pos++;
            led_set_line(g_history[g_hist_pos]);
        } else {
            g_hist_pos = -1;
            led_set_line(g_saved_draft);
        }
        led_refresh();
        break;
    case 'C':
        if (g_line_cur < g_line_len) { g_line_cur++; write(STDOUT_FILENO, "\x1b[C", 3); }
        break;
    case 'D':
        if (g_line_cur > 0) { g_line_cur--; write(STDOUT_FILENO, "\x1b[D", 3); }
        break;
    case 'H':
        g_line_cur = 0; led_refresh(); break;
    case 'F':
        g_line_cur = g_line_len; led_refresh(); break;
    case '3':
        if (read(STDIN_FILENO, seq + 2, 1) == 1 && seq[2] == '~') {
            led_delete_at(); led_refresh();
        }
        break;
    case '1':
        if (read(STDIN_FILENO, seq + 2, 1) == 1 && seq[2] == '~') {
            g_line_cur = 0; led_refresh();
        }
        break;
    case '4':
        if (read(STDIN_FILENO, seq + 2, 1) == 1 && seq[2] == '~') {
            g_line_cur = g_line_len; led_refresh();
        }
        break;
    }
    return 1;
}

static int read_line(const char *prompt) {
    g_line_len = 0;
    g_line_cur = 0;
    g_line[0]  = '\0';
    g_hist_pos = -1;

    printf("%s", prompt);
    fflush(stdout);

    if (!g_interactive) {
        if (!fgets(g_line, sizeof(g_line), stdin)) return -1;
        g_line[strcspn(g_line, "\n")] = '\0';
        g_line_len = (int)strlen(g_line);
        g_line_cur = g_line_len;
        return 0;
    }

    if (term_raw() != 0) return -1;

    while (1) {
        unsigned char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) { term_restore(); return -1; }

        if (c == 3) {  /* Ctrl+C */
            g_line_len = 0; g_line_cur = 0;
            write(STDOUT_FILENO, "^C\n", 3);
            term_restore();
            return -2;
        }
        if (c == 4) {  /* Ctrl+D */
            if (g_line_len == 0) {
                write(STDOUT_FILENO, "\n", 1);
                term_restore();
                return -1;
            }
            led_delete_at(); led_refresh(); continue;
        }
        if (c == '\r' || c == '\n') {
            write(STDOUT_FILENO, "\n", 1);
            term_restore();
            return 0;
        }
        if (c == 12) { /* Ctrl+L */
            write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
            printf("%s", prompt);
            led_refresh();
            continue;
        }
        if (c == 1)  { g_line_cur = 0; led_refresh(); continue; }        /* Ctrl+A */
        if (c == 5)  { g_line_cur = g_line_len; led_refresh(); continue; }/* Ctrl+E */
        if (c == 21) { led_kill_to_start(); led_refresh(); continue; }   /* Ctrl+U */
        if (c == 11) { led_kill_to_end();   led_refresh(); continue; }   /* Ctrl+K */
        if (c == 23) { led_delete_word();   led_refresh(); continue; }   /* Ctrl+W */

        if (c == 27) { handle_escape_seq(); continue; }

        if (c == 127 || c == 8) {
            if (g_line_cur > 0) { led_delete_before(); led_refresh(); }
            continue;
        }
        if (c == 9) { handle_tab(); continue; }

        if (c >= 32 && c < 127) {
            led_insert(c); led_refresh(); continue;
        }
    }
}

/* ─────────────────────────── Expansion ─────────────────────────── */
static char *expand_env(const char *in) {
    size_t cap = strlen(in) * 4 + 64;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t i = 0, j = 0;

    while (in[i]) {
        if (j + 512 > cap) {
            cap *= 2;
            char *tmp = realloc(out, cap);
            if (!tmp) { free(out); return NULL; }
            out = tmp;
        }
        if (in[i] == '\\' && in[i + 1] == '$') { out[j++] = '$'; i += 2; continue; }
        if (in[i] == '\\' && in[i + 1] == '\\') { out[j++] = '\\'; i += 2; continue; }

        if (in[i] == '~' && (i == 0 || in[i - 1] == ' ' ||
                              in[i - 1] == '=' || in[i - 1] == ':')) {
            const char *home = getenv("HOME");
            if (home) {
                size_t hl = strlen(home);
                if (j + hl + 1 > cap) {
                    cap = (j + hl + 1) * 2;
                    char *tmp = realloc(out, cap);
                    if (!tmp) { free(out); return NULL; }
                    out = tmp;
                }
                memcpy(&out[j], home, hl);
                j += hl; i++;
                continue;
            }
        }
        if (in[i] == '$') {
            i++;
            if (in[i] == '?') { out[j++] = '0' + (g_last_status % 10); i++; continue; }
            if (in[i] == '{') {
                i++;
                char var[256]; size_t k = 0;
                while (in[i] && in[i] != '}' && k < sizeof(var) - 1)
                    var[k++] = in[i++];
                var[k] = '\0';
                if (in[i] == '}') i++;
                const char *v = getenv(var);
                if (v) {
                    size_t vl = strlen(v);
                    if (j + vl + 1 > cap) {
                        cap = (j + vl + 1) * 2;
                        char *tmp = realloc(out, cap);
                        if (!tmp) { free(out); return NULL; }
                        out = tmp;
                    }
                    memcpy(&out[j], v, vl);
                    j += vl;
                }
                continue;
            }
            char var[256]; size_t k = 0;
            while ((in[i] == '_' || isalnum((unsigned char)in[i])) &&
                   k < sizeof(var) - 1)
                var[k++] = in[i++];
            var[k] = '\0';
            const char *v = getenv(var);
            if (v) {
                size_t vl = strlen(v);
                if (j + vl + 1 > cap) {
                    cap = (j + vl + 1) * 2;
                    char *tmp = realloc(out, cap);
                    if (!tmp) { free(out); return NULL; }
                    out = tmp;
                }
                memcpy(&out[j], v, vl);
                j += vl;
            }
            continue;
        }
        out[j++] = in[i++];
    }
    out[j] = '\0';
    return out;
}

static int expand_globs(char **args, int argc, char ***out_argv) {
    char **result = malloc(sizeof(char *) * MAX_ARGS);
    if (!result) return 0;
    int rc = 0;

    for (int i = 0; i < argc; i++) {
        const char *a = args[i];
        int has_glob = 0;
        for (const char *p = a; *p; p++)
            if (*p == '*' || *p == '?' || *p == '[') { has_glob = 1; break; }

        if (!has_glob) {
            if (rc < MAX_ARGS - 1) result[rc++] = strdup(a);
            continue;
        }
        glob_t g; memset(&g, 0, sizeof(g));
        int r = glob(a, GLOB_NOCHECK | GLOB_TILDE, NULL, &g);
        if (r == 0) {
            for (size_t k = 0; k < g.gl_pathc && rc < MAX_ARGS - 1; k++)
                result[rc++] = strdup(g.gl_pathv[k]);
            globfree(&g);
        } else {
            if (rc < MAX_ARGS - 1) result[rc++] = strdup(a);
        }
    }
    result[rc] = NULL;
    *out_argv = result;
    return rc;
}

/* ─────────────────────────── Tokenizer ─────────────────────────── */
static int tokenize(char *line, char **tokens, int maxtok) {
    int n = 0;
    char *p = line;
    while (*p && n < maxtok - 1) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        char *start = p;
        char quote = 0;
        char *w = p;
        while (*p) {
            if (quote) {
                if (*p == quote) { quote = 0; p++; }
                else { *w++ = *p++; }
            } else if (*p == '\'' || *p == '"') {
                quote = *p; p++;
            } else if (isspace((unsigned char)*p)) {
                break;
            } else if (*p == '|' || *p == '<' || *p == '>' ||
                       *p == '&' || *p == ';') {
                if (w == start) {
                    *w++ = *p++;
                    if ((*(p - 1) == '>' && *p == '>') ||
                        (*(p - 1) == '&' && *p == '&'))
                        *w++ = *p++;
                }
                break;
            } else {
                *w++ = *p++;
            }
        }
        *w = '\0';
        if (w > start) tokens[n++] = start;
        else if (*p) {
            tokens[n++] = p;
            p++;
            if ((*(p - 1) == '>' && *p == '>') ||
                (*(p - 1) == '&' && *p == '&')) p++;
        }
    }
    tokens[n] = NULL;
    return n;
}

/* ─────────────────────────── Built-ins ─────────────────────────── */
static const char *BUILTINS[] = {
    "cd", "exit", "quit", "pwd", "echo", "clear", "help",
    "history", "env", "set", "unset", "export", "jobs", "type",
    NULL
};

static int is_builtin(const char *name) {
    for (int i = 0; BUILTINS[i]; i++)
        if (strcmp(name, BUILTINS[i]) == 0) return 1;
    return 0;
}

static int builtin_help(void) {
    printf("\n");
    printf(BLD FG_BC "  +----------------------------------------------+\n" RST);
    printf(BLD FG_BC "  |" RST BLD FG_BM "        MiniShell v2.1 - Help                " RST BLD FG_BC "|\n" RST);
    printf(BLD FG_BC "  +----------------------------------------------+\n\n" RST);

    printf(BLD FG_BY "  > Built-in Commands\n" RST);
    printf("    " FG_BG "cd" RST " [dir]         " DIM "change directory" RST "\n");
    printf("    " FG_BG "pwd" RST "                " DIM "print working directory" RST "\n");
    printf("    " FG_BG "echo" RST " [args]        " DIM "print arguments" RST "\n");
    printf("    " FG_BG "clear" RST "              " DIM "clear screen" RST "\n");
    printf("    " FG_BG "history" RST "            " DIM "show command history" RST "\n");
    printf("    " FG_BG "env" RST "                " DIM "list environment" RST "\n");
    printf("    " FG_BG "set" RST " VAR=VAL        " DIM "set env variable" RST "\n");
    printf("    " FG_BG "unset" RST " VAR            " DIM "unset env variable" RST "\n");
    printf("    " FG_BG "export" RST " VAR=VAL       " DIM "export env variable" RST "\n");
    printf("    " FG_BG "jobs" RST "               " DIM "list background jobs" RST "\n");
    printf("    " FG_BG "type" RST " cmd            " DIM "show command type" RST "\n");
    printf("    " FG_BG "exit" RST " | " FG_BG "quit" RST "        " DIM "exit shell" RST "\n\n");

    printf(BLD FG_BY "  > Features\n" RST);
    printf("    " FG_BM "-" RST " Pipes:              " FG_BM "cmd1 | cmd2 | cmd3" RST "\n");
    printf("    " FG_BM "-" RST " Redirection:        " FG_BM ">  >>  <" RST "\n");
    printf("    " FG_BM "-" RST " Background:         " FG_BM "cmd &" RST "\n");
    printf("    " FG_BM "-" RST " Globbing:           " FG_BM "*.c  file?.txt  [abc]*" RST "\n");
    printf("    " FG_BM "-" RST " Env vars:           " FG_BM "$HOME $USER ${PATH}" RST "\n");
    printf("    " FG_BM "-" RST " Sequential:         " FG_BM "cmd1 ; cmd2" RST "\n\n");

    printf(BLD FG_BY "  > Line Editing\n" RST);
    printf("    " FG_BM "Left/Right" RST "     Move cursor\n");
    printf("    " FG_BM "Up/Down" RST "        Navigate history\n");
    printf("    " FG_BM "Tab" RST "             Autocomplete files & commands\n");
    printf("    " FG_BM "Ctrl+A/E" RST "        Start / end of line\n");
    printf("    " FG_BM "Ctrl+U/K" RST "        Kill before / after cursor\n");
    printf("    " FG_BM "Ctrl+W" RST "          Delete word\n");
    printf("    " FG_BM "Ctrl+L" RST "          Clear screen\n");
    printf("    " FG_BM "Ctrl+C" RST "          Cancel current line\n");
    printf("    " FG_BM "Ctrl+D" RST "          Exit shell (empty line)\n\n");
    return 0;
}

static int builtin_cd(char **args, int argc) {
    const char *path;
    if (argc < 2) {
        path = getenv("HOME");
        if (!path) path = "/";
    } else if (strcmp(args[1], "-") == 0) {
        path = getenv("OLDPWD");
        if (!path) { err_msg("cd: OLDPWD not set"); return 1; }
        printf("%s\n", path);
    } else {
        path = args[1];
    }
    char oldpwd[PATH_MAX];
    if (!getcwd(oldpwd, sizeof(oldpwd))) oldpwd[0] = '\0';

    if (chdir(path) != 0) {
        err_msg("cd: %s: %s", path, strerror(errno));
        return 1;
    }
    if (oldpwd[0]) setenv("OLDPWD", oldpwd, 1);
    return 0;
}

static int builtin_pwd(void) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) { printf("%s\n", cwd); return 0; }
    err_msg("pwd: %s", strerror(errno));
    return 1;
}

static int builtin_echo(char **args, int argc) {
    int nl = 1, i = 1;
    if (i < argc && strcmp(args[i], "-n") == 0) { nl = 0; i++; }
    for (; i < argc; i++) {
        printf("%s", args[i]);
        if (i < argc - 1) putchar(' ');
    }
    if (nl) putchar('\n');
    return 0;
}

static int builtin_history(void) {
    printf("\n" BLD FG_BC "  --- Command History ---\n" RST);
    int start = g_history_count > 50 ? g_history_count - 50 : 0;
    for (int i = start; i < g_history_count; i++)
        printf("  " FG_BY "%4d" RST "  %s\n", i + 1, g_history[i]);
    printf("\n");
    return 0;
}

static int builtin_set(char **args, int argc) {
    if (argc < 2) {
        extern char **environ;
        for (char **e = environ; *e; e++) printf("%s\n", *e);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(args[i], '=');
        if (!eq) { err_msg("set: invalid syntax '%s'", args[i]); continue; }
        *eq = '\0';
        if (setenv(args[i], eq + 1, 1) != 0)
            err_msg("set: %s", strerror(errno));
        *eq = '=';
    }
    return 0;
}

static int builtin_unset(char **args, int argc) {
    for (int i = 1; i < argc; i++) unsetenv(args[i]);
    return 0;
}

static int builtin_env(void) {
    extern char **environ;
    for (char **e = environ; *e; e++) printf("%s\n", *e);
    return 0;
}

static int builtin_type(char **args, int argc) {
    for (int i = 1; i < argc; i++) {
        if (is_builtin(args[i])) {
            printf("%s is a shell builtin\n", args[i]);
            continue;
        }
        const char *path = getenv("PATH");
        if (!path) path = "/usr/bin:/bin";
        char *dup = strdup(path);
        char *save = NULL;
        int found = 0;
        for (char *dir = strtok_r(dup, ":", &save); dir;
             dir = strtok_r(NULL, ":", &save)) {
            char full[PATH_MAX];
            snprintf(full, sizeof(full), "%s/%s", dir, args[i]);
            if (command_ok(full)) {
                printf("%s is %s\n", args[i], full);
                found = 1; break;
            }
        }
        free(dup);
        if (!found) printf("%s: not found\n", args[i]);
    }
    return 0;
}

/* Job tracking */
typedef struct { int id; pid_t pid; char *cmd; } Job;
static Job g_jobs[128];
static int g_job_count = 0;
static int g_next_job_id = 1;

static int builtin_jobs(void) {
    if (g_job_count == 0) {
        printf("  " DIM "no background jobs" RST "\n");
        return 0;
    }
    for (int i = 0; i < g_job_count; i++) {
        int status;
        int alive = (waitpid(g_jobs[i].pid, &status, WNOHANG) == 0);
        printf("  " FG_BY "[%d]" RST "  %s  %s\n",
               g_jobs[i].id,
               alive ? FG_BG "Running" RST : FG_R "Done" RST,
               g_jobs[i].cmd);
    }
    return 0;
}

static void job_add(pid_t pid, const char *cmd) {
    if (g_job_count >= 128) return;
    g_jobs[g_job_count].id  = g_next_job_id++;
    g_jobs[g_job_count].pid = pid;
    g_jobs[g_job_count].cmd = strdup(cmd);
    g_job_count++;
}

static void job_reap(void) {
    int i = 0;
    while (i < g_job_count) {
        int status;
        pid_t r = waitpid(g_jobs[i].pid, &status, WNOHANG);
        if (r > 0) {
            int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            printf("\n  " FG_BY "[%d]" RST FG_BG " Done" RST " (%d)  %s\n",
                   g_jobs[i].id, code, g_jobs[i].cmd);
            free(g_jobs[i].cmd);
            memmove(&g_jobs[i], &g_jobs[i + 1],
                    (g_job_count - i - 1) * sizeof(Job));
            g_job_count--;
        } else {
            i++;
        }
    }
}

static int run_builtin(char **args, int argc) {
    if (argc == 0) return 0;
    const char *c = args[0];

    if (strcmp(c, "exit") == 0 || strcmp(c, "quit") == 0) {
        history_save();
        printf("\n" BLD FG_BM "  * Goodbye! *" RST "\n\n");
        exit(0);
    }
    if (strcmp(c, "cd") == 0)      return builtin_cd(args, argc);
    if (strcmp(c, "pwd") == 0)     return builtin_pwd();
    if (strcmp(c, "echo") == 0)    return builtin_echo(args, argc);
    if (strcmp(c, "history") == 0) return builtin_history();
    if (strcmp(c, "set") == 0)     return builtin_set(args, argc);
    if (strcmp(c, "unset") == 0)   return builtin_unset(args, argc);
    if (strcmp(c, "export") == 0)  return builtin_set(args, argc);
    if (strcmp(c, "env") == 0)     return builtin_env();
    if (strcmp(c, "type") == 0)    return builtin_type(args, argc);
    if (strcmp(c, "jobs") == 0)    return builtin_jobs();
    if (strcmp(c, "help") == 0)    return builtin_help();
    if (strcmp(c, "clear") == 0) {
        printf("\x1b[2J\x1b[H");
        print_banner();
        return 0;
    }
    return 0;
}

/* ─────────────────────────── Signals ─────────────────────────── */
static void sigint_handler(int sig) {
    (void)sig;
    if (g_foreground_pid > 0)
        kill(g_foreground_pid, SIGINT);
}

static void sigchld_handler(int sig) { (void)sig; }

/* ─────────────────────────── Command struct ─────────────────────────── */
typedef struct {
    char **argv;
    int    argc;
    char  *infile;
    char  *outfile;
    int    append;
    int    bg;
} Command;

static int parse_single(char **tokens, int ntokens, Command *cmd) {
    cmd->argv = calloc(MAX_ARGS, sizeof(char *));
    if (!cmd->argv) return -1;
    cmd->argc = 0;
    cmd->infile = NULL;
    cmd->outfile = NULL;
    cmd->append = 0;
    cmd->bg = 0;

    for (int i = 0; i < ntokens; i++) {
        char *t = tokens[i];
        if (strcmp(t, "<") == 0) {
            if (i + 1 >= ntokens) { err_msg("syntax error: expected file after '<'"); return -1; }
            cmd->infile = tokens[++i];
        } else if (strcmp(t, ">") == 0) {
            if (i + 1 >= ntokens) { err_msg("syntax error: expected file after '>'"); return -1; }
            cmd->outfile = tokens[++i];
            cmd->append = 0;
        } else if (strcmp(t, ">>") == 0) {
            if (i + 1 >= ntokens) { err_msg("syntax error: expected file after '>>'"); return -1; }
            cmd->outfile = tokens[++i];
            cmd->append = 1;
        } else if (strcmp(t, "&") == 0) {
            cmd->bg = 1;
        } else {
            if (cmd->argc < MAX_ARGS - 1)
                cmd->argv[cmd->argc++] = t;
        }
    }
    cmd->argv[cmd->argc] = NULL;
    return 0;
}

static void execute_pipe_chain(char **tokens, int ntokens) {
    char *segments[MAX_PIPES];
    int seg_count = 0;
    int start = 0;
    for (int i = 0; i <= ntokens; i++) {
        if (i == ntokens || strcmp(tokens[i], "|") == 0) {
            segments[seg_count++] = (char *)(intptr_t)start;
            start = i + 1;
            if (seg_count >= MAX_PIPES) break;
        }
    }

    Command cmds[MAX_PIPES];
    int ncmds = 0;
    int global_bg = 0;

    for (int s = 0; s < seg_count; s++) {
        int sstart = (int)(intptr_t)segments[s];
        int send = (s == seg_count - 1) ? ntokens
                                        : (int)(intptr_t)segments[s + 1] - 1;
        if (sstart >= send) {
            err_msg("syntax error near '|'");
            for (int k = 0; k < ncmds; k++) free(cmds[k].argv);
            return;
        }
        if (parse_single(&tokens[sstart], send - sstart, &cmds[ncmds]) != 0) {
            for (int k = 0; k <= ncmds; k++) free(cmds[k].argv);
            return;
        }
        if (cmds[ncmds].bg) global_bg = 1;
        ncmds++;
    }

    /* Single built-in without redirection */
    if (ncmds == 1 && is_builtin(cmds[0].argv[0]) &&
        !cmds[0].infile && !cmds[0].outfile && !cmds[0].bg) {
        char **expanded = NULL;
        int eargc = expand_globs(cmds[0].argv, cmds[0].argc, &expanded);
        if (eargc > 0) g_last_status = run_builtin(expanded, eargc);
        for (int i = 0; i < eargc; i++) free(expanded[i]);
        free(expanded);
        free(cmds[0].argv);
        return;
    }

    int was_raw = g_raw_mode;
    if (was_raw) term_restore();

    int in_fd = STDIN_FILENO;
    pid_t pids[MAX_PIPES];
    int npids = 0;

    for (int i = 0; i < ncmds; i++) {
        Command *c = &cmds[i];
        int need_pipe = (i < ncmds - 1);
        int pipefd[2];
        if (need_pipe && pipe(pipefd) < 0) {
            err_msg("pipe: %s", strerror(errno));
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            err_msg("fork: %s", strerror(errno));
            if (need_pipe) { close(pipefd[0]); close(pipefd[1]); }
            break;
        }

        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);

            if (in_fd != STDIN_FILENO) { dup2(in_fd, STDIN_FILENO); close(in_fd); }
            if (c->infile) {
                int fd = open(c->infile, O_RDONLY);
                if (fd < 0) { err_msg("%s: %s", c->infile, strerror(errno)); _exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (c->outfile) {
                int flags = O_WRONLY | O_CREAT | (c->append ? O_APPEND : O_TRUNC);
                int fd = open(c->outfile, flags, 0644);
                if (fd < 0) { err_msg("%s: %s", c->outfile, strerror(errno)); _exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            if (need_pipe) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            if (is_builtin(c->argv[0])) {
                char **expanded = NULL;
                int eargc = expand_globs(c->argv, c->argc, &expanded);
                int rc = eargc > 0 ? run_builtin(expanded, eargc) : 1;
                for (int k = 0; k < eargc; k++) free(expanded[k]);
                free(expanded);
                _exit(rc);
            }

            char **expanded = NULL;
            int eargc = expand_globs(c->argv, c->argc, &expanded);
            if (eargc == 0) _exit(0);
            execvp(expanded[0], expanded);
            err_msg("command not found: %s", expanded[0]);
            _exit(127);
        }

        pids[npids++] = pid;
        if (in_fd != STDIN_FILENO) close(in_fd);
        if (need_pipe) {
            close(pipefd[1]);
            in_fd = pipefd[0];
        }
    }
    if (in_fd != STDIN_FILENO) close(in_fd);

    if (global_bg) {
        for (int i = 0; i < npids; i++) {
            char jobdesc[256];
            snprintf(jobdesc, sizeof(jobdesc), "%s", cmds[0].argv[0]);
            job_add(pids[i], jobdesc);
        }
        printf("  " FG_BY "[bg]" RST " PID %d\n", pids[npids - 1]);
    } else {
        g_foreground_pid = pids[npids - 1];
        int status = 0;
        for (int i = 0; i < npids; i++)
            waitpid(pids[i], &status, 0);
        g_foreground_pid = -1;
        if (WIFEXITED(status))        g_last_status = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) g_last_status = 128 + WTERMSIG(status);
    }

    for (int i = 0; i < ncmds; i++) free(cmds[i].argv);

    if (was_raw) term_raw();
}

/* ─────────────────────────── Execute line ─────────────────────────── */
static void execute_line(char *line) {
    char *p = line;
    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p == ';')) p++;
        if (!*p) break;

        char *start = p;
        char quote = 0;
        while (*p) {
            if (quote) {
                if (*p == quote) quote = 0;
                p++;
            } else if (*p == '\'' || *p == '"') {
                quote = *p++;
            } else if (*p == ';') {
                break;
            } else {
                p++;
            }
        }
        char saved = *p;
        *p = '\0';

        char *tokens[MAX_ARGS];
        int ntok = tokenize(start, tokens, MAX_ARGS);

        if (ntok > 0) {
            char *expanded[MAX_ARGS];
            int ne = 0;
            for (int i = 0; i < ntok; i++) {
                char *e = expand_env(tokens[i]);
                if (e) expanded[ne++] = e;
            }
            expanded[ne] = NULL;
            if (ne > 0) execute_pipe_chain(expanded, ne);
            for (int i = 0; i < ne; i++) free(expanded[i]);
        }
        *p = saved;
        if (*p == ';') p++;
    }
}

/* ─────────────────────────── Cleanup & Main ─────────────────────────── */
static void cleanup(void) {
    term_restore();
    history_save();
    for (int i = 0; i < g_history_count; i++) free(g_history[i]);
    for (int i = 0; i < g_job_count; i++) free(g_jobs[i].cmd);
}

int main(void) {
    g_interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    struct sigaction sc;
    memset(&sc, 0, sizeof(sc));
    sc.sa_handler = sigchld_handler;
    sc.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigemptyset(&sc.sa_mask);
    sigaction(SIGCHLD, &sc, NULL);

    atexit(cleanup);

    history_load();

    if (g_interactive) print_banner();

    while (1) {
        job_reap();

        char prompt[PROMPT_BUF];
        build_prompt(prompt, sizeof(prompt));

        int r = read_line(prompt);
        if (r == -1) {
            printf("\n" BLD FG_BM "  * Goodbye! *" RST "\n\n");
            break;
        }
        if (r == -2) continue;

        int len = g_line_len;
        while (len > 0 && isspace((unsigned char)g_line[len - 1])) len--;
        g_line[len] = '\0';
        if (len == 0) continue;

        history_add(g_line);

        char buf[LINE_MAX_LEN];
        memcpy(buf, g_line, len + 1);
        execute_line(buf);
    }

    return 0;
}