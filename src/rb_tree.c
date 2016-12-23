/*
 * rb_tree.c

 *
 *  Created on: 19 ���. 2016 �.
 *      Author: �����
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "rb_tree.h"

/*check node color function*/
static int is_red ( struct rb_node *node ) {
	return node != NULL && node->red == 1;
}

/* ������� ��� ������������ �������� ���� */
static struct rb_node *rb_single ( struct rb_node *root, int dir ) {
	struct rb_node *save = root->link[!dir];

	root->link[!dir] = save->link[dir];
	save->link[dir] = root;

	root->red = 1;
	save->red = 0;

	return save;
}

/* ������� ��� ����������� �������� ���� */
static struct rb_node *rb_double ( struct rb_node *root, int dir ) {
	root->link[!dir] = rb_single ( root->link[!dir], !dir );
	return rb_single ( root, dir );
}

/*create new node function*/
static struct rb_node *make_node ( int new_ip ) {
	struct rb_node *rn = malloc ( sizeof *rn );

	if ( rn != NULL ) {
		rn->ip_addr = new_ip;
		rn->ip_cnt = 0;
		rn->red = 1; /* �������������� ������� ������ */
		rn->link[0] = NULL;
		rn->link[1] = NULL;
	}
	return rn;
}


/*insert node in red/black tree*/
int rb_insert ( struct rb_tree *tree, int new_ip ) {
   /* ���� ����������� ������� ����������� ������ � �� ������ ������ �� �����*/
   if ( tree->root == NULL ) {
     tree->root = make_node ( new_ip );

     tree->root->ip_cnt = 1;

     if ( tree->root == NULL )
       return 0;
   }
   else {
     struct rb_node head = {0}; /* ��������� ������ ������*/
     struct rb_node *g, *t;     /* ������� � �������� */
     struct rb_node *p, *q;     /* �������� � �������� */
     int dir = 0, last;

     /* ��������������� ���������� */
     t = &head;
     g = p = NULL;
     q = t->link[1] = tree->root;

     /* �������� ���� ������� �� ������ */
     for ( ; ; )  {
       if ( q == NULL ) {
         /* ������� ���� */
         p->link[dir] = q = make_node ( new_ip );
         tree->count ++ ;
         if ( q == NULL )
           return 0;
       }
       else if ( is_red ( q->link[0] ) && is_red ( q->link[1] ) )
       {
         /* ����� ����� */
         q->red = 1;
         q->link[0]->red = 0;
         q->link[1]->red = 0;
       }
        /* ���������� 2-� ������� ������ */
       if ( is_red ( q ) && is_red ( p ) ) {
         int dir2 = t->link[1] == g;

         if ( q == p->link[last] )
           t->link[dir2] = rb_single ( g, !last );
         else
           t->link[dir2] = rb_double ( g, !last );
       }

       /* ����� ���� � ������ ��� ����  - ����������� ����� ��������� � ������� �� �������*/
       if ( q->ip_addr == new_ip ) { q->ip_cnt++; break;}

       last = dir;
       dir = q->ip_addr < new_ip;

       if ( g != NULL )
         t = g;
       g = p, p = q;
       q = q->link[dir];
     }

     /* �������� ��������� �� ������ ������*/
     tree->root = head.link[1];
   }
   /* ������� ������ ������ ������ */
   tree->root->red = 0;

   return 1;
}

int rb_find(struct rb_tree tree, struct rb_node *node, int ip) {
	struct rb_node *it = tree.root;

	while (it != NULL) {
		if (it->ip_addr == ip) {
			memcpy(node, it, sizeof(struct rb_node));
			return 1;
		}
		else {
			int dir = it->ip_addr < ip;
			it = it->link[dir];
		}
	}

	return 0;
}

static void rb_rm_all(struct rb_node *node) {
	if (node == 0) return;

	rb_rm_all(node->link[0]);
	rb_rm_all(node->link[1]);
	free(node);
}

void rb_clear(struct rb_tree *tree) {
	if (tree->root != NULL) {
		rb_rm_all(tree->root);
		tree->root = 0;
		tree->count = 0;
	}
}

void rb_print( struct rb_node *node ) {
	if (node == NULL) return;

	rb_print(node->link[0]);
	printf("ip addr = %d count = %d\n", node->ip_addr, node->ip_cnt);
	rb_print(node->link[1]);
}



static void rb_fileprint(struct rb_node *node,  FILE *fp) {
	if (node == NULL) return;
	static struct in_addr addr;


	rb_fileprint(node->link[0], fp);



	addr.s_addr = node->ip_addr;

	fprintf(fp,"%s:%d\n", inet_ntoa(addr), node->ip_cnt);			//change
	rb_fileprint(node->link[1], fp);
}
void rb_savetofile(char *fname, struct rb_tree tree) {
	FILE	*fp;

	if ((fp = fopen(fname, "w")) != NULL) {
		rb_fileprint(tree.root, fp);
		fclose(fp);
	}
	else
		printf("can't open file");
}

static int strto_rbnode(char *strnode, int maxsize, int *ip, int *ip_cnt) {
	char	*str_ip;
	char	*str_cnt;
	static struct in_addr addr;

	str_ip = strnode;
	if ((str_cnt = strchr(strnode,':')) != NULL) {
		*str_cnt = '\0';
		 str_cnt++;

		//*ip = atoi(str_ip);
		inet_aton(str_ip, &addr);
		*ip = addr.s_addr;
		*ip_cnt = atoi(str_cnt);
		return 1;
	}
	return 0;
}

void rb_loadfromfile(char *fname, struct rb_tree *tree) {
	FILE	*fp;
	#define	MAX_SIZE	250
	char	buf[MAX_SIZE];
	int		ip,cnt, i;

	if ((fp = fopen(fname, "r")) != NULL) {
		while(fgets(buf,sizeof(buf),fp) != NULL) {

			if (strto_rbnode(buf, MAX_SIZE, &ip, &cnt)) {
				if (cnt > 1) for (i = 0; i < cnt; i++)	rb_insert(tree, ip);
				else  										rb_insert(tree, ip);
			}
			else break;
			//puts(buf);
		}
	}
	/*else
		printf("can't open file");*/
}
