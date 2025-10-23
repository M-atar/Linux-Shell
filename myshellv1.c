#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>
#include <dirent.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/stat.h>

#define MY_MAX_ARGS 64
#define MAX_CMDS 10
#define MAX_PATH_ENTRIES 5000
#define HISTORY_FILE "/home/spec/Desktop/Isletim_sistemleri_odev/myshell_history"


char *first_word_generator(const char *text, int state);
char **my_completion(const char *text,int start,int end);
void parse_args(char *cmd, char **args, char **in_file, char **out_file);
void init_path_cmds();
void free_path_cmds();


const char *builtins[] = {"cd", "pwd", "exit", NULL};
char **path_cmds = NULL;
int path_count = 0;

// ---------------- PATH KOMUTLARI ----------------

void init_path_cmds() {
    if (path_cmds) return;
    char *p = getenv("PATH");
    if (!p) return;

    char *copy = strdup(p);
    if (!copy) { perror("strdup basarisiz"); return; }

    path_cmds = (char**)calloc(MAX_PATH_ENTRIES + 1, sizeof(char*));
    if (!path_cmds) { perror("calloc basarisiz"); free(copy); return; }

    path_count = 0;
    char *dir = strtok(copy, ":");

    while (dir && path_count < MAX_PATH_ENTRIES) {
        DIR *d = opendir(dir);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) && path_count < MAX_PATH_ENTRIES) {
                if (e->d_name[0] != '.' || (e->d_name[1] != '\0' && (e->d_name[1] != '.' || e->d_name[2] != '\0')) ) {
                    if (e->d_type == DT_REG || e->d_type == DT_LNK || e->d_type == DT_UNKNOWN) {
                        path_cmds[path_count++] = strdup(e->d_name);
                    }
                }
            }
            closedir(d);
        }
        dir = strtok(NULL, ":");
    }
    path_cmds[path_count] = NULL;
    free(copy);
}

void free_path_cmds() {
    if (path_cmds) {
        for (int i = 0; i < path_count; i++) {
            if(path_cmds[i]) free(path_cmds[i]);
        }
        free(path_cmds);
        path_cmds = NULL;
        path_count = 0;
    }
}

// ---------------- KOMUT TAMAMLAMA JENERATÖRÜ ----------------

char *first_word_generator(const char *text, int state) {
    static int bi = 0;
    static int pi = 0;
    int len = strlen(text);

    if (state == 0) {
        bi = 0;
        pi = 0;
        init_path_cmds();
    }

    if (path_cmds == NULL) {
        goto builtins_only;
    }

    while (builtins[bi] != NULL) {
        if (strncmp(builtins[bi], text, len) == 0) {
            return strdup(builtins[bi++]);
        }
        bi++;
    }

    while (pi < path_count) {
        if (path_cmds[pi] && strncmp(path_cmds[pi], text, len) == 0) {
            return strdup(path_cmds[pi++]);
        }
        pi++;
    }

    builtins_only:

    if (path_cmds == NULL) {
        while (builtins[bi] != NULL) {
            if (strncmp(builtins[bi], text, len) == 0) {
                return strdup(builtins[bi++]);
            }
            bi++;
        }
    }

    return NULL;
}


// ---------------- ANA TAMAMLAMA FONKSİYONU ----------------

char **my_completion(const char *text, int start, int end) {
    rl_attempted_completion_over = 1;

    char **matches = NULL;

    if (start == 0) {
        matches = rl_completion_matches(text, first_word_generator);
    } else {
        matches = rl_completion_matches(text, rl_filename_completion_function);
    }

    return matches;
}

// ---------------- KOMUT AYRIŞTIRICI ve ANA FONKSİYON ----------------

void parse_args(char *cmd, char **args, char **in_file, char **out_file){
    int i=0;
    char *token=strtok(cmd," ");
    *in_file=NULL;
    *out_file=NULL;

    while(token!=NULL && i<MY_MAX_ARGS-1){
        if(strcmp(token,"<")==0){
            token=strtok(NULL," ");
            if(token) *in_file=token;
        } else if(strcmp(token,">")==0){
            token=strtok(NULL," ");
            if(token) *out_file=token;
        } else {
            args[i++]=token;
        }
        token=strtok(NULL," ");
    }
    args[i]=NULL;
}


