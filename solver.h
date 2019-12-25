////
////  solver.h
////  sotuken
////
////  Created by 石崎駿 on 2019/09/10.
////  Copyright © 2019年 石崎駿. All rights reserved.
////
//
//#ifndef SOLVER_H
//#define SOLVER_H
//
//#include "sudoku_utility.h"
//
//typedef struct __search_node__
//{
//    int col;
//    int row;
//} search_node, *p_search_node;
//
//typedef struct __opt_table_struct__
//{
//    unsigned int *row_num_flag_array;
//    unsigned int *col_num_flag_array;
//    unsigned int *block_num_flag_array;
//    unsigned int **opt_num_array;
//} opt_table, *p_opt_table;
//
//p_sudoku_problems solve_sudoku(p_sudoku_problems sudoku);
//
//p_opt_table pre_processing(p_sudoku_problems sudoku, p_opt_table table);
//
//
//
//#endif
//

#ifndef SOLVER_H
#define SOLVER_H

#include "sudoku_utility.h"
#include "priority_queue_heap.h"

typedef struct __opt_table_struct__{
    p_sudoku_problems sudoku;
    unsigned int *row_num_flag_array;
    unsigned int *col_num_flag_array;
    unsigned int *block_num_flag_array;
    unsigned int *opt_num_array;
    p_priority_queue queue;
} opt_table, *p_opt_table;

enum {
    N_PAIR = 0,
    H_PAIR = 1,
    TRIPLE = 2,
    LOCK_C_1 = 3
};

p_sudoku_problems solve_sudoku(p_sudoku_problems sudoku);

//int pre_processing(p_opt_table table, int isEmpty, p_opt_table pruning_table);
void pre_processing(p_opt_table table);

#endif
