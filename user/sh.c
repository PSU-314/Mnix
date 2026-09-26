// Shell.

#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "user/user.h"

// Parsed command representation
#define EXEC 1
#define REDIR 2
#define PIPE 3
#define LIST 4
#define BACK 5

#define MAXARGS 10

struct cmd {
        int type;
};

struct execcmd {
        int type;
        char *argv[MAXARGS];
        char *eargv[MAXARGS];
};

struct redircmd {
        int type;
        struct cmd *cmd;
        char *file;
        char *efile;
        int mode;
        int fd;
};

struct pipecmd {
        int type;
        struct cmd *left;
        struct cmd *right;
};

struct listcmd {
        int type;
        struct cmd *left;
        struct cmd *right;
};

struct backcmd {
        int type;
        struct cmd *cmd;
};

int fork1(void); // Fork but panics on failure.
void panic(char *);
struct cmd *parsecmd(char *);
void runcmd(struct cmd *) __attribute__((noreturn));

// Execute cmd.  Never returns.
void runcmd(struct cmd *cmd) {
    int p[2];
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if (cmd == 0)
        exit(1);

    switch (cmd->type) {
    default:
        panic("runcmd");

    case EXEC:
        ecmd = (struct execcmd *)cmd;
        if (ecmd->argv[0] == 0)
            exit(1);
        exec(ecmd->argv[0], ecmd->argv);
        fprintf(2, "exec %s failed\n", ecmd->argv[0]);
        break;

    case REDIR:
        rcmd = (struct redircmd *)cmd;
        close(rcmd->fd);
        if (open(rcmd->file, rcmd->mode) < 0) {
            fprintf(2, "open %s failed\n", rcmd->file);
            exit(1);
        }
        runcmd(rcmd->cmd);
        break;

    case LIST:
        lcmd = (struct listcmd *)cmd;
        if (fork1() == 0)
            runcmd(lcmd->left);
        wait(0);
        runcmd(lcmd->right);
        break;

    case PIPE:
        pcmd = (struct pipecmd *)cmd;
        if (pipe(p) < 0)
            panic("pipe");
        if (fork1() == 0) {
            close(1);
            dup(p[1]);
            close(p[0]);
            close(p[1]);
            runcmd(pcmd->left);
        }
        if (fork1() == 0) {
            close(0);
            dup(p[0]);
            close(p[0]);
            close(p[1]);
            runcmd(pcmd->right);
        }
        close(p[0]);
        close(p[1]);
        wait(0);
        wait(0);
        break;

    case BACK:
        bcmd = (struct backcmd *)cmd;
        if (fork1() == 0)
            runcmd(bcmd->cmd);
        break;
    }
    exit(0);
}

int getcmd(char *buf, int nbuf) {
    write(2, "$ ", 2);
    memset(buf, 0, nbuf);
    gets(buf, nbuf);
    if (buf[0] == 0) // EOF
        return -1;
    return 0;
}

void updatepwd(char *pwdbuf, char *path) {
    char tmp[512];
    char *component;
    char *tokens[64];
    int ntok = 0;
    int i;
    int len;

    // Start from root if path is absolute, else from the current cwd.
    if (path[0] == '/') {
        strcpy(tmp, "/");
    } else {
        strcpy(tmp, pwdbuf);
        if (tmp[strlen(tmp) - 1] != '/')
            strcpy(tmp + strlen(tmp), "/");
    }
    strcpy(tmp + strlen(tmp), path);

    // Tokenize tmp on '/', collecting components into tokens[],
    // applying "." (skip) and ".." (pop) as we go.
    component = tmp;
    while (*component == '/')
        component++;
    while (*component) {
        char *start = component;
        while (*component && *component != '/')
            component++;
        len = component - start;
        *component = 0; // NUL-terminate this component in place

        if (len == 0 || (len == 1 && start[0] == '.')) {
            // skip empty or "."
        } else if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (ntok > 0)
                ntok--; // pop last component
        } else {
            tokens[ntok++] = start;
        }

        component++;
        while (*component == '/')
            component++;
    }

    // Rebuild cwdbuf from tokens.
    strcpy(pwdbuf, "/");
    for (i = 0; i < ntok; i++) {
        if (pwdbuf[strlen(pwdbuf) - 1] != '/')
            strcpy(pwdbuf + strlen(pwdbuf), "/");
        strcpy(pwdbuf + strlen(pwdbuf), tokens[i]);
    }
}

