#include "TensorMemory.h"

namespace TensorFrost {

size_t GetLinearSize(const vector<size_t>& shape) {
	size_t size = 1;
	for (size_t dim : shape) {
		size *= dim;
	}
	return size;
}

vector<size_t> GetShape(const TFTensor *tensor) {
	vector<size_t> shape;
	for (size_t i = 0; i < tensor->dim; i++) {
		shape.push_back(tensor->shape[i]);
	}
	return shape;
}

size_t GetSize(const TFTensor *tensor) {
	size_t size = 1;
	for (size_t i = 0; i < tensor->dim; i++) {
		size *= tensor->shape[i];
	}
	return size;
}

TFBuffer * TensorMemoryManager::AllocateBuffer(size_t size) {
    TFBuffer* buffer = CreateBuffer(size);
    //add the buffer to the list of allocated buffers
    allocated_buffers[size].insert(buffer);
    return buffer;
}

TFTensor * TensorMemoryManager::Allocate(const vector<size_t> &shape, const TFType type, bool read_only) {
    size_t size = GetLinearSize(shape);

    if (size == 0) {
        throw invalid_argument("Trying to allocate a tensor with size 0");
    }

    TFBuffer* buf = TryAllocateBuffer(size);
    buf->read_only = read_only;
    return MakeTensor(shape, buf, type);
}

TFTensor * TensorMemoryManager::AllocateWithData(const vector<size_t> &shape, const vector<uint32_t> &data,
    const TFType type, bool read_only) {
    TFTensor* tensor_memory = Allocate(shape, type);
    SetDataAtOffset(tensor_memory, 0, data);
    return tensor_memory;
}

void TensorMemoryManager::Free(TFTensor tensor) {
    DeallocateBuffer(tensor.buffer);
}

TFTensor * TensorMemoryManager::MakeTensor(size_t *shape, size_t dim, TFBuffer *buf, TFType type) {
    TFTensor* tensor = new TFTensor();
    tensor->buffer = buf;
    tensor->dim = dim;
    tensor->shape = shape;
    tensor->type = type;
    return tensor;
}

TFTensor * TensorMemoryManager::MakeTensor(const vector<size_t> &shape, TFBuffer *buf, TFType type) {
    size_t* shape_arr = new size_t[shape.size()];
    std::copy(shape.begin(), shape.end(), shape_arr);
    return MakeTensor(shape_arr, shape.size(), buf, type);
}

size_t TensorMemoryManager::GetAllocatedSize() const {
    size_t total = 0;
    for(auto& [size, buffers]: allocated_buffers) {
        total += size * buffers.size();
    }
    return total;
}

size_t TensorMemoryManager::GetUnusedAllocatedSize() const {
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

void TensorMemoryManager::DeallocateBuffer(TFBuffer *buffer) {
    used_buffers.erase(buffer);
    unused_time[buffer] = 0;
}

void TensorMemoryManager::RemoveBuffer(TFBuffer *buffer) {
    size_t size = buffer->size;
    allocated_buffers[size].erase(buffer);
    unused_time.erase(buffer);
    DeleteBuffer(buffer);
}

void TensorMemoryManager::UpdateTick() {
    unordered_set<TFBuffer*> buffers_to_delete;

    //increment the unused time of all buffers
    for(auto& [buffer, time]: unused_time) {
        if(time > MAX_UNUSED_TIME) {
            buffers_to_delete.insert(buffer);
        } else {
            unused_time[buffer] = time + 1;
        }
    }

    //delete all buffers that are marked for deletion
    for(auto& buffer: buffers_to_delete) {
        RemoveBuffer(buffer);
    }
}

TFBuffer *TensorMemoryManager::TryAllocateBuffer(size_t size) {
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
            if(used_buffers.contains(buf)) {
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
    } else {
        unused_time.erase(buffer);
    }
    used_buffers.insert(buffer);
    UpdateTick();
    return buffer;
}

TensorMemoryManager::~TensorMemoryManager() {
    for(auto& [size, buffers]: allocated_buffers) {
        for(auto& buffer: buffers) {
            DeleteBuffer(buffer);
        }
    }
}

TensorMemoryManager* global_memory_manager = nullptr;

}  // namespace TensorFrost