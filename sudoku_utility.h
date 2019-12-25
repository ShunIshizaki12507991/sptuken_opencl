//
//  sudoku_utility.h
//  sotuken
//
//  Created by 石崎駿 on 2019/09/10.
//  Copyright © 2019年 石崎駿. All rights reserved.
//

#define __GCC__
#ifndef SUDOKU_UTILITY_H
#define SUDOKU_UTILITY_H

typedef struct __sudoku_problems__{
    int size;
    unsigned int *table;
    int blank_num;
} sudoku_problems, *p_sudoku_problems;

typedef struct __sudoku_problem_array__{
    int problems_count;
    p_sudoku_problems *sudoku_array;
} sudoku_problem_array, *p_sudoku_problem_array;

void free_sudoku(p_sudoku_problems s);
void print_sudoku(p_sudoku_problems b);
void print_ans(p_sudoku_problems b);
p_sudoku_problems new_sudoku(int sudoku_size);
p_sudoku_problem_array load_sudoku_from_file(char *file_name);

#endif