int main(void) {
    static char buf[100];
    static char pwdbuf[512] = "/";
    static const char *clearcmd = "cls\n";
    static const char *pwdcmd = "pwd\n";
    int fd;

    // Ensure that three file descriptors are open.
    while ((fd = open("console", O_RDWR)) >= 0) {
        if (fd >= 3) {
            close(fd);
            break;
        }
    }

    // Read and run input commands.
    while (getcmd(buf, sizeof(buf)) >= 0) {
        char *cmd = buf;
        while (*cmd == ' ' || *cmd == '\t')
            cmd++;
        if (*cmd == '\n') // is a blank command
            continue;
        if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' ') {
            // Chdir must be called by the parent, not the child.
            cmd[strlen(cmd) - 1] = 0; // chop \n
            if (chdir(cmd + 3) < 0)
                fprintf(2, "cannot cd %s\n", cmd + 3);
            else
                updatepwd(pwdbuf, cmd + 3);
        } else if (memcmp(cmd, pwdcmd, 4) == 0) {
            printf("%s\n", pwdbuf);
        } else if (memcmp(cmd, clearcmd, 4) == 0) {
            printf("\033[2J\033[H");
        } else {
            if (fork1() == 0)
                runcmd(parsecmd(cmd));
            wait(0);
        }
    }
    exit(0);
}

void panic(char *s) {
    fprintf(2, "%s\n", s);
    exit(1);
}

int fork1(void) {
    int pid;

    pid = fork();
    if (pid == -1)
        panic("fork");
    return pid;
}

// PAGEBREAK!
//  Constructors

struct cmd *execcmd(void) {
    struct execcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = EXEC;
    return (struct cmd *)cmd;
}

struct cmd *redircmd(struct cmd *subcmd, char *file, char *efile, int mode,
                     int fd) {
    struct redircmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = REDIR;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->efile = efile;
    cmd->mode = mode;
    cmd->fd = fd;
    return (struct cmd *)cmd;
}

struct cmd *pipecmd(struct cmd *left, struct cmd *right) {
    struct pipecmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = PIPE;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd *)cmd;
}

struct cmd *listcmd(struct cmd *left, struct cmd *right) {
    struct listcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = LIST;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd *)cmd;
}

struct cmd *backcmd(struct cmd *subcmd) {
    struct backcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = BACK;
    cmd->cmd = subcmd;
    return (struct cmd *)cmd;
}
// PAGEBREAK!
//  Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq) {
    char *s;
    int ret;

    s = *ps;
    while (s < es && strchr(whitespace, *s))
        s++;
    if (q)
        *q = s;
    ret = *s;
    switch (*s) {
    case 0:
        break;
    case '|':
    case '(':
    case ')':
    case ';':
    case '&':
    case '<':
        s++;
        break;
    case '>':
        s++;
        if (*s == '>') {
            ret = '+';
            s++;
        }
        break;
    default:
        ret = 'a';
        while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
            s++;
        break;
    }
    if (eq)
        *eq = s;

    while (s < es && strchr(whitespace, *s))
        s++;
    *ps = s;
    return ret;
}