int main(){
    char cwd[PATH_MAX];
    const char *username = getenv("USER");
    if (!username) username = "user";

    rl_completion_append_character = ' ';
    rl_basic_word_break_characters = " \t\n\"\\'`@$><=;|&{(";

    read_history(HISTORY_FILE);

    rl_attempted_completion_function = my_completion;

    while(1){
        if(getcwd(cwd,sizeof(cwd))==NULL) {
            strcpy(cwd, "myshell");
        }

        char prompt[PATH_MAX + 64];
        snprintf(prompt, sizeof(prompt),
                 "\033[32m%s\033[0m:\033[34m%s\033[0m$ ",
                 username,
                 cwd);

        char *input=readline(prompt);

        if(!input) break;

        if(strlen(input)==0){
            free(input);
            continue;
        }

        add_history(input);
        write_history(HISTORY_FILE);

        char *commands[MAX_CMDS];
        int num_cmds=0;
        char *token=strtok(input,"|");
        while(token && num_cmds<MAX_CMDS){
            commands[num_cmds++]=token;
            token=strtok(NULL,"|");
        }

        // --- TEK KOMUT DURUMU ---
        if(num_cmds==1){
            char *args[MY_MAX_ARGS],*in_file,*out_file;
            parse_args(commands[0],args,&in_file,&out_file);

            if(!args[0]){ free(input); continue; }

            // Yerleşik Komutlar
            if(strcmp(args[0],"exit")==0){ free(input); free_path_cmds(); printf("shellden cıkıs yapılıyor...\n"); break; }
            else if(strcmp(args[0],"cd")==0){
                if(!args[1]) fprintf(stderr,"Kullanım: cd <dizin>\n");
                else if(chdir(args[1])!=0) perror("cd başarısız");
                free(input); continue;
            } else if(strcmp(args[0],"pwd")==0){
                if(getcwd(cwd,sizeof(cwd))!=NULL) printf("%s\n",cwd);
                else perror("pwd başarısız");
                free(input); continue;
            }

            // Harici Komutlar için dinamik argüman listesi
            char *exec_args[MY_MAX_ARGS + 1]; // +1 for --color=auto
            int current_arg_idx = 0;

            if (strcmp(args[0], "ls") == 0) {
                exec_args[current_arg_idx++] = args[0];
                exec_args[current_arg_idx++] = (char*) "--color=auto";

                // giriline diğer argümanları kopyala
                for (int i = 1; args[i] != NULL && current_arg_idx < MY_MAX_ARGS; i++) {
                    exec_args[current_arg_idx++] = args[i];
                }
            } else {
                // ls değilse, mevcut argümanları doğrudan kullan
                for (int i = 0; args[i] != NULL && current_arg_idx < MY_MAX_ARGS; i++) {
                    exec_args[current_arg_idx++] = args[i];
                }
            }
            exec_args[current_arg_idx] = NULL; // Diziyi NULL ile sonlandır

            pid_t pid=fork();
            if(pid==0){
                if(in_file){
                    int fd=open(in_file,O_RDONLY);
                    if (fd < 0) { perror("Giris dosyasi acilamadi"); exit(1); }
                    dup2(fd,STDIN_FILENO);
                    close(fd);
                }
                if(out_file){
                    int fd=open(out_file,O_WRONLY|O_CREAT|O_TRUNC,0644);
                    if (fd < 0) { perror("Cikis dosyasi acilamadi"); exit(1); }
                    dup2(fd,STDOUT_FILENO);
                    close(fd);
                }

                execvp(exec_args[0],exec_args);
                perror("Komut calistirilmadi (execvp basarisiz)");
                exit(1);
            } else if (pid < 0) {
                perror("fork basarisiz");
            } else {
                wait(NULL);
            }
            free(input);
            continue;
        }

        // --- BORU HATTI (PIPE) DURUMU ---

        int pipefd[2 * (MAX_CMDS - 1)];

        for(int i=0; i<num_cmds-1; i++) {
            if(pipe(pipefd + i*2) < 0){
                perror("pipe olusturma basarisiz");
                exit(EXIT_FAILURE);
            }
        }

        for(int i=0; i<num_cmds; i++){
            char *args[MY_MAX_ARGS],*in_file,*out_file;
            parse_args(commands[i],args,&in_file,&out_file);
            if(!args[0]) continue;

            // Boru hattı için de dinamik argüman listesi
            char *exec_args[MY_MAX_ARGS + 1]; // +1 çünkü --color=auto
            int current_arg_idx = 0;

            if (strcmp(args[0], "ls") == 0) {
                exec_args[current_arg_idx++] = args[0];
                exec_args[current_arg_idx++] = (char*) "--color=auto";
                for (int j = 1; args[j] != NULL && current_arg_idx < MY_MAX_ARGS; j++) {
                    exec_args[current_arg_idx++] = args[j];
                }
            } else {
                for (int j = 0; args[j] != NULL && current_arg_idx < MY_MAX_ARGS; j++) {
                    exec_args[current_arg_idx++] = args[j];
                }
            }
            exec_args[current_arg_idx] = NULL;

            pid_t pid=fork();
            if(pid==0){
                if(i != 0) {
                    if(dup2(pipefd[(i-1)*2], STDIN_FILENO) < 0) { perror("dup2 STDIN (pipe) basarisiz"); exit(1); }
                }
                if(in_file){
                    int fd=open(in_file,O_RDONLY);
                    if (fd < 0) { perror("Giris dosyasi acilamadi"); exit(1); }
                    if(dup2(fd,STDIN_FILENO) < 0) { perror("dup2 STDIN (file) basarisiz"); exit(1); }
                    close(fd);
                }
                if(i != num_cmds-1) {
                    if(dup2(pipefd[i*2+1], STDOUT_FILENO) < 0) { perror("dup2 STDOUT (pipe) basarisiz"); exit(1); }
                }
                if(out_file){
                    int fd=open(out_file,O_WRONLY|O_CREAT|O_TRUNC,0644);
                    if (fd < 0) { perror("Cikis dosyasi acilamadi"); exit(1); }
                    if(dup2(fd,STDOUT_FILENO) < 0) { perror("dup2 STDOUT (file) basarisiz"); exit(1); }
                    close(fd);
                }

                for(int k=0; k < 2 * (num_cmds-1); k++) {
                    close(pipefd[k]);
                }

                execvp(exec_args[0],exec_args);
                perror("Komut calistirilmadi (execvp basarisiz)");
                exit(1);
            } else if (pid < 0) {
                perror("fork basarisiz");
            }
        }

        for(int i=0; i < 2 * (num_cmds-1); i++) close(pipefd[i]);

        for(int i=0; i<num_cmds; i++) wait(NULL);

        free(input);
    }

    free_path_cmds();
    return 0;
}
