/**
 * Shell
 * CS 241 - Spring 2020
 */
#include "format.h"
#include "shell.h"
#include "vector.h"
#include "sstring.h"
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

typedef struct process {
    char *command;
    pid_t pid;
    bool is_background;
    //bool in_ps;
} process;

//global
static vector* processes = NULL;
static vector* pids = NULL;
static vector* info_structs = NULL;
static char* output = NULL;
static bool redirect_output = false;
static bool redirect_append = false;
static bool redirect_input = false;
static int current_pid = -1;
static int shell_pid = 0;

void print_vector(vector* vector) {
    // empty, size, capcacity, content
    printf("Empty: %d\n", vector_empty(vector));
    printf("Size: %zu\n", vector_size(vector));
    printf("Capacity: %zu\n", vector_capacity(vector));
    
    size_t i = 0;
    for (i = 0; i < vector_size(vector); i++){
        printf("%zu : %s \n", i, (char*)vector_get(vector, i));
    }
    printf("\n");
}

int use_string_command(char* command, bool* is_background);
char* get_command();
int run_command(char** argv, char* command, bool background, FILE* input_file);
vector* exec_from_file(char* file_name); 
FILE* load_history_file(char* file_name); 
void handle_EOF(int signal);
void handle_SIGINT(int signal);
int cd_command(char* path);
int check_built_in(char** command);
vector* format_command(char* command);
int check_operator(char* command, vector* all_commands);
void kill_process(char* pid, char* kill_command);
void stop_process(char* pid, char* stop_command);
void cont_process(char* pid, char* cont_command);
void print_session();
void free_pid();
void free_info_structs();

int shell(int argc, char *argv[]) {
    //set shell pgid
    shell_pid = getpid();
    setpgid(shell_pid, 0);
    //handle signal
    signal(EOF, handle_EOF); 
    signal(SIGINT, handle_SIGINT); 
    processes = vector_create(string_copy_constructor, string_destructor, string_default_constructor);
    pids = vector_create(NULL, NULL, NULL);
    info_structs = vector_create(NULL, NULL, NULL);
    vector* background_pids = vector_create(int_copy_constructor, int_destructor, int_default_constructor);
    //shell_pig -> pids
    process* shell_process = malloc(sizeof(struct process));
    shell_process->command =  "./shell";
    shell_process->pid = shell_pid;
    shell_process->is_background = true;
    vector_push_back(pids, shell_process);
    //check -h -f
    bool arg_h = FALSE;
    
    bool arg_f = FALSE;
    int file_name_index_f = 0;
    int file_name_index_h = 0;
    int i = 0;
    //int num_operations = 1;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0){
            //-f
            printf("there is -f\n");
            arg_f = TRUE;
            if (i == argc-1) {
                print_usage();
                exit(1);
            } 
            file_name_index_f = i+1;
        } else if (strcmp(argv[i], "-h")==0){
            //-h
            arg_h = TRUE;
            printf("there is -h\n");
            file_name_index_h = i+1;
            if (i == argc-1) {
                print_usage();
                exit(1);
            } 
        }
    }
    //TODO: account for -h
    
    FILE* history_file = NULL;
    if(arg_h) {
        history_file = load_history_file(argv[file_name_index_h]); 
        //return return_status_h;
    }
    bool wirte_to_history_file = false;
    if (history_file != NULL){
        wirte_to_history_file = true;
    } else {
        if (arg_h)
        print_history_file_error();
    }
    //    -f : fopen and check for "no such file"
    if(arg_f) {
        vector* f_process = exec_from_file(argv[file_name_index_f]); 
        if (f_process == NULL){
            exit(1);
        } else{
            if (wirte_to_history_file) {
                printf("wiritng....\n");
                size_t j = 0;
                for(j = 0; j < vector_size(f_process); j++){
                    //write to h file
                    fprintf(history_file,"%s\n", vector_get(f_process, j));
                }
                fclose(history_file);
            }
            vector_destroy(f_process);
            free_pid();
            free_info_structs();
            vector_destroy(background_pids);
            exit(0);
        }
        return 0;
    }
    //shell
    
    //int i = 3;
    bool* is_background = malloc(sizeof(bool));
    while (1){

        //get command -> run command
        char* command = get_command();
        *is_background = false;
        //call the function that does everything
        int exec_status = use_string_command(command, is_background);
        if (exec_status==-1) {
            //exit command
            //write to h file
            if (wirte_to_history_file) {
                printf("wiritng....\n");
                size_t j = 0;
                for(j = 0; j < vector_size(processes); j++){
                    //write to h file
                    fprintf(history_file,"%s\n", vector_get(processes, j));
                }
                fclose(history_file);
            }
            vector_destroy(processes);
            free(is_background);
            free_pid();
            free_info_structs();
            vector_destroy(background_pids);
            if (command != NULL) free(command);
            exit(0);
        }
        printf("exit status: %d\n", exec_status);
        //take care of background
        if (*is_background) {
            //wait
            vector_push_back(background_pids, &exec_status);
        }
        //check all background
        size_t index = 0;
        int status;
        printf("num of background p: %zu \n",vector_size(background_pids));
        for (index = 0; index < vector_size(background_pids); index++) {
            int result = waitpid(*(int*)vector_get(background_pids, index), &status, WNOHANG);
            if (result > 0) {
                printf("background finished!\n");
                vector_erase(background_pids, index);
            }
        }
        free(command);
    }
    print_vector(processes);

    return 0;
}
//free all the process struct -> destroy pid vector
void free_pid() {
    size_t i = 0;
    for(i = 0; i < vector_size(pids); i++) {
        process* this = vector_get(pids, i);
        if (this != NULL) {
            if (strcmp(this-> command, "./shell") != 0) {
                free(this->command);
            }
            free(this);
        }
    }
    vector_destroy(pids);
}
// free all var -> all struct -> destory vector
void free_info_structs(){
    size_t i = 0;
    for(i = 0; i < vector_size(info_structs); i++) {
        process_info* this = vector_get(info_structs, i);
        if (this != NULL) {
             //if (this-> time_str != NULL) {
                 free(this->time_str);
             //}
            free(this);
        }
    }
    vector_destroy(info_structs);
}


char* get_command(){
    pid_t shell_pid = getpid();
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    //printf("pid: %d\n", shell_pid);
    //printf("pwd: %s\n", cwd);
    print_prompt(cwd, shell_pid);
    
    char* command = NULL;
    size_t n = 0;
    int get_result = getline(&command, &n, stdin);
    if (get_result == -1) {
        printf();
        exit(0);
    }
    return command;
}

int run_command(char** argv, char* command, bool background, FILE* input_file){
    if (strlen(command) == 0){
            //free(command);
            return 1;
        }
        /////////////pipline/////////////////////
        int link[2];
        //pid_t pid;
        char foo[4096];

        if (pipe(link)==-1){
            return 1;
        }
        //////////////////////////////////
        pid_t pid = fork();
        if (pid < 0) { // fork failure   --------------------------?
            print_fork_failed();
            return 1;
        } else if (pid > 0) {
            //parent
            if (!background) {
                current_pid = pid;
                //setpgid(pid,0);
            } else {
                setpgid(pid,0);
            }
            //setpgid(pid,2);
            ////////////kill//////////////
            process* this_process = malloc(sizeof(struct process));
            char* command_copy = malloc(strlen(command)+1);
            strcpy(command_copy, command);
            this_process->command = command_copy; 
            this_process->pid = pid;
            this_process->is_background = background;
            //this_process->in_ps = false;
            vector_push_back(pids, this_process);
            /////////////////////////
            if (!background) {
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status)) {
                    current_pid = -1;
                    if (redirect_append || redirect_output) {
                        close(link[1]);
                        int nbytes = read(link[0], foo, sizeof(foo));
                        if (!redirect_output && !redirect_append) {
                            foo[nbytes] = '\0';
                            printf("%s", foo);
                        }
                        if (output != NULL) {
                            free(output);
                        }
                        output = malloc(nbytes+1);
                        strcpy(output, foo);
                        output[nbytes] = '\0';               
                    }
                    
                    return WEXITSTATUS(status);
                    //return 0;
                }
                return 1;
            } else {
                //running backgrounf process
                //return pid -> wait in shell()
                 return pid;
            }
            
            return 0;
        } else {
        //child
        //get pid and print message
        pid_t my_pid = getpid();
        print_command_executed(my_pid);

        if (redirect_append || redirect_output) {
            dup2 (link[1], STDOUT_FILENO);
            close(link[0]);
            close(link[1]);
        }
        if(redirect_input) {
            //pip stdin
            dup2(fileno(input_file), STDIN_FILENO);
            fclose(input_file);
        }
        //////////////////////////
        //run exec -> take care of failure
        execvp(argv[0],&argv[0]); 
        print_exec_failed(command);
        //vector_destroy(command_formatted);
        //free(command);
        exit(1);
        }
}

//return: processes vector, NULL on failure
vector* exec_from_file(char* file_name) {
    //vector* processes = vector_create(string_copy_constructor, string_destructor, string_default_constructor);
    
    printf("trying to get the file: %s\n", file_name);
    // fopen (maybe), try other functions
    char *line_buffer = NULL;
    size_t line_buffer_size = 0;
    //int line_count = 0;
    ssize_t line_length;
    FILE *file = fopen(file_name, "r");
    //if file X exist, print_script_file_error()
    if (!file) {
        print_script_file_error();
        return NULL;
    }
    //get line
    line_length = getline(&line_buffer, &line_buffer_size, file);
    
    bool* is_background = malloc(sizeof(bool));
    while (line_length >= 0) {
        *is_background = false;
        // clear '\n'
        char* command = line_buffer;
        if (*(command+strlen(command)-1) == '\n'){
            //printf("ending with new line char");
            *(command+strlen(command)-1) = '\0';
        }

        //print 
        pid_t shell_pid = getpid();
        char cwd[PATH_MAX];
        getcwd(cwd, sizeof(cwd));
        print_prompt(cwd, shell_pid);
        printf("%s\n", line_buffer);

        //call the function that does everything
        int exec_status = use_string_command(line_buffer, is_background);
        
        if (exec_status==-1) {
            //exit command
            //write to h file
            free(is_background);
            return processes;
        }
        printf("exit status: %d\n", exec_status);
        //use_string_command(line_buffer, NULL);

        //next line
        line_length = getline(&line_buffer, &line_buffer_size, file);
    
    }
    //free(line_buffer);
    fclose(file);


    free(is_background);
    return processes;
}


//return the exit status, -1 if exit
int use_string_command(char* command, bool* is_background) {
        int i = 0;
        int* exit_status = malloc(sizeof(int));
        *exit_status = -10;
        redirect_output = false;
        redirect_append = false;
        redirect_input = false;
        char* file_name = NULL;
        FILE* dest_file = NULL;
        FILE* input_file = NULL;

        //clear '\n'
        printf("command: %s\n", command);
        if (*(command+strlen(command)-1) == '\n'){
            //printf("ending with new line char");
            *(command+strlen(command)-1) = '\0';
        }

        vector* all_commands = vector_create(string_copy_constructor, string_destructor, string_default_constructor);
        int num_operations = 1;
        int buil_in_return = check_operator(command, all_commands);
        if (buil_in_return == 2){
            //;
            num_operations = 2;

        } else if (buil_in_return == 3){
            //&&
            num_operations = 2;
        }else if (buil_in_return == 4){
            //||
            num_operations = 2;
        }
        
        if (num_operations == 1) vector_push_back(all_commands, command);
        //free(command);
        // format and run every command
        vector* command_formatted = NULL;
        for (i = 0; i < num_operations; i++) {
            if (buil_in_return == 3 && *exit_status == 1) {
                // &&
                return 0;
            } else if (buil_in_return == 4 && *exit_status == 0){
                // ||
                return 0;
            }
            printf("this command : %s\n", (char*)vector_get(all_commands,i));
            // check exit
            if (strcmp(vector_get(all_commands,i), "exit")==0) {
                free(command);
                //vector_destroy(processes);
                vector_destroy(all_commands);
                free(exit_status);
                return -1;
            }
            
            // check !history
            if (strcmp(vector_get(all_commands,i), "!history")==0) {
                //free(command);
                vector_destroy(all_commands);
                //print
                size_t j = 0;
                for (j = 0; j < vector_size(processes); j++) {
                    print_history_line(j, vector_get(processes, j));
                }
                return 0;
            }
            // check #<n>
            if (*(char*)vector_get(all_commands,i) == '#'){
                //printf("# command!"\n);
                int location = atoi((char*)vector_get(all_commands,i)+1);
                printf("locatin: %d\n", location);
                if (location > (int)vector_size(processes)-1){
                    print_invalid_index();
                    vector_destroy(all_commands);
                    return 0;
                }
                //do not commit this out
                printf("%s\n",vector_get(processes,location));
                //run
                use_string_command(vector_get(processes,location), is_background);
                vector_destroy(all_commands);
                return 0;
            }
            //check !<prefix>
            if (*(char*)vector_get(all_commands,i) == '!'){
                //printf("!prefix command!\n");
                if (vector_size(processes)==0) {
                    // no previous command
                    print_no_history_match();
                    return 0;
                }
                if (strcmp(vector_get(all_commands,i), "!")==0){
                    //last command
                    //do not commit this out
                    printf("%s\n",vector_get(processes,(vector_size(processes)-1)));
                    use_string_command(vector_get(processes,(vector_size(processes)-1)), is_background);
                    return 0;
                }
                
                //find
                size_t k = 0;
                bool found = false;
                for (k = (vector_size(processes)-1); (int)k != -1; k--) {
                    char *result = strstr(vector_get(processes, k), (char*)vector_get(all_commands,i)+1);
                    if (result != NULL && *result == *(char*)vector_get(processes, k)) {
                        found = true;
                        break;
                    }
                }
                //print if not find
                if (!found) {
                    print_no_history_match();
                    return 1;
                }
                //run
                printf("%s\n",vector_get(processes, k));
                use_string_command(vector_get(processes,k), is_background);
                return 0;
            }
            // push command to private history
            if (i == 0){
                vector_push_back(processes, command);
            }
            
            char partial_command[128];
            strcpy(partial_command, vector_get(all_commands,i));
            //format 
            command_formatted = format_command(vector_get(all_commands,i));
            //free(command);
            
            char*argv_command[vector_size(command_formatted)+1];
            size_t i = 0;
            for (i = 0; i < vector_size(command_formatted); i++){
                // vector => argv
                argv_command[i] = vector_get(command_formatted, i);
            } 
            argv_command[i] = NULL;
            //check ps
            if (strcmp(argv_command[0], "ps") == 0) {
                print_session();
                free(exit_status);
                vector_destroy(all_commands);
                return 0;
            }
            //check kill
            if (strcmp(argv_command[0], "kill") == 0) {
                kill_process(argv_command[1], partial_command);
                return 0;
            }
            //check stop
            if (strcmp(argv_command[0], "stop") == 0) {
                stop_process(argv_command[1], partial_command);
                return 0;
            }
            //check cont
            if (strcmp(argv_command[0], "cont") == 0) {
                cont_process(argv_command[1], partial_command);
                return 0;
            }
            //check background
            if (strcmp(argv_command[i-1], "&") == 0){
                *is_background = true;
                printf("found background command\n");
                argv_command[i-1] = NULL;
            }
            //check > output
            i = 0;
            while(argv_command[i] != NULL){
                // find '>'
                if (strcmp(argv_command[i], ">") == 0) {
                    //printf("found > !!!! \n");
                    redirect_output = true;
                    file_name = argv_command[i+1];
                    break;
                }
                i++;
            }
            if (redirect_output) {
                printf("file name: %s\n", file_name);
                argv_command[i] = NULL;
                dest_file = fopen(file_name, "w");
                if (dest_file == NULL) {
                    print_redirection_file_error();
                    return 1;
                }
            } else {
                // check >> append
                i = 0;
                while(argv_command[i] != NULL){
                    // find '>'
                    if (strcmp(argv_command[i], ">>") == 0) {
                        //printf("found > !!!! \n");
                        redirect_append = true;
                        file_name = argv_command[i+1];
                        break;
                    }
                    i++;
                }
                if (redirect_append) {
                    printf("file name: %s\n", file_name);
                    argv_command[i] = NULL;
                    dest_file = fopen(file_name, "a");
                    if (dest_file == NULL) {
                        print_redirection_file_error();
                        return 1;
                    }
                } else {
                    //check < input
                    i = 0;
                    while(argv_command[i] != NULL){
                        // find '>'
                        if (strcmp(argv_command[i], "<") == 0) {
                            printf("found < !!!! \n");
                            redirect_input = true;
                            file_name = argv_command[i+1];
                            break;
                        }
                        i++;
                    }
                    if (redirect_input) {
                        printf("file name: %s\n", file_name);
                        argv_command[i] = NULL;
                        input_file = fopen(file_name, "r");
                        if (input_file == NULL) {
                            print_redirection_file_error();
                            return 1;
                        }
                    }
                }
            }
            //check built in
            int built_in = check_built_in(argv_command);
            if(built_in == -1) {
                *exit_status = run_command(argv_command, partial_command, *is_background, input_file);
                if (*is_background) {
                    break;
                }
            
            } else {
                //it is build in command
                *exit_status = built_in;
            }
            // > file
            //char* example = "example";
            if (redirect_output || redirect_append) {
                //printf("outpus: %lu : %s\n", strlen(output)+1, output);
                fwrite(output, sizeof(char), strlen(output), dest_file);
                fclose(dest_file);
            }
            
        }
        int status = *exit_status;
        if (command_formatted != NULL) {
            vector_destroy(command_formatted);
        }
        free(exit_status);
        vector_destroy(all_commands);
        return status;
}

void kill_process(char* pid, char* kill_command) {
    printf("kill this pid: %s\n", pid);
    if (pid == NULL) {
        print_invalid_command(kill_command);
        return;
    }
    //pid => int
    int pid_int = atoi(pid);
    printf("kill this pid_int: %d\n", pid_int);
    //find pid
    size_t i = 0;
    bool found = false;
    process* this_process = NULL;
    char* command = NULL;
    printf("vector size: %lu\n", vector_size(pids));
    for (i = 0; i < vector_size(pids); i++) {
        this_process = vector_get(pids, i);
        if (this_process->pid == pid_int){
            command = this_process->command;
            printf("found such process\n");
            found = true;
            break;
        }
    }
    if (found) {
        //kill and print
        int result = kill(pid_int, SIGTERM);
        
        if (result == 0) {
            printf("checkcheck\n");
            print_killed_process(pid_int, command);
        }
        printf("checkcheck\n");
        return;
    } else {
        // print -> on such process with this pid
        print_no_process_found(pid_int);
    }
}

void stop_process(char* pid, char* stop_command) {
    printf("stoping this pid: %s\n", pid);
    if (pid == NULL) {
        print_invalid_command(stop_command);
        return;
    }
    int pid_int = atoi(pid);
    //find pid
    size_t i = 0;
    bool found = false;
    process* this_process = NULL;
    char* command = NULL;
    printf("vector size: %lu\n", vector_size(pids));
    for (i = 0; i < vector_size(pids); i++) {
        this_process = vector_get(pids, i);
        if (this_process->pid == pid_int){
            command = this_process->command;
            printf("found such process\n");
            found = true;
            break;
        }
    }
    if (found) {
        //kill and print
        int result = kill(pid_int, SIGSTOP);
        
        if (result == 0) {
            print_stopped_process(pid_int, command);
        }
        return;
    } else {
        // print -> on such process with this pid
        print_no_process_found(pid_int);
    }
}

void cont_process(char* pid, char* cont_command) {
    if (pid == NULL) {
        print_invalid_command(cont_command);
        return;
    }
    int pid_int = atoi(pid);
    //find pid
    size_t i = 0;
    bool found = false;
    process* this_process = NULL;
    char* command = NULL;
    printf("vector size: %lu\n", vector_size(pids));
    for (i = 0; i < vector_size(pids); i++) {
        this_process = vector_get(pids, i);
        if (this_process->pid == pid_int){
            command = this_process->command;
            printf("found such process\n");
            found = true;
            break;
        }
    }
    if (found) {
        //kill and print
        int result = kill(pid_int, SIGCONT);
        
        if (result == 0) {
            print_continued_process(pid_int, command);
        }
        return;
    } else {
        // print -> on such process with this pid
        print_no_process_found(pid_int);
    }
}

void handle_EOF(int signal) {
    printf("I am EOF signal handler\n");
    //free momory
    exit(0);
}

void handle_SIGINT(int signal) {
    printf("I am SIGINT signal handler\n");
    size_t i = 0;
    for (i = 0; i < vector_size(pids); i++) {
        process* this = vector_get(pids, i);
        printf("this pid is: %d\n", this->pid);
        printf("this pgid is: %d\n", getpgid(this->pid));
    }
    //kill running programs
    printf("this pid is: %d\n", vector_size(pids));
    if (current_pid != -1 && vector_size(pids) != 1) {
        //there is a runing foreground process
        kill(current_pid, SIGINT); 
        //killpg(2, SIGINT);
    }
    exit(0);
}



// return 0 if successful, 1 if fail, -1 if not found
int check_built_in(char** command) {
    //check cd
    if (strcmp(command[0], "cd") == 0){
        //get cd
        if (command[1] == NULL || strlen(command[1])==0) {
            // no provided directory
            print_no_directory(NULL);
            return 1;
        } else {
            return cd_command((char*)(command[1]));
        }
        
    }
    //other build ins

    return -1;
}

//return 1 on failure, 0 on success
int cd_command(char* path){
    //printf("working on cd....\n");
    int result = 0;
    if(path[0] != '/') {
        char taget_path[PATH_MAX];
        strcpy(taget_path,path);
        char current_directory[PATH_MAX];
        //append current path
        getcwd(current_directory,sizeof(current_directory));
        strcat(current_directory,"/");
        strcat(current_directory,taget_path);
        //cd
        result = chdir(current_directory);
    } else {
        result = chdir(path);
    }
    if (result == -1){
        print_no_directory(path);
        return 1;
    } 
    return 0;
}



vector* format_command(char* command) {
    //format the command
    sstring *command_sstring = cstr_to_sstring(command);
    vector* command_formatted = sstring_split(command_sstring, ' ');
    sstring_destroy(command_sstring);
    return command_formatted;
}

// return: 1: nothing, 2: ';', 3: "&&", 4: "||"   
// check for operator -> push to vector -> return num
int check_operator(char* command, vector* all_commands) {
    
    sstring *command_sstring = cstr_to_sstring(command);
    
    // check ; -> copy to vector
    sstring_substitute(command_sstring, 0, "; ", ";");
    vector* temp_vector = sstring_split(command_sstring, ';');
    if (vector_size(temp_vector) > 1) {
        sstring_destroy(command_sstring);
        //copy to vector all_commands
        vector_push_back(all_commands, vector_get(temp_vector, 0));
        vector_push_back(all_commands, vector_get(temp_vector, 1));
        vector_destroy(temp_vector);

        return 2;
    }
    vector_destroy(temp_vector);
    // format-> traverse -> find && ||
    vector* command_formatted = format_command(command);
    size_t i = 0;
    bool and = false;
    bool or = false;
    for (i = 0; i < vector_size(command_formatted); i++){
        if (strcmp(vector_get(command_formatted, i), "&&") == 0){
            printf("found &&\n");
            and = true;
            break;
        }
        if (strcmp(vector_get(command_formatted, i), "||") == 0){
            printf("found ||\n");
            or = true;
            break;
        }
    }
    // check &&
    // split -> return
    if (and || or){
        char* empty = "";
        char* space = " ";
        sstring * first_command = cstr_to_sstring(empty);
        sstring * space_command = cstr_to_sstring(space);
        size_t index = i;
        for (i = 0; i < index; i++){
            sstring * temp = cstr_to_sstring(vector_get(command_formatted, i));
            sstring_append(first_command, temp);
            sstring_append(first_command, space_command);
            sstring_destroy(temp);
        }
        char* first_command_string = sstring_to_cstr(first_command);
        first_command_string[strlen(first_command_string)-1] = '\0';
        sstring_destroy(first_command);
        printf("first command: %lu : %s\n", strlen(first_command_string), first_command_string);
        vector_push_back(all_commands, first_command_string);
        free(first_command_string);
        //second string
        sstring * second_command = cstr_to_sstring(empty);
        for (i = index+1; i < vector_size(command_formatted); i++){
            sstring * temp = cstr_to_sstring(vector_get(command_formatted, i));
            sstring_append(second_command, temp);
            sstring_append(second_command, space_command);
            sstring_destroy(temp);
        }
        char* second_command_string = sstring_to_cstr(second_command);
        second_command_string[strlen(second_command_string)-1] = '\0';
        printf("second command: %lu : %s\n", strlen(second_command_string), second_command_string);
        
        sstring_destroy(second_command);
        vector_push_back(all_commands, second_command_string);
        free(second_command_string);

        printf("0 item: %s\n", (char*)vector_get(all_commands, 0));
        printf("1 item: %s\n", (char*)vector_get(all_commands, 1));

        //return 3;
    }
    if (and){
        vector_destroy(command_formatted);
        return 3;
    }
    //check ||
    if (or){
        vector_destroy(command_formatted);
        return 4;
    }
    vector_destroy(command_formatted);
    sstring_destroy(command_sstring);
    return 1;
}

//return NULL on failure
FILE* load_history_file(char* file_name){
    printf("trying to get file: %s\n", file_name);
    
    FILE * h_file = fopen(file_name, "a");
    
    return h_file;
}


void print_session() {
    //printf("ps is called\n");
    print_process_info_header();
    size_t i = 0;
    int pid = 0;
    printf("pid vector size: %lu\n", vector_size(pids));
    for (i = 0; i < vector_size(pids); i++) {
        process* this_process = vector_get(pids, i);
        if (this_process->is_background == false) {
            continue;
        }

        pid = this_process->pid;

        process_info* process_info = malloc(sizeof(struct process_info));
        //read from file
        char filename[256];
        sprintf(filename, "/proc/%d/stat", pid);
        FILE *stat_file = fopen(filename, "r");
        if (stat_file == NULL) {
            continue;
        }
        char *line_buffer = NULL;
        size_t line_buffer_size = 0;
        getline(&line_buffer, &line_buffer_size, stat_file);

        //format the commands
        vector* infos = format_command(line_buffer);
        free(line_buffer);
        process_info->pid = atoi(vector_get(infos, 0));
        process_info->nthreads = atoi(vector_get(infos, 19));
        process_info->vsize = atoi(vector_get(infos, 22))/1000;
        process_info->state = *(char*)vector_get(infos, 2);
        fclose(stat_file);
        process_info->command = this_process->command;

        ///////////start/////////////
        char* end_one;
        long time_after_boot = strtol((char*)vector_get(infos, 21), &end_one, 10);
        long sec_after_boot = time_after_boot / sysconf(_SC_CLK_TCK);
        printf("sec_after_boot: %lu\n", sec_after_boot);
        process_info->start_str = NULL;
        //get system boot time
        FILE* proc_stat = fopen("/proc/stat", "r");
        if (proc_stat == NULL) {
            exit(1);
        }

        line_buffer = NULL;
        line_buffer_size = 0;
        size_t line_size = 1;
        while(line_size != 0){
            line_size = getline(&line_buffer, &line_buffer_size, proc_stat);
            if (strncmp (line_buffer, "btime", 5) == 0) {
                printf("btime: %s\n", line_buffer);
                line_buffer += 6;
                printf("btime: %s\n", line_buffer);
                break;
            }
            free(line_buffer);
            line_buffer = NULL;
            line_buffer_size = 0;
        }
        fclose(proc_stat);
        
        long sec_boot_time = strtol(line_buffer, &end_one, 10);
        free(line_buffer-6);
        long sec_start_time = sec_boot_time + sec_after_boot;
        struct tm * start_time_struct = localtime(&sec_start_time);
        char buffer_one[128];
        size_t buffer_len_one = 128;
        int size_one = time_struct_to_string(buffer_one, buffer_len_one,
                             start_time_struct);
        char* start_string = malloc(size_one*sizeof(char)+1);
        strcpy(start_string, buffer_one);
        printf("buffer: %s\n", buffer_one);
        start_string[size_one] = '\0';
        process_info->start_str = start_string;
        //free(line_buffer);
        //printf("boot_time: %s\n", boot_time);
        //////////time//////////////////
        char* end;
        long utime = strtol((char*)vector_get(infos, 13), &end, 10);

        long stime = strtol((char*)vector_get(infos, 14), &end, 10);

        long total_time = utime + stime;
        long time_sec = total_time / sysconf(_SC_CLK_TCK);
        int minute = time_sec / 60;
        int second = time_sec % 60;

        char buffer[128];
        size_t buffer_len = 128;
        int size = execution_time_to_string(buffer, buffer_len, minute, second);
        char* time_string = malloc(size*sizeof(char)+1);
        strcpy(time_string, buffer);
        printf("buffer: %s\n", buffer);
        time_string[size] = '\0';
        printf("time_string: %s\n", time_string);
        process_info->time_str = time_string;
        // Some useful debuggin messages
        //printf("check-2\n");
        //printf("%d\n", process_info->pid);
        //printf("%ld\n", process_info->nthreads);
        //printf("%c\n", process_info->state);
        print_process_info(process_info);
        vector_destroy(infos);

    }

    vector_clear(info_structs);
}