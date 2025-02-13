#include <assert.h>

#include "../log/pr.h"
#include "mem_pool.h"

void Mempool::mem_init(MEM_CAP size, int chunk_num)
{
    Chunk *prev; 
    mp_pool[size] = new (std::nothrow) Chunk(size);
    if (mp_pool[size] == nullptr) {
        PR_ERROR("new chunk %d error", static_cast<int>(size));
        exit(1);
    }
    prev = mp_pool[size];

    for (int i = 1; i < chunk_num; i ++) {
        prev->next = new (std::nothrow)Chunk(size);
        if (prev->next == nullptr) {
            PR_ERROR("new chunk m4K error");
            exit(1);
        }
        prev = prev->next;
    }
    mp_total_size_kb += size/1024 * chunk_num;
}

Mempool::Mempool() : mp_total_size_kb(0), mp_left_size_kb(0)
{
    //按照chuck大小依次初始化每一条chuck链
    mem_init(m4K, 2000);
    mem_init(m16K, 500);
    mem_init(m64K, 250);
    mem_init(m256K, 100);
    mem_init(m1M, 25);
    mem_init(m4M, 10);
    mp_left_size_kb = mp_total_size_kb;
}

//分配一个大小为n的chuck
Chunk *Mempool::alloc_chunk(int n) 
{
    int index;
    bool found = false;
    for(index = mLow; index <= mUp; index = index * MEM_CAP_MULTI_POWER)
    {
        if(n <= index)
        {
            found = true;
            break;
        }
    }
    //大小超过可以分配的最大的chuck
    if(!found)
    {
        return nullptr;
    }
    //对mp_pool加锁
    lock_guard<mutex> lck(mp_mutex);
    //如果对应的链上没有可以分配的内存块
    if (mp_pool[index] == nullptr) {
        //如果现有内存池的大小 + 需要分配的chuck大小 >= 内存的最大容量
        //则无法进行分配
        if (mp_total_size_kb + index/1024 >= MAX_POOL_SIZE) {
            PR_ERROR("beyond the limit size of memory!\n");
            exit(1);
        }

        //给index对应的链上开辟一块新chuck
        Chunk *new_buf = new (std::nothrow) Chunk(index);
        if (new_buf == nullptr) {
            PR_ERROR("new chunk error\n");
            exit(1);
        }
        //将调用new新分配的chuck直接返回
        mp_total_size_kb += index/1024;
        return new_buf;
    }
    //如果对应的链上有可以分配的chuck，返回链首的chuck
    Chunk *target = mp_pool[index];
    mp_pool[index] = target->next;
    target->next = nullptr;
    mp_left_size_kb -= index/1024;

    return target;
}

//回收chuck
void Mempool::retrieve(Chunk *block)
{
    int index = block->capacity;
    block->length = 0;
    block->head = 0;

    lock_guard<mutex> lck(mp_mutex);
    assert(mp_pool.find(index) != mp_pool.end());

    block->next = mp_pool[index];
    mp_pool[index] = block;
    mp_left_size_kb += block->capacity/1024;
}

int Mempool::get_list_size_byte(MEM_CAP index)
{
    int size = 0;
    lock_guard<mutex> lck(mp_mutex);
    assert(mp_pool.find(index) != mp_pool.end());
    Chunk *node = mp_pool[index];

    while(node)
    {
        size += node->capacity;
        node = node->next;
    } 

    return size;
}

void Mempool::print_list_content(MEM_CAP index)
{
    lock_guard<mutex> lck(mp_mutex);
    int cnt = 0;
    printf("***************start to print %dkb chunk_size list data*******************\n", index/1024);
    assert(mp_pool.find(index) != mp_pool.end());
    Chunk *node = mp_pool[index];

    while (node)
    {
        if(cnt <= 5)
            node->print_data();
        cnt++;
        node = node->next;
    }
    printf("...\n");
    printf("******************end, node cnt is %d************************\n\n", cnt);
}