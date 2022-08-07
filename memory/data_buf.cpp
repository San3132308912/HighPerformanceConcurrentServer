#include <sys/ioctl.h>
#include <unistd.h>
#include <memory.h>
#include <assert.h>

#include "data_buf.h"
#include "pr.h"

BufferBase::BufferBase() 
{
}

//清空并回收chuck
BufferBase::~BufferBase()
{
    clear();
}

//返回chuck中存储的字符长度
const int BufferBase::length() const 
{
    return data_buf != nullptr ? data_buf->length : 0;
}

//从chuck中删除len个字符，如果删除后chuck中没有字符，回收chuck
void BufferBase::pop(int len) 
{
    assert(data_buf != nullptr && len <= data_buf->length);

    data_buf->pop(len);
    if(data_buf->length == 0) {
        Mempool::get_instance().retrieve(data_buf);
        data_buf = nullptr;
    }
}

//回收chuck
void BufferBase::clear()
{
    if (data_buf != nullptr)  {
        Mempool::get_instance().retrieve(data_buf);
        data_buf = nullptr;
    }
}

//从fd对应的文件中读取数据，返回读取的字符的个数
int InputBuffer::read_from_fd(int fd)
{
    int need_read;
    // FIONREAD: get readable bytes num in kernel buffer
    if (ioctl(fd, FIONREAD, &need_read) == -1) {
        PR_ERROR("ioctl FIONREAD error\n");
        return -1;
    }
    
    if (data_buf == nullptr) {
        data_buf = Mempool::get_instance().alloc_chunk(need_read);
        if (data_buf == nullptr) {
            PR_INFO("no free buf for alloc\n");
            return -1;
        }
    }
    else {
        assert(data_buf->head == 0);
        if (data_buf->capacity - data_buf->length < (int)need_read) {   
            Chunk *new_buf = Mempool::get_instance().alloc_chunk(need_read + data_buf->length);
            if (new_buf == nullptr) {
                PR_INFO("no free buf for alloc\n");
                return -1;
            }
            new_buf->copy(data_buf);
            Mempool::get_instance().retrieve(data_buf);
            data_buf = new_buf;
        }
    }
    //开始读数据
    int already_read = 0;
    do { 
        if(need_read == 0) {
            already_read = read(fd, data_buf->data + data_buf->length, m4K);
        } else {
            already_read = read(fd, data_buf->data + data_buf->length, need_read);
        }
    } while (already_read == -1 && errno == EINTR);
    if (already_read > 0)  {
        if (need_read != 0) {
            assert(already_read == need_read);
        }
        data_buf->length += already_read;
    }

    return already_read;
}

//返回data_buf中的数据
const char *InputBuffer::get_from_buf() const 
{
    return data_buf != nullptr ? data_buf->data + data_buf->head : nullptr;
}

//调整data_buffer
void InputBuffer::adjust()
{
    if (data_buf != nullptr) {
        data_buf->adjust();
    }
}

//写data_buf
int OutputBuffer::write2buf(const char *data, int len)
{
    if (data_buf == nullptr) {
        data_buf = Mempool::get_instance().alloc_chunk(len);
        if (data_buf == nullptr) {
            PR_INFO("no free buf for alloc\n");
            return -1;
        }
    }
    else {
        assert(data_buf->head == 0);
        if (data_buf->capacity - data_buf->length < len) {
            Chunk *new_buf = Mempool::get_instance().alloc_chunk(len + data_buf->length);
            if (new_buf == nullptr) {
                PR_INFO("no free buf for alloc\n");
                return -1;
            }
            new_buf->copy(data_buf);
            Mempool::get_instance().retrieve(data_buf);
            data_buf = new_buf;
        }
    }

    memcpy(data_buf->data + data_buf->length, data, len);
    data_buf->length += len;

    return 0;
}

//写fd
int OutputBuffer::write2fd(int fd)
{
    assert(data_buf != nullptr && data_buf->head == 0);

    int already_write = 0;

    do { 
        already_write = write(fd, data_buf->data, data_buf->length);
    } while (already_write == -1 && errno == EINTR);

    if (already_write > 0) {
        data_buf->pop(already_write);
        data_buf->adjust();
    }
    //如果没有写完，则说明此时写fd没有完成
    if (already_write == -1 && errno == EAGAIN) {
        already_write = 0;
    }

    return already_write;
}