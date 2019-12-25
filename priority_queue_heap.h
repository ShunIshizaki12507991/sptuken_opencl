//
//  priority_queue_heap.h
//  sotuken
//
//  Created by 石崎駿 on 2019/10/08.
//  Copyright © 2019年 石崎駿. All rights reserved.
//

#ifndef priority_queue_heap_h
#define priority_queue_heap_h

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
} priority_queue, *p_priority_queue;

p_priority_queue queue_create(int capacity);
void free_queue(p_priority_queue queue);
void swap_node(p_search_node a, p_search_node b);
int isSearchNodeEmpty(p_priority_queue queue);
void enqueue(p_priority_queue queue, search_node s_node);
void dequeue(p_priority_queue queue, p_search_node s_node);
void remove_queue_elem(p_priority_queue queue, int index);
void print_queue(priority_queue queue, int index, int depth);
void heap_down(p_priority_queue queue, int start);
void heap_up(p_priority_queue queue, int start);
//int search_node_index(priority_queue queue, int row, int col);
void search_node_index(priority_queue queue, int row, int col, int *node_index);

#endif /* priority_queue_heap_h */
