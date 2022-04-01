#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<ctype.h>
#include<fcntl.h>
#include<sys/wait.h>
#include<sys/stat.h>

#define CMD_BUF_SIZE 512
#define TOK_BUF_SIZE 128

// linked list node struct
struct Alias {
    char* alias_name;
    char* alias_value;
    struct Alias* next;
};

struct Alias* head = NULL; 

// frees linked list
void free_list() {
    struct Alias* tmp;
    while (head != NULL) {
        tmp = head;
        head = head->next;
        free(tmp->alias_name);
        free(tmp->alias_value);
        free(tmp);
    }
}

// prints aliases
void print_aliases() {
    struct Alias* ptr = head;

    while (ptr != NULL) {
        char* name = ptr->alias_name;
        char* value = ptr->alias_value;
        write(1, name, strlen(name));
        write(1, value, strlen(value));
        write(1, "\n", 1);
        ptr = ptr->next;
    }
}

// prints an alias value
void print_alias(char* target) {
    struct Alias* ptr = head;

    while (ptr != NULL) {
        char* name = ptr->alias_name;
        char* value = ptr->alias_value;
        if (strcmp(target, name) == 0) {
            write(1, name, strlen(name));
            write(1, value, strlen(value));
            write(1, "\n", 1);
        }
        ptr = ptr->next;
    }
}

// functions declaration 
int mysh_alias(char** tokens);
int mysh_unalias(char** tokens);
char** parse_cmd(char* cmd);
int tok_len(char** tokens);
int is_redirect(char* cmd);

// list of builtin names
char *builtin_list[] = { "alias", "unalias" };

// size of builtins list
int builtin_size() {
    return sizeof(builtin_list) / sizeof(char*);
}

// list of builtin functions
int (*func[]) (char**) = {
    &mysh_alias,
    &mysh_unalias
};   

// helper to locate the output file index
int find_output(char** tokens, char* cmd) {
    int location = -1;
    
    int str_sum = 0;
    int str_length = strlen(cmd);
    for (int i = 0; i < str_length; i++) {
        if (cmd[i] == '>') break;   
        if (cmd[i] == ' ' || cmd[i] == '\t') continue;
        str_sum++; 
    }

    int tok_sum = 0;
    int tok_length = tok_len(tokens);
    for (int i = 0; i < tok_length; i++) {
        int len = strlen(tokens[i]);
        if (tok_sum + len <= str_sum) {
            tok_sum += len;
            location = i;
        }
    }

    return location+1;
}

// helper to count the number of redirect symbols
int count_redirect(char* cmd) {
    int count = 0;
    int str_length = strlen(cmd);
    for (int i = 0; i < str_length; i++) {
        if (cmd[i] == '>') count++;
    }
    
    return count;
}

// helper to get the length of tokens
int tok_len(char** tokens) {
    int ctr = 0;
    int l = 0;
    char* token = tokens[ctr];
    while (token != NULL) {
        l++;
        ctr++;
        token = tokens[ctr];
    }
    return l++;
}

// helper to check if line is a redirection command
int is_redirect(char* cmd) {
    int str_length = strlen(cmd);
    for (int i = 0; i < str_length; i++) {
        if (cmd[i] == '>') return i; // index of redirect symbol
    }

    return -1; // redirect symbol not found
}

// helper to trim leading/trailing whitespace
char *trim(char *str) {
  char *end;
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)
    return str;
  
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;
  
  end[1] = '\0';

  return str;
}

// helper to check for an empty command
int empty_cmd(char* cmd) {
    while (isspace(*cmd) && cmd++);
    return !*cmd;
}

// helper that returns the size of the replacement value for alias
int get_value_size(char** tokens) {
    int size = 0;
    int length = tok_len(tokens);
    for (int i = 2; i < length; i++) {
        size = size + 1 + strlen(tokens[i]);
    }

    return size;
}

