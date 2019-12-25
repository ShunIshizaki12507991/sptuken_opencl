#define IS_DEBUG 0
#pragma OPENCL EXTENSION cl_intel_printf : enable

typedef struct __sudoku_problems__{
    int size;
    uint *table;
    int blank_num;
} sudoku_problems;
typedef __global sudoku_problems *pg_sudoku_problems;

typedef struct __search_node__{
    int col;
    int row;
    int block;
    int num_opts;
    int isApplied;
} search_node, *p_search_node;

typedef struct __priority_queue__{
    p_search_node s_node;
    int capacty;
    int first_empty;
} priority_queue;
typedef __global priority_queue *pg_priority_queue;

#define INF (0x7fffffff)

void swap_node(search_node *a, search_node *b)
{
    int swap;
    swap = a->row;
    a->row = b->row;
    b->row = swap;
    
    swap = a->col;
    a->col = b->col;
    b->col = swap;
    
    swap = a->block;
    a->block = b->block;
    b->block = swap;
    
    swap = a->num_opts;
    a->num_opts = b->num_opts;
    b->num_opts = swap;
}

void heap_down(__global priority_queue *queue, int start){
    int child_index1, child_index2;
    child_index1 = start << 1;
    child_index2 = (start << 1) + 1;
    
    if (child_index1 > queue->first_empty) {
        return;
    }
    queue->s_node[queue->first_empty].num_opts = INF;
    while (child_index2 < queue->first_empty) {
        if (queue->s_node[child_index1].num_opts < queue->s_node[child_index2].num_opts) {
            swap_node(&(queue->s_node[child_index1]), &(queue->s_node[start]));
            start = child_index1;
        } else {
            swap_node(&(queue->s_node[child_index2]), &(queue->s_node[start]));
            start = child_index2;
        }
        child_index1 = start << 1;
        child_index2 = (start << 1) + 1;
        if (child_index1 > queue->first_empty) {
            return;
        }
    }
}

void heap_up(__global priority_queue *queue, int start){
    int parent_index = start >> 1;
    
    while (start > 1 && queue->s_node[parent_index].num_opts >= queue->s_node[start].num_opts) {
        if (queue->s_node[parent_index].num_opts > queue->s_node[start].num_opts) {
            swap_node(&(queue->s_node[start]), &(queue->s_node[parent_index]));
        }
        start = parent_index;
        parent_index = start >> 1;
    }
}

void enqueue(__global priority_queue *queue, search_node node){
    if (queue->capacty < queue->first_empty) {
        return;
    }
    
    int index = queue->first_empty;
    queue->first_empty++;
    queue->s_node[index] = node;
    heap_up(queue, index);
}

void dequeue(__global priority_queue *queue, search_node *s_node){
    if (queue->first_empty <= -1) {
        return;
    }
    *s_node = queue->s_node[1];
}

void remove_queue_elem(__global priority_queue *queue, int index){
    if (queue->first_empty <= -1) {
        return;
    }
    queue->first_empty--;
    int last_index = queue->first_empty;
    queue->s_node[index] = queue->s_node[last_index];
    queue->s_node[last_index].row = -1;
    queue->s_node[last_index].col = -1;
    queue->s_node[last_index].block = -1;
    queue->s_node[last_index].num_opts = 0;
    queue->s_node[last_index].isApplied = 0;
    heap_down(queue, index);
}

void search_node_index(priority_queue queue, int row, int col, int *node_index) {
    int i;
    for (i = 0; i < queue.first_empty; i++) {
        if (queue.s_node[i].row == row && queue.s_node[i].col == col) {
            *node_index = i;
            return;
        }
    }
    *node_index = -1;
}

typedef struct __opt_table_struct__{
    pg_sudoku_problems sudoku;
    uint *row_num_flag_array;
    uint *col_num_flag_array;
    uint *block_num_flag_array;
    uint *opt_num_array;
    pg_priority_queue queue;
} opt_table;
typedef __global opt_table *pg_opt_table;

enum {
    N_PAIR = 0,
    H_PAIR = 1,
    TRIPLE = 2,
    LOCK_C_1 = 3
};

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
    n = n - ((n >> 1) & 0x55555555);
    n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
    n = (n + (n >> 4)) & 0x0f0f0f0f;
    n = n + (n >> 8);
    n = n + (n >> 16);
    *cnt = n & 0x3f;
    //    *cnt = popcount(n);
    return;
}

