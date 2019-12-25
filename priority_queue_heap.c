//
//  priority_queue_heap.c
//  sotuken
//
//  Created by 石崎駿 on 2019/10/08.
//  Copyright © 2019年 石崎駿. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include "priority_queue_heap.h"

#define INF (0x7fffffff)
#define M_INF (0xffffffff)

void free_queue(p_priority_queue queue){
//    int i;
//    for (i = 0; i < queue->first_empty; i++) {
        free(queue->s_node);
//    }
//    free(queue);
}

p_priority_queue queue_create(int capacity){
    p_priority_queue ret = (p_priority_queue)malloc(sizeof(priority_queue));
    ret->s_node = (p_search_node)malloc(sizeof(search_node) * (capacity + 1));
    ret->capacty = capacity;
    ret->first_empty = 1;
    int i = 0;
    for (; i < capacity + 1; i++) {
        ret->s_node[i].row = -1;
        ret->s_node[i].col = -1;
        ret->s_node[i].block = -1;
        ret->s_node[i].num_opts = 0;
        ret->s_node[i].isApplied = 0;
//        ret->s_node[i].sudokuEntropy = 1.0;
    }
    return ret;
}

void swap_node(p_search_node a, p_search_node b) {
    int swap;
//    double d_swap;
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
    
    swap = a->isApplied;
    a->isApplied = b->isApplied;
    b->isApplied = swap;
    
}

int isSearchNodeEmpty(p_priority_queue queue) {
    if (queue->first_empty - 1 == 0) {
        return 1;
    }
    return 0;
}

void enqueue(p_priority_queue queue, search_node node){
    if (queue->capacty < queue->first_empty) {
        return;
    }
    
    int index = queue->first_empty;
    queue->first_empty++;
//    queue->s_node[index] = node;
    heap_up(queue, index);
}

void dequeue(p_priority_queue queue, p_search_node s_node){
    if (queue->first_empty <= -1) {
        return;
    }
    *s_node = queue->s_node[1];
//    remove_queue_elem(queue, 1);
//    int last_index = queue->first_empty - 1;
//    queue->s_node[1] = queue->s_node[last_index];
//    queue->s_node[last_index].row = -1;
//    queue->s_node[last_index].col = -1;
//    queue->s_node[last_index].block = -1;
//    queue->s_node[last_index].num_opts = 0;
//    queue->s_node[last_index].isApplied = 0;
//    heap_down(queue, 1);
}

void remove_queue_elem(p_priority_queue queue, int index){
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
//    queue->s_node[last_index].sudokuEntropy = 0.0;
    heap_down(queue, index);
}

void print_queue(priority_queue queue, int index, int depth){
//    printf("node[%2d] = row: %2d, col: %2d, block: %2d, num of opts: %2d\n",
//           index, queue->s_node[index].row, queue->s_node[index].col, queue->s_node[index].block, queue->s_node[index].num_opts);
    
    int i = 0;
    for (; i < queue.first_empty; i++) {
        printf("node[%3d] = (row: %2d, col: %2d, block: %2d, num of opts: %2d, flag: %d)\n",
               i, queue.s_node[i].row, queue.s_node[i].col, queue.s_node[i].block, queue.s_node[i].num_opts, queue.s_node[i].isApplied);
    }
//    printf("node[%3d] = (row: %2d, col: %2d, block: %2d, num of opts: %2d)\n",
//           1, queue.s_node[1].row, queue.s_node[1].col, queue.s_node[1].block, queue.s_node[1].num_opts);
    printf("\n");
    
//    int i;
//    for( i = 1; i < depth; ++i ){
//        printf( "    " );
//    }
//    if( depth > 0 ){
//        printf( "+---" );
//    }
//    printf("%2d: %2d\n", index, queue->s_node[index].num_opts);

//    index *= 2;
//    if (index < queue->first_empty) {
//        print_queue(queue, index, depth++);
//    }
//
//    index++;
//    if (index < queue->first_empty) {
//        print_queue(queue, index, depth++);
//    }
    
}

void heap_down(p_priority_queue queue, int start){
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

void heap_up(p_priority_queue queue, int start){
    int parent_index = start >> 1;
    
    while (start > 1 && queue->s_node[parent_index].num_opts >= queue->s_node[start].num_opts) {
        if (queue->s_node[parent_index].num_opts > queue->s_node[start].num_opts) {
            swap_node(&(queue->s_node[start]), &(queue->s_node[parent_index]));
        }
        start = parent_index;
        parent_index = start >> 1;
    }
//    while (queue->s_node[parent_index].opt_num > queue->s_node[start].opt_num) {
//        swap_node(&(queue->s_node[start]), &(queue->s_node[parent_index]));
//        start = parent_index;
//        parent_index = start >> 1;
//    }
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
