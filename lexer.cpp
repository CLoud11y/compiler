#include <assert.h>
#include <stdio.h>
#include <string.h>

// 输出缓冲区大小
#define MAXSIZE 1000
// 关键字表
#define KEYNUM 6
const char* kwtab[KEYNUM] = {"while", "if", "else", "int", "return", "void"};

// 判断是否为字母
int isLetter(char c) {
    if (((c <= 'z') && (c >= 'a')) || ((c <= 'Z') && (c >= 'A')))
        return 1;
    else
        return 0;
}

// 判断是否为数字
int isDigit(char c) {
    if (c >= '0' && c <= '9')
        return 1;
    else
        return 0;
}

// 判断是否为关键字 是则返回种类码 否则返回0
int isKey(char* arr) {
    for (int i = 0; i < KEYNUM; i++) {
        if (0 == strcmp(arr, kwtab[i]))
            return i + 1;
    }
    return 0;
}

// 输出结果
void write(FILE* fpout, int line_num, int syn, char* arr, const char* des) {
    fprintf(fpout, "%d\t%d\t%s\t%s\n", line_num, syn, arr, des);
}

// 词法分析
void analyse(FILE* fpin, FILE* fpout) {
    char arr[MAXSIZE];  // 存放一个单词
    int idx = 0;
    char ch;  // 当前读入的字符
    int line_num = 1;
    while ((ch = fgetc(fpin)) != EOF) {
        // 碰到空格则跳过 换行符加行号
        if (ch == ' ' || ch == '\t' || ch == '\n') {
            if (ch == '\n')
                line_num++;
            continue;
        }
        // 处理字符串
        if (isLetter(ch)) {
            idx = 0;
            while (isLetter(ch)) {
                arr[idx++] = ch;
                ch = fgetc(fpin);
            }
            // 文件指针回退
            fseek(fpin, -1L, SEEK_CUR);
            arr[idx] = '\0';
            int syn = isKey(arr);  // 判断关键字
            if (syn == 0)          // 不是关键字 是普通标识符
                write(fpout, line_num, 10, arr, "ID");
            else
                write(fpout, line_num, syn, arr, arr);
            // 处理数字
        } else if (isDigit(ch)) {
            int flag = 1;  // 标识数字是否合法
            idx = 0;
            while (isDigit(ch) | isLetter(ch)) {
                if (isLetter(ch))
                    flag = 0;
                arr[idx++] = ch;
                ch = fgetc(fpin);
            }
            // 文件指针回退
            fseek(fpin, -1L, SEEK_CUR);
            arr[idx] = '\0';
            if (flag == 1) {  // 数字合法
                write(fpout, line_num, 11, arr, "NUM");
            } else {
                write(fpout, line_num, -1, arr, "error");
            }
            // 处理专用符号
        } else {
            switch (ch) {
                case '+':
                    write(fpout, line_num, 12, (char*)"+", "+");
                    break;
                case '-':
                    write(fpout, line_num, 13, (char*)"-", "-");
                    break;
                case '*':
                    ch = fgetc(fpin);
                    if (ch == '/')  // 注释的结束 没有与注释开始匹配 错误
                        write(fpout, line_num, -1, (char*)"*/", "ERROR");
                    else {
                        fseek(fpin, -1L, SEEK_CUR);
                        write(fpout, line_num, 14, (char*)"*", "*");
                    }
                    break;
                case '/':
                    ch = fgetc(fpin);
                    if (ch == '*') {  // 注释的开始
                        // 向后找注释结束符*/
                        idx = 0;
                        arr[idx++] = '/';
                        arr[idx++] = '*';
                        char pre, cur = ch;
                        int flag = 0;             // 是否找到注释结束符
                        int cur_line = line_num;  // 注释开始时的行数
                        do {
                            pre = cur;
                            cur = fgetc(fpin);
                            if (cur == EOF)
                                break;
                            if (cur == '\n') {
                                line_num++;
                                cur = ' ';  // 处理掉换行符
                            }
                            arr[idx++] = cur;
                            // 注释长度不能大于缓冲区长度 否则arr溢出
                            assert(idx < MAXSIZE);
                            if (pre == '*' && cur == '/') {
                                flag = 1;
                                break;
                            }
                        } while (1);
                        arr[idx] = '\0';
                        if (flag == 1) {
                            write(fpout, cur_line, 15, arr, "ANNOTATION");
                        } else {
                            line_num = cur_line;  // 未找到注释结束符 回退
                            // 回退到/*之后
                            fseek(fpin, (long)-idx + 2, SEEK_CUR);
                            write(fpout, line_num, -1, (char*)"/*", "ERROR");
                        }
                    } else {
                        fseek(fpin, -1L, SEEK_CUR);
                        write(fpout, line_num, 16, (char*)"/", "/");
                    }
                    break;
                case '<':
                    ch = fgetc(fpin);
                    if (ch == '=') {
                        write(fpout, line_num, 17, (char*)"<=", "<=");
                    } else {
                        fseek(fpin, -1L, SEEK_CUR);
                        write(fpout, line_num, 18, (char*)"<", "<");
                    }
                    break;
                case '>':
                    ch = fgetc(fpin);
                    if (ch == '=') {
                        write(fpout, line_num, 19, (char*)">=", ">=");
                    } else {
                        fseek(fpin, -1L, SEEK_CUR);
                        write(fpout, line_num, 20, (char*)">", ">");
                    }
                    break;
                case '=':
                    ch = fgetc(fpin);
                    if (ch == '=') {
                        write(fpout, line_num, 21, (char*)"==", "==");
                    } else {
                        fseek(fpin, -1L, SEEK_CUR);
                        write(fpout, line_num, 22, (char*)"=", "=");
                    }
                    break;
                case '!':
                    ch = fgetc(fpin);
                    if (ch == '=') {
                        write(fpout, line_num, 23, (char*)"!=", "!=");
                    } else {
                        fseek(fpin, -1L, SEEK_CUR);
                        write(fpout, line_num, -1, (char*)"!", "ERROR");
                    }
                    break;
                case ',':
                    write(fpout, line_num, 24, (char*)",", ",");
                    break;
                case ';':
                    write(fpout, line_num, 25, (char*)";", ";");
                    break;
                case '(':
                    write(fpout, line_num, 26, (char*)"(", "(");
                    break;
                case ')':
                    write(fpout, line_num, 27, (char*)")", ")");
                    break;
                case '[':
                    write(fpout, line_num, 28, (char*)"[", "[");
                    break;
                case ']':
                    write(fpout, line_num, 29, (char*)"]", "]");
                    break;
                case '{':
                    write(fpout, line_num, 30, (char*)"{", "{");
                    break;
                case '}':
                    write(fpout, line_num, 31, (char*)"}", "}");
                    break;

                default:
                    write(fpout, line_num, -1, &ch, "ERROR");
                    break;
            }
        }
    }
}

int main() {
    const char* in_path = "test.txt";
    const char* out_path = "lex_result.txt";
    FILE *fpin, *fpout;
    fpin = fopen(in_path, "r");
    fpout = fopen(out_path, "w");
    analyse(fpin, fpout);
}