int mysh_alias(char** tokens) {
    int blacklist_size = 3;
    char* blacklist[] = { "alias", "unalias", "exit" };
    int length = tok_len(tokens);
    
    if (length == 1) { // alias
        print_aliases();
    } else if (length == 2) { // alias <name>
        print_alias(tokens[1]);
    } else { // alias <name> <value>
        for (int i = 0; i < blacklist_size; i++) {
            if (strcmp(tokens[1], blacklist[i]) == 0) {
                write(STDERR_FILENO, "alias: Too dangerous to alias that.\n", 36);
                return 1;
            }
        }
         
        int value_size = get_value_size(tokens);
        char value[value_size];
        strcpy(value, "");
        for (int i = 2; i < length; i++) {
            strcat(value, " ");
            strcat(value, tokens[i]);
        }

        struct Alias* prev = NULL;
        for (struct Alias* curr = head; curr != NULL; curr = curr->next) { // find same alias_name
            if (strcmp(tokens[1], curr->alias_name) == 0) {
                struct Alias* tmp = NULL;
                if (curr == head) {
                    tmp = head;
                    head = head->next;
                } else {
                    tmp = curr;
                    prev->next = curr->next;
                }
                free(tmp->alias_name);
                free(tmp->alias_value);
                free(tmp->next);
                free(tmp);            
                break;
            }
            prev = curr;
        }

        struct Alias* new = (struct Alias*) malloc(sizeof(struct Alias));
        
        new->alias_name = strdup(tokens[1]); 
        new->alias_value = strdup(value);

        if (head == NULL) {
            head = new;
            head->next = NULL;
        } else {
            new->next = head; 
            head = new;
        }
    }

    return 0;
}

int mysh_unalias(char** tokens) {
    int length = tok_len(tokens);

    if (length == 1 || length > 2) {
        char unalias_err_msg[] = "unalias: Incorrect number of arguments.\n";
        write(STDERR_FILENO, unalias_err_msg, strlen(unalias_err_msg));
    } else {
        char* target = tokens[1];
        struct Alias* curr = head;
        struct Alias* prev = NULL;

        if (head == NULL) return 1;
        
        while (strcmp(target, curr->alias_name) != 0) {
            if (curr->next == NULL) return 1;
            else {
                prev = curr;
                curr = curr->next;
            }
        }

        struct Alias* tmp = NULL;
        if (curr == head) {
            tmp = head;
            head = head->next;
        } else {
            tmp = curr;
            prev->next = curr->next;
        }
        free(tmp->alias_name);
        free(tmp->alias_value);
        free(tmp->next);
        free(tmp);
    }

    return 0;
}

