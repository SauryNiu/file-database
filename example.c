#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "FileDatabase.h"

#define EXAMPLE_FILE_DB "example.db"
#define FILE_DB_SIZE 20
static file_db_t* file_db;

typedef struct _db_head
{
    int version;
    char reserve[252];
}db_head_t;

typedef struct _db_data
{
    int key;
    char value[60];
}db_data_t;

static int get_key(void *ele)
{
    if(NULL == ele)
    {
        return -1;
    }
    db_data_t* record = (db_data_t*)ele;
    return record->key;
}

static void visit(void* ele)
{
    if(NULL == ele) 
    {
        printf("ele null!!!");
    }
    db_data_t* data = (db_data_t*)ele;
    printf("[visit] key[%d]->value[%s]\r\n", data->key, data->value);
}

int main(void)
{
    db_head_t head;
    head.version = 1;
    file_db = file_db_init(EXAMPLE_FILE_DB, sizeof(db_head_t), sizeof(db_data_t), get_key, &head);
    
    if(NULL == file_db)
    {
        printf("create db error\r\n");
        return -1;
    }
    db_data_t element;
   
    int cnt = 0;
    srand(time(NULL));

    int key[FILE_DB_SIZE];
    int index = 0;
    for(int i = 0; i < FILE_DB_SIZE; ++i)
    {
        int num = rand()%FILE_DB_SIZE;
        key[index++] = num;
        //printf("index :%d -> key %d\r\n", index, key[index]);
        element.key = num;
        memset(element.value, 0, 60);
        sprintf(element.value, "element%d", num);
        
        if(0 == file_db->add(file_db->_this, &element))
        {
            cnt++;
        }
    }

    printf("Add cnt %d, total %d\r\n", cnt, file_db->size(file_db->_this));

    int random_key = rand()%FILE_DB_SIZE;
    printf("Del key[%d], res[%d]\r\n", key[random_key], file_db->del(file_db->_this, key[random_key]));
    
    random_key = rand()%FILE_DB_SIZE;
    memset(element.value, 0, 60);
    sprintf(element.value, "edit%d", key[random_key]);
    element.key = key[random_key];
    printf("Edit key[%d], res[%d]\r\n", key[random_key], file_db->edit(file_db->_this, key[random_key], &element));
    
    random_key = rand()%FILE_DB_SIZE;
    db_data_t* ele_query = (db_data_t*) file_db->query(file_db->_this, key[random_key]);
    if(NULL == ele_query)
    {
        printf("ele_query null\r\n");
    }
    else
    {
        printf("Query key[%d], value[%s]\r\n", ele_query->key, ele_query->value);
    }

    file_db->traverse(file_db->_this, visit);

    file_db->clear(file_db->_this);
    file_db->free(file_db->_this);
    //file_db->destory(file_db->_this);
    return 0;
}