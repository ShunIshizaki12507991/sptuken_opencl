//
//  main.c
//  sotuken
//
//  Created by 石崎駿 on 2019/09/10.
//  Copyright © 2019年 石崎駿. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "solver.h"
#include "sudoku_utility.h"
#include "solver.h"

int main(int argc, char *argv[]){
    sudoku_problem_array *problems;
    int i, j;
    int notReach = 0;
    for (i = 1; i < argc; i++) {
        problems = load_sudoku_from_file(argv[i]);
        for (j = 0; j < problems->problems_count; j++) {
            clock_t start, end;
            print_sudoku(problems->sudoku_array[j]);
            printf("\n");
            start = clock();
            problems->sudoku_array[j] = solve_sudoku(problems->sudoku_array[j]);
            if (problems->sudoku_array[j] != NULL && problems->sudoku_array[j]->blank_num == 0) {
                print_sudoku(problems->sudoku_array[j]);
                end = clock();
                printf("complete solve time: %.6f\n", (double)(end - start) / CLOCKS_PER_SEC);
            } else {
                notReach++;
            }
        }
    }
    if (notReach != 0)printf("%d problems cannot solved\n", notReach);
    return 0;
}
