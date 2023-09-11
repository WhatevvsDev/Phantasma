#pragma once
#include <unordered_map>
#include <string>
#include <array>

#define CL_HPP_TARGET_OPENCL_VERSION 300
#include <CL/opencl.hpp>
#include <glm/glm.hpp>

// To deal with templated c++ garbage
// Abstracts over any type of vector
struct ComputeDataHandle
{
    template<typename T>
    inline ComputeDataHandle(const std::vector<T>& data)
    {
        size_t data_size = sizeof(T);
        size_t data_count = data.size();

        data_byte_size = data_size * data_count;
        data_ptr = (void*)data.data();
    }

    template<typename T>
    inline ComputeDataHandle(T* data, const size_t count)
    {
        data_byte_size = count * sizeof(data[0]);
        data_ptr = data;
    }

    friend struct ComputeOperation;
private:
    void* data_ptr;
    size_t data_byte_size;
};

struct ComputeOperation
{
    ComputeOperation(const std::string& kernel_name);

    ComputeOperation& write(const ComputeDataHandle& data);

    // Data should already be resized to accomodate data!
    // For now only supports one read buffer.
    ComputeOperation& read(const ComputeDataHandle& data);

    ComputeOperation& local_dispatch(glm::ivec3 size);

    ComputeOperation& global_dispatch(glm::ivec3 size);

    void execute();
    
private:
    struct read_destination
    {
        void* data_destination;
        size_t data_byte_count;
        cl::Buffer buffer;
    };

    int arg_count { 0 };

    glm::ivec3 local_dispatch_size{1, 1, 1};
    glm::ivec3 global_dispatch_size{1, 1, 1};

    cl::Kernel& kernel;

    std::vector<cl::Buffer> write_buffers;
    std::vector<read_destination> read_buffers;
};

namespace Compute
{
	void init();

	void create_kernel(const std::string& path, const std::string& entry_point);
}