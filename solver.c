////
////  solver.c
////  sotuken
////
////  Created by 石崎駿 on 2019/09/10.
////  Copyright © 2019年 石崎駿. All rights reserved.
////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif
#include "solver.h"
#include "sudoku_utility.h"
#include "priority_queue_heap.h"
#define getPosition(row, col, width) (col) * (width) + (row)
#define IS_DEBUG 0
#define MULTI 1
#define MEM_SIZE (128)
#define MAX_SOURCE_SIZE (0x530000)

cl_platform_id platform_id = NULL;
cl_device_id device_id = NULL;
cl_context context = NULL;
cl_command_queue command_queue = NULL;
cl_program program = NULL;
cl_kernel kernel_pruning = NULL;

void print_solving_sudoku(p_sudoku_problems b){
    int i, j;
    int s = b->size;
    int s2 = s * s;

    for (i = 0; i < s2; i++) {
        for (j = 0; j < s2; j++) {
            if (b->table[i * s2 + j] > 0 && b->table[i * s2 + j] <= s2) {
                printf(" %2d", b->table[i * s2 + j]);
            } else {
                printf("   ");
            }
            if (j % s == s - 1 && j < s2 - 1) {
                printf("|");
            }
        }
        printf("\n");
        if (i % s == s - 1 && i < s2 - 1) {
            for (j = 0; j < s2 * 3 + s - 1; j++) {
                printf("-");
            }
            printf("\n");
        }
    }
    printf("\n");
    printf("\n");
}

void print_opt_array(p_opt_table table) {
    int i, j;
    int line_size = table->sudoku->size * table->sudoku->size;
    for (i = 0; i < line_size; i++) {
        for (j = 0; j < line_size; j++) {
            printf("ROW = %d, COL = %d, NUM = %d\n", j, i, table->opt_num_array[i * line_size + j]);
        }
    }
}

void free_opt_table(p_opt_table table) {
    free_sudoku(table->sudoku);
    free(table->row_num_flag_array);
    free(table->col_num_flag_array);
    free(table->block_num_flag_array);
    free(table->opt_num_array);
    free_queue(table->queue);
    free(table);
}

void bit_pow(int n, int *m){
    int x = 1;
    int a = 2;
    while (n > 0) {
        if (n & 1) {
            x *= a;
            n--;
        }
        a *= a;
        n >>= 1;
    }
    *m = x;
    return;
}

void bit_count(int n, int *cnt){
    //    n = n - ((n >> 1) & 0x55555555);
    //    n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
    //    n = (n + (n >> 4)) & 0x0f0f0f0f;
    //    n = n + (n >> 8);
    //    n = n + (n >> 16);
    //    return n & 0x3f;
    *cnt = __builtin_popcount(n);
    return;
}

void fetch_opt_num(unsigned int opt_nums, int *num) {
    if (opt_nums == 0)
        return;
    int n = 0;
    while (1) {
        if (opt_nums & (1 << n)) {
            *num = n +  1;
            return;
        }
        n++;
    }
}

void delete_opt_num(p_opt_table table, int row, int col, int block, int target, int node_index) {
    remove_queue_elem(table->queue, node_index);
    table->block_num_flag_array[block] &= ~(1 << target);
    table->col_num_flag_array[col] &= ~(1 << target);
    table->row_num_flag_array[row] &= ~(1 << target);
    int i_row, i_col;
    int size = table->sudoku->size;
    int line_size = size * size;
    int opt_nums, index;
    table->opt_num_array[getPosition(row, col, line_size)] = 0;
    
    for (i_row = 0; i_row < line_size; i_row++) {
        if (table->sudoku->table[getPosition(i_row, col, line_size)] == 0) {
            search_node_index(*(table->queue), i_row, col, &index);
            table->opt_num_array[getPosition(i_row, col, line_size)] &= ~(1 << target);
            if (index != -1) {
                bit_count(table->opt_num_array[getPosition(i_row, col, line_size)], &opt_nums);
                table->queue->s_node[index].num_opts = opt_nums;
                heap_up(table->queue, index);
            }
        }
    }

    for (i_col = 0; i_col < line_size; i_col++) {
        if (table->sudoku->table[getPosition(row, i_col, line_size)] == 0) {
            search_node_index(*(table->queue), row, i_col, &index);
            table->opt_num_array[getPosition(row, i_col, line_size)] &= ~(1 << target);
            if (index != -1) {
                bit_count(table->opt_num_array[getPosition(row, i_col, line_size)], &opt_nums);
                table->queue->s_node[index].num_opts = opt_nums;
                heap_up(table->queue, index);
            }
        }
    }

    int topRow = (block % size) * size;
    int topCol = (block / size) * size;
    for (i_col = 0; i_col < size; i_col++) {
        for (i_row = 0; i_row < size; i_row++) {
            if (table->sudoku->table[getPosition(topRow + i_row, topCol + i_col, line_size)] == 0
                && (topRow + i_row != row || topCol + i_col != col)
                && (table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, line_size)] & (1 << target))
                ) {
                search_node_index(*(table->queue), topRow + i_row, topCol + i_col, &index);
                table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, line_size)] &= ~(1 << target);
                if (index != -1) {
                    bit_count(table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, line_size)], &opt_nums);
                    table->queue->s_node[index].num_opts = opt_nums;
                    heap_up(table->queue, index);
                }
            }
        }
    }
    
}

void update_opt_table(p_opt_table table, int row, int col, int block, int target, int node_index) {
    int line_size = table->sudoku->size * table->sudoku->size;
//    table->block_num_flag_array[block] &= ~(1 << target);
//    table->col_num_flag_array[col] &= ~(1 << target);
//    table->row_num_flag_array[row] &= ~(1 << target);
    table->opt_num_array[getPosition(row, col, line_size)] &= ~(1 << target);
    bit_count(table->opt_num_array[getPosition(row, col, line_size)], &(table->queue->s_node[node_index].num_opts));
}

void copy_sudoku_problem(p_sudoku_problems a, p_sudoku_problems b){
    int size = a->size;
    int line_size = size * size;
    int block_count = line_size * line_size;

    b->size = a->size;
    int i;
    for (i = 0; i < block_count; i++) {
        b->table[i] = a->table[i];
    }
    b->blank_num = a->blank_num;
}

//p_opt_table init_opt_table(int sudoku_size, int blank_num){
p_opt_table init_opt_table(p_sudoku_problems sudoku){
    int block_size = sudoku->size * sudoku->size;
    int set_up_bit;
    bit_pow(block_size, &set_up_bit);
    set_up_bit--;
    p_opt_table p_table;
    p_table = (p_opt_table)malloc(sizeof(opt_table));
    if (p_table == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        return NULL;
    }
    p_table->block_num_flag_array = (unsigned int *)malloc(sizeof(unsigned int) * block_size);
    if (p_table->block_num_flag_array == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        free(p_table);
        return NULL;
    }
    p_table->col_num_flag_array = (unsigned int *)malloc(sizeof(unsigned int) * block_size);
    if (p_table->col_num_flag_array == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        free(p_table->block_num_flag_array);
        free(p_table);
        return NULL;
    }
    p_table->row_num_flag_array = (unsigned int *)malloc(sizeof(unsigned int) * block_size);
    if (p_table->row_num_flag_array == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        free(p_table->col_num_flag_array);
        free(p_table->block_num_flag_array);
        free(p_table);
        return NULL;
    }
    p_table->opt_num_array = (unsigned int *)malloc(sizeof(unsigned int) * block_size * block_size);
    if (p_table->opt_num_array == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        free(p_table->row_num_flag_array);
        free(p_table->col_num_flag_array);
        free(p_table->block_num_flag_array);
        free(p_table);
        return NULL;
    }
    p_table->queue = queue_create(sudoku->blank_num);
    if (&(p_table->queue) == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        free(p_table->opt_num_array);
        free(p_table->row_num_flag_array);
        free(p_table->col_num_flag_array);
        free(p_table->block_num_flag_array);
        free(p_table);
        return NULL;
    }
    int row, col;
    for (row = 0; row < block_size; row++) {
        p_table->block_num_flag_array[row] = set_up_bit;
        p_table->row_num_flag_array[row] = set_up_bit;
        p_table->col_num_flag_array[row] = set_up_bit;
        for (col = 0; col < block_size; col++) {
            p_table->opt_num_array[getPosition(row, col, block_size)] = set_up_bit;
        }
    }
    p_table->sudoku = new_sudoku(sudoku->size);
    copy_sudoku_problem(sudoku, p_table->sudoku);
    return p_table;
}

