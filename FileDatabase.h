#ifndef _FILE_DATABASE_H_
#define _FILE_DATABASE_H_

typedef struct _file_db file_db_t;

struct _file_db
{
    file_db_t* _this;
    void* _private_;

/*
@func: 
    添加元素到文件数据库中

@para: 
    db : 文件数据库指针
    ele : 要被添加的元素指针

@return:
    int : < 0 : 失败， 0 ： 成功

@note:
    ele 传入的建议是非指针变量的地址，如果使用的是指向动态内存的指针，则用完后需自行释放资源
*/
    int (*add)(file_db_t* db, void *ele);

/*
@func: 
    通过键值删除指定键值对应的元素

@para: 
    db : 文件数据库指针
    key : 元素对应的键值

@return:
    int : < 0 : 失败， 0 ： 成功

@note:
    none.
*/
    int (*del)(file_db_t* db, int key);

/*
@func: 
    根据键值编辑指定的元素

@para: 
    db : 文件数据库指针
    key : 元素对应的键值
    ele ： 目标元素，将替换给定键值所对应的元素的值

@return:
    int : < 0 : 失败， 0 ： 成功

@note:
    ele 参数通过用户传入的获取键值的函数计算得到的键值需要和本函数传入键值 key 一致
*/   
    int (*edit)(file_db_t* db, int key, void *ele);

/*
@func: 
    根据键值查询文件数据库中的元素

@para: 
    db : 文件数据库指针
    key : 元素的键值

@return:
    void* : NULL 查询失败， other 查询到的元素的指针

@note:
    none.
*/
    void* (*query)(file_db_t* db, int key);

/*
@func: 
    写入文件数据库的文件头

@para: 
    db : 文件数据库指针
    head : 写入的文件头指针

@return:
    int : < 0 : 失败， 0 ： 成功

@note:
    none.
*/
    int (*write_head)(file_db_t* db, void* head);

/*
@func: 
    读取文件数据库的文件头到指定内存中

@para: 
    db : 文件数据库指针
    head : 读出的文件头指针

@return:
    int : < 0 : 失败， 0 ： 成功

@note:
    none.
*/
    int (*read_head)(file_db_t* db, void* head);

/*
@func: 
    返回文件数据库中的元素个数

@para: 
    db : 文件数据库指针

@return:
    int : < 0 : 失败， other ： 元素个数

@note:
    none.
*/
    int (*size)(file_db_t* db);

/*
@func: 
    遍历文件数据库中的所有内容，然后使用传入的函数指针对每个元素执行操作

@para: 
    db : 文件数据库指针
    visit : 对元素操作的函数指针

@return:
    int : < 0 : 失败， 0 ： 成功

@note:
    none.
*/
    int (*traverse)(file_db_t* db, void (*visit)(void*));

/*
@func: 
    清除文件数据库内容，但是保存文件头

@para: 
    db : 文件数据库指针

@return:
    int : -1 : 失败， 0 ： 成功

@note:
    【重要】此函数不能与 file_db_destory 同时使用
*/
    int (*clear)(file_db_t* db);

/*
@func: 
    并释放文件数据库相关动态内存

@para: 
    db : 文件数据库指针

@return:
    int : -1 : 失败， 0 ： 成功

@note:
    【重要】此函数不能与 file_db_destory 同时使用
*/
    int (*free)(file_db_t* db);

/*
@func: 
    销毁数据库文件并释放相关动态内存

@para: 
    db : 文件数据库指针

@return:
    int : -1 : 失败， 0 ： 成功

@note:
    【重要】此函数不能与 file_db_free 同时使用
*/
    int (*destory)(file_db_t* db);
};


/*
@func: 
    使用指定的文件初始化文件数据库

@para: 
    path : 存放数据的文件所在路径 

@return:
    file_db_t* : 文件数据库指针

@note:
    如果不再使用该数据库需要调用销毁函数释放内存
    【重要】保存的数据的数据类型大小必须是固定的
*/
extern file_db_t* file_db_init(const char* path, int head_size, int data_size, int (*pf_hash_func)(void *), void* head);




#endif /* end #ifndef _FILE_DATABASE_H_ */