//
//  sudoku_utility.c
//  sotuken
//
//  Created by 石崎駿 on 2019/09/10.
//  Copyright © 2019年 石崎駿. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sudoku_utility.h"

#ifdef __GCC__
#define strtok_s(a, b, c) strtok(a, b)
#define fopen_s(a, b, c) \
{                      \
*(a) = fopen(b, c);  \
}
#endif

void free_sudoku(p_sudoku_problems s)
{
    free(s->table);
    free(s);
}

void print_sudoku(p_sudoku_problems b)
{
    int i, j;
    int s = b->size;
    int s2 = s * s;
    
    for (i = 0; i < s2; i++)
    {
        for (j = 0; j < s2; j++)
        {
            if (b->table[i * s2 + j] > 0 && b->table[i * s2 + j] <= s2)
            {
                printf(" %2d", b->table[i * s2 + j]);
            }
            else
            {
                printf("   ");
            }
            if (j % s == s - 1 && j < s2 - 1)
            {
                printf("|");
            }
        }
        printf("\n");
        if (i % s == s - 1 && i < s2 - 1)
        {
            for (j = 0; j < s2 * 3 + s - 1; j++)
            {
                printf("-");
            }
            printf("\n");
        }
    }
}

p_sudoku_problems new_sudoku(int sudoku_size)
{
    p_sudoku_problems ret;
    
    ret = (p_sudoku_problems)malloc(sizeof(sudoku_problems));
    if (ret == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        return NULL;
    }
    ret->table = (unsigned int *)malloc(sizeof(unsigned int) * sudoku_size * sudoku_size * sudoku_size * sudoku_size);
    if (ret->table == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        free(ret);
        return NULL;
    }
    
    ret->size = sudoku_size;
    ret->blank_num = 0;
    return ret;
}

p_sudoku_problem_array load_sudoku_from_file(char *file_name)
{
    FILE *fp;
    p_sudoku_problem_array ret;
    char buf[1024];
    int load_flag = 0;
    int count = 0;
    int size;
    int i;
    int line = 0;
    int val;
    
    fopen_s(&fp, file_name, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Failed to open file = \"%s\"\n", file_name);
        return NULL;
    }
    ret = (p_sudoku_problem_array)malloc(sizeof(sudoku_problem_array));
    if (ret == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        fclose(fp);
        return NULL;
    }
    
    /* 問題数をカウント,#があれば問題とみなす */
    while (fgets(buf, 1024, fp)) {
        if (buf[0] == '#') {
            count++;
        }
    }
    fseek(fp, 0, SEEK_SET);
    ret->problems_count = count;
    ret->sudoku_array = (p_sudoku_problems *)malloc(sizeof(p_sudoku_problems) * count);
    if (ret == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        free(ret);
        fclose(fp);
        return NULL;
    }
    /* 問題を読み込み */
    count = -1;
    while (fgets(buf, 1024, fp)) {
        /*フラグが1の場合に読み込まれた行を','で分解，数値変換して問題読み込み*/
        if (load_flag) {
            char *tmp, *dummy;
            tmp = strtok_s(buf, ",", &dummy);
            val = atoi(tmp);
            if (val == 0)
                ret->sudoku_array[count]->blank_num++;
            ret->sudoku_array[count]->table[line * size * size] = val;
            if (tmp != NULL) {
                for (i = 1; i < size * size; i++) {
                    tmp = strtok_s(NULL, ",", &dummy);
                    if (tmp == NULL){
                        break;
                    }
                    val = atoi(tmp);
                    if (val == 0)
                        ret->sudoku_array[count]->blank_num++;
                    ret->sudoku_array[count]->table[line * size * size + i] = val;
                }
            }
            line++;
            if (line == size * size) {
                load_flag = 0;
            }
        }
        /*#が行頭にあれば続く行が問題とみなす．ここで問題サイズの取得とメモリ確保*/
        if (buf[0] == '#') {
            load_flag = 1; /*問題読み込みフラグ*/
            line = 0;
            count++;
            size = buf[1] - '0';
            if (size < 1 || size > 5) {
                fprintf(stderr, "invalid size = %d\n", size);
                for (i = 0; i < count; i++) {
                    free_sudoku(ret->sudoku_array[i]);
                }
                free(ret->sudoku_array);
                free(ret);
                fclose(fp);
                return NULL;
            }
            ret->sudoku_array[count] = new_sudoku(size);
            if (ret->sudoku_array[count] == NULL) {
                fprintf(stderr, "Failed to allocate memory\n");
                for (i = 0; i < count + 1; i++) {
                    free_sudoku(ret->sudoku_array[i]);
                }
                free(ret->sudoku_array);
                free(ret);
                fclose(fp);
                return NULL;
            }
        }
    }
    fclose(fp);
    return ret;
}