void copy_opt_table(p_opt_table a, p_opt_table b, int size){
    int line_size = size * size;

    int i, j;
    b->queue->capacty = a->queue->capacty;
    b->queue->first_empty = a->queue->first_empty;
    for (j = 1; j <= a->queue->first_empty; j++) {
        b->queue->s_node[j].row = a->queue->s_node[j].row;
        b->queue->s_node[j].col = a->queue->s_node[j].col;
        b->queue->s_node[j].block = a->queue->s_node[j].block;
        b->queue->s_node[j].num_opts = a->queue->s_node[j].num_opts;
        b->queue->s_node[j].isApplied = a->queue->s_node[j].isApplied;
    }
    for (i = 0; i < line_size; i++) {
        b->block_num_flag_array[i] = a->block_num_flag_array[i];
        b->row_num_flag_array[i] = a->row_num_flag_array[i];
        b->col_num_flag_array[i] = a->col_num_flag_array[i];
        for (j = 0; j < line_size; j++) {
            b->opt_num_array[getPosition(j, i, line_size)] = a->opt_num_array[getPosition(j, i, line_size)];
        }
    }
    copy_sudoku_problem(a->sudoku, b->sudoku);
}

void nakedSingle(p_opt_table table, int *code) {
    int size = table->sudoku->size;
    int line_size = size * size;
    int isChanged = 0;
    int row, col, block, position, opt_num;
    while (table->queue->s_node[1].num_opts == 1) {
        row = table->queue->s_node[1].row;
        col = table->queue->s_node[1].col;
        block = table->queue->s_node[1].block;
        position = col * line_size + row;
        fetch_opt_num(table->opt_num_array[position], &opt_num);
        table->sudoku->table[position] = opt_num;
        table->sudoku->blank_num--;
        delete_opt_num(table, row, col, block, opt_num - 1, 1);
#if IS_DEBUG == 1
        printf("ROW = %d COL = %d HAS NAKED SINGLE NUM = %d\n", row, col, opt_num);
#endif
        isChanged = 1;
    }
    *code = isChanged;
}

void hiddenSingle(p_opt_table table, int *code){
    int i;
    int i_row, i_col;
    int isChanged = 0;
    int size = table->sudoku->size;
    int line_size = size * size;
    int opt_num, index;
    int q0 = 0, q1 = 0;

    for (i = 0; i < line_size; i++) {
        //row
        q0 = 0; q1 = 0;
        for (i_row = 0; i_row < line_size; i_row++) {
            if (table->opt_num_array[getPosition(i_row, i, line_size)] != 0) {
                int q0_tmp = q0 | table->opt_num_array[getPosition(i_row, i, line_size)];
                q1 = (q1 | table->opt_num_array[getPosition(i_row, i, line_size)]) & q0;
                q0 = q0_tmp;
            }
        }
        int res = q0 & ~q1;
        if (res != 0) {
            for (i_row = 0; i_row < line_size; i_row++) {
                if (table->opt_num_array[getPosition(i_row, i, line_size)] != 0 && (table->opt_num_array[getPosition(i_row, i, line_size)] & res) == res) {
                    isChanged = 1;
                    fetch_opt_num(res & table->opt_num_array[getPosition(i_row, i, line_size)], &opt_num);
                    table->sudoku->table[getPosition(i_row, i, line_size)] = opt_num;
                    table->sudoku->blank_num--;
                    search_node_index(*(table->queue), i_row, i, &index);
                    delete_opt_num(table, i_row, i, (i / size) * size + (i_row / size), opt_num - 1, index);
#if IS_DEBUG == 1
                    printf("ROW = %d COL = %d HAS HIDDEN SINGLE IN ROW NUM = %d\n", i_row, i, opt_num);
#endif
                    break;
                }
            }
        }

        //col
        q0 = q1 = 0;
        for (i_col = 0; i_col < line_size; i_col++) {
            if (table->opt_num_array[getPosition(i, i_col, line_size)] != 0) {
                int q0_tmp = q0 | table->opt_num_array[getPosition(i, i_col, line_size)];
                q1 = (q1 | table->opt_num_array[getPosition(i, i_col, line_size)]) & q0;
                q0 = q0_tmp;
            }
        }
        res = q0 & ~q1;
        if (res != 0) {
            for (i_col = 0; i_col < line_size; i_col++) {
                if (table->opt_num_array[getPosition(i, i_col, line_size)] != 0 && (table->opt_num_array[getPosition(i, i_col, line_size)] & res) == res) {
                    isChanged = 1;
                    fetch_opt_num(res & table->opt_num_array[getPosition(i, i_col, line_size)], &opt_num);
                    table->sudoku->table[getPosition(i, i_col, line_size)] = opt_num;
                    table->sudoku->blank_num--;
                    search_node_index(*(table->queue), i, i_col, &index);
                    delete_opt_num(table, i, i_col, (i_col / size) * size + (i / size), opt_num - 1, index);
#if IS_DEBUG == 1
                    printf("ROW = %d COL = %d HAS HIDDEN SINGLE IN COL NUM = %d\n", i, i_col, opt_num);
#endif
                    break;
                }
            }
        }

        //block
        q0 = q1 = 0;
        int topRow = (i % size) * size, topCol = (i / size) * size;
        for (i_col = 0; i_col < size; i_col++) {
            for (i_row = 0; i_row < size; i_row++) {
                int col = topCol + i_col, row = topRow + i_row;
                if (table->opt_num_array[getPosition(row, col, line_size)] != 0) {
                    int q0_tmp = q0 | table->opt_num_array[getPosition(row, col, line_size)];
                    q1 = (q1 | table->opt_num_array[getPosition(row, col, line_size)]) & q0;
                    q0 = q0_tmp;
                }
            }
        }
        res = q0 & ~q1;
        if (res != 0) {
            for (i_col = 0; i_col < size; i_col++) {
                for (i_row = 0; i_row < size; i_row++) {
                    int col = topCol + i_col, row = topRow + i_row;
                    if (table->opt_num_array[getPosition(row, col, line_size)] != 0 && (table->opt_num_array[getPosition(row, col, line_size)] & res) == res) {
                        isChanged = 1;
                        fetch_opt_num(res & table->opt_num_array[getPosition(row, col, line_size)], &opt_num);
                        table->sudoku->table[getPosition(row, col, line_size)] = opt_num;
                        table->sudoku->blank_num--;
                        search_node_index(*(table->queue), row, col, &index);
                        delete_opt_num(table, row, col, i, opt_num - 1, index);
#if IS_DEBUG == 1
                        printf("ROW = %d COL = %d HAS HIDDEN SINGLE IN BLOCK NUM = %d\n", row, col, opt_num);
#endif
                        break;
                    }
                }
            }
        }

    }
    *code = isChanged;
    return;
}