int peek(char **ps, char *es, char *toks) {
    char *s;

    s = *ps;
    while (s < es && strchr(whitespace, *s))
        s++;
    *ps = s;
    return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *parsecmd(char *s) {
    char *es;
    struct cmd *cmd;

    es = s + strlen(s);
    cmd = parseline(&s, es);
    peek(&s, es, "");
    if (s != es) {
        fprintf(2, "leftovers: %s\n", s);
        panic("syntax");
    }
    nulterminate(cmd);
    return cmd;
}

struct cmd *parseline(char **ps, char *es) {
    struct cmd *cmd;

    cmd = parsepipe(ps, es);
    while (peek(ps, es, "&")) {
        gettoken(ps, es, 0, 0);
        cmd = backcmd(cmd);
    }
    if (peek(ps, es, ";")) {
        gettoken(ps, es, 0, 0);
        cmd = listcmd(cmd, parseline(ps, es));
    }
    return cmd;
}

struct cmd *parsepipe(char **ps, char *es) {
    struct cmd *cmd;

    cmd = parseexec(ps, es);
    if (peek(ps, es, "|")) {
        gettoken(ps, es, 0, 0);
        cmd = pipecmd(cmd, parsepipe(ps, es));
    }
    return cmd;
}

struct cmd *parseredirs(struct cmd *cmd, char **ps, char *es) {
    int tok;
    char *q, *eq;

    while (peek(ps, es, "<>")) {
        tok = gettoken(ps, es, 0, 0);
        if (gettoken(ps, es, &q, &eq) != 'a')
            panic("missing file for redirection");
        switch (tok) {
        case '<':
            cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
            break;
        case '>':
            cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE | O_TRUNC, 1);
            break;
        case '+': // >>
            cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
            break;
        }
    }
    return cmd;
}

struct cmd *parseblock(char **ps, char *es) {
    struct cmd *cmd;

    if (!peek(ps, es, "("))
        panic("parseblock");
    gettoken(ps, es, 0, 0);
    cmd = parseline(ps, es);
    if (!peek(ps, es, ")"))
        panic("syntax - missing )");
    gettoken(ps, es, 0, 0);
    cmd = parseredirs(cmd, ps, es);
    return cmd;
}

struct cmd *parseexec(char **ps, char *es) {
    char *q, *eq;
    int tok, argc;
    struct execcmd *cmd;
    struct cmd *ret;

    if (peek(ps, es, "("))
        return parseblock(ps, es);

    ret = execcmd();
    cmd = (struct execcmd *)ret;

    argc = 0;
    ret = parseredirs(ret, ps, es);
    while (!peek(ps, es, "|)&;")) {
        if ((tok = gettoken(ps, es, &q, &eq)) == 0)
            break;
        if (tok != 'a')
            panic("syntax");
        cmd->argv[argc] = q;
        cmd->eargv[argc] = eq;
        argc++;
        if (argc >= MAXARGS)
            panic("too many args");
        ret = parseredirs(ret, ps, es);
    }
    cmd->argv[argc] = 0;
    cmd->eargv[argc] = 0;
    return ret;
}

// NUL-terminate all the counted strings.
struct cmd *nulterminate(struct cmd *cmd) {
    int i;
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if (cmd == 0)
        return 0;

    switch (cmd->type) {
    case EXEC:
        ecmd = (struct execcmd *)cmd;
        for (i = 0; ecmd->argv[i]; i++)
            *ecmd->eargv[i] = 0;
        break;

    case REDIR:
        rcmd = (struct redircmd *)cmd;
        nulterminate(rcmd->cmd);
        *rcmd->efile = 0;
        break;

    case PIPE:
        pcmd = (struct pipecmd *)cmd;
        nulterminate(pcmd->left);
        nulterminate(pcmd->right);
        break;

    case LIST:
        lcmd = (struct listcmd *)cmd;
        nulterminate(lcmd->left);
        nulterminate(lcmd->right);
        break;

    case BACK:
        bcmd = (struct backcmd *)cmd;
        nulterminate(bcmd->cmd);
        break;
    }
    return cmd;
}