void fetch_opt_num(uint opt_nums, int *num) {
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

void is_against_rule(__global opt_table *table, int row, int col, int block, int opt_num, int *isAgainst){
    int size = table->sudoku->size;
    int line_size = size * size;
    int i_row = 0, i_col = 0;
    int bit_cnt;
    for (i_row = 0; i_row < line_size; i_row++) {
        if (table->sudoku->table[col * line_size + i_row] == opt_num) {
            *isAgainst = 1;
            return;
        }
        if (table->opt_num_array[col * line_size + i_row] != 0) {
            bit_count(table->opt_num_array[col * line_size + i_row] & ~(1 << (opt_num - 1)), &bit_cnt);
            if (bit_cnt == 0) {
                *isAgainst = 1;
                return;
            }
        }
    }
    
    for (i_col = 0; i_col < line_size; i_col++) {
        if (table->sudoku->table[i_col * line_size + row] == opt_num) {
            *isAgainst = 1;
            return;
        }
        if (table->opt_num_array[i_col * line_size + row] != 0) {
            bit_count(table->opt_num_array[i_col * line_size + row] & ~(1 << (opt_num - 1)), &bit_cnt);
            if (bit_cnt == 0) {
                *isAgainst = 1;
                return;
            }
        }
    }
    
    int topRow = (block % size) * size;
    int topCol = (int)(block / size) * size;
    for (i_col = 0; i_col < size; i_col++) {
        for (i_row = 0; i_row < size; i_row++) {
            if (table->sudoku->table[(i_col + topCol) * line_size + (i_row + topRow)] == opt_num) {
                *isAgainst = 1;
                return;
            }
            if (table->opt_num_array[(i_col + topCol) * line_size + i_row + topRow] != 0) {
                bit_count(table->opt_num_array[(i_col + topCol) * line_size + i_row + topRow] & ~(1 << (opt_num - 1)), &bit_cnt);
                if (bit_cnt == 0) {
                    *isAgainst = 1;
                    return;
                }
            }
        }
    }
    *isAgainst = 0;
    return;
}

void delete_opt_num(__global opt_table *table, int row, int col, int block, int target, int node_index) {
    int size = table->sudoku->size;
    int line_size = size * size;
    remove_queue_elem(table->queue, node_index);
    table->block_num_flag_array[block] &= ~(1 << target);
    table->col_num_flag_array[col] &= ~(1 << target);
    table->row_num_flag_array[row] &= ~(1 << target);
    table->opt_num_array[col * line_size + row] = 0;
    int i_row, i_col;
    int opt_nums, index;
    
    for (i_row = 0; i_row < line_size; i_row++) {
        if (table->sudoku->table[col * line_size + i_row] == 0) {
            search_node_index(*(table->queue), i_row, col, &index);
            table->opt_num_array[col * line_size + i_row] &= ~(1 << target);
            if (index != -1) {
                bit_count(table->opt_num_array[col * line_size + i_row], &opt_nums);
                table->queue->s_node[index].num_opts = opt_nums;
                heap_up(table->queue, index);
            }
        }
    }
    
    for (i_col = 0; i_col < line_size; i_col++) {
        if (table->sudoku->table[i_col * line_size + row] == 0) {
            search_node_index(*(table->queue), row, i_col, &index);
            table->opt_num_array[i_col * line_size + row] &= ~(1 << target);
            if (index != -1) {
                bit_count(table->opt_num_array[i_col * line_size + row], &opt_nums);
                table->queue->s_node[index].num_opts = opt_nums;
                heap_up(table->queue, index);
            }
        }
    }
    
    int topRow = (block % size) * size;
    int topCol = (block / size) * size;
    for (i_col = 0; i_col < size; i_col++) {
        for (i_row = 0; i_row < size; i_row++) {
            if (table->sudoku->table[(topCol + i_col) * line_size + (topRow + i_row)] == 0 && (topRow + i_row != row || topCol + i_col != col)
                && (table->opt_num_array[(topCol + i_col) * line_size + topRow + i_row] & (1 << target))) {
                search_node_index(*(table->queue), topRow + i_row, topCol + i_col, &index);
                table->opt_num_array[(topCol + i_col) * line_size + topRow + i_row] &= ~(1 << target);
                if (index != -1) {
                    bit_count(table->opt_num_array[(topCol + i_col) * line_size + topRow + i_row], &opt_nums);
                    table->queue->s_node[index].num_opts = opt_nums;
                    heap_up(table->queue, index);
                }
            }
        }
    }
}

void update_opt_table(__global opt_table *table, int row, int col, int block, int target, int node_index) {
    int line_size = table->sudoku->size * table->sudoku->size;
    table->block_num_flag_array[block] &= ~(1 << target);
    table->col_num_flag_array[col] &= ~(1 << target);
    table->row_num_flag_array[row] &= ~(1 << target);
    table->opt_num_array[col * line_size + row] &= ~(1 << target);
    bit_count(table->opt_num_array[col * line_size + row], &(table->queue->s_node[node_index].num_opts));
}

void copy_sudoku_problem(__global sudoku_problems *a, __global sudoku_problems *b){
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

void copy_opt_table(__global opt_table *a, __global opt_table *b, int size){
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
            b->opt_num_array[i * line_size + j] = a->opt_num_array[i * line_size + j];
        }
    }
    copy_sudoku_problem(a->sudoku, b->sudoku);
}

