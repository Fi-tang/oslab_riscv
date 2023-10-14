/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * Copyright (C) 2018 Institute of Computing
 * Technology, CAS Author : Han Shukai (email :
 * hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * Changelog: 2019-8 Reimplement queue.h.
 * Provide Linux-style doube-linked list instead of original
 * unextendable Queue implementation. Luming
 * Wang(wangluming@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * */

#ifndef INCLUDE_LIST_H_
#define INCLUDE_LIST_H_

#include <type.h>

// double-linked list
typedef struct list_node
{
    struct list_node *next, *prev;
} list_node_t;

typedef list_node_t list_head;

// LIST_HEAD is used to define the head of a list.
#define LIST_HEAD(name) struct list_node name = {&(name), &(name)}

/* TODO: [p2-task1] implement your own list API */
static inline void Initialize_QueueNode(list_head *node){
    node -> next = node;
    node -> prev = node;
}

static inline void Enque_FromTail(list_head *head, list_head *node){
    if(head -> next == head){
        head -> next = node;
        node -> next = head;
        head -> prev = node;
        node -> prev = head;
    }
    else{
        list_head *last_node = head -> next;
        while(last_node -> next != head){
            last_node = last_node -> next;
        }
        last_node -> next = node;
        node -> prev = last_node;
        node -> next = head;
        head -> prev = node;
    }
}

static inline list_head *Deque_FromHead(list_head *head){
    if(head -> next == head){
        return NULL;
    }
    else{
        list_head *deque_node = head -> next;
        if(deque_node -> next == head){
            head -> next = head;
            head -> prev = head;
        }
        else{
            head -> next = deque_node -> next;
            deque_node -> next -> prev = head;
        }
        return deque_node;
    }
}

#endif