void nakedPair(p_opt_table table, int *code) {
    int row, col, block;
    int i_row, i_col;
    int topRow, topCol;
    int size = table->sudoku->size;
    int block_size = size * size;
    int row_found = -1, col_found = -1, block_col_found = -1, block_row_found = -1;
    int i, isChanged = 0, node_index;
    int mask, bit_cnt;

    for (i = 1; i < table->queue->first_empty; i++) {
        if (!(table->queue->s_node[i].isApplied & (1 << N_PAIR)) && table->queue->s_node[i].num_opts == 2) {
            mask = 0;
            row = table->queue->s_node[i].row;
            col = table->queue->s_node[i].col;
            block = table->queue->s_node[i].block;
            topRow = (block % size) * size;
            topCol = (block / size) * size;
            row_found = -1;
            col_found = -1;
            block_col_found = -1;
            block_row_found = -1;
            
            bit_count(table->row_num_flag_array[row], &bit_cnt);
            if (bit_cnt > 2) {
                for (i_col = 0; i_col < block_size; i_col++) {
                    if (i_col != col && table->opt_num_array[getPosition(row, i_col, block_size)] == table->opt_num_array[getPosition(row, col, block_size)]) {
                        col_found = i_col;
                        search_node_index(*(table->queue), row, i_col, &node_index);
                        table->queue->s_node[node_index].isApplied = (1 << N_PAIR);
                        mask = table->opt_num_array[getPosition(row, i_col, block_size)];
                        table->queue->s_node[i].isApplied |= (1 << N_PAIR);
#if IS_DEBUG == 1
                        printf("NAKED PAIR ROW = (%d, %d) (%d, %d)\n", row, col, row, col_found);
#endif
                        break;
                    }
                }
                if (col_found != -1 && mask != 0) {
                    for (i_col = 0; i_col < block_size; i_col++) {
                        if (i_col != col_found && i_col != col && table->opt_num_array[getPosition(row, i_col, block_size)] != 0) {
                            isChanged = 1;
                            search_node_index(*(table->queue), row, i_col, &node_index);
                            table->opt_num_array[getPosition(row, i_col, block_size)] &= ~mask;
                            bit_count(table->opt_num_array[getPosition(row, i_col, block_size)], &(table->queue->s_node[node_index].num_opts));
                            table->queue->s_node[node_index].isApplied = (1 << N_PAIR);
                            heap_up(table->queue, node_index);
                        }
                    }
                    mask = 0;
                }
            }

            bit_count(table->col_num_flag_array[col], &bit_cnt);
            if (bit_cnt > 2) {
                for (i_row = 0; i_row < block_size; i_row++) {
                    if (i_row != row && table->opt_num_array[getPosition(i_row, col, block_size)] == table->opt_num_array[getPosition(row, col, block_size)]) {
                        row_found = i_row;
                        search_node_index(*(table->queue), i_row, col, &node_index);
                        table->queue->s_node[node_index].isApplied = (1 << N_PAIR);
                        mask = table->opt_num_array[getPosition(i_row, col, block_size)];
                        table->queue->s_node[i].isApplied |= (1 << N_PAIR);
#if IS_DEBUG == 1
                        printf("NAKED PAIR COL = (%d, %d) (%d, %d)\n", row, col, row_found, col);
#endif
                        break;
                    }
                }
                if (row_found != -1 && mask != 0) {
                    for (i_row = 0; i_row < block_size; i_row++) {
                        if (i_row != row_found && i_row != row && table->opt_num_array[getPosition(i_row, col, block_size)] != 0) {
                            isChanged = 1;
                            search_node_index(*(table->queue), i_row, col, &node_index);
                            table->opt_num_array[getPosition(i_row, col, block_size)] &= ~mask;
                            bit_count(table->opt_num_array[getPosition(i_row, col, block_size)], &(table->queue->s_node[node_index].num_opts));
                            table->queue->s_node[node_index].isApplied |= (1 << N_PAIR);
                            heap_up(table->queue, node_index);
                        }
                    }
                    mask = 0;
                }
            }

            bit_count(table->block_num_flag_array[block], &bit_cnt);
            if (bit_cnt > 2) {
                for (i_col = 0; i_col < size; i_col++) {
                    for (i_row = 0; i_row < size; i_row++) {
                        if ((topRow + i_row != row || topCol + i_col != col)
                            && table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, block_size)] == table->opt_num_array[getPosition(row, col, block_size)]) {
                            block_row_found = topRow + i_row;
                            block_col_found = topCol + i_col;
                            search_node_index(*(table->queue), block_row_found, block_col_found, &node_index);
                            table->queue->s_node[node_index].isApplied = (1 << N_PAIR);
                            mask = table->opt_num_array[getPosition(block_row_found, block_col_found, block_size)];
                            table->queue->s_node[i].isApplied |= (1 << N_PAIR);
#if IS_DEBUG == 1
                            printf("NAKED PAIR BLOCK = (%d, %d) (%d, %d)\n", row, col, block_row_found, block_col_found);
#endif
                            break;
                        }
                    }
                }
                if (block_row_found != -1 && block_col_found != -1 && mask != 0) {
                    for (i_col = 0; i_col < size; i_col++) {
                        for (i_row = 0; i_row < size; i_row++) {
                            if (topRow + i_row == row && topCol + i_col == col) {
                                continue;
                            } else if (topRow + i_row == block_row_found && topCol + i_col == block_col_found) {
                                continue;
                            } else if (table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, block_size)] != 0) {
                                isChanged = 1;
                                search_node_index(*(table->queue), topRow + i_row, topCol + i_col, &node_index);
                                table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, block_size)] &= ~mask;
                                bit_count(table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, block_size)], &(table->queue->s_node[node_index].num_opts));
                                table->queue->s_node[node_index].isApplied |= (1 << N_PAIR);
                                heap_up(table->queue, node_index);
                            }
                        }
                    }
                    mask = 0;
                }
            }

        }
    }
    *code = isChanged;
    return;
}