int execute_cmd(char** tokens, char* cmd, FILE* fp) {
    pid_t pid;
    int status;
    int fd;
        
    pid = fork(); // create new child process

    if (pid < 0) { // error forking
        write(STDERR_FILENO, "fork() failed.\n", 15);
    } else if (pid == 0) { // child process
        int redirect_idx = is_redirect(cmd);
        int tok_length = tok_len(tokens);
        int cmd_length = strlen(cmd);
        int count = count_redirect(cmd);

        if (redirect_idx >= 0) {
            if (redirect_idx == 0 || redirect_idx == cmd_length-1 || count > 1) { // began with a >, ends with a >, multiple >, or too many files
                write(STDERR_FILENO, "Redirection misformatted.\n", 26); 
                for (int i = 0; i < tok_length; i++) free(tokens[i]);
                free(tokens);
                free(cmd);
                fclose(fp);
                _exit(1); 
            } else if (redirect_idx > 0) { // command has redirection w/ no errors
                int output_idx = find_output(tokens, cmd);
                if (output_idx != tok_length-1) {
                    write(STDERR_FILENO, "Redirection misformatted.\n", 26); 
                    for (int i = 0; i < tok_length; i++) free(tokens[i]);
                    free(tokens);
                    free(cmd);
                    fclose(fp);
                    _exit(1);
                } 
                char* out = tokens[output_idx];
                tokens[output_idx] = NULL; // trim tokens so only command is left (no '>')
                
                fd = creat(out, 0644);
                if (fd == -1) { // output file error
                    write(STDERR_FILENO, "Cannot write to file ", 21);
                    write(STDERR_FILENO, out, strlen(out));
                    write(STDERR_FILENO, ".\n", 2);
                    
                    for (int i = 0; i < tok_length; i++) free(tokens[i]);
                    free(tokens);
                    free(cmd);
                    fclose(fp);
                    _exit(1);
                }

                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }

        if (execv(tokens[0], tokens) == -1) { // failed to execute
            write(STDERR_FILENO, tokens[0], strlen(tokens[0]));
            write(STDERR_FILENO, ": Command not found.\n", 21);
        }

        for (int i = 0; i < tok_length; i++) free(tokens[i]);
        free(tokens);      
        free(cmd);
        fclose(fp);  
        _exit(0);
    } else { // parent process (wait for child to finish)
        do {
            waitpid(pid, &status, WUNTRACED);
        } while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int launch(char** tokens, char* cmd, FILE* fp) {
    char* target = tokens[0];
    int size = builtin_size();
    
    // compare with builtins
    for (int i = 0; i < size; i++) {
        if (strcmp(target, builtin_list[i]) == 0) { // if cmd is a builtin, exec builtin
            return (*func[i]) (tokens);
        }
    }

    // compare with aliases
    for (struct Alias* curr = head; curr != NULL; curr = curr->next) {
        if (strcmp(target, curr->alias_name) == 0) {
            char* true_cmd = strdup(curr->alias_value);
            char* tmp1 = true_cmd;
            true_cmd = trim(true_cmd);
            char* tmp2 = tokens[0];
            tokens[0] = strdup(true_cmd);
            free(tmp1);
            free(tmp2);
            int alias_status = execute_cmd(tokens, cmd, fp);
            return alias_status;
        }
    }

    return execute_cmd(tokens, cmd, fp); // else, exec regular command
}

char** parse_cmd(char* cmd) {
    char** tokens = malloc(sizeof(char *) * TOK_BUF_SIZE);
    char* token;
    char* delim;
    char* dup_cmd = strdup(cmd);
    char* redirect_delimiter = " '\n''\t'>";
    char* whitespace_delimiter = " :'\n''\t'";
    int redirect_idx = is_redirect(cmd);
    int ctr = 0;

    if (redirect_idx == -1) delim = whitespace_delimiter;
    if (redirect_idx > -1) delim = redirect_delimiter;
    token = strtok(dup_cmd, delim);
    while (token != NULL) {
        token = trim(token);
        tokens[ctr] = strdup(token);
        token = strtok(NULL, delim);
        ctr++;
    }
    tokens[ctr] = token;
    free(dup_cmd);

    return tokens;
}

char* read_cmd(FILE* fp) {
    char buffer[CMD_BUF_SIZE];
    char* cmd_ptr = NULL;
    char cmd_length = 0;

    // read from stdin/file
    while (fgets(buffer, CMD_BUF_SIZE, fp)) {
        buffer[strcspn(buffer, "\n")] = 0;
        int buf_length = strlen(buffer);

        if (!cmd_ptr) {
            cmd_ptr = malloc(buf_length+1); // the first command
        } else {
            free(cmd_ptr);
            cmd_ptr = NULL;
            cmd_ptr = malloc(buf_length+1);           
        }

        strcpy(cmd_ptr+cmd_length, buffer);
        cmd_length += buf_length;
        return cmd_ptr;
    }

    return cmd_ptr;
}

int main(int argc, char* argv[]) {
    int mode = 0; // 0 for interactive, 1 for batch
    char* cmd;
    char** args;
    FILE* fp;

    if (argc > 2) { // incorrect number of CLI args
        char args_err_msg[] = "Usage: mysh [batch-file]\n";
        write(STDERR_FILENO, args_err_msg, strlen(args_err_msg));
        exit(1);
    }

    if (argc == 2) { // batch mode we need to assign the file to read from
        mode = 1;
        fp = fopen(argv[1], "r");
        if (fp == NULL) {
            write(STDERR_FILENO, "Error: Cannot open file ", 24);
            write(STDERR_FILENO, argv[1], strlen(argv[1]));
            write(STDERR_FILENO, ".\n", 2);
            exit(1);
        }
    }

    do { // main loop
        if (!mode) write(1, "mysh> ", 6);
                
        if (mode) { // batch mode
            cmd = read_cmd(fp);
        } else { // interactive mode
            cmd = read_cmd(stdin);
        }
        
        if (!cmd) { // bad command
            break;
        }

        if (strcmp(cmd, "\n") == 0 || empty_cmd(cmd)) { // no command i.e. whitespace
            if (mode) {
                write(1, cmd, strlen(cmd)); // batch mode print whitespaces
                write(1, "\n", 1);
            }
            free(cmd);
            continue;
        }
 
        if (mode) {
            write(1, cmd, strlen(cmd)); // echo the command
            write(1, "\n", 1);
        }

        if (strcmp(cmd, "exit") == 0) { // exit shell
            break;
        }
      
        args = parse_cmd(cmd);
        launch(args, cmd, fp);
        
        free(cmd);
        int length = tok_len(args);
        for (int i = 0; i < length; i++) free(args[i]);
        free(args);
    } while (1);

    if (mode) fclose(fp);
    free(cmd);
    free_list();

    exit(0);
}

