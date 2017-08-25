/* mem_pool.h */

#ifndef MEM_POOL_H_FILE
#define MEM_POOL_H_FILE

#include <stddef.h>

struct sp_mem_page;

struct sp_mem_pool {
  size_t page_size;
  struct sp_mem_page *page_list;
};

void sp_init_mem_pool(struct sp_mem_pool *p);
void sp_destroy_mem_pool(struct sp_mem_pool *p);
void *sp_malloc(struct sp_mem_pool *p, size_t size);
void *sp_realloc(struct sp_mem_pool *p, void *data, size_t size);

#define sp_free(p, data) sp_realloc((p), (data), 0)

#endif /* MEM_POOL_H_FILE */
