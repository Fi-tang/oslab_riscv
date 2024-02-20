#include<iostream>
using namespace std;

struct ListNode {
	int val;
	struct ListNode* next;
}ListNode;

struct SentienlNode {
	struct ListNode* head;
} SentienlNode;

struct SentienlNode* global_free;

void kinit() {
	global_free = (struct SentienlNode*)malloc(sizeof(struct SentienlNode));
	
	struct ListNode* prev_node = global_free->head;
	for (int i = 0; i < 10; i++) {
		struct ListNode* current_node = (struct ListNode*)malloc(sizeof(struct ListNode));
		current_node->val = i;
		current_node->next = NULL;
		if (i == 0) {
			global_free->head = current_node;
			prev_node = global_free->head;
		}
		else {
			prev_node->next = current_node;
			prev_node = current_node;
		}
	}
}

void printList(struct SentienlNode *list) {
	struct ListNode* temp = list ->head;
	while (temp != NULL) {
		cout << " " << temp->val << "-> ";
		temp = temp->next;
	}
	cout << "NULL" <<  endl;
}

void freepage(int addr) {
	struct ListNode* current_node = (struct ListNode*)malloc(sizeof(struct ListNode));
	current_node->val = addr;
	current_node->next = global_free->head;
	global_free->head = current_node;
}


void* kmalloc(int size) {  
	// 需要有一个结点进行遍历
	// 需要有另一个结点进行创建
	struct SentienlNode* malloc_free = (struct SentienlNode*)malloc(sizeof(struct SentienlNode));
	malloc_free->head = NULL;

	struct ListNode* global_temp = global_free->head;
	struct ListNode* prev_node = NULL;
	while (global_temp != NULL && size > 0) {
		if (malloc_free->head == NULL) {
			malloc_free->head = global_temp;
		}
		prev_node = global_temp;
		global_temp = global_temp->next;
		size--;
	}
	prev_node->next = NULL;
	global_free->head = global_temp;
	return malloc_free;
}


int main() {
	kinit();
	cout << "global_free: " << endl;
	printList(global_free);
	cout << "new head" << endl;
	printList((struct SentienlNode*)kmalloc(5));
	cout << "global_free: " << endl;
	printList(global_free);
}