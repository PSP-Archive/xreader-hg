#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "stack.h"
#ifdef DMALLOC_H
#include "dmalloc.h"
#endif

// Discard entry if it exceeds the maximum size
#define STACK_MAX_SIZE 256

void stack_init(Stack * stack)
{
	stack->size = 0;
	stack->head.next = NULL;
}

void stack_push(Stack * stack, int value)
{
	StackEntry *p, *prev;

	if (stack->size < STACK_MAX_SIZE) {
		stack->size++;
	} else {
		// discard stack bottom entry
		p = &stack->head;
		prev = NULL;

		while (p->next != NULL) {
			prev = p;
			p = p->next;
		}

		prev->next = NULL;
		free(p);
	}

	p = (StackEntry *) malloc(sizeof(*p));

	if (p == NULL) {
		return;
	}

	p->value = value;
	p->next = stack->head.next;
	stack->head.next = p;
}

int stack_pop(Stack * stack, int *value)
{
	StackEntry *p;

	if (stack->size == 0) {
		return -1;
	}

	p = stack->head.next;
	*value = p->value;

	stack->head.next = p->next;
	free(p);

	stack->size--;

	return 0;
}

int stack_size(Stack * stack)
{
	return stack->size;
}

void stack_clear(Stack *stack)
{
	StackEntry *p;

	while(stack->size > 0) {
		p = stack->head.next;
		stack->head.next = p->next;
		stack->size--;
		free(p);
	}
}
