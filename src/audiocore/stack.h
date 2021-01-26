#ifndef STACK_H
#define STACK_H

typedef struct _StackEntry
{
	int value;
	struct _StackEntry *next;
} StackEntry;

typedef struct _Stack
{
	int size;
	struct _StackEntry head;
} Stack;

void stack_init(Stack * stack);
void stack_push(Stack * stack, int value);
int stack_pop(Stack * stack, int *value);
void stack_clear(Stack *stack);

#endif
