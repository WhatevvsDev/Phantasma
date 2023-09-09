#pragma once
#include <unordered_map>
#include <string>

struct ComputeOperation
{
    ComputeOperation(const std::string& kernel_name);

    template<typename T>
    ComputeOperation& write(const std::vector<T>& data);

    // Data should already be resized to accomodate data!
    // For now only supports one read buffer.
    template<typename T>
    ComputeOperation& read(const std::vector<T>& data);

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