#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdbool.h>
#include "./include/include/bufferpool.h"
#include "./include/include/lrucache.h"
#include<time.h>


unsigned int hash(const char *key)
{
    unsigned int hash_value = 0;
    while (*key)
    {
        hash_value = (hash_value * 31) + *key++ ;
    }

    return hash_value % CACHE_SIZE;
}


void move_to_front(file_cache_t* cache, cached_file_t* node){
    if (cache->cache_head == node) return; // Already at front

    // Unlink node from current position
    if (node->lru_prev) node->lru_prev->lru_next = node->lru_next;
    if (node->lru_next) node->lru_next->lru_prev = node->lru_prev;

    // Update tail if needed
    if (cache->cache_tail == node) cache->cache_tail = node->lru_prev;

    // Insert at head
    node->lru_prev = NULL;
    node->lru_next = cache->cache_head;
    if (cache->cache_head) cache->cache_head->lru_prev = node;
    cache->cache_head = node;
    if (!cache->cache_tail) cache->cache_tail = node;
}


void add_to_front(file_cache_t* cache, cached_file_t *newNode){
    newNode->lru_prev = NULL;
    newNode->lru_next = cache->cache_head;
    if (cache->cache_head) cache->cache_head->lru_prev = newNode;
    cache->cache_head = newNode;
    if (!cache->cache_tail) cache->cache_tail = newNode;
}

//LRU removal
void lru_file_remove(file_cache_t *cache){
    //if(cache->cached_files_count == CACHE_SIZE){
        cached_file_t *lruNode = cache->cache_tail;
        if(lruNode){
            cache->cache_tail = lruNode->lru_prev;
            free(lruNode);
        }
    //}
}

void file_remove_from_lru(file_cache_t *cache, const char *filename){
        cached_file_t *curr = cache->cache_head;
        while(curr){
            if(strcmp(curr->filename, filename)==0){
                if(cache->cache_tail==curr){
                    cache->cache_tail = curr->lru_prev;
                    free(curr);
                    return;
                }
                else if(cache->cache_head == curr){
                    cache->cache_head = curr->lru_next;
                    free(curr);
                    return;
                }
                curr->lru_prev->lru_next = curr->lru_next;
                curr->lru_next->lru_prev = curr->lru_prev;
                free(curr);
                return;
            }
            curr = curr->lru_next;
        }
        printf("file: %s is removed from lru cache also..\n", filename);
}

void print_lru_status(file_cache_t *cache){
    cached_file_t *curr = cache->cache_head;
    printf("Current lru status: ");
    while (curr)
    {
        printf("%s, ", curr->filename);
        curr = curr->lru_next;
    }
}
//To create file cache
file_cache_t* create_file_cache() {
    file_cache_t *cache = malloc(sizeof(file_cache_t));
    if(!cache) return NULL;

    cache->buffer_pool = create_buffer_pool(POOL_SIZE);
    if(!cache->buffer_pool)
    {
        free(cache);
        return NULL;
    }  
    
    cache->cached_files_count = 0;
    cache->cache_head = NULL;
    cache->cache_tail = NULL;

    for (int iterator = 0; iterator < CACHE_SIZE; iterator++)
    {
        cache->files[iterator] = NULL;
    }

    return cache;
} 

//To read the cached file
char* read_file_cached(file_cache_t *cache, const char *filename)
{
    unsigned int index = hash(filename);

    cached_file_t *cached = cache->files[index];

    while(cached)
    {
        //printf("1111");
        if(strcmp(cached->filename, filename) == 0)
        {   //printf("2222");
            cached->last_accessed = time(NULL);
            move_to_front(cache, cached);
            printf("File cache HIT for %s \n", filename);
            return cached->buffer->data;
        }
        cached = cached->next;
    }

    printf("File cache MISS for %s \n", filename);

    buffer_t *buffer = acquire_buffer(cache->buffer_pool);
    if(!buffer)
    {
        printf("No buffers are available");
        return NULL;
    }
    //printf("1111111");
    int fd = open(filename, O_RDONLY);
    //printf("222222");
    if(fd == -1)
    {
        release_buffer(cache->buffer_pool, buffer);
        printf("Error in opening file...111");
        return NULL;
    }

    size_t bytes_read = read(fd, buffer->data, BUFFER_SIZE - 1);
    close(fd);

    if(bytes_read == -1)
    {
        release_buffer(cache->buffer_pool, buffer);
        printf("Error in reading file...111");
        return NULL;
    }

    buffer->data[bytes_read] = '\0';


//To add file as a new cache entry
cached_file_t *new_cached = malloc(sizeof(cached_file_t));
new_cached->lru_next = NULL;
new_cached->lru_prev = NULL;

if(!new_cached)
{
    release_buffer(cache->buffer_pool, buffer);
    printf("new_cached is not created...");
    return NULL;
}

strncpy(new_cached->filename, filename, sizeof(new_cached->filename) - 1);
new_cached->filename[sizeof(new_cached->filename) - 1] = '\0';

new_cached->buffer = buffer;
new_cached->file_size = bytes_read;
new_cached->last_accessed = time(NULL);
new_cached->next = cache->files[index];
cache->files[index] = new_cached;
if(cache->cached_files_count == CACHE_SIZE)
{
    printf("LRU cache is full removing the lru file...");
    lru_file_remove(cache);
}
//printf("3333\n");
add_to_front(cache, new_cached);
//printf("44444\n");
cache->cached_files_count++;
printf("Cached file %s of size %zu bytes \n", filename, bytes_read);
return buffer->data;
}

//to remove the cached file
void remove_cached_file(file_cache_t* cache, const char* fileName)
{
        unsigned int index = hash(fileName);
        cached_file_t* cachedFile = cache->files[index];
        cached_file_t* prev = NULL;
        while (cachedFile)
        {
            if(strcmp(cachedFile->filename, fileName) == 0){
                if(prev!=NULL){
                    prev->next = cachedFile->next;
                }
                else{
                    cache->files[index] = cachedFile->next;
                }

                if(cachedFile->buffer){
                    release_buffer(cache->buffer_pool, cachedFile->buffer);
                }
                free(cachedFile);
                printf("fileName: %s | removed from cache", fileName);
                cache->cached_files_count--;
            }
            prev = cachedFile;
            cachedFile = cachedFile->next;
        }
        return ;
}


int main(){
    file_cache_t* cache = create_file_cache();
    read_file_cached(cache, "test_file.txt");
    printf("\n");
    print_lru_status(cache);
    printf("\n");
    read_file_cached(cache, "test_file1.txt");
    printf("\n");
    print_lru_status(cache);
    printf("\n");
    read_file_cached(cache, "test_file.txt");
    printf("\n");
    print_lru_status(cache);
    printf("\n");
    remove_cached_file(cache, "test_file1.txt");
    printf("\n");
    file_remove_from_lru(cache, "test_file1.txt");
    printf("\n");
    print_lru_status(cache);
    printf("\n");
}