void hiddenPair(p_opt_table table, int *code){
    int i_row, i_col;
    int size = table->sudoku->size;
    int line_size = size * size;
    int i, isChanged = 0, node_index1, node_index2;
    int block_found = 0;
    int bit_cnt, mask;
    int q0 = 0, q1 = 0, q2 = 0;

    for (i = 0; i < line_size; i++) {
        block_found = 0;
        //row
        bit_count(table->col_num_flag_array[i], &bit_cnt);
        if (bit_cnt > 2) {
            q0 = 0; q1 = 0; q2 = 0;
            for (i_row = 0; i_row < line_size; i_row++) {
                int q0_tmp = q0 | table->opt_num_array[getPosition(i_row, i, line_size)];
                int q1_tmp = (q1 | table->opt_num_array[getPosition(i_row, i, line_size)]) & q0;
                q0 = q0_tmp;
                q2 = (q2 | table->opt_num_array[getPosition(i_row, i, line_size)]) & q1;
                q1 = q1_tmp;
            }
            int res = 0;
            res = q0 & q1 & ~q2;
            bit_count(res, &bit_cnt);
            if (bit_cnt >= 2) {
                for (i_row = 0; i_row < line_size - 1; i_row++) {
                    bit_count(table->opt_num_array[getPosition(i_row, i, line_size)] & res, &bit_cnt);
                    if (bit_cnt != 2)continue;
                    mask = table->opt_num_array[getPosition(i_row, i, line_size)] & res;
                    int j_row;
                    search_node_index(*(table->queue), i_row, i, &node_index1);
                    if (table->queue->s_node[node_index1].isApplied & (1 << H_PAIR)) continue;
                    for (j_row = i_row + 1; j_row < line_size; j_row++) {
                        if (i_row != j_row && (table->opt_num_array[getPosition(j_row, i, line_size)] & mask) == mask) {
                            isChanged = 1;
                            table->opt_num_array[getPosition(i_row, i, line_size)] = mask;
                            table->queue->s_node[node_index1].num_opts = 2;
                            table->queue->s_node[node_index1].isApplied |= (1 << H_PAIR);
                            heap_up(table->queue, node_index1);
                            search_node_index(*(table->queue), j_row, i, &node_index2);
                            table->opt_num_array[getPosition(j_row, i, line_size)] = mask;
                            table->queue->s_node[node_index2].num_opts = 2;
                            table->queue->s_node[node_index2].isApplied |= (1 << H_PAIR);
                            heap_up(table->queue, node_index2);
#if IS_DEBUG == 1
                            printf("ROW HIDDEN PAIR = (%d, %d) (%d, %d)\n", i_row, i, j_row, i);
#endif
                            break;
                        }
                    }
                }
            }
        }

        //col
        bit_count(table->row_num_flag_array[i], &bit_cnt);
        if (bit_cnt > 2) {
            q0 = 0; q1 = 0; q2 = 0;
            for (i_col = 0; i_col < line_size; i_col++) {
                int q0_tmp = q0 | table->opt_num_array[getPosition(i, i_col, line_size)];
                int q1_tmp = (q1 | table->opt_num_array[getPosition(i, i_col, line_size)]) & q0;
                q0 = q0_tmp;
                q2 = (q2 | table->opt_num_array[getPosition(i, i_col, line_size)]) & q1;
                q1 = q1_tmp;
            }
            int res = q0 & q1 & ~q2;
            bit_count(res, &bit_cnt);
            if (bit_cnt >= 2) {
                for (i_col = 0; i_col < line_size - 1; i_col++) {
                    bit_count(table->opt_num_array[getPosition(i, i_col, line_size)] & res, &bit_cnt);
                    if (bit_cnt != 2)continue;
                    mask = table->opt_num_array[getPosition(i, i_col, line_size)] & res;
                    int j_col;
                    search_node_index(*(table->queue), i, i_col, &node_index1);
                    if (table->queue->s_node[node_index1].isApplied & (1 << H_PAIR)) continue;
                    for (j_col = i_col + 1; j_col < line_size; j_col++) {
                        if (i_col != j_col && (table->opt_num_array[getPosition(i, j_col, line_size)] & mask) == mask) {
                            isChanged = 1;
                            table->opt_num_array[getPosition(i, i_col, line_size)] = mask;
                            table->queue->s_node[node_index1].num_opts = 2;
                            table->queue->s_node[node_index1].isApplied |= (1 << H_PAIR);
                            heap_up(table->queue, node_index1);
                            table->opt_num_array[getPosition(i, j_col, line_size)] = mask;
                            search_node_index(*(table->queue), i, j_col, &node_index2);
                            table->queue->s_node[node_index2].num_opts = 2;
                            table->queue->s_node[node_index2].isApplied |= (1 << H_PAIR);
                            heap_up(table->queue, node_index2);
#if IS_DEBUG == 1
                            printf("COL HIDDEN PAIR = (%d, %d) (%d, %d)\n", i, i_col, i, j_col);
#endif
                            break;
                        }
                    }
                }
            }
        }

        //block
        bit_count(table->block_num_flag_array[i], &bit_cnt);
        if (bit_cnt > 2) {
            q0 = 0; q1 = 0; q2 = 0;
            int topRow = (i % size) * size, topCol = (i / size) * size;
            for (i_col = 0; i_col < size; i_col++) {
                for (i_row = 0; i_row < size; i_row++) {
                    int col = topCol + i_col, row = topRow + i_row;
                    int q0_tmp = q0 | table->opt_num_array[getPosition(row, col, line_size)];
                    int q1_tmp = (q1 | table->opt_num_array[getPosition(row, col, line_size)]) & q0;
                    q0 = q0_tmp;
                    q2 = (q2 | table->opt_num_array[getPosition(row, col, line_size)]) & q1;
                    q1 = q1_tmp;
                }
            }
            int res = q0 & q1 & ~q2;
            bit_count(res, &bit_cnt);
            if (bit_cnt >= 2) {
                for (i_col = 0; i_col < size; i_col++) {
                    for (i_row = 0; i_row < size; i_row++) {
                        int col = topCol + i_col, row = topRow + i_row;
                        bit_count(table->opt_num_array[getPosition(row, col, line_size)] & res, &bit_cnt);
                        if (bit_cnt != 2)continue;
                        mask = table->opt_num_array[getPosition(row, col, line_size)] & res;
                        search_node_index(*(table->queue), row, col, &node_index1);
                        if (table->queue->s_node[node_index1].isApplied & (1 << H_PAIR)) continue;
                        int j_col, j_row;
                        for (j_col = 0; j_col < size; j_col++) {
                            for (j_row = 0; j_row < size; j_row++) {
                                if (topRow + j_row == row && topCol + j_col == col) {
                                    continue;
                                }
                                if ((table->opt_num_array[getPosition(topRow + j_row, topCol + j_col, line_size)] & mask) == mask) {
                                    isChanged = 1;
                                    block_found = 1;
                                    table->opt_num_array[getPosition(row, col, line_size)] = mask;
                                    table->queue->s_node[node_index1].num_opts = 2;
                                    table->queue->s_node[node_index1].isApplied |= (1 << H_PAIR);
                                    heap_up(table->queue, node_index1);
                                    search_node_index(*(table->queue), topRow + j_row, topCol + j_col, &node_index2);
                                    table->opt_num_array[getPosition(topRow + j_row, topCol + j_col, line_size)] = mask;
                                    table->queue->s_node[node_index2].num_opts = 2;
                                    table->queue->s_node[node_index2].isApplied |= (1 << H_PAIR);
                                    heap_up(table->queue, node_index2);
#if IS_DEBUG == 1
                                    printf("BLOCK HIDDEN PAIR = (%d, %d) (%d, %d)\n", row, col, topRow + j_row, topCol + j_col);
#endif
                                    break;
                                }
                            }
                            if (block_found) break;
                        }
                        if (block_found) break;
                    }
                    if (block_found) break;
                }
            }
        }
    }
    *code = isChanged;
    return;
}

