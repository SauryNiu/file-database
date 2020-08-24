#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "AVLTree.h"
#include "FileDatabase.h"

// 调试日志开关
#define DEBUG_LOG 0

#define FILE_DB_LOG_DEBUG(fmt, ...) \
    do{ \
        if(DEBUG_LOG) \
        {\
            printf("%s at %d " fmt "\r\n", __FILE__, __LINE__, ##__VA_ARGS__);\
        }\
    }while(0);


/*

文件结构：
+--------+-------+--------+
|  head  |  cnt  |  data  |  ... 
+--------+-------+--------+

*/

typedef struct _file_db_private
{
    char m_path[128];  // 文件路径
    int m_head_size;   // 文件头大小
    int m_data_size;   // 用户数据大小
    int m_data_cnt;    // 文件数据库中记录的用户数据的数量

    int (*pf_get_ele_key)(void *); // 用户获取元素的键值函数指针
    void (*pf_visit)(void*); // 用户访问元素的函数指针

    pthread_mutex_t m_file_db_mutex;  // 文件数据库的文件锁

    avl_tree_t *m_tree; // avl 树指针
}file_db_private_t;

typedef struct _file_db_record
{
    int offset; // 当前元素在文件中的偏移量
    void *db;   // 当前元素对应的文件数据库指针
    void *ele;  // 当前元素保存的用户数据，这里才是文件中真正记录的数据
}file_db_record_t;

/*
@func: 
    获取文件数据库的私有成员变量

@para: 
    db ： 文件数据库指针

@return:
    NULL : 失败， other ： 获取到的成员变量

@note:
    none.
*/
static file_db_private_t* get_private_member(file_db_t* db)
{
    if(NULL == db) 
    {
        FILE_DB_LOG_DEBUG("db is NULL");
        return NULL;
    }
    return (file_db_private_t*)(db->_private_);
}


/*
@func: 
    获取记录元素的键值

@para: 
    record_ele ： 指定的记录元素

@return:
    int : < 0 : 失败， other ： 获取的键值

@note:
    本函数最终也是调用用户传入函数获取键值
*/
static int file_db_get_key(void *record_ele)
{
    if(NULL == record_ele) 
    {
        FILE_DB_LOG_DEBUG("[file_db_get_key]: record_ele null!");
        return -1;
    }
    file_db_record_t* record_data = (file_db_record_t*)record_ele;
    void *ele = record_data->ele;
    file_db_t* db = (file_db_t*)record_data->db;
    file_db_private_t* _this = get_private_member(db);
    if(NULL == _this)
    {
        FILE_DB_LOG_DEBUG("[file_db_get_key]: private member null!");
        return -2;
    }

    return _this->pf_get_ele_key(ele);
}

/*
@func: 
    释放文件数据库指定元素的资源

@para: 
    record_ele ： 指定的记录元素

@return:
    int : < 0 : 失败， 0 ： 成功

@note:
    none.
*/
static int file_db_free_ele(void *record_ele)
{
    if(NULL == record_ele) return -1;
    free(((file_db_record_t*)record_ele)->ele);
    return 0;
}

/*
@func: 
    在 avl 树中遍历每个元素需要执行的操作函数

@para: 
    record_ele : avl 树记录的节点元素

@return:
    none.

@note:
    此函数对内部数据稍作处理，然后使用用户传入的访问函数来处理用户元素
*/
static void file_db_visit_record(void* record_ele)
{
    if(NULL == record_ele) 
    {
        FILE_DB_LOG_DEBUG("[file_db_visit]: record_ele null!");
        return;
    }
    file_db_record_t* record_data = (file_db_record_t*)record_ele;
    void *ele = record_data->ele;
    file_db_t* db = (file_db_t*)record_data->db;
    file_db_private_t* _this = get_private_member(db);
    if(NULL == _this)
    {
        FILE_DB_LOG_DEBUG("[file_db_visit]: private member null!");
        return;
    }
    _this->pf_visit(ele);
}

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
static int file_db_add(file_db_t* db, void* ele)
{
    file_db_private_t* _this = get_private_member(db);
    if(NULL == _this) 
        return -4;

    if(NULL != _this->m_tree->query_by_key(_this->m_tree->_this, _this->pf_get_ele_key(ele)))
        return -5;

    file_db_record_t* record_data = (file_db_record_t*)malloc(sizeof(file_db_record_t));
    void* ele_memory = malloc(_this->m_data_size);
    memcpy(ele_memory, ele, _this->m_data_size);

    pthread_mutex_lock(&_this->m_file_db_mutex);
    FILE *db_fp = fopen(_this->m_path, "rb+");
    if(NULL == db_fp) 
    {
        FILE_DB_LOG_DEBUG("open db file error");
        goto RUNTIME_ERROR;
    }
    fseek(db_fp, 0, SEEK_END);
    record_data->offset = ftell(db_fp);
    record_data->db = db;
    int write_len = fwrite(ele_memory, _this->m_data_size, 1, db_fp);
    if(write_len != 1) 
    {
        FILE_DB_LOG_DEBUG("write ele error, write[%d] but [%d]", _this->m_data_size, write_len);
        fclose(db_fp);
        goto RUNTIME_ERROR;
    }
    _this->m_data_cnt++;
    FILE_DB_LOG_DEBUG("Write cnt %d, key %d", _this->m_data_cnt, _this->pf_get_ele_key(ele));
    fseek(db_fp, _this->m_head_size, SEEK_SET);
    write_len = fwrite(&_this->m_data_cnt, sizeof(int), 1, db_fp);
    if(write_len != 1)
    {
        FILE_DB_LOG_DEBUG("write cnt error");
        _this->m_data_cnt--;
        fclose(db_fp);
        truncate(_this->m_path, record_data->offset);
        goto RUNTIME_ERROR;
    }
    fflush(db_fp);
    fclose(db_fp);
    record_data->ele = ele_memory;
    pthread_mutex_unlock(&_this->m_file_db_mutex);
    return _this->m_tree->add(_this->m_tree->_this, (void *)record_data);

RUNTIME_ERROR:
    free(ele_memory);
    free(record_data);
    pthread_mutex_unlock(&_this->m_file_db_mutex);
    return -6;
}

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
static int file_db_del(file_db_t* db, int key)
{
    file_db_private_t* _this = get_private_member(db);

    if(NULL == _this) 
    {
        FILE_DB_LOG_DEBUG("Filedatabase error!");
        return -1;
    }
    file_db_record_t* record_data = _this->m_tree->query_by_key(_this->m_tree->_this, key);

    if(NULL == record_data)
    {
        FILE_DB_LOG_DEBUG("No such element. Del fail!");
        return -2;
    }

    pthread_mutex_lock(&_this->m_file_db_mutex);
    FILE* db_fp = fopen(_this->m_path, "rb+");

    if(record_data->offset < _this->m_head_size + sizeof(int) + _this->m_data_size * (_this->m_data_cnt - 1))
    {
        fseek(db_fp, 0, SEEK_END);
        FILE_DB_LOG_DEBUG("File position %ld", ftell(db_fp));
        fseek(db_fp, -_this->m_data_size, SEEK_END);
        FILE_DB_LOG_DEBUG("File position %ld", ftell(db_fp))
        void* buff = malloc(_this->m_data_size);
        
        if(1 != fread(buff, _this->m_data_size, 1, db_fp))
        {
            FILE_DB_LOG_DEBUG("Read tail element error! file position %ld", ftell(db_fp));
            free(buff);
            pthread_mutex_unlock(&_this->m_file_db_mutex);
            return -3;
        }

        fseek(db_fp, record_data->offset, SEEK_SET);

        if(1 != fwrite(buff, _this->m_data_size, 1, db_fp))
        {
            FILE_DB_LOG_DEBUG("Write tail element error!");

            free(buff);
            fclose(db_fp);
            pthread_mutex_unlock(&_this->m_file_db_mutex);
        }

        free(buff);
    }
    _this->m_data_cnt--;
    fseek(db_fp, _this->m_head_size, SEEK_SET);
    int write_len = fwrite(&_this->m_data_cnt, sizeof(int), 1, db_fp);
    if(write_len != 1)
    {
        FILE_DB_LOG_DEBUG("write cnt error");
        _this->m_data_cnt++;
        fclose(db_fp);
        pthread_mutex_unlock(&_this->m_file_db_mutex);
        return -4;
    }

    fclose(db_fp);
    truncate(_this->m_path, _this->m_head_size + sizeof(int) + _this->m_data_cnt * _this->m_data_size );
    pthread_mutex_unlock(&_this->m_file_db_mutex);

    return _this->m_tree->del_node_by_key(_this->m_tree->_this, key);
}

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
static int file_db_edit(file_db_t* db, int key, void *ele)
{
    file_db_private_t* _this = get_private_member(db);

    if(NULL == _this || NULL == ele) return -1;

    if(key != _this->pf_get_ele_key(ele))
    {
        FILE_DB_LOG_DEBUG("Edit error, Key value and element do not match");
        return -1;
    }
    pthread_mutex_lock(&_this->m_file_db_mutex);
    file_db_record_t* record_data = (file_db_record_t*)(_this->m_tree->query_by_key(_this->m_tree->_this, key));
    if(NULL == record_data)
    {
        FILE_DB_LOG_DEBUG("Edit error, Query data error");
        return -2;
    }
    
    memset(record_data->ele, 0, _this->m_data_size);
    memcpy(record_data->ele, ele, _this->m_data_size);
    FILE_DB_LOG_DEBUG("query key[%d], ele key[%d], get key[%d]", key, _this->pf_get_ele_key(ele), _this->pf_get_ele_key(record_data->ele));
    FILE* db_fp = fopen(_this->m_path, "rb+");

    if(NULL == db_fp) 
    {
        FILE_DB_LOG_DEBUG("Edit error, File error");
        return -3;
    }
    fseek(db_fp, record_data->offset, SEEK_SET);

    if(fwrite(record_data->ele, _this->m_data_size, 1, db_fp) != 1)
    {
        fclose(db_fp);
        pthread_mutex_unlock(&_this->m_file_db_mutex);
        FILE_DB_LOG_DEBUG("Edit element, write new error!");
        return -4;
    }
    fclose(db_fp);
    pthread_mutex_unlock(&_this->m_file_db_mutex);
    return 0;
}

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
static void* file_db_query(file_db_t* db, int key)
{
    file_db_private_t* _this = get_private_member(db);
    if(NULL == _this) 
    {
        FILE_DB_LOG_DEBUG("_this is NULL");
        return NULL;
    }
    file_db_record_t* record_data = (file_db_record_t*)(_this->m_tree->query_by_key(_this->m_tree->_this, key));
    if(NULL == record_data) 
    {
        FILE_DB_LOG_DEBUG("record_data is NULL");
        return NULL;
    }
    return record_data->ele;
}

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
static int file_db_write_head(file_db_t* db, void* head)
{
    file_db_private_t* _this = get_private_member(db);

    if(NULL == _this || NULL == head) return -1;
    pthread_mutex_lock(&_this->m_file_db_mutex);
    FILE* db_fp = fopen(_this->m_path, "rb+");

    if(NULL == db_fp) return -1;

    if(fwrite(head, _this->m_head_size, 1, db_fp) != 1)
    {
        fclose(db_fp);
        pthread_mutex_unlock(&_this->m_file_db_mutex);
        return -1;
    }

    fclose(db_fp);
    pthread_mutex_unlock(&_this->m_file_db_mutex);
    return 0;
}


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
static int file_db_read_head(file_db_t* db, void* head)
{
    file_db_private_t* _this = get_private_member(db);

    if(NULL == _this || NULL == head) return -1;

    FILE* db_fp = fopen(_this->m_path, "rb+");

    if(NULL == db_fp) return -1;

    if(fread(head, _this->m_head_size, 1, db_fp) != 1)
    {
        fclose(db_fp);
        return -1;
    }

    fclose(db_fp);
    return 0;
}


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
static int file_db_size(file_db_t* db)
{
    file_db_private_t* _this = get_private_member(db);

    if(NULL == _this) return -1;

    return _this->m_data_cnt;
}

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
static int file_db_traverse(file_db_t* db, void (*visit)(void*))
{
    if(NULL == db || NULL == visit)
    {
        FILE_DB_LOG_DEBUG("[file_db_traverse] para error");
        return -1;
    }

    file_db_private_t* _this = get_private_member(db);

    if(NULL == _this) return -2;

    _this->pf_visit = visit;
    _this->m_tree->preorder(_this->m_tree->_this, file_db_visit_record);
}

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
static int file_db_clear(file_db_t* db)
{
    file_db_private_t* _this = get_private_member(db);

    if(NULL == _this) return -1;
    int cnt = 0;

    pthread_mutex_lock(&_this->m_file_db_mutex);
    FILE* db_fp = fopen(_this->m_path, "rb+");
    if(NULL == db_fp)
    {
        pthread_mutex_unlock(&_this->m_file_db_mutex);
        return -1;
    }
    fseek(db_fp, _this->m_head_size, SEEK_SET);
    if(1 != fwrite(&cnt, sizeof(int), 1, db_fp))
    {
        fclose(db_fp);
        pthread_mutex_unlock(&_this->m_file_db_mutex);
        FILE_DB_LOG_DEBUG("[file_db_clear] : write cnt error");
        return -2;
    } 
    fclose(db_fp);
    truncate(_this->m_path, _this->m_head_size + sizeof(int));
    pthread_mutex_unlock(&_this->m_file_db_mutex);
    
    _this->m_data_cnt = 0;

    _this->m_tree->clear_node(_this->m_tree->_this);

    return 0;
}

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
static int file_db_free(file_db_t* db)
{
    file_db_private_t* _this = get_private_member(db);

    if(NULL == _this) return -1;

    if(_this->m_data_cnt > 0)
        _this->m_tree->clear_node(_this->m_tree->_this);

    _this->m_tree->destory(&_this->m_tree);

    free(_this);
    free(db);
    return 0;
}

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
static int file_db_destory(file_db_t* db)
{
    file_db_private_t* _this = get_private_member(db);

    if(NULL == _this) 
    {
        FILE_DB_LOG_DEBUG("destory error");
        return -1;
    }
    unlink(_this->m_path);

    return file_db_free(db);
}
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
file_db_t* file_db_init(const char* path, int head_size, int data_size, int (*pf_hash_func)(void *), void* head)
{
    if(NULL == path || NULL == pf_hash_func || NULL == head) return NULL;
    
    file_db_private_t* _private_ = (file_db_private_t*) malloc(sizeof(file_db_private_t));
    if(NULL == _private_)
    {
        FILE_DB_LOG_DEBUG("private null!");
        return NULL;
    }
    
    
    avl_tree_t* tree = avl_tree_create(sizeof(file_db_record_t), file_db_get_key, file_db_free_ele, 1);
    if(NULL == tree)
    {
        free(_private_);
        FILE_DB_LOG_DEBUG("avl tree null!");
        return NULL;
    }

    file_db_t* file_db = (file_db_t*) malloc(sizeof(file_db_t));
    if(NULL == file_db)
    {
        free(_private_);
        tree->destory(&tree);
        FILE_DB_LOG_DEBUG("file db null!");
        return NULL;
    }

    memset(_private_, 0, sizeof(file_db_private_t));
    memset(file_db, 0, sizeof(file_db_t));
    
    
    strcpy(_private_->m_path, path);
    _private_->m_head_size = head_size;
    _private_->m_data_size = data_size;
    _private_->m_tree = tree;
    _private_->m_data_cnt = 0;
    _private_->pf_get_ele_key = pf_hash_func;
    pthread_mutex_init(&_private_->m_file_db_mutex, NULL);
 
    
    file_db->_this = file_db;
    file_db->_private_ = (void*)_private_;
   
    file_db->add = file_db_add;
    file_db->del = file_db_del;
    file_db->edit = file_db_edit;
    file_db->query = file_db_query;
    file_db->write_head = file_db_write_head;
    file_db->read_head = file_db_read_head;
    file_db->size = file_db_size;
    file_db->traverse = file_db_traverse;
    file_db->clear = file_db_clear;
    file_db->free = file_db_free;
    file_db->destory = file_db_destory;

    FILE* db_fp = fopen(_private_->m_path, "rb");
    if(NULL == db_fp)
    {
        db_fp = fopen(_private_->m_path, "wb");
        if(NULL == db_fp)
        {
            FILE_DB_LOG_DEBUG("create db error!");
            file_db_free(file_db);
            return NULL;
        }

        if(1 != fwrite(head, head_size, 1, db_fp))
        {
            FILE_DB_LOG_DEBUG("write head error!");
            file_db_free(file_db);
            fclose(db_fp);
            unlink(path);
            return NULL;
        }

        if(1 != fwrite(&_private_->m_data_cnt, sizeof(int), 1, db_fp))
        {
            FILE_DB_LOG_DEBUG("write head error!");
            file_db_free(file_db);
            fclose(db_fp);
            unlink(path);
            return NULL;
        }
        fclose(db_fp);
    }
    else
    {
        if(1 != fread(head, head_size, 1, db_fp))
        {
            FILE_DB_LOG_DEBUG("read head error!");
            fclose(db_fp);
            file_db_free(file_db);
            return NULL;
        }
        if(1 != fread(&_private_->m_data_cnt, sizeof(int), 1, db_fp))
        {
            FILE_DB_LOG_DEBUG("read cnt error!");
            fclose(db_fp);
            file_db_free(file_db);
            return NULL;
        }

        file_db_record_t* record_data = NULL;
        void* element = NULL;

        for(int i = 0; i < _private_->m_data_cnt; ++i)
        {
            record_data = (file_db_record_t*)malloc(sizeof(file_db_record_t));
            if(NULL == record_data)
            {
                FILE_DB_LOG_DEBUG("record data null!");
                file_db_free(file_db);
                fclose(db_fp);
                return NULL;
            }

            element = malloc(_private_->m_data_size);
            if(NULL == element)
            {
                FILE_DB_LOG_DEBUG("element null!");
                file_db_free(file_db);
                free(record_data);
                fclose(db_fp);
                return NULL;
            }
            memset(record_data, 0, sizeof(file_db_record_t));
            memset(element, 0, _private_->m_data_size);

            record_data->offset = ftell(db_fp);
            record_data->db = file_db;

            if(1 != fread(element, _private_->m_data_size, 1, db_fp))
            {
                FILE_DB_LOG_DEBUG("read element error!");
                file_db_free(file_db);
                free(record_data);
                free(element);
                fclose(db_fp);
                return NULL;
            }
            
            record_data->ele = element;
            _private_->m_tree->add( _private_->m_tree->_this, record_data);
        }
        fclose(db_fp);
    }
    
    FILE_DB_LOG_DEBUG("init data size %d, head size %d, data cnt %d", _private_->m_data_size, _private_->m_head_size, _private_->m_data_cnt);

    return file_db;
}