void nakedSingle(__global opt_table *table, int *code) {
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

void hiddenSingle(__global opt_table *table, int *code){
    int i;
    int i_row, i_col;
    int isChanged = 0;
    int size = table->sudoku->size;
    int line_size = size * size;
    int opt_num, index;
    for (i = 0; i < line_size; i++) {
        //row
        int q0 = 0, q1 = 0;
        for (i_row = 0; i_row < line_size; i_row++) {
            if (table->opt_num_array[i * line_size + i_row] != 0) {
                int tmp_q1 = q1;
                q1 |= (q0 & table->opt_num_array[i * line_size + i_row]);
                q0 = (q0 & ~table->opt_num_array[i * line_size + i_row]) | (table->opt_num_array[i * line_size + i_row] & ~tmp_q1 & ~q0);
            }
        }
        int res = q0 & ~q1;
        if (res != 0) {
            for (i_row = 0; i_row < line_size; i_row++) {
                if (table->opt_num_array[i * line_size + i_row] != 0 && (table->opt_num_array[i * line_size + i_row] & res) == res) {
                    isChanged = 1;
                    fetch_opt_num(res & table->opt_num_array[i * line_size + i_row], &opt_num);
                    table->sudoku->table[i * line_size + i_row] = opt_num;
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
            if (table->opt_num_array[i_col * line_size + i] != 0) {
                int tmp_q1 = q1;
                q1 |= (q0 & table->opt_num_array[i_col * line_size + i]);
                q0 = (q0 & ~table->opt_num_array[i_col * line_size + i]) | (table->opt_num_array[i_col * line_size + i] & ~tmp_q1 & ~q0);
            }
        }
        res = q0 & ~q1;
        if (res != 0) {
            for (i_col = 0; i_col < line_size; i_col++) {
                if (table->opt_num_array[i_col * line_size + i] != 0 && (table->opt_num_array[i_col * line_size + i] & res) == res) {
                    isChanged = 1;
                    fetch_opt_num(res & table->opt_num_array[i_col * line_size + i], &opt_num);
                    table->sudoku->table[i_col * line_size + i] = opt_num;
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
                if (table->opt_num_array[col * line_size + row] != 0) {
                    int tmp_q1 = q1;
                    q1 |= (q0 & table->opt_num_array[col * line_size + row]);
                    q0 = (q0 & ~table->opt_num_array[col * line_size + row]) | (table->opt_num_array[col * line_size + row] & ~tmp_q1 & ~q0);
                }
            }
        }
        res = q0 & ~q1;
        if (res != 0) {
            for (i_col = 0; i_col < size; i_col++) {
                for (i_row = 0; i_row < size; i_row++) {
                    int col = topCol + i_col, row = topRow + i_row;
                    if (table->opt_num_array[col * line_size + row] != 0 && (table->opt_num_array[col * line_size + row] & res) == res) {
                        isChanged = 1;
                        fetch_opt_num(res & table->opt_num_array[col * line_size + row], &opt_num);
                        table->sudoku->table[col * line_size + row] = opt_num;
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

void nakedPair(__global opt_table *table, int *code) {
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
                    if (i_col != col && table->opt_num_array[i_col * block_size + row] == table->opt_num_array[col * block_size + row]) {
                        col_found = i_col;
                        search_node_index(*(table->queue), row, i_col, &node_index);
                        table->queue->s_node[node_index].isApplied = (1 << N_PAIR);
                        mask = table->opt_num_array[i_col * block_size + row];
                        table->queue->s_node[i].isApplied |= (1 << N_PAIR);
#if IS_DEBUG == 1
                        printf("NAKED PAIR = (%d, %d) (%d, %d)\n", row, col, row, col_found);
#endif
                        break;
                    }
                }
                if (col_found != -1 && mask != 0) {
                    for (i_col = 0; i_col < block_size; i_col++) {
                        if (i_col != col_found && i_col != col && table->opt_num_array[i_col * block_size + row] != 0) {
                            isChanged = 1;
                            search_node_index(*(table->queue), row, i_col, &node_index);
                            table->opt_num_array[i_col * block_size + row] &= ~mask;
                            bit_count(table->opt_num_array[i_col * block_size + row], &(table->queue->s_node[node_index].num_opts));
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
                    if (i_row != row && table->opt_num_array[col * block_size + i_row] == table->opt_num_array[col * block_size + row]) {
                        row_found = i_row;
                        search_node_index(*(table->queue), i_row, col, &node_index);
                        table->queue->s_node[node_index].isApplied = (1 << N_PAIR);
                        mask = table->opt_num_array[col * block_size + i_row];
                        table->queue->s_node[i].isApplied |= (1 << N_PAIR);
#if IS_DEBUG == 1
                        printf("NAKED PAIR = (%d, %d) (%d, %d)\n", row, col, row_found, col);
#endif
                        break;
                    }
                }
                if (row_found != -1 && mask != 0) {
                    for (i_row = 0; i_row < block_size; i_row++) {
                        if (i_row != row_found && i_row != row && table->opt_num_array[col * block_size + i_row] != 0) {
                            isChanged = 1;
                            search_node_index(*(table->queue), i_row, col, &node_index);
                            table->opt_num_array[col * block_size + i_row] &= ~mask;
                            bit_count(table->opt_num_array[col * block_size + i_row], &(table->queue->s_node[node_index].num_opts));
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
                            && table->opt_num_array[(topCol + i_col) * block_size + topRow + i_row] == table->opt_num_array[col * block_size + row]) {
                            block_row_found = topRow + i_row;
                            block_col_found = topCol + i_col;
                            search_node_index(*(table->queue), block_row_found, block_col_found, &node_index);
                            table->queue->s_node[node_index].isApplied = (1 << N_PAIR);
                            mask = table->opt_num_array[block_col_found * block_size + block_row_found];
                            table->queue->s_node[i].isApplied |= (1 << N_PAIR);
#if IS_DEBUG == 1
                            printf("NAKED PAIR = (%d, %d) (%d, %d)\n", row, col, block_row_found, block_col_found);
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
                            } else if (table->opt_num_array[(topCol + i_col) * block_size + topRow + i_row] != 0) {
                                isChanged = 1;
                                search_node_index(*(table->queue), topRow + i_row, topCol + i_col, &node_index);
                                table->opt_num_array[(topCol + i_col) * block_size + topRow + i_row] &= ~mask;
                                bit_count(table->opt_num_array[(topCol + i_col) * block_size + topRow + i_row], &(table->queue->s_node[node_index].num_opts));
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

void hiddenPair(__global opt_table *table, int *code){
    int i_row, i_col;
    int size = table->sudoku->size;
    int line_size = size * size;
    int i, isChanged = 0, node_index1, node_index2;
    int block_found = 0;
    int mask, bit_cnt, res;
    int q0 = 0, q1 = 0, q2 = 0;
    
    for (i = 0; i < line_size; i++) {
        block_found = 0;
        //row
        bit_count(table->col_num_flag_array[i], &bit_cnt);
        if (bit_cnt > 2) {
            q0 = 0; q1 = 0; q2 = 0;
            for (i_row = 0; i_row < line_size; i_row++) {
                int q0_tmp = q0 | table->opt_num_array[i * line_size + i_row];
                int q1_tmp = (q1 | table->opt_num_array[i * line_size + i_row]) & q0;
                q0 = q0_tmp;
                q2 = (q2 | table->opt_num_array[i * line_size + i_row]) & q1;
                q1 = q1_tmp;
            }
            res = q0 & q1 & ~q2;
            bit_count(res, &bit_cnt);
            if (bit_cnt >= 2) {
                for (i_row = 0; i_row < line_size - 1; i_row++) {
                    bit_count(table->opt_num_array[i * line_size + i_row] & res, &bit_cnt);
                    if (bit_cnt != 2)continue;
                    mask = table->opt_num_array[i * line_size + i_row] & res;
                    int j_row;
                    search_node_index(*(table->queue), i_row, i, &node_index1);
                    if (table->queue->s_node[node_index1].isApplied & (1 << H_PAIR)) continue;
                    for (j_row = i_row + 1; j_row < line_size; j_row++) {
                        if (i_row != j_row && (table->opt_num_array[i * line_size + j_row] & mask) == mask) {
                            isChanged = 1;
                            table->opt_num_array[i * line_size + i_row] = mask;
                            table->queue->s_node[node_index1].num_opts = 2;
                            table->queue->s_node[node_index1].isApplied |= (1 << H_PAIR);
                            heap_up(table->queue, node_index1);
                            search_node_index(*(table->queue), j_row, i, &node_index2);
                            table->opt_num_array[i * line_size + j_row] = mask;
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
                int q0_tmp = q0 | table->opt_num_array[i_col * line_size + i];
                int q1_tmp = (q1 | table->opt_num_array[i_col * line_size + i]) & q0;
                q0 = q0_tmp;
                q2 = (q2 | table->opt_num_array[i_col * line_size + i]) & q1;
                q1 = q1_tmp;
            }
            int res = q0 & q1 & ~q2;
            bit_count(res, &bit_cnt);
            if (bit_cnt >= 2) {
                for (i_col = 0; i_col < line_size - 1; i_col++) {
                    bit_count(table->opt_num_array[i_col * line_size + i] & res, &bit_cnt);
                    if (bit_cnt != 2)continue;
                    mask = table->opt_num_array[i_col * line_size + i] & res;
                    int j_col;
                    search_node_index(*(table->queue), i, i_col, &node_index1);
                    if (table->queue->s_node[node_index1].isApplied & (1 << H_PAIR)) continue;
                    for (j_col = i_col + 1; j_col < line_size; j_col++) {
                        if (i_col != j_col && (table->opt_num_array[j_col * line_size + i] & mask) == mask) {
                            isChanged = 1;
                            table->opt_num_array[i_col * line_size + i] = mask;
                            table->queue->s_node[node_index1].num_opts = 2;
                            table->queue->s_node[node_index1].isApplied |= (1 << H_PAIR);
                            heap_up(table->queue, node_index1);
                            table->opt_num_array[j_col * line_size + i] = mask;
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
                    int q0_tmp = q0 | table->opt_num_array[col * line_size + row];
                    int q1_tmp = (q1 | table->opt_num_array[col * line_size + row]) & q0;
                    q0 = q0_tmp;
                    q2 = (q2 | table->opt_num_array[col * line_size + row]) & q1;
                    q1 = q1_tmp;
                }
            }
            res = q0 & q1 & ~q2;
            bit_count(res, &bit_cnt);
            if (bit_cnt >= 2) {
                for (i_col = 0; i_col < size; i_col++) {
                    for (i_row = 0; i_row < size; i_row++) {
                        int col = topCol + i_col, row = topRow + i_row;
                        bit_count(table->opt_num_array[col * line_size + row] & res, &bit_cnt);
                        if (bit_cnt != 2)continue;
                        mask = table->opt_num_array[col * line_size + row] & res;
                        search_node_index(*(table->queue), row, col, &node_index1);
                        if (table->queue->s_node[node_index1].isApplied & (1 << H_PAIR)) continue;
                        int j_col, j_row;
                        for (j_col = 0; j_col < size; j_col++) {
                            for (j_row = 0; j_row < size; j_row++) {
                                if (topRow + j_row == row && topCol + j_col == col) {
                                    continue;
                                }
                                if ((table->opt_num_array[(topCol + j_col) * line_size + topRow + j_row] & mask) == mask) {
                                    isChanged = 1;
                                    block_found = 1;
                                    table->opt_num_array[col * line_size + row] = mask;
                                    table->queue->s_node[node_index1].num_opts = 2;
                                    table->queue->s_node[node_index1].isApplied |= (1 << H_PAIR);
                                    heap_up(table->queue, node_index1);
                                    search_node_index(*(table->queue), topRow + j_row, topCol + j_col, &node_index2);
                                    table->opt_num_array[(topCol + j_col) * line_size + topRow + j_row] = mask;
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

void nakedTriple(__global opt_table *table, int *code){
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
                    bit_count(table->opt_num_array[i_col1 * block_size + row] | table->opt_num_array[col * block_size + row], &bit_cnt);
                    if (i_col1 != col && table->opt_num_array[i_col1 * block_size + row] != 0 && bit_cnt == 3) {
                        mask = table->opt_num_array[i_col1 * block_size + row] | table->opt_num_array[col * block_size + row];
                        for (i_col2 = i_col1 + 1; i_col2 < block_size; i_col2++) {
                            bit_count(table->opt_num_array[i_col2 * block_size + row] | mask, &bit_cnt);
                            if (i_col2 != col && table->opt_num_array[i_col2 * block_size + row] != 0 && bit_cnt == 3) {
                                col_found1 = i_col1; col_found2 = i_col2;
                                search_node_index(*(table->queue), row, col_found1, &node_index);
                                table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                search_node_index(*(table->queue), row, col_found2, &node_index);
                                table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                mask |= table->opt_num_array[i_col2 * block_size + row];
                                table->queue->s_node[i].isApplied |= (1 << TRIPLE);
#if IS_DEBUG == 1
                                printf("NAKED TRIPLE = (%d, %d) (%d, %d) (%d, %d)\n", row, col, row, col_found1, row, col_found2);
#endif
                                break;
                            }
                        }
                        if (col_found1 != -1 && col_found2 != -1 && mask != 0) break;
                    }
                }
                if (col_found1 != -1 && col_found2 != -1 && mask != 0) {
                    for (i_col = 0; i_col < block_size; i_col++) {
                        if (table->opt_num_array[i_col * block_size + row] != 0 && i_col != col_found1 && i_col != col_found2 && i_col != col) {
                            isChanged = 1;
                            search_node_index(*(table->queue), row, i_col, &node_index);
                            table->opt_num_array[i_col * block_size + row] &= ~mask;
                            bit_count(table->opt_num_array[i_col * block_size + row], &(table->queue->s_node[node_index].num_opts));
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
                    bit_count(table->opt_num_array[col * block_size + i_row1] | table->opt_num_array[col * block_size + row], &bit_cnt);
                    if (table->opt_num_array[col * block_size + i_row1] != 0 && i_row1 != row && bit_cnt == 3) {
                        mask = table->opt_num_array[col * block_size + i_row1] | table->opt_num_array[col * block_size + row];
                        for (i_row2 = i_row1 + 1; i_row2 < block_size; i_row2++) {
                            bit_count(table->opt_num_array[col * block_size + i_row2] | mask, &bit_cnt);
                            if (table->opt_num_array[col * block_size + i_row2] != 0 && i_row2 != row && bit_cnt == 3) {
                                row_found1 = i_row1; row_found2 = i_row2;
                                search_node_index(*(table->queue), row_found1, col, &node_index);
                                table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                search_node_index(*(table->queue), row_found2, col, &node_index);
                                table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                mask |= table->opt_num_array[col * block_size + i_row2];
                                table->queue->s_node[i].isApplied |= (1 << TRIPLE);
#if IS_DEBUG == 1
                                printf("NAKED TRIPLE = (%d, %d) (%d, %d) (%d, %d)\n", row, col, row_found1, col, row_found2, col);
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
                        if (table->opt_num_array[col * block_size + i_row] != 0 && i_row != row_found1 && i_row != row_found2 && i_row != row) {
                            isChanged = 1;
                            table->opt_num_array[col * block_size + i_row] &= ~mask;
                            bit_count(table->opt_num_array[col * block_size + i_row], &(table->queue->s_node[node_index].num_opts));
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
                        bit_count(mask | table->opt_num_array[(topCol + i_col) * block_size + topRow + i_row] | table->opt_num_array[col * block_size + row], &bit_cnt);
                        search_node_index(*(table->queue), topRow + i_row, topCol + i_col, &node_index);
                        if (table->opt_num_array[(topCol + i_col) * block_size + topRow + i_row] != 0
                            && !(table->queue->s_node[node_index].isApplied & (1 << TRIPLE))
                            && (topRow + i_row != row || topCol + i_col != col) && bit_cnt == 3) {
                            if ((block_row_found1 == -1 && block_col_found1 == -1) || (block_row_found2 == -1 && block_col_found2 == -1)) {
                                if (block_row_found1 == -1 && block_col_found1 == -1) {
                                    block_row_found1 = topRow + i_row;
                                    block_col_found1 = topCol + i_col;
                                    mask = table->opt_num_array[block_col_found1 * block_size + block_row_found1] | table->opt_num_array[col * block_size + row];
                                } else if (block_row_found2 == -1 && block_col_found2 == -1) {
                                    block_row_found2 = topRow + i_row;
                                    block_col_found2 = topCol + i_col;
                                    search_node_index(*(table->queue), block_row_found1, block_col_found1, &node_index);
                                    table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                    search_node_index(*(table->queue), block_row_found2, block_col_found2, &node_index);
                                    table->queue->s_node[node_index].isApplied |= (1 << TRIPLE);
                                    mask |= table->opt_num_array[block_col_found2 * block_size + block_row_found2];
                                    table->queue->s_node[i].isApplied |= (1 << TRIPLE);
#if IS_DEBUG == 1
                                    printf("NAKED TRIPLE = (%d, %d) (%d, %d) (%d, %d)\n", row, col, block_row_found1, block_col_found1, block_row_found2, block_col_found2);
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
                            } else if (table->opt_num_array[(topCol + i_col) * block_size + topRow + i_row] != 0){
                                isChanged = 1;
                                search_node_index(*(table->queue), topRow + i_row, topCol + i_col, &node_index);
                                table->opt_num_array[(topCol + i_col) * block_size + topRow + i_row] &= ~mask;
                                bit_count(table->opt_num_array[(topCol + i_col) * block_size + topRow + i_row], &(table->queue->s_node[node_index].num_opts));
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

void lockedCandidateType1(__global opt_table *table, int *code) {
    int isChanged = 0;
    int size = table->sudoku->size;
    int line_size = size * size;
    int num;
    int topRow, topCol;
    int row, col, block, pos;
    int i_row, i_col, i_block, node_index;
    int res = 0;
    int q0, q1, q2;
    for (block = 0; block < line_size; block++) {
        topRow = (block % size) * size;
        topCol = (block / size) * size;
        
        for(col = 0; col < size; col++){
            q0 = 0; q1 = 0; q2 = 0;
            for (i_row = 0; i_row < size; i_row++) {
                pos = (topCol + col) * line_size + topRow + i_row;
                int q0_tmp = q0 | table->opt_num_array[pos];
                int q1_tmp = (q1 | table->opt_num_array[pos]) & q0;
                q0 = q0_tmp;
                q2 = (q2 | table->opt_num_array[pos]) & q1;
                q1 = q1_tmp;
            }
            
            res = q0 & q1;
            if (res != 0) {
                for (i_col = 0; i_col < size; i_col++) {
                    for (i_row = 0; i_row < size; i_row++) {
                        if (i_col == col) continue;
                        res &= ~table->opt_num_array[(topCol + i_col) * line_size + topRow + i_row];
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
                        pos = (topCol + col) * line_size + row;
                        num = table->opt_num_array[pos];
                        if (!(num & res))continue;
                        search_node_index(*(table->queue), row, topCol + col, &node_index);
                        if (table->queue->s_node[node_index].isApplied & (1 << LOCK_C_1)) {
                            isChanged = 0;
                            break;
                        }
                        isChanged = 1;
                        table->opt_num_array[pos] &= ~res;
                        bit_count(table->opt_num_array[pos], &(table->queue->s_node[node_index].num_opts));
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
                pos = (topCol + i_col) * line_size + topRow + row;
                int q0_tmp = q0 | table->opt_num_array[pos];
                int q1_tmp = (q1 | table->opt_num_array[pos]) & q0;
                q0 = q0_tmp;
                q2 = (q2 | table->opt_num_array[pos]) & q1;
                q1 = q1_tmp;
            }
            
            res = q0 & q1;
            if (res != 0) {
                for (i_col = 0; i_col < size; i_col++) {
                    for (i_row = 0; i_row < size; i_row++) {
                        if (i_row == row) continue;
                        res &= ~table->opt_num_array[(topCol + i_col) * line_size + topRow + i_row];
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
                        pos = col * line_size + topRow + row;
                        num = table->opt_num_array[pos];
                        if (!(num & res))continue;
                        search_node_index(*(table->queue), topRow + row, col, &node_index);
                        if (table->queue->s_node[node_index].isApplied & (1 << LOCK_C_1)) {
                            isChanged = 0;
                            break;
                        }
                        isChanged = 1;
                        table->opt_num_array[pos] &= ~res;
                        bit_count(table->opt_num_array[pos], &(table->queue->s_node[node_index].num_opts));
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

__kernel void pruning_setup(pg_opt_table table, __global uint *row_num_flag_array, __global uint *col_num_flag_array,
                            __global uint *block_num_flag_array, __global uint *opt_num_array,
                            __global sudoku_problems *table_sudoku, uint sudoku_table, int blank_num, int size,
                            __global priority_queue *table_queue, search_node queue_s_node, int first_empty, int capacity,
                            __global opt_table *pruning_table, __global int *code) {
    table->row_num_flag_array = row_num_flag_array;
    table->col_num_flag_array = col_num_flag_array;
    table->block_num_flag_array = block_num_flag_array;
    table->opt_num_array = opt_num_array;

//    table_sudoku->table = &sudoku_table;
    table_sudoku->blank_num = blank_num;
    table_sudoku->size = size;
    table->sudoku = table_sudoku;
//
    table_queue->capacty = capacity;
    table_queue->first_empty = first_empty;
//    table_queue->s_node = &queue_s_node;
    table->queue = table_queue;
    printf("size = %d, blank num = %d, first_empty = %d, capacity = %d\n", table->sudoku->size, table->sudoku->blank_num, table->queue->first_empty, table->queue->capacty);
    int i, j;
    int line_size = table->sudoku->size * table->sudoku->size;
    for (i = 0; i < line_size; i++) {
        for (j = 0; j < line_size; j++) {
//            printf("ROW = %d, COL = %d, NUM = %d\n", j, i, opt_num_array[i * line_size + j]);
//            printf("ROW = %d, COL = %d, NUM = %d\n", j, i, table->opt_num_array[i * line_size + j]);
            printf("ROW = %d, COL = %d, NUM = %d\n", j, i, (&sudoku_table)[i * line_size + j]);
        }
    }
    return;
    copy_opt_table(table, pruning_table, size);
}

void pruning(__global opt_table *table, __global opt_table *pruning_table, int *code){
    int row, col, block, pos;
    int line_size = table->sudoku->size * table->sudoku->size;
    int nums, num;
    int index;
    int isChanged = 0, isChanged1 = 0, isChanged2 = 0;
    col = get_global_id(0);
    row = get_global_id(1);
//    for (col = 0; col < line_size; col++) {
//        for (row = 0; row < line_size; row++) {
    pos = col * line_size + row;
//    printf("opt_table: %x\n", table);
//    printf("sudoku %x\n",table->sudoku);
//    printf("table %x\n",table->sudoku->table);
//    printf("row_num_flag_array %x\n",table->row_num_flag_array);
//    printf("col_num_flag_array %x\n",table->col_num_flag_array);
//    printf("block_num_flag_array %x\n",table->block_num_flag_array);
//    printf("opt_num_array %x\n",table->opt_num_array);
//    printf("queue: %x\n", table->queue);
//    printf("search_node: %x\n\n", table->queue->s_node);
    printf("size = %d, line_size = %d, row = %d, col = %d, pos = %d: %d\n", table->sudoku->size, line_size, row, col, pos, table->opt_num_array[pos]);
    return;
    if (table->opt_num_array[pos] != 0) {
        copy_opt_table(table, pruning_table, table->sudoku->size);
        nums = table->opt_num_array[pos];
        block = (col / table->sudoku->size) * table->sudoku->size + (row / table->sudoku->size);
        search_node_index(*(pruning_table->queue), row, col, &index);
        for (fetch_opt_num(nums, &num); nums != 0; nums &= ~(1 << (num - 1)), fetch_opt_num(nums, &num)) {
            printf("pos = %d, num = %d\n", pos, num);
            search_node_index(*(table->queue), row, col, &index);
            copy_opt_table(table, pruning_table, table->sudoku->size);
            pruning_table->sudoku->blank_num--;
            pruning_table->sudoku->table[pos] = num;
            delete_opt_num(pruning_table, table->queue->s_node[index].row, table->queue->s_node[index].col, table->queue->s_node[index].block, num - 1, index);
            do {
                nakedSingle(pruning_table, &isChanged2);
                hiddenSingle(pruning_table, &isChanged1);
            } while (isChanged1 | isChanged2);
            if (pruning_table->queue->first_empty > 1 && pruning_table->queue->s_node[1].num_opts == 0) {
                isChanged = 1;
                //#if IS_DEBUG == 1
                printf("PRUNING NUM = %d ROW = %d, COL = %d\n", num, row, col);
                //#endif
                search_node_index(*(table->queue), row, col, &index);
                table->opt_num_array[pos] &= ~(1 << (num - 1));
                barrier(CLK_GLOBAL_MEM_FENCE);
                bit_count(table->opt_num_array[pos], &(table->queue->s_node[index].num_opts));
                barrier(CLK_GLOBAL_MEM_FENCE);
                heap_up(table->queue, index);
                barrier(CLK_GLOBAL_MEM_FENCE);
                if (table->opt_num_array[pos] == 0)
                    return;
                search_node_index(*(table->queue), row, col, &index);
            }
        }
    }
//        }
//    }
    *code = isChanged;
    printf("pos = %d finished\n", pos);
}

//void pre_processing(__global opt_table *table){
//    int row, col, block;
//    int size = table->sudoku->size;
//    int line_size = size * size;
//    int bit_cnt;
//    for (col = 0; col < line_size; col++) {
//        for (row = 0; row < line_size; row++) {
//            block = (int)(col / size) * size + (int)(row / size);
//            if (table->sudoku->table[col * line_size + row] != 0) {
//                table->block_num_flag_array[block] &= ~(1 << (table->sudoku->table[col * line_size + row] - 1));
//                table->col_num_flag_array[col] &= ~(1 << (table->sudoku->table[col * line_size + row] - 1));
//                table->row_num_flag_array[row] &= ~(1 << (table->sudoku->table[col * line_size + row] - 1));
//            }
//        }
//    }
//    for (col = 0; col < line_size; col++) {
//        for (row = 0; row < line_size; row++) {
//            block = (int)(col / size) * size + (int)(row / size);
//            if (table->sudoku->table[col * line_size + row] == 0) {
//                table->opt_num_array[col * line_size + row] = (table->block_num_flag_array[block] & table->col_num_flag_array[col] & table->row_num_flag_array[row]);
//                int index = table->queue->first_empty;
//                table->queue->first_empty++;
//                table->queue->s_node[index].block = block;
//                table->queue->s_node[index].row = row;
//                table->queue->s_node[index].col = col;
//                bit_count(table->opt_num_array[col * line_size + row], &(table->queue->s_node[index].num_opts));
//                table->queue->s_node[index].isApplied = 0;
//                heap_up(table->queue, index);
//            } else {
//                table->opt_num_array[col * line_size + row] = 0;
//            }
//        }
//    }
//}

//void refine_table(__global opt_table *table,
//                  __global opt_table *pruning_table,
//                  int *code) {
//    int isChanged = 0, isSettled1 = 0, isSettled2 = 0;
//    int flag_HS = 0, flag_NS = 0, flag_LCT1 = 0, flag_NT = 0, flag_HP = 0, flag_NP = 0, flag_P = 0;
//    do {
//        flag_HS = 0; flag_NS = 0; flag_LCT1 = 0;
//        flag_NT = 0; flag_HP = 0; flag_NP = 0; flag_P = 0;
////        printf("START SINGLE\n");
//        do {
//            hiddenSingle(table, &isSettled1);
//            flag_HS |= isSettled1;
//            nakedSingle(table, &isSettled2);
//            flag_NS |= isSettled2;
//            isChanged |= (flag_HS | flag_NS);
//        } while (isSettled1 | isSettled2);
////        printf("END SINGLE\n");
//        if (table->queue->s_node[1].num_opts == 0 && !(table->queue->first_empty - 1 == 0)) {
//            *code = -1;
//            return;
//        }
//        if (table->sudoku->blank_num == 0) {
//            *code = 2;
//            return;
//        }
//
////        printf("START LCT1\n");
//        lockedCandidateType1(table, &flag_LCT1);
//        if (flag_LCT1) {
//            do {
//                hiddenSingle(table, &isSettled1);
//                flag_HS |= isSettled1;
//                nakedSingle(table, &isSettled2);
//                flag_NS |= isSettled2;
//                isChanged |= (flag_HS | flag_NS);
//            } while (isSettled1 | isSettled2);
//            isChanged |= flag_LCT1;
//            if (table->queue->s_node[1].num_opts == 0 && !(table->queue->first_empty - 1 == 0)) {
//                *code = -1;
//                return;
//            }
//            if (table->sudoku->blank_num == 0) {
//                *code = 2;
//                return;
//            }
//        }
////        printf("END LCT1\n");
//
////        printf("START NAKED PAIR\n");
//        do {
//            nakedPair(table, &flag_NP);
//            do {
//                hiddenSingle(table, &isSettled1);
//                flag_HS |= isSettled1;
//                nakedSingle(table, &isSettled2);
//                flag_NS |= isSettled2;
//                isChanged |= (flag_HS | flag_NS);
//            } while (isSettled1 | isSettled2);
//            isChanged |= flag_NP;
//            if (table->queue->s_node[1].num_opts == 0 && !(table->queue->first_empty - 1 == 0)) {
//                *code = -1;
//                return;
//            }
//            if (table->sudoku->blank_num == 0) {
//                *code = 2;
//                return;
//            }
//        } while(flag_NP);
////        printf("END NAKED PAIR\n");
//
////        printf("START HIDDEN PAIR\n");
//        hiddenPair(table, &flag_HP);
//        if (flag_HP) {
//            do {
//                hiddenSingle(table, &isSettled1);
//                flag_HS |= isSettled1;
//                nakedSingle(table, &isSettled2);
//                flag_NS |= isSettled2;
//                isChanged |= (flag_HS | flag_NS);
//            } while (isSettled1 | isSettled2);
//            isChanged |= flag_NP;
//            if (table->queue->s_node[1].num_opts == 0  && !(table->queue->first_empty - 1 == 0)) {
//                *code = -1;
//                return;
//            }
//            if (table->sudoku->blank_num == 0) {
//                *code = 2;
//                return;
//            }
//        }
////        printf("END HIDDEN PAIR\n");
//
////        printf("START NAKED TRIPLE\n");
//        do {
//            nakedTriple(table, &flag_NT);
//            do {
//                hiddenSingle(table, &isSettled1);
//                flag_HS |= isSettled1;
//                nakedSingle(table, &isSettled2);
//                flag_NS |= isSettled2;
//                isChanged |= (flag_HS | flag_NS);
//            } while (isSettled1 | isSettled2);
//            isChanged |= flag_NT;
//            if (table->queue->s_node[1].num_opts == 0 && !(table->queue->first_empty - 1 == 0)) {
//                *code = -1;
//                return;
//            }
//            if (table->sudoku->blank_num == 0) {
//                *code = 2;
//                return;
//            }
//        } while(flag_NT);
////        printf("END NAKED TRIPLE\n");
//
//        do {
//#if IS_DEBUG == 1
//            printf("START PRUNING\n");
//            printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
//            pruning(table, pruning_table, &flag_P);
//            printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
//            printf("END PRUNING\n");
//#else
//            pruning(table, pruning_table, &flag_P);
//#endif
//            if (table->queue->s_node[1].num_opts == 0 && !(table->queue->first_empty - 1 == 0)) {
//                *code = -1;
//                return;
//            }
//            if (flag_P) {
//                do {
//                    hiddenSingle(table, &isSettled1);
//                    flag_HS |= isSettled1;
//                    nakedSingle(table, &isSettled2);
//                    flag_NS |= isSettled2;
//                    isChanged |= (flag_HS | flag_NS);
//                } while (isSettled1 | isSettled2);
//            }
//            isChanged |= flag_P;
//            if (table->queue->s_node[1].num_opts == 0 && !(table->queue->first_empty - 1 == 0)) {
//                *code = -1;
//                return;
//            }
//            if (table->sudoku->blank_num == 0) {
//                *code = 2;
//                return;
//            }
//        } while(flag_P);
//
//    } while (flag_HS | flag_NS | flag_LCT1 | flag_NP | flag_HP | flag_NT | flag_P);
//
//    if (table->queue->s_node[1].num_opts == 0 && !(table->queue->first_empty - 1 == 0)) {
//        *code = -1;
//        return;
//    }
//
//    *code = isChanged;
//    return;
//}

//void back_tracking(__global pg_opt_table *tables,
//                   __global sudoku_problems *sudoku,
//                   __global opt_table *pruning_table){
//    int row, col, block, nums, num;
//    int position;
//    int size = sudoku->size;
//    int line_size = size * size;
//    int solution_count = 0;
//    int brunch_cnt = 0;
//    int code = 0;
//    __global pg_opt_table *p_table = tables;
//    while (p_table >= tables) {
//        printf("%d\n", p_table[0]->sudoku->blank_num);
//        refine_table(p_table[0], pruning_table, &code);
//        printf("%d\n", p_table[0]->sudoku->blank_num);
//        printf("code = %d\n", code);
//        if (code == -1) {
//            p_table--;
//            continue;
//        } else if (code == 2) {
//            solution_count++;
//            if (solution_count == 1) {
//                printf("%s\n", "Get Solution");
//                copy_sudoku_problem(p_table[0]->sudoku, sudoku);
//                sudoku->blank_num = 0;
//            }
//            p_table--;
//            continue;
//        } else {
//            copy_opt_table(p_table[0], p_table[1], size);
//            row = p_table[0]->queue->s_node[1].row;
//            col = p_table[0]->queue->s_node[1].col;
//            block = p_table[0]->queue->s_node[1].block;
//            position = col * line_size + row;
//            nums = p_table[0]->opt_num_array[position];
//            fetch_opt_num(nums, &num);
//            p_table[1]->sudoku->blank_num--;
//            p_table[1]->sudoku->table[position] = num;
//#if IS_DEBUG == 1
//            printf("SET NUM = %d ROW = %d, COL = %d\n", num, row, col);
//#endif
//            delete_opt_num(p_table[1], row, col, block, num - 1, 1);
//            update_opt_table(p_table[0], row, col, block, num - 1, 1);
//            brunch_cnt++;
//            p_table++;
//            continue;
//        }
//    }
//#if IS_DEBUG == 2
//    printf("branch = %d\n", brunch_cnt);
//    printf("solution = %d\n", solution_count);
//#endif
//}

