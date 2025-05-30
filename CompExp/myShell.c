#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64

// 内部命令处理函数
void help() {
    printf("简易Shell帮助:\n");
    printf("内部命令: help, exit\n");
    printf("外部命令: 任何系统命令如ls, cp等\n");
    printf("管道支持: 使用 || 分隔命令，如 ls || grep c\n");
    printf("重定向支持:\n");
    printf("  >  将输出重定向到文件（覆盖）\n");
    printf("  >> 将输出重定向到文件（追加）\n");
    printf("  <  将文件内容作为输入\n");
}

// 执行单个命令（带重定向处理）
void execute_command(char *args[]) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // 子进程执行命令
        // 检查重定向符号
        int i = 0;
        int in_fd = -1, out_fd = -1;
        int append_mode = 0;
        char *input_file = NULL, *output_file = NULL;
        
        while (args[i] != NULL) {
            if (strcmp(args[i], "<") == 0) {
                // 输入重定向
                if (args[i+1] == NULL) {
                    printf("错误: 缺少输入文件\n");
                    exit(EXIT_FAILURE);
                }
                input_file = args[i+1];
                args[i] = NULL; // 截断参数列表
            } else if (strcmp(args[i], ">") == 0) {
                // 输出重定向（覆盖）
                if (args[i+1] == NULL) {
                    printf("错误: 缺少输出文件\n");
                    exit(EXIT_FAILURE);
                }
                output_file = args[i+1];
                append_mode = 0;
                args[i] = NULL;
            } else if (strcmp(args[i], ">>") == 0) {
                // 输出重定向（追加）
                if (args[i+1] == NULL) {
                    printf("错误: 缺少输出文件\n");
                    exit(EXIT_FAILURE);
                }
                output_file = args[i+1];
                append_mode = 1;
                args[i] = NULL;
            }
            i++;
        }
        
        // 处理输入重定向
        if (input_file != NULL) {
            in_fd = open(input_file, O_RDONLY);
            if (in_fd == -1) {
                perror("打开输入文件失败");
                exit(EXIT_FAILURE);
            }
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        
        // 处理输出重定向
        if (output_file != NULL) {
            int flags = O_WRONLY | O_CREAT;
            if (append_mode) {
                flags |= O_APPEND;
            } else {
                flags |= O_TRUNC;
            }
            out_fd = open(output_file, flags, 0644);
            if (out_fd == -1) {
                perror("打开输出文件失败");
                exit(EXIT_FAILURE);
            }
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        
        // 执行命令
        execvp(args[0], args);
        // 如果execvp返回，说明执行失败
        printf("命令执行失败: %s\n", args[0]);
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // fork失败
        printf("创建子进程失败\n");
    } else {
        // 父进程等待子进程完成
        wait(NULL);
    }
}

