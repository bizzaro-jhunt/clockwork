/*
  Copyright 2011 James Hunt <james@jameshunt.us>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIST_H
#define LIST_H

#include <stddef.h> /* for offsetof(3) macro */

/**
  Fast, structure-agnostic doubly-linked circular list.

  This implementation was adapted from the Linux kernel list_head
  code, in include/linux/list.h.  Mostly, I removed the register
  optimizations, and implemented most operations in terms of
  __list_splice.
 */
struct list {
	struct list *next;   /* the next node in the list */
	struct list *prev;   /* the previous node in the list */
};

/** Declare / initialize an empty list */
#define LIST(n) struct list n = { &(n), &(n) }

/** Initialize a list pointer */
#define list_init(n) ((n)->next = (n)->prev = (n))

/** Retrieve data node (type t) from list l, in member m */
#define list_node(l,t,m) ((t*)((char*)(l) - offsetof(t,m)))

/** Retrieve first data node (type t).  See list_node */
#define list_head(l,t,m) list_node((l)->next, t, m)

/** Retrieve last data node (type t).  See list_node */
#define list_tail(l,t,m) list_node((l)->prev, t, m)

/** Iterate over a list */
#define for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/** Iterate over a list (safely) */
#define for_each_safe(pos, tmp, head) \
	for (pos = (head)->next, tmp = pos->next; pos != (head); pos = tmp, tmp = pos->next)

/** Iterate over a list, accessing data nodes */
#define for_each_node(pos, head, memb) \
	for (pos = list_node((head)->next, typeof(*pos), memb); \
	     &pos->memb != (head);                              \
	     pos = list_node(pos->memb.next, typeof(*pos), memb))

/** Iterate over a list (safely), accessing data nodes */
#define for_each_node_safe(pos, tmp, head, memb) \
	for (pos = list_node((head)->next, typeof(*pos), memb),            \
	         tmp = list_node(pos->memb.next, typeof(*pos), memb);      \
	     &pos->memb != (head);                                         \
	     pos = tmp, tmp = list_node(tmp->memb.next, typeof(*tmp), memb))

/** Iterate backwards over a list */
#define for_each_r(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

/** Iterate backwards over a list (safely) */
#define for_each_safe_r(pos, tmp, head) \
	for (pos = (head)->prev, tmp = pos->prev; pos != (head); pos = tmp, tmp = pos->prev)

/** Iterate backwards over a list, accessing data nodes. */
#define for_each_node_r(pos, head, memb) \
	for (pos = list_node((head)->prev, typeof(*pos), memb); \
	     &pos->memb != (head);                              \
	     pos = list_node(pos->memb.prev, typeof(*pos), memb))

/** Iterate backwards over a list (safely), accessing data nodes. */
#define for_each_node_safe_r(pos, tmp, head, memb) \
	for (pos = list_node((head)->prev, typeof(*pos), memb),            \
	         tmp = list_node(pos->memb.prev, typeof(*pos), memb);      \
	     &pos->memb != (head);                                         \
	     pos = tmp, tmp = list_node(tmp->memb.prev, typeof(*tmp), memb))

/** Is list l empty? */
static inline int list_empty(const struct list *l)
{
	return l->next == l;
}

static inline void __list_splice(struct list *prev, struct list *next)
{
	prev->next = next;
	next->prev = prev;
}

/** add n at the head of l */
static inline void list_add_head(struct list *n, struct list *l)
{
	__list_splice(n, l->next);
	__list_splice(l, n);
}

/** add n at the tail of l */
static inline void list_add_tail(struct list *n, struct list *l)
{
	__list_splice(l->prev, n);
	__list_splice(n, l);
}

/** remove n from its list */
static inline void list_del(struct list *n)
{
	__list_splice(n->prev, n->next);
	list_init(n);
}

/* INTERNAL: replace o with n */
static inline void __list_replace(struct list *o, struct list *n)
{
	n->next = o->next;
	n->next->prev = n;
	n->prev = o->prev;
	n->prev->next = n;
}

/** Replace o with n in o's list; initialize o */
static inline void list_replace(struct list *o, struct list *n)
{
	__list_replace(o, n);
	list_init(o);
}

/** Move n to the head of l */
static inline void list_move_head(struct list *n, struct list *l)
{
	__list_splice(n->prev, n->next);
	list_add_head(n, l);
}

/** Move n to the tail of l */
static inline void list_move_tail(struct list *n, struct list *l)
{
	__list_splice(n->prev, n->next);
	list_add_tail(n, l);
}

/** Join tail onto the end of head */
static inline void list_join(struct list *head, struct list *tail)
{
	__list_splice(head->prev, tail->next);
	__list_splice(tail->prev, head);
	list_init(tail);
}

#endif
