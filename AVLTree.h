#ifndef _AVL_TREE_H_
#define _AVL_TREE_H_

#include <stdbool.h>
#include <pthread.h>

typedef struct _avl_tree avl_tree_t;


struct _avl_tree
{   
    avl_tree_t *_this;
    void *_private_;    // 私有成员

    int (*pf_hash)(void *); //用户提供的 hash 函数
    int (*pf_free_element)(void *ele); // 用户提供的节点元素中保存的动态内存的释放方法

    int (*add)(avl_tree_t *tree, void *node);    // 添加节点
    void* (*query_by_key)(avl_tree_t *tree, int key); // 通过键值查询节点
    void (*preorder)(avl_tree_t* tree,  void( *visit)(void* ele));    // 前序遍历
    int (*size)(avl_tree_t* tree);  // 节点数量
    int (*del_node_by_key)(avl_tree_t* tree, int key);  // 通过键值删除节点
    int (*del_node_by_element)(avl_tree_t* tree, void *element);    //通过元素删除节点
    void (*clear_node)(avl_tree_t *tree);   // 清除全部节点
    void (*destory)(avl_tree_t** tree); //销毁树
};


/*
@func: 
    创建一颗平衡二叉树

@para: 
    element_size : 节点保存元素的大小，单位字节
    pf_hash_func ； 从节点元素获得键值 key 的方法，由用户提供
    pf_free_element_func ： 若节点元素不包含额外的动态内存， 此参数可传 NULL；若节点包含的元素中还包含额外的动态内存，用户需传入此函数以正确释放内存
    thread_safe ： 是否启用线程安全，若用户保证不会涉及冲突，可关闭线程安全功能

@return:
    avl_tree_t* : 创建平衡二叉树的指针
*/
extern avl_tree_t* avl_tree_create(int element_size, int (*pf_hash_func)(void *), int (*pf_free_element_func)(void *), bool thread_safe);


#endif /* end #ifndef _AVL_TREE_H_ */