// 处理管道命令（带重定向处理）
void execute_pipeline(char *cmd1[], char *cmd2[]) {
    int pipefd[2];
    pid_t pid1, pid2;
    
    if (pipe(pipefd) == -1) {
        perror("管道创建失败");
        return;
    }
    
    pid1 = fork();
    if (pid1 == 0) {
        // 第一个命令的子进程
        close(pipefd[0]); // 关闭读端
        dup2(pipefd[1], STDOUT_FILENO); // 将标准输出重定向到管道写端
        close(pipefd[1]);
        
        // 处理第一个命令可能的输入重定向
        int i = 0;
        int in_fd = -1;
        char *input_file = NULL;
        
        while (cmd1[i] != NULL) {
            if (strcmp(cmd1[i], "<") == 0) {
                if (cmd1[i+1] == NULL) {
                    printf("错误: 缺少输入文件\n");
                    exit(EXIT_FAILURE);
                }
                input_file = cmd1[i+1];
                cmd1[i] = NULL;
            }
            i++;
        }
        
        if (input_file != NULL) {
            in_fd = open(input_file, O_RDONLY);
            if (in_fd == -1) {
                perror("打开输入文件失败");
                exit(EXIT_FAILURE);
            }
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        
        execvp(cmd1[0], cmd1);
        perror("第一个命令执行失败");
        exit(EXIT_FAILURE);
    }
    
    pid2 = fork();
    if (pid2 == 0) {
        // 第二个命令的子进程
        close(pipefd[1]); // 关闭写端
        dup2(pipefd[0], STDIN_FILENO); // 将标准输入重定向到管道读端
        close(pipefd[0]);
        
        // 处理第二个命令可能的输出重定向
        int i = 0;
        int out_fd = -1;
        int append_mode = 0;
        char *output_file = NULL;
        
        while (cmd2[i] != NULL) {
            if (strcmp(cmd2[i], ">") == 0) {
                if (cmd2[i+1] == NULL) {
                    printf("错误: 缺少输出文件\n");
                    exit(EXIT_FAILURE);
                }
                output_file = cmd2[i+1];
                append_mode = 0;
                cmd2[i] = NULL;
            } else if (strcmp(cmd2[i], ">>") == 0) {
                if (cmd2[i+1] == NULL) {
                    printf("错误: 缺少输出文件\n");
                    exit(EXIT_FAILURE);
                }
                output_file = cmd2[i+1];
                append_mode = 1;
                cmd2[i] = NULL;
            }
            i++;
        }
        
        if (output_file != NULL) {
            int flags = O_WRONLY | O_CREAT;
            if (append_mode) {
                flags |= O_APPEND;
            } else {
                flags |= O_TRUNC;
            }
            out_fd = open(output_file, flags, 0644);
            if (out_fd == -1) {
                perror("打开输出文件失败");
                exit(EXIT_FAILURE);
            }
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        
        execvp(cmd2[0], cmd2);
        perror("第二个命令执行失败");
        exit(EXIT_FAILURE);
    }
    
    // 父进程关闭管道两端并等待子进程
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

// 解析命令
void parse_and_execute(char *input) {
    char *token;
    char *args[MAX_ARGS];
    int arg_count = 0;
    
    // 检查是否有管道符 ||
    char *pipe_pos = strstr(input, "||");
    if (pipe_pos != NULL) {
        // 分割管道两边的命令
        *pipe_pos = '\0';
        char *cmd1 = input;
        char *cmd2 = pipe_pos + 2;
        
        // 解析第一个命令
        char *args1[MAX_ARGS];
        int arg_count1 = 0;
        token = strtok(cmd1, " \t\n");
        while (token != NULL && arg_count1 < MAX_ARGS - 1) {
            args1[arg_count1++] = token;
            token = strtok(NULL, " \t\n");
        }
        args1[arg_count1] = NULL;
        
        // 解析第二个命令
        char *args2[MAX_ARGS];
        int arg_count2 = 0;
        token = strtok(cmd2, " \t\n");
        while (token != NULL && arg_count2 < MAX_ARGS - 1) {
            args2[arg_count2++] = token;
            token = strtok(NULL, " \t\n");
        }
        args2[arg_count2] = NULL;
        
        // 执行管道命令
        execute_pipeline(args1, args2);
        return;
    }
    
    // 没有管道符，解析普通命令
    token = strtok(input, " \t\n");
    while (token != NULL && arg_count < MAX_ARGS - 1) {
        args[arg_count++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[arg_count] = NULL;
    
    if (arg_count == 0) {
        return; // 空命令
    }
    
    // 处理内部命令
    if (strcmp(args[0], "help") == 0) {
        help();
    } else if (strcmp(args[0], "exit") == 0) {
        exit(0);
    } else {
        // 外部命令
        execute_command(args);
    }
}

int main() {
    char input[MAX_CMD_LEN];
    
    while (1) {
        printf("myshell> ");
        fflush(stdout);
        
        if (fgets(input, MAX_CMD_LEN, stdin) == NULL) {
            printf("\n");
            break; // 处理Ctrl+D
        }
        
        // 去除换行符
        input[strcspn(input, "\n")] = '\0';
        
        // 解析并执行命令
        parse_and_execute(input);
    }
    
    return 0;
}
