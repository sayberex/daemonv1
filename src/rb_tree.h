/*
 * rb_tree.h
 *
 *  Created on: 19 ���. 2016 �.
 *      Author: �����
 */

#ifndef RB_TREE_H_
#define RB_TREE_H_

/*structure that describe node of red/black tree*/
struct rb_node  {
	int					red;
	int					ip_addr;
	int					ip_cnt;
	struct	rb_node 	*link[2];
};

/*structure that describe red/black tree*/
struct rb_tree  {
	struct rb_node 		*root;	// tree root ptr
	int 				count;	// amount nodes
};

extern	int		rb_insert	( struct rb_tree *tree, int new_ip );

extern	void	rb_clear	(struct rb_tree *tree);

extern	int		rb_find		(struct rb_tree tree, struct rb_node *node, int ip);

extern	void	rb_print	( struct rb_node *node );

extern	void	rb_savetofile(char *fname, struct rb_tree tree);

extern	void	rb_loadfromfile(char *fname, struct rb_tree *tree);

#endif /* RB_TREE_H_ */
