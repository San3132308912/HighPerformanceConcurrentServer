#ifndef __MEM_POOL_H__
#define __MEM_POOL_H__

#include <unordered_map>
#include <mutex>

#include "chunk.h"

using namespace std;

typedef unordered_map<int, Chunk*> pool_t;

#define MEM_CAP_MULTI_POWER (4)

//chuck大小
typedef enum {
    mLow    = 4096,
    m4K     = mLow,
    m16K    = m4K * MEM_CAP_MULTI_POWER,
    m64K    = m16K * MEM_CAP_MULTI_POWER,
    m256K   = m64K * MEM_CAP_MULTI_POWER,
    m1M     = m256K * MEM_CAP_MULTI_POWER,
    m4M     = m1M * MEM_CAP_MULTI_POWER,
    mUp     = m4M
} MEM_CAP;

//内存池的最大容量
#define MAX_POOL_SIZE (4U *1024 *1024) 


class Mempool 
{
public:
    static Mempool& get_instance() {
        static Mempool mp_instance;
        return &mp_instance;
    }
    
    //获取一个大小为n的chunk
    Chunk *alloc_chunk(int n);
    //默认获取一个大小为m4K的chuck
    Chunk *alloc_chunk() { return alloc_chunk(m4K); }

    //回收一个chuck
    void retrieve(Chunk *block);

    // FIXME: use smart ptr to manage chunk or add destroy interface to recycle memory.
    // static void destroy();

    ~Mempool() = default;

    // api for debug
    [[deprecated("mem pool debug api deprecated!")]]
    int get_total_size_kb(){ return mp_total_size_kb; }
    [[deprecated("mem pool debug api deprecated!")]]
    int get_left_size_kb(){ return mp_left_size_kb; }
    [[deprecated("mem pool debug api deprecated!")]]
    int get_list_size_byte(MEM_CAP index);
    [[deprecated("mem pool debug api deprecated!")]]
    void print_list_content(MEM_CAP index);
    
private:
    Mempool();
    //=delete禁止编译器默认生成的函数，可以将其定义为private，或者使用=delete
    //拷贝构造
    Mempool(const Mempool&) = delete;
    //移动拷贝构造
    Mempool(Mempool&&) = delete;
    //重载拷贝构造运算符
    Mempool& operator=(const Mempool&) = delete;
    //重载移动拷贝构造运算符
    Mempool& operator=(Mempool&&) = delete;
    
    //初始化内存池
    void mem_init(MEM_CAP size, int chunk_num);

    pool_t mp_pool;
    uint64_t mp_total_size_kb;
    uint64_t mp_left_size_kb;
    mutex mp_mutex;
};

#endif