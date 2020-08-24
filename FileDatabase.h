#ifndef _FILE_DATABASE_H_
#define _FILE_DATABASE_H_

typedef struct _file_db file_db_t;

struct _file_db
{
    file_db_t* _this;
    void* _private_;

    int (*add)(file_db_t* db, void *ele);
    int (*del)(file_db_t* db, int key);
    int (*edit)(file_db_t* db, int key, void *ele);
    void* (*query)(file_db_t* db, int key);
    int (*write_head)(file_db_t* db, void* head);
    int (*read_head)(file_db_t* db, void* head);
    int (*size)(file_db_t* db);
    int (*traverse)(file_db_t* db, void (*visit)(void*));
    int (*clear)(file_db_t* db);
    int (*free)(file_db_t* db);
    int (*destory)(file_db_t* db);
};


extern file_db_t* file_db_init(const char* path, int head_size, int data_size, int (*pf_hash_func)(void *), void* head);




#endif /* end #ifndef _FILE_DATABASE_H_ */