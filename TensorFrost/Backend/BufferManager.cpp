#include "BufferManager.h"

namespace TensorFrost {
    void BufferManager::DeallocateBuffer(TFBuffer *buffer) {
        used_buffers.erase(buffer);
        unused_time[buffer] = 0;
    }

    void BufferManager::RemoveBuffer(TFBuffer *buffer) {
        if(!buffers_to_delete.contains(buffer)) {
            throw std::runtime_error("Buffer not marked for deletion");
        }
        size_t size = buffer->size;
        allocated_buffers[size].erase(buffer);
        unused_time.erase(buffer);
        delete buffer;
    }

    void BufferManager::UpdateTick() {
        //increment the unused time of all buffers
        for(auto& [buffer, time]: unused_time) {
            if(time > MAX_UNUSED_TIME) {
                buffers_to_delete.insert(buffer);
            } else {
                unused_time[buffer] = time + 1;
            }
        }
    }

    TFBuffer *BufferManager::TryAllocateBuffer(size_t size) {
        //try to find a non-used buffer of the correct size
        TFBuffer* buffer = nullptr;
        bool found = false;
        //find the smallest buffer that is larger than the requested size
        size_t min_size = size;
        size_t max_size = 8 * size;
        //get iterator to the first buffer that is larger than the requested size
        auto it = allocated_buffers.lower_bound(min_size);
        //if no buffer is larger than the requested size, get the first buffer
        if(it == allocated_buffers.end()) {
            it = allocated_buffers.begin();
        }
        //iterate through the buffers
        for(; it != allocated_buffers.end(); it++) {
            if(it->first > max_size) {
                break;
            }
            if(it->first < size) {
                continue;
            }
            for(auto buf: it->second) {
                if(used_buffers.contains(buf) && !buffers_to_delete.contains(buf)) {
                    continue;
                }
                buffer = buf;
                found = true;
            }
            if(found) {
                break;
            }
        }
        //if no buffer was found, create a new one
        if(!found) {
            buffer = AllocateBuffer(size);
        }
        else {
            unused_time.erase(buffer);
        }
        used_buffers.insert(buffer);
        return buffer;
    }

    size_t BufferManager::GetRequiredAllocatedStorage() const {
        size_t total = 0;
        for(auto& [size, buffers]: allocated_buffers) {
            total += (uint32_t)size * (uint32_t)buffers.size();
        }
        return total;
    }

    size_t BufferManager::GetUnusedAllocatedStorage() const {
        size_t total = 0;
        for(auto& [size, buffers]: allocated_buffers) {
            for(auto& buffer: buffers) {
                if(!used_buffers.contains(buffer)) {
                    total += size;
                }
            }
        }
        return total;
    }

    BufferManager::~BufferManager() {
        for(auto& [size, buffers]: allocated_buffers) {
            for(auto& buffer: buffers) {
                delete buffer;
            }
        }
    }
}  // namespace TensorFrost