void nakedTriple(p_opt_table table, int *code){
    int row, col, block;
    int i_row, i_row1, i_row2;
    int i_col, i_col1, i_col2;
    int topRow, topCol;
    int size = table->sudoku->size;
    int block_size = size * size;
    int row_found1, col_found1, row_found2, col_found2;
    int block_col_found1, block_row_found1, block_col_found2, block_row_found2;
    int i, isChanged = 0, node_index;
    int mask, bit_cnt;

    for (i = 1; i <= table->queue->first_empty; i++) {
        if (!(table->queue->s_node[i].isApplied & (1 << TRIPLE)) && (table->queue->s_node[i].num_opts == 2 || table->queue->s_node[i].num_opts == 3)) {
            mask = 0;
            row = table->queue->s_node[i].row;
            col = table->queue->s_node[i].col;
            block = table->queue->s_node[i].block;
            topRow = (block % size) * size; topCol = (block / size) * size;
            row_found1 = -1; row_found2 = -1;
            col_found1 = -1; col_found2 = -1;
            block_col_found1 = -1; block_col_found2 = -1;
            block_row_found1 = -1; block_row_found2 = -1;

            //col
            bit_count(table->row_num_flag_array[row], &bit_cnt);
            if (bit_cnt > 3) {
                for (i_col1 = 0; i_col1 < block_size; i_col1++) {
                    bit_count(table->opt_num_array[getPosition(row, i_col1, block_size)] | table->opt_num_array[getPosition(row, col, block_size)], &bit_cnt);
                    if (i_col1 != col && table->opt_num_array[getPosition(row, i_col1, block_size)] != 0 && bit_cnt == 3) {
                        mask = table->opt_num_array[getPosition(row, i_col1, block_size)] | table->opt_num_array[getPosition(row, col, block_size)];
                        for (i_col2 = i_col1 + 1; i_col2 < block_size; i_col2++) {
                            bit_count(table->opt_num_array[getPosition(row, i_col2, block_size)] | mask, &bit_cnt);
                            if (i_col2 != col && table->opt_num_array[getPosition(row, i_col2, block_size)] != 0 && bit_cnt == 3) {
                                col_found1 = i_col1; col_found2 = i_col2;
                                search_node_index(*(table->queue), row, col_found1, &node_index);
                                table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                search_node_index(*(table->queue), row, col_found2, &node_index);
                                table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                mask |= table->opt_num_array[getPosition(row, i_col2, block_size)];
                                table->queue->s_node[i].isApplied |= (1 << TRIPLE);
#if IS_DEBUG == 1
                                printf("NAKED TRIPLE ROW = (%d, %d) (%d, %d) (%d, %d)\n", row, col, row, col_found1, row, col_found2);
#endif
                                break;
                            }
                        }
                        if (col_found1 != -1 && col_found2 != -1 && mask != 0) break;
                    }
                }
                if (col_found1 != -1 && col_found2 != -1 && mask != 0) {
                    for (i_col = 0; i_col < block_size; i_col++) {
                        if (table->opt_num_array[getPosition(row, i_col, block_size)] != 0 && i_col != col_found1 && i_col != col_found2 && i_col != col) {
                            isChanged = 1;
                            search_node_index(*(table->queue), row, i_col, &node_index);
                            table->opt_num_array[getPosition(row, i_col, block_size)] &= ~mask;
                            bit_count(table->opt_num_array[getPosition(row, i_col, block_size)], &(table->queue->s_node[node_index].num_opts));
                            table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                            heap_up(table->queue, node_index);
                        }
                    }
                }
                mask = 0;
            }

            //row
            bit_count(table->col_num_flag_array[col], &bit_cnt);
            if (bit_cnt > 3) {
                for (i_row1 = 0; i_row1 < block_size; i_row1++) {
                    bit_count(table->opt_num_array[getPosition(i_row1, col, block_size)] | table->opt_num_array[getPosition(row, col, block_size)], &bit_cnt);
                    if (table->opt_num_array[getPosition(i_row1, col, block_size)] != 0 && i_row1 != row && bit_cnt == 3) {
                        mask = table->opt_num_array[getPosition(i_row1, col, block_size)] | table->opt_num_array[getPosition(row, col, block_size)];
                        for (i_row2 = i_row1 + 1; i_row2 < block_size; i_row2++) {
                            bit_count(table->opt_num_array[getPosition(i_row2, col, block_size)] | mask, &bit_cnt);
                            if (table->opt_num_array[getPosition(i_row2, col, block_size)] != 0 && i_row2 != row && bit_cnt == 3) {
                                row_found1 = i_row1; row_found2 = i_row2;
                                search_node_index(*(table->queue), row_found1, col, &node_index);
                                table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                search_node_index(*(table->queue), row_found2, col, &node_index);
                                table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                mask |= table->opt_num_array[getPosition(i_row2, col, block_size)];
                                table->queue->s_node[i].isApplied |= (1 << TRIPLE);
#if IS_DEBUG == 1
                                printf("NAKED TRIPLE COL = (%d, %d) (%d, %d) (%d, %d)\n", row, col, row_found1, col, row_found2, col);
#endif
                                break;
                            }
                        }
                        if (row_found1 != -1 && row_found2 != -1 && mask != 0) break;
                    }
                }
                if (row_found1 != -1 && row_found2 != -1 && mask != 0) {
                    for (i_row = 0; i_row < block_size; i_row++) {
                        search_node_index(*(table->queue), i_row, col, &node_index);
                        if (table->opt_num_array[getPosition(i_row, col, block_size)] != 0 && i_row != row_found1 && i_row != row_found2 && i_row != row) {
                            isChanged = 1;
                            table->opt_num_array[getPosition(i_row, col, block_size)] &= ~mask;
                            bit_count(table->opt_num_array[getPosition(i_row, col, block_size)], &(table->queue->s_node[node_index].num_opts));
                            table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                            heap_up(table->queue, node_index);
                        }
                    }
                }
                mask = 0;
            }

            //block
            bit_count(table->block_num_flag_array[block], &bit_cnt);
            if (bit_cnt > 3) {
                for (i_col = 0; i_col < size; i_col++) {
                    for (i_row = 0; i_row < size; i_row++) {
                        bit_count(mask | table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, block_size)] | table->opt_num_array[getPosition(row, col, block_size)], &bit_cnt);
                        search_node_index(*(table->queue), topRow + i_row, topCol + i_col, &node_index);
                        if (table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, block_size)] != 0
//                            && !(table->queue->s_node[node_index].isApplied & (1 << TRIPLE))
                            && (topRow + i_row != row || topCol + i_col != col) && bit_cnt == 3) {
                            if ((block_row_found1 == -1 && block_col_found1 == -1) || (block_row_found2 == -1 && block_col_found2 == -1)) {
                                if (block_row_found1 == -1 && block_col_found1 == -1) {
                                    block_row_found1 = topRow + i_row;
                                    block_col_found1 = topCol + i_col;
                                    mask = table->opt_num_array[getPosition(block_row_found1, block_col_found1, block_size)] | table->opt_num_array[getPosition(row, col, block_size)];
                                } else if (block_row_found2 == -1 && block_col_found2 == -1) {
                                    block_row_found2 = topRow + i_row;
                                    block_col_found2 = topCol + i_col;
                                    search_node_index(*(table->queue), block_row_found1, block_col_found1, &node_index);
                                    table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                    search_node_index(*(table->queue), block_row_found2, block_col_found2, &node_index);
                                    table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                    mask |= table->opt_num_array[getPosition(block_row_found2, block_col_found2, block_size)];
                                    table->queue->s_node[i].isApplied |= (1 << TRIPLE);
#if IS_DEBUG == 1
                                    printf("NAKED TRIPLE BLOCK = (%d, %d) (%d, %d) (%d, %d)\n", row, col, block_row_found1, block_col_found1, block_row_found2, block_col_found2);
#endif
                                    break;
                                }
                            }
                        }
                    }
                    if (block_row_found1 != -1 && block_col_found1 != -1 && block_row_found2 != -1 && block_col_found2 != -1 && mask != 0)break;
                }
                if ((block_row_found1 != -1 && block_col_found1 != -1) && (block_row_found2 != -1 && block_col_found2 != -1) && mask != 0) {
                    for (i_col = 0; i_col < size; i_col++) {
                        for (i_row = 0; i_row < size; i_row++) {
                            if (topRow + i_row == row && topCol + i_col == col) {
                                continue;
                            } else if (topRow + i_row == block_row_found1 && topCol + i_col == block_col_found1) {
                                continue;
                            } else if (topRow + i_row == block_row_found2 && topCol + i_col == block_col_found2){
                                continue;
                            } else if (table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, block_size)] != 0){
                                isChanged = 1;
                                search_node_index(*(table->queue), topRow + i_row, topCol + i_col, &node_index);
                                table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, block_size)] &= ~mask;
                                bit_count(table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, block_size)], &(table->queue->s_node[node_index].num_opts));
                                table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                heap_up(table->queue, node_index);
                            }
                        }
                    }
                }
                mask = 0;
            }
        }
    }
    *code = isChanged;
    return;
}

void lockedCandidateType1(p_opt_table table, int *code) {
    int isChanged = 0;
    int size = table->sudoku->size;
    int line_size = size * size;
    int num;
    int topRow, topCol;
    int row, col, block;
    int i_row, i_col, i_block, node_index;
    int res = 0;
    int q0, q1, q2;
    for (block = 0; block < line_size; block++) {
        topRow = (block % size) * size;
        topCol = (block / size) * size;
        
        for(col = 0; col < size; col++){
            q0 = 0; q1 = 0; q2 = 0;
            for (i_row = 0; i_row < size; i_row++) {
                int q0_tmp = q0 | table->opt_num_array[getPosition(topRow + i_row, topCol + col, line_size)];
                int q1_tmp = (q1 | table->opt_num_array[getPosition(topRow + i_row, topCol + col, line_size)]) & q0;
                q0 = q0_tmp;
                q2 = (q2 | table->opt_num_array[getPosition(topRow + i_row, topCol + col, line_size)]) & q1;
                q1 = q1_tmp;
            }
            
            res = q0 & q1;
            if (res != 0) {
                for (i_col = 0; i_col < size; i_col++) {
                    for (i_row = 0; i_row < size; i_row++) {
                        if (i_col == col) continue;
                        res &= ~table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, line_size)];
                        if (res == 0)break;
                    }
                    if (res == 0) break;
                }
            }
            
            if (res != 0) {
                for (i_block = 0; i_block < size; i_block++) {
                    if (i_block + topCol == block) continue;
                    for (i_row = 0; i_row < size; i_row++) {
                        row = (i_block % size) * size + i_row;
                        num = table->opt_num_array[getPosition(row, topCol + col, line_size)];
                        if (!(num & res))continue;
                        search_node_index(*(table->queue), row, topCol + col, &node_index);
                        if (table->queue->s_node[node_index].isApplied & (1 << LOCK_C_1)) {
                            isChanged = 0;
                            break;
                        }
                        isChanged = 1;
                        table->opt_num_array[getPosition(row, topCol + col, line_size)] &= ~res;
                        bit_count(table->opt_num_array[getPosition(row, topCol + col, line_size)], &(table->queue->s_node[node_index].num_opts));
                        heap_up(table->queue, node_index);
                    }
                }
#if IS_DEBUG == 1
                if (isChanged) printf("LOCK CANDIDATE 1(ROW) = BLOCK: %2d, ROW: %2d, NUM: %2d\n", block, topCol + col, res);
#endif
            }
        }
        
        for(row = 0; row < size; row++){
            q0 = 0; q1 = 0; q2 = 0;
            for (i_col = 0; i_col < size; i_col++) {
                int q0_tmp = q0 | table->opt_num_array[getPosition(topRow + row, topCol + i_col, line_size)];
                int q1_tmp = (q1 | table->opt_num_array[getPosition(topRow + row, topCol + i_col, line_size)]) & q0;
                q0 = q0_tmp;
                q2 = (q2 | table->opt_num_array[getPosition(topRow + row, topCol + i_col, line_size)]) & q1;
                q1 = q1_tmp;
            }
            
            res = q0 & q1;
            if (res != 0) {
                for (i_col = 0; i_col < size; i_col++) {
                    for (i_row = 0; i_row < size; i_row++) {
                        if (i_row == row) continue;
                        res &= ~table->opt_num_array[getPosition(topRow + i_row, topCol + i_col, line_size)];
                        if (res == 0)break;
                    }
                    if (res == 0) break;
                }
            }
            
            if (res != 0) {
                for (i_block = block % size; i_block < line_size; i_block += size) {
                    if (i_block == block) continue;
                    for (i_col = 0; i_col < size; i_col++) {
                        col = (i_block / size) * size + i_col;
                        num = table->opt_num_array[getPosition(topRow + row, col, line_size)];
                        if (!(num & res))continue;
                        search_node_index(*(table->queue), topRow + row, col, &node_index);
                        if (table->queue->s_node[node_index].isApplied & (1 << LOCK_C_1)) {
                            isChanged = 0;
                            break;
                        }
                        isChanged = 1;
                        table->opt_num_array[getPosition(topRow + row, col, line_size)] &= ~res;
                        bit_count(table->opt_num_array[getPosition(topRow + row, col, line_size)], &(table->queue->s_node[node_index].num_opts));
                        heap_up(table->queue, node_index);
                    }
                }
#if IS_DEBUG == 1
                if (isChanged) printf("LOCK CANDIDATE 1(COL) = BLOCK: %2d, COL: %2d, NUM: %2d\n", block, topRow + row, res);
#endif
            }
        }
        
    }
    
    *code = isChanged;
    return;
}

void pruning_launch(p_opt_table table, p_opt_table pruning_table, int *code) {
    int size = table->sudoku->size;
    int line_size = size * size;
    int isChanged = 0;
    cl_mem table_mem, row_num_flag_array_mem, col_num_flag_array_mem, block_num_flag_array_mem, opt_num_array_mem;
    cl_mem table_sudoku_mem, sudoku_table_mem;
    cl_mem table_queue_mem;
    cl_mem s_node_mem;
    cl_mem pruning_table_mem;
    cl_mem code_mem;
    cl_int ret;
    size_t global_work_size[] = { size * size , size * size };
    size_t local_work_size[] = { 1 , 1 };
    
    table_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(opt_table), NULL, &ret);
    if (ret != CL_SUCCESS)
        return;
    if (clEnqueueWriteBuffer(command_queue, table_mem, CL_TRUE, 0, sizeof(opt_table), table, 0, NULL, NULL) != CL_SUCCESS)
        return;
    row_num_flag_array_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned int) * line_size, NULL, &ret);
    if (ret != CL_SUCCESS)
        return;
    if (clEnqueueWriteBuffer(command_queue, row_num_flag_array_mem, CL_TRUE, 0, sizeof(unsigned int) * line_size, table->row_num_flag_array, 0, NULL, NULL) != CL_SUCCESS)
        return;
    col_num_flag_array_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned int) * line_size, NULL, &ret);
    if (ret != CL_SUCCESS)
        return;
    if (clEnqueueWriteBuffer(command_queue, col_num_flag_array_mem, CL_TRUE, 0, sizeof(unsigned int) * line_size, table->col_num_flag_array, 0, NULL, NULL) != CL_SUCCESS)
        return;
    block_num_flag_array_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned int) * line_size, NULL, &ret);
    if (ret != CL_SUCCESS)
        return;
    if (clEnqueueWriteBuffer(command_queue, block_num_flag_array_mem, CL_TRUE, 0, sizeof(unsigned int) * line_size, table->block_num_flag_array, 0, NULL, NULL) != CL_SUCCESS)
        return;
    opt_num_array_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned int) * line_size * line_size, NULL, &ret);
    if (ret != CL_SUCCESS)
        return;
    ret = clEnqueueWriteBuffer(command_queue, opt_num_array_mem, CL_TRUE, 0, sizeof(unsigned int) * line_size * line_size, table->opt_num_array, 0, NULL, NULL);
    if (ret != CL_SUCCESS)
        return;
    
    table_sudoku_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(sudoku_problems), NULL, &ret);
    if (ret != CL_SUCCESS)
        return;
    if (clEnqueueWriteBuffer(command_queue, table_sudoku_mem, CL_TRUE, 0, sizeof(sudoku_problems), table->sudoku, 0, NULL, NULL) != CL_SUCCESS)
        return;
    sudoku_table_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(unsigned int) * line_size * line_size, NULL, &ret);
    if (ret != CL_SUCCESS)
        return;
    ret = clEnqueueWriteBuffer(command_queue, sudoku_table_mem, CL_TRUE, 0, sizeof(unsigned int) * line_size * line_size, table->sudoku->table, 0, NULL, NULL);
    if (ret != CL_SUCCESS)
        return;
    
    table_queue_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(priority_queue), NULL, &ret);
    if (ret != CL_SUCCESS)
        return;
    if (clEnqueueWriteBuffer(command_queue, table_queue_mem, CL_TRUE, 0, sizeof(priority_queue), table->queue, 0, NULL, NULL) != CL_SUCCESS)
        return;
    
    s_node_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(search_node) * table->queue->capacty, NULL, &ret);
    if (ret != CL_SUCCESS)
        return;
    if (clEnqueueWriteBuffer(command_queue, s_node_mem, CL_TRUE, 0, sizeof(search_node) * table->queue->capacty, table->queue->s_node, 0, NULL, NULL) != CL_SUCCESS)
        return;
    
    printf("opt_table: %x\n", table);
    printf("sudoku %x\n",table->sudoku);
    printf("table %x\n",table->sudoku->table);
    printf("row_num_flag_array %x\n",table->row_num_flag_array);
    printf("col_num_flag_array %x\n",table->col_num_flag_array);
    printf("block_num_flag_array %x\n",table->block_num_flag_array);
    printf("opt_num_array %x\n",table->opt_num_array);
    printf("queue: %x\n", table->queue);
    printf("search_node: %x\n\n", table->queue->s_node);
    
    pruning_table_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(opt_table), NULL, &ret);
    if (ret != CL_SUCCESS)
        return;
    if (clEnqueueWriteBuffer(command_queue, pruning_table_mem, CL_TRUE, 0, sizeof(opt_table), pruning_table, 0, NULL, NULL) != CL_SUCCESS)
        return;
    
    code_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int), NULL, &ret);
    if (ret != CL_SUCCESS)
        return;
    
    if (clSetKernelArg(kernel_pruning, 0, sizeof(cl_mem), (void*)&table_mem) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg0 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 1, sizeof(cl_mem), (void*)&row_num_flag_array_mem) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg1 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 2, sizeof(cl_mem), (void*)&col_num_flag_array_mem) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 3, sizeof(cl_mem), (void*)&block_num_flag_array_mem) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 4, sizeof(cl_mem), (void*)&opt_num_array_mem) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 5, sizeof(cl_mem), (void*)&table_sudoku_mem) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 6, sizeof(cl_mem), (void*)&sudoku_table_mem) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 7, sizeof(int), (void*)&(table->sudoku->blank_num)) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 8, sizeof(int), (void*)&(table->sudoku->size)) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 9, sizeof(cl_mem), (void*)&table_queue_mem) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 10, sizeof(cl_mem), (void*)&s_node_mem) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 11, sizeof(int), (void*)&(table->queue->first_empty)) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 12, sizeof(int), (void*)&(table->queue->capacty)) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 13, sizeof(cl_mem), (void*)&pruning_table_mem) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }
    if (clSetKernelArg(kernel_pruning, 14, sizeof(cl_mem), (void*)&code_mem) != CL_SUCCESS) {
        fprintf(stderr, "Failed to set arg2 in kernel.\n");
        exit(1);
    }

    ret = clEnqueueNDRangeKernel(command_queue, kernel_pruning, 2, NULL, global_work_size, local_work_size, 0, NULL, NULL);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Failed to execute kernel.\n");
        exit(1);
    }
    
    if (clEnqueueReadBuffer(command_queue, code_mem, CL_TRUE, 0, sizeof(int), &isChanged, 0, NULL, NULL) != CL_SUCCESS) {
        fprintf(stderr, "Failed to read buffer.\n");
        exit(1);
    }
    
    ret = clReleaseMemObject(table_mem);
    ret = clReleaseMemObject(pruning_table_mem);
    ret = clReleaseMemObject(code_mem);
    
}

void pruning(p_opt_table table, p_opt_table pruning_table, int *code){
    int row, col, block, pos;
    int line_size = table->sudoku->size * table->sudoku->size;
    int nums, num;
    int index;
    int isChanged = 0, isChanged1 = 0, isChanged2 = 0;
    for (col = 0; col < line_size; col++) {
        for (row = 0; row < line_size; row++) {
            pos = getPosition(row, col, line_size);
            if (table->opt_num_array[pos] != 0) {
                copy_opt_table(table, pruning_table, table->sudoku->size);
                nums = table->opt_num_array[pos];
                block = (col / table->sudoku->size) * table->sudoku->size + (row / table->sudoku->size);
                search_node_index(*(pruning_table->queue), row, col, &index);
//                if (pruning_table->queue->s_node[index].num_opts > 3 || pruning_table->queue->s_node[index].num_opts == 1) break;
                for (fetch_opt_num(nums, &num); nums != 0; nums &= ~(1 << (num - 1)), fetch_opt_num(nums, &num)) {
                    copy_opt_table(table, pruning_table, table->sudoku->size);
                    pruning_table->sudoku->blank_num--;
                    pruning_table->sudoku->table[pos] = num;
                    delete_opt_num(pruning_table, row, col, block, num - 1, index);
                    do {
                        nakedSingle(pruning_table, &isChanged2);
                        hiddenSingle(pruning_table, &isChanged1);
                    } while (isChanged1 | isChanged2);
                    if (!isSearchNodeEmpty(pruning_table->queue) && pruning_table->queue->s_node[1].num_opts == 0) {
                        isChanged = 1;
#if IS_DEBUG == 1
                        printf("PRUNING NUM = %d ROW = %d, COL = %d\n", num, row, col);
                        print_sudoku(table->sudoku);
#endif
                        search_node_index(*(table->queue), row, col, &index);
                        table->opt_num_array[pos] &= ~(1 << (num - 1));
                        bit_count(table->opt_num_array[pos], &(table->queue->s_node[index].num_opts));
                        heap_up(table->queue, index);
                        if (table->opt_num_array[pos] == 0)
                            return;
                        search_node_index(*(table->queue), row, col, &index);
                    }
                }
            }
        }
    }
    *code = isChanged;
}

void pre_processing(p_opt_table table){
    int row, col, block;
    int size = table->sudoku->size;
    int line_size = size * size;
    for (col = 0; col < line_size; col++) {
        for (row = 0; row < line_size; row++) {
            block = (int)(col / size) * size + (int)(row / size);
            if (table->sudoku->table[col * line_size + row] != 0) {
                table->block_num_flag_array[block] &= ~(1 << (table->sudoku->table[col * line_size + row] - 1));
                table->col_num_flag_array[col] &= ~(1 << (table->sudoku->table[col * line_size + row] - 1));
                table->row_num_flag_array[row] &= ~(1 << (table->sudoku->table[col * line_size + row] - 1));
            }
        }
    }
    for (col = 0; col < line_size; col++) {
        for (row = 0; row < line_size; row++) {
            block = (int)(col / size) * size + (int)(row / size);
            if (table->sudoku->table[getPosition(row, col, line_size)] == 0) {
                table->opt_num_array[getPosition(row, col, line_size)] = (table->block_num_flag_array[block] & table->col_num_flag_array[col] & table->row_num_flag_array[row]);
                int index = table->queue->first_empty;
                table->queue->first_empty++;
                table->queue->s_node[index].block = block;
                table->queue->s_node[index].row = row;
                table->queue->s_node[index].col = col;
                bit_count(table->opt_num_array[getPosition(row, col, line_size)], &(table->queue->s_node[index].num_opts));
                table->queue->s_node[index].isApplied = 0;
                heap_up(table->queue, index);
            } else {
                table->opt_num_array[getPosition(row, col, line_size)] = 0;
            }
        }
    }
}

void refine_table(p_opt_table table, p_opt_table pruning_table, int *code) {
    int isChanged = 0, isSettled1 = 0, isSettled2 = 0;
    int flag_HS = 0, flag_NS = 0, flag_LCT1 = 0, flag_NT = 0, flag_HP = 0, flag_NP = 0, flag_P = 0;
    do {
        flag_HS = 0; flag_NS = 0; flag_LCT1 = 0;
        flag_NT = 0; flag_HP = 0; flag_NP = 0; flag_P = 0;
        do {
            hiddenSingle(table, &isSettled1);
            flag_HS |= isSettled1;
            nakedSingle(table, &isSettled2);
            flag_NS |= isSettled2;
            isChanged |= (flag_HS | flag_NS);
        } while (isSettled1 | isSettled2);
        if (table->queue->s_node[1].num_opts == 0 && !isSearchNodeEmpty(table->queue)) {
            *code = -1;
            return;
        }
        if (table->sudoku->blank_num == 0) {
            *code = 2;
            return;
        }
        
        lockedCandidateType1(table, &flag_LCT1);
        if (flag_LCT1) {
            do {
                hiddenSingle(table, &isSettled1);
                flag_HS |= isSettled1;
                nakedSingle(table, &isSettled2);
                flag_NS |= isSettled2;
                isChanged |= (flag_HS | flag_NS);
            } while (isSettled1 | isSettled2);
            isChanged |= flag_LCT1;
            if (table->queue->s_node[1].num_opts == 0 && !isSearchNodeEmpty(table->queue)) {
                *code = -1;
                return;
            }
            if (table->sudoku->blank_num == 0) {
                *code = 2;
                return;
            }
        }

        do {
            nakedPair(table, &flag_NP);
            do {
                hiddenSingle(table, &isSettled1);
                flag_HS |= isSettled1;
                nakedSingle(table, &isSettled2);
                flag_NS |= isSettled2;
                isChanged |= (flag_HS | flag_NS);
            } while (isSettled1 | isSettled2);
            isChanged |= flag_NP;
            if (table->queue->s_node[1].num_opts == 0 && !isSearchNodeEmpty(table->queue)) {
                *code = -1;
                return;
            }
            if (table->sudoku->blank_num == 0) {
                *code = 2;
                return;
            }
        } while(flag_NP);
        
        hiddenPair(table, &flag_HP);
        if (flag_HP) {
            do {
                hiddenSingle(table, &isSettled1);
                flag_HS |= isSettled1;
                nakedSingle(table, &isSettled2);
                flag_NS |= isSettled2;
                isChanged |= (flag_HS | flag_NS);
            } while (isSettled1 | isSettled2);
            isChanged |= flag_NP;
            if (table->queue->s_node[1].num_opts == 0  && !isSearchNodeEmpty(table->queue)) {
                *code = -1;
                return;
            }
            if (table->sudoku->blank_num == 0) {
                *code = 2;
                return;
            }
        }
        
        do {
            nakedTriple(table, &flag_NT);
            do {
                hiddenSingle(table, &isSettled1);
                flag_HS |= isSettled1;
                nakedSingle(table, &isSettled2);
                flag_NS |= isSettled2;
                isChanged |= (flag_HS | flag_NS);
            } while (isSettled1 | isSettled2);
            isChanged |= flag_NT;
            if (table->queue->s_node[1].num_opts == 0 && !isSearchNodeEmpty(table->queue)) {
                *code = -1;
                return;
            }
            if (table->sudoku->blank_num == 0) {
                *code = 2;
                return;
            }
        } while(flag_NT);
        
        do {
#if IS_DEBUG == 1
            print_queue(*(table->queue), 0, 1);
            printf("START PRUNING\n");
            printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
//            pruning(table, pruning_table, &flag_P);
            flag_P = pruning_launch(table, pruning_table);
            printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
            printf("END PRUNING\n");
            print_queue(*(table->queue), 0, 1);
#else
//            pruning(table, pruning_table, &flag_P);
            pruning_launch(table, pruning_table, &flag_P);
#endif
            if (table->queue->s_node[1].num_opts == 0 && !isSearchNodeEmpty(table->queue)) {
                *code = -1;
                return;
            }
            if (flag_P) {
                do {
                    hiddenSingle(table, &isSettled1);
                    flag_HS |= isSettled1;
                    nakedSingle(table, &isSettled2);
                    flag_NS |= isSettled2;
                    isChanged |= (flag_HS | flag_NS);
                } while (isSettled1 | isSettled2);
            }
            isChanged |= flag_P;
            if (table->queue->s_node[1].num_opts == 0 && !isSearchNodeEmpty(table->queue)) {
                *code = -1;
                return;
            }
            if (table->sudoku->blank_num == 0) {
                *code = 2;
                return;
            }
        } while(flag_P);
        
    } while (flag_HS | flag_NS | flag_LCT1 | flag_NP | flag_HP | flag_NT | flag_P);
    
    if (table->queue->s_node[1].num_opts == 0 && !isSearchNodeEmpty(table->queue)) {
        *code = -1;
        return;
    }
    
    *code = isChanged;
    return;
}

void back_tracking(p_opt_table *tables, p_sudoku_problems sudoku, p_opt_table pruning_table){
    int row, col, block, position, nums, num;
    int size = tables[0]->sudoku->size;
    int line_size = size * size;
    int solution_count = 0;
    int brunch_cnt = 0;
    int code = 0;
    p_opt_table *p_table = tables;
//    p_opt_table pruning_table = init_opt_table(sudoku);
    while (p_table >= tables) {
        refine_table(p_table[0], pruning_table, &code);
        if (code == -1) {
            p_table--;
            continue;
        } else if (code == 2) {
            solution_count++;
            if (solution_count == 1) {
                copy_sudoku_problem(p_table[0]->sudoku, sudoku);
            }
            p_table--;
            continue;
        } else {
            copy_opt_table(p_table[0], p_table[1], size);
            row = p_table[0]->queue->s_node[1].row;
            col = p_table[0]->queue->s_node[1].col;
            block = p_table[0]->queue->s_node[1].block;
            position = col * line_size + row;
            nums = p_table[0]->opt_num_array[getPosition(row, col, line_size)];
            fetch_opt_num(nums, &num);
            p_table[0]->opt_num_array[getPosition(row, col, line_size)] &= ~num;
            p_table[0]->queue->s_node[1].num_opts--;
            p_table[1]->sudoku->blank_num--;
            p_table[1]->sudoku->table[position] = num;
#if IS_DEBUG == 1
            printf("SET NUM = %d ROW = %d, COL = %d\n", num, row, col);
#endif
            delete_opt_num(p_table[1], row, col, block, num - 1, 1);
            update_opt_table(p_table[0], row, col, block, num - 1, 1);
            brunch_cnt++;
            p_table++;
            continue;
        }
    }
#if IS_DEBUG == 1 || IS_DEBUG == 2
    printf("branch = %d\n", brunch_cnt);
    printf("solution count = %d\n", solution_count);
#endif
}

p_sudoku_problems solve_sudoku(p_sudoku_problems sudoku){
    int i;
    int size = sudoku->size;
    int max_stack_size = size * size * size * size;
    p_opt_table *p_tables = (p_opt_table*)malloc(sizeof(p_opt_table) * (max_stack_size + 2));
    p_opt_table pruning_table = init_opt_table(sudoku);
    for (i = 0; i < max_stack_size + 2; i++) {
        p_tables[i] = init_opt_table(sudoku);
        if (i != 0) {
            p_tables[i]->sudoku = new_sudoku(size);
        }
    }
    pre_processing(p_tables[0]);
    copy_opt_table(p_tables[0], pruning_table, size);
    copy_sudoku_problem(p_tables[0]->sudoku, sudoku);
//#if MULTI == 0
//    back_tracking(p_tables, sudoku, pruning_table);
//#else
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret;
    
    FILE *fp;
    const char fileName[] = "./solver.cl";
    size_t source_size;
    char *source_str;
    
    /* カーネルコードの読み込み */
    fp = fopen(fileName, "r");
    if (!fp) {
        fprintf(stderr, "Failed to load kernel.\n");
        exit(1);
    }
    source_str = (char *)malloc(MAX_SOURCE_SIZE);
    source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
    fclose(fp);
    /* Obtain platform device information */
    ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    if (ret != CL_SUCCESS)
        return NULL;
    ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &ret_num_devices);
    if (ret != CL_SUCCESS)
        return NULL;
    
    /* コンテクストの作成 */
    context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
    if (ret != CL_SUCCESS)
        return NULL;
    
    /* コマンドキューの作成 */
    command_queue = clCreateCommandQueue(context, device_id, 0, &ret);
    if (ret != CL_SUCCESS)
        return NULL;
    
    /* プログラムのビルド */
    program = clCreateProgramWithSource(context, 1, (const char **)&source_str, (const size_t *)&source_size, &ret);
    if (ret != CL_SUCCESS){
        fprintf(stderr, "Failed to build program.\n");
        exit(1);
    }
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    if (ret != CL_SUCCESS){
        fprintf(stderr, "Failed to build program.\n");
        exit(1);
    }
    kernel_pruning = clCreateKernel(program, "pruning_setup", &ret);
    if (ret != CL_SUCCESS){
        fprintf(stderr, "Failed to create kernel.\n");
        exit(1);
    }
    
    back_tracking(p_tables, sudoku, pruning_table);

//    table_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(opt_table) * (max_stack_size + 2), NULL, &ret);
//    if (ret != CL_SUCCESS)
//        return NULL;
//    if (clEnqueueWriteBuffer(command_queue, table_mem, CL_TRUE, 0, sizeof(opt_table) * (max_stack_size + 2), p_tables, 0, NULL, NULL) != CL_SUCCESS)
//        return NULL;
//    sudoku_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(sudoku_problems), NULL, &ret);
//    if (ret != CL_SUCCESS)
//        return NULL;
//    if (clEnqueueWriteBuffer(command_queue, sudoku_mem, CL_TRUE, 0, sizeof(sudoku_problems), sudoku, 0, NULL, NULL) != CL_SUCCESS)
//        return NULL;
//    pruning_table_mem = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(opt_table), NULL, &ret);
//    if (ret != CL_SUCCESS)
//        return NULL;
//    if (clEnqueueWriteBuffer(command_queue, pruning_table_mem, CL_TRUE, 0, sizeof(opt_table), pruning_table, 0, NULL, NULL) != CL_SUCCESS)
//        return NULL;
    
    /* Set OpenCL kernel arguments */
//    if (clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&table_mem) != CL_SUCCESS) {
//        fprintf(stderr, "Failed to set arg0 in kernel.\n");
//        exit(1);
//    }
//    if (clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&sudoku_mem) != CL_SUCCESS) {
//        fprintf(stderr, "Failed to set arg1 in kernel.\n");
//        exit(1);
//    }
//    if (clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&pruning_table_mem) != CL_SUCCESS) {
//        fprintf(stderr, "Failed to set arg2 in kernel.\n");
//        exit(1);
//    }
//    size_t global_work_size[] = { 1 , 1 , 1 };
//    size_t local_work_size[] = { 1 , 1 , 1 };
    size_t global_work_size[] = { size * size, size * size, size * size };
    size_t local_work_size[] = { size * size, size * size, 1 };
    
    /* Execute OpenCL kernel */
//    ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, global_work_size, local_work_size, 0, NULL, &evt_solved);
//    if (ret != CL_SUCCESS) {
//        fprintf(stderr, "Failed to execute kernel.\n");
//        exit(1);
//    }
//
//    if (clEnqueueTask(command_queue, kernel, 0, NULL, &evt_solved)) {
//        fprintf(stderr, "Failed to execute kernel.\n");
//        exit(1);
//    }
    /* Obtain result from memory buffer */
//    if (clEnqueueReadBuffer(command_queue, sudoku_mem, CL_TRUE, 0, sizeof(sudoku_problems), sudoku, 0, NULL, NULL) != CL_SUCCESS) {
//        fprintf(stderr, "Failed to read buffer.\n");
//        exit(1);
//    }
    
    /* Finish process */
    ret = clFlush(command_queue);
    ret = clFinish(command_queue);
    ret = clReleaseKernel(kernel_pruning);
    ret = clReleaseProgram(program);
    ret = clReleaseCommandQueue(command_queue);
    ret = clReleaseContext(context);
//    ret = clReleaseMemObject(table_mem);
//    ret = clReleaseMemObject(sudoku_mem);
    
    free(source_str);

    for (i = 0; i < max_stack_size + 2; i++) {
        free_opt_table(p_tables[i]);
    }

    return sudoku;
}
