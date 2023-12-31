#pragma once

enum class ComputeKernelState
{
	Empty,
	Source,
	Program,
	Built,
	Compiled,
	RANGE
};

enum class ComputeKernelRecompilationCondition
{
	Force,
	SourceChanged
};

// To allow recompilation at runtime
struct ComputeKernel
{
	ComputeKernel(const std::string& path, const std::string& entry_point);
	void compile();
	bool is_valid();
	bool has_been_changed();

	std::string path;
	std::string entry_point;

	friend struct ComputeOperation;

private:
	FILETIME last_write_time {};
	ComputeKernelState state { ComputeKernelState::Empty };
	cl::Kernel cl_kernel;
	
};

// To deal with templated c++ garbage
// Abstracts over any type of vector
struct ComputeDataHandle
{
	inline ComputeDataHandle() {};

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
	friend struct ComputeReadBuffer;
	friend struct ComputeWriteBuffer;
	friend struct ComputeReadWriteBuffer;
private:
	void* data_ptr { nullptr };
	size_t data_byte_size { 0 };
};

struct ComputeReadBuffer
{
	ComputeReadBuffer(const ComputeDataHandle& data);

	friend struct ComputeOperation;
private:
	cl::Buffer internal_buffer;
	ComputeDataHandle data_handle;
};

struct ComputeWriteBuffer
{
	ComputeWriteBuffer(const ComputeDataHandle& data);
	void update(const ComputeDataHandle& data);

	friend struct ComputeOperation;
private:
	cl::Buffer internal_buffer;
};

struct ComputeReadWriteBuffer
{
	ComputeReadWriteBuffer(const ComputeDataHandle& data);

	friend struct ComputeOperation;
private:
	cl::Buffer internal_buffer;
	ComputeDataHandle data_handle;
};

struct ComputeGPUOnlyBuffer
{
	ComputeGPUOnlyBuffer(size_t data_size);

	cl::Buffer internal_buffer;
};

struct ComputeOperation
{
	ComputeOperation(const std::string& kernel_name);

	ComputeOperation& write(const ComputeDataHandle& data);

	ComputeOperation& write(const ComputeWriteBuffer& buffer);

	ComputeOperation& write(const ComputeGPUOnlyBuffer& buffer);

	// Data should already be resized to accomodate data!
	// Buffer should not be created inline
	ComputeOperation& read(const ComputeReadBuffer& buffer);

	ComputeOperation& read(const ComputeGPUOnlyBuffer& buffer);

	ComputeOperation& read_write(const ComputeReadWriteBuffer& buffer);

	ComputeOperation& read_write(const ComputeGPUOnlyBuffer& buffer);

	ComputeOperation& global_dispatch(glm::ivec3 size);

	void execute();
	
private:
	struct read_destination
	{
		ComputeDataHandle data_handle;
		cl::Buffer buffer;
	};

	int arg_count { 0 };

	glm::ivec3 global_dispatch_size{1, 1, 1};

	ComputeKernel* kernel { nullptr };

	std::vector<ComputeWriteBuffer> write_buffers_non_persistent;
	std::vector<ComputeReadBuffer const *> read_buffers;
	std::vector<ComputeReadWriteBuffer const *> readwrite_buffers;

};

namespace Compute
{
	void init();

	void create_kernel(const std::string& path, const std::string& entry_point);

	bool recompile_kernels(ComputeKernelRecompilationCondition condition);

	bool kernel_exists(const std::string& kernel_name);
}