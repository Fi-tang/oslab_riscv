/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define SHELL_BEGIN 10

int command_spaceNum(char command_buffer[1000]){
    int spaceNum = 0;
    int i = 0;
    int command_buffer_start_index = 0;
    while(command_buffer[i] == ' '){
        i++;
    }
    command_buffer_start_index = i;
    for(  ; command_buffer[i] != '\0'; i++){
        if(command_buffer[i] == ' '){
            spaceNum++;
        }
    }
    // step-1: get spaceNum
    printf("spaceNum = %d\n", spaceNum);
    return spaceNum;
}

void split_command_to_multiple_line(char command_buffer[1000], int spaceNum, char command_split[spaceNum + 1][1000]){
    int command_buffer_start_index = 0;
    while(command_buffer[command_buffer_start_index] == ' '){
        command_buffer_start_index++;
    }
    int k = command_buffer_start_index;
    for(int m = 0; m < spaceNum + 1; m++){
        int split_count = 0;
        while(1){
            if(command_buffer[k] != ' ' && command_buffer[k] != '\0'){
                command_split[m][split_count++] = command_buffer[k];
                printf("%d [%c] -> ", k, command_buffer[k]);
                k++;
            }
            else if(command_buffer[k] == ' '){
                k++;
                break;
            }
            else{
                break;
            }
        }
        command_split[m][split_count] = '\0';
        printf("\n");
    }
    printf("[After parsing]: \n");
    for(int m = 0; m < spaceNum + 1; m++){  
        printf("%d %s\n",m, command_split[m]);
    }
}


void handle_single_command(char command_buffer[1000]){
    if(strcmp(command_buffer, "ps") == 0){
        sys_ps();
    }
    else if(strcmp(command_buffer, "clear") == 0){
        sys_clear();
    }
    else{
        printf("Error: Unknown Command %s\n", command_buffer);
    }
}

void handle_multiple_command(char command_buffer[1000], int spaceNum){
    char command_split[spaceNum + 1][1000];
    split_command_to_multiple_line(command_buffer, spaceNum, command_split);
    //=============================== split ================================================
    if(strcmp(command_split[0], "exec") == 0){
        char taskname[100];
        strcpy(taskname, command_split[1]);
        printf("starting task %s\n", taskname);
        pid_t task_start_id = sys_exec(taskname, spaceNum + 1, command_split);
        printf("Info: execute %s sucessfully, pid = %d ...\n", taskname, task_start_id);
    }
    else{
        printf("task-1: unhandled command!\n");
    }
}

int main(void)
{
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    printf("> root@UCAS_OS: ");

    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        int input_character;
        char command_buffer[1000];

        int command_real_index = 0;
        while(1){
            input_character = sys_getchar();
            if(input_character == -1){
                ;
            }
            else if(input_character == 8 || input_character == 127){
                printf("%c", (char)input_character);
                if(command_real_index > 0){
                    command_real_index -= 1;
                }
            }
            else if(input_character == 13){     // enter '\n'
               printf("\n");
               break;
            }
            else{
                command_buffer[command_real_index++] = (char)input_character;
                printf("%c", (char)input_character);
            }
        }
        command_buffer[command_real_index] = '\0';
        printf("COMMAND_LINE: %s\n", command_buffer);
        //=============================================== Input finished ====================================================
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)

        int spaceNum = 0;
        spaceNum = command_spaceNum(command_buffer);
        if(spaceNum == 0){
            handle_single_command(command_buffer);
        }
        else{
            handle_multiple_command(command_buffer, spaceNum);
        }
        //============================================= Split finished =======================================================
        // TODO [P3-task1]: ps, exec, kill, clear
        
        printf("> root@UCAS_OS: ");  
        /************************************************************/
        /* Do not touch this comment. Reserved for future projects. */
        /************************************************************/    
    }

    return 0;
}