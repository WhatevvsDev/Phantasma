#include "Compute.h"

const char *get_cl_error_string(cl_int error)
{
switch(error){
    // run-time and JIT compiler errors
    case 0: return "CL_SUCCESS";
    case -1: return "CL_DEVICE_NOT_FOUND";
    case -2: return "CL_DEVICE_NOT_AVAILABLE";
    case -3: return "CL_COMPILER_NOT_AVAILABLE";
    case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case -5: return "CL_OUT_OF_RESOURCES (SEG_FAULT on NVIDIA platforms, possibly too much recursion)";
    case -6: return "CL_OUT_OF_HOST_MEMORY";
    case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case -8: return "CL_MEM_COPY_OVERLAP";
    case -9: return "CL_IMAGE_FORMAT_MISMATCH";
    case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case -11: return "CL_BUILD_PROGRAM_FAILURE";
    case -12: return "CL_MAP_FAILURE";
    case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
    case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
    case -15: return "CL_COMPILE_PROGRAM_FAILURE";
    case -16: return "CL_LINKER_NOT_AVAILABLE";
    case -17: return "CL_LINK_PROGRAM_FAILURE";
    case -18: return "CL_DEVICE_PARTITION_FAILED";
    case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

    // compile-time errors
    case -30: return "CL_INVALID_VALUE";
    case -31: return "CL_INVALID_DEVICE_TYPE";
    case -32: return "CL_INVALID_PLATFORM";
    case -33: return "CL_INVALID_DEVICE";
    case -34: return "CL_INVALID_CONTEXT";
    case -35: return "CL_INVALID_QUEUE_PROPERTIES";
    case -36: return "CL_INVALID_COMMAND_QUEUE";
    case -37: return "CL_INVALID_HOST_PTR";
    case -38: return "CL_INVALID_MEM_OBJECT";
    case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case -40: return "CL_INVALID_IMAGE_SIZE";
    case -41: return "CL_INVALID_SAMPLER";
    case -42: return "CL_INVALID_BINARY";
    case -43: return "CL_INVALID_BUILD_OPTIONS";
    case -44: return "CL_INVALID_PROGRAM";
    case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
    case -46: return "CL_INVALID_KERNEL_NAME";
    case -47: return "CL_INVALID_KERNEL_DEFINITION";
    case -48: return "CL_INVALID_KERNEL";
    case -49: return "CL_INVALID_ARG_INDEX";
    case -50: return "CL_INVALID_ARG_VALUE";
    case -51: return "CL_INVALID_ARG_SIZE";
    case -52: return "CL_INVALID_KERNEL_ARGS";
    case -53: return "CL_INVALID_WORK_DIMENSION";
    case -54: return "CL_INVALID_WORK_GROUP_SIZE";
    case -55: return "CL_INVALID_WORK_ITEM_SIZE";
    case -56: return "CL_INVALID_GLOBAL_OFFSET";
    case -57: return "CL_INVALID_EVENT_WAIT_LIST";
    case -58: return "CL_INVALID_EVENT";
    case -59: return "CL_INVALID_OPERATION";
    case -60: return "CL_INVALID_GL_OBJECT";
    case -61: return "CL_INVALID_BUFFER_SIZE";
    case -62: return "CL_INVALID_MIP_LEVEL";
    case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
    case -64: return "CL_INVALID_PROPERTY";
    case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
    case -66: return "CL_INVALID_COMPILER_OPTIONS";
    case -67: return "CL_INVALID_LINKER_OPTIONS";
    case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";

    // extension errors
    case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
    case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
    case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
    case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
    case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
    case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
    default: return "Unknown OpenCL error";
    }
}

struct
{
    cl::Context context;
    cl::Device device;
    cl::Platform platform; // Driver
    cl::CommandQueue queue;
    std::string common_source { "" };
    FILETIME common_source_last_write_time;

    std::unordered_map<std::string, ComputeKernel> kernels;

} compute;

#ifdef _DEBUG
#define CHECKCL(func) \
{\
    cl_int function_result = func;\
    if(function_result != CL_SUCCESS) LOGERROR(get_cl_error_string(function_result));\
}
#else
#define CHECKCL(func) func;
#endif

void load_common_shader_source()
{
    std::string expected_common_path = get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\compute\\common.cl";

    WIN32_FILE_ATTRIBUTE_DATA fileData;

    GetFileAttributesExA(expected_common_path.c_str(), GetFileExInfoStandard, &fileData);

    FILETIME current_write_time = fileData.ftLastWriteTime;

    bool common_source_updated = CompareFileTime(&current_write_time, &compute.common_source_last_write_time) != 0;

    compute.common_source_last_write_time = current_write_time;

    if(!common_source_updated)
        return;

    compute.common_source = read_file_to_string(expected_common_path);

    if(compute.common_source.length() == 0)
    {
        LOGDEFAULT("Could not find shader common source, or source was empty!");
    }
    else
    {
        LOGDEBUG("Loaded shader common source.");
    }
}

ComputeKernel::ComputeKernel(const std::string& path, const std::string& entry_point)
    : path(path)
    , entry_point(entry_point)
{
    WIN32_FILE_ATTRIBUTE_DATA fileData;

    GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileData);

    last_write_time = fileData.ftLastWriteTime;
}

void ComputeKernel::compile()
{
    ComputeKernelState previous_state = state;
    state = ComputeKernelState::Empty;

    cl::Program::Sources sources;

    // First try to add common source, then add shader-specific source
    load_common_shader_source();
    bool common_shader_source_exists = compute.common_source.length() != 0;

    if(common_shader_source_exists)
    {
        sources.push_back(compute.common_source);
    }

    sources.push_back(read_file_to_string(path));


    state = ComputeKernelState::Source;

    cl::Program created_program(compute.context, sources);
    state = ComputeKernelState::Program;

    cl_int error = created_program.build({compute.device}, "-w");

    if(error != CL_SUCCESS)
    {
        LOGERROR(std::format("Failed to create kernel: {} \n {}", path, created_program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(compute.device)));
        return;
    }
    state = ComputeKernelState::Built;

    if(previous_state != ComputeKernelState::Empty)
    {
        LOGDEBUG(std::format("Recompiled kernel: {}", path));
    }
    else
    {
        LOGDEBUG(std::format("Created kernel: {} {}", get_file_name_from_path_string(path), common_shader_source_exists ? " - With common source" : ""));
    }

    cl_kernel = cl::Kernel(created_program, entry_point.c_str(), &error);

    if(error != CL_SUCCESS)
    {
        LOGERROR(std::format("Failed to create kernel: {} \n {} error", path, get_cl_error_string(error)))     
        return;
    }

    state = ComputeKernelState::Compiled;
}

bool ComputeKernel::is_valid()
{
    return state == ComputeKernelState::Compiled;
}

bool ComputeKernel::has_been_changed()
{
    WIN32_FILE_ATTRIBUTE_DATA fileData;

    GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileData);

    FILETIME current_write_time = fileData.ftLastWriteTime;

    bool shader_updated = CompareFileTime(&current_write_time, &last_write_time) == 1;

    last_write_time = current_write_time;

    return shader_updated;
}

ComputeReadBuffer::ComputeReadBuffer(const ComputeDataHandle& data)
    : internal_buffer(cl::Buffer(compute.context, CL_MEM_READ_ONLY, data.data_byte_size))
    , data_handle(data)
{
        
}

ComputeWriteBuffer::ComputeWriteBuffer(const ComputeDataHandle& data)
    : internal_buffer(cl::Buffer(compute.context, CL_MEM_WRITE_ONLY, data.data_byte_size))
{
    // Automatically write itself to GPU
    CHECKCL(compute.queue.enqueueWriteBuffer(internal_buffer, CL_TRUE, 0, data.data_byte_size, data.data_ptr));
    compute.queue.finish();
}

void ComputeWriteBuffer::update(const ComputeDataHandle& data)
{
    CHECKCL(compute.queue.enqueueWriteBuffer(internal_buffer, CL_TRUE, 0, data.data_byte_size, data.data_ptr));
    compute.queue.finish();
}

ComputeReadWriteBuffer::ComputeReadWriteBuffer(const ComputeDataHandle& data)
    : internal_buffer(cl::Buffer(compute.context, CL_MEM_READ_WRITE, data.data_byte_size))
    , data_handle(data)
{
    
}

ComputeGPUOnlyBuffer::ComputeGPUOnlyBuffer(size_t data_size)
    : internal_buffer(cl::Buffer(compute.context, CL_MEM_HOST_NO_ACCESS, data_size))
{
    
}

ComputeOperation::ComputeOperation(const std::string& kernel_name)
    : kernel(&compute.kernels.find(kernel_name)->second)
{

}

ComputeOperation& ComputeOperation::write(const ComputeGPUOnlyBuffer& buffer)
{
    kernel->cl_kernel.setArg(arg_count, buffer.internal_buffer);

    arg_count++;
    return *this;
}

ComputeOperation& ComputeOperation::read_write(const ComputeGPUOnlyBuffer& buffer)
{
    kernel->cl_kernel.setArg(arg_count, buffer.internal_buffer);

    arg_count++;
    return *this;
}

ComputeOperation& ComputeOperation::write(const ComputeDataHandle& data)
{
    // Create and push new temporary buffer
    ComputeWriteBuffer cwb(data);

    write_buffers_non_persistent.push_back(std::move(cwb));
    auto& cwb_ref = write_buffers_non_persistent.back();
    
    CHECKCL(compute.queue.enqueueWriteBuffer(cwb_ref.internal_buffer, CL_TRUE, 0, data.data_byte_size, data.data_ptr));

    kernel->cl_kernel.setArg(arg_count, cwb_ref);

    arg_count++;
    return *this;
}

ComputeOperation& ComputeOperation::write(const ComputeWriteBuffer& buffer)
{    
    kernel->cl_kernel.setArg(arg_count, buffer.internal_buffer);

    arg_count++;
    return *this;
}

ComputeOperation& ComputeOperation::read(const ComputeReadBuffer& buffer)
{
    read_buffers.push_back(&buffer);

    kernel->cl_kernel.setArg(arg_count, buffer.internal_buffer);

    arg_count++;
    return *this;
}

ComputeOperation& ComputeOperation::read(const ComputeGPUOnlyBuffer& buffer)
{
    kernel->cl_kernel.setArg(arg_count, buffer.internal_buffer);

    arg_count++;
    return *this;
}

ComputeOperation& ComputeOperation::read_write(const ComputeReadWriteBuffer& buffer)
{
    // Push buffer
    readwrite_buffers.push_back(&buffer);

    CHECKCL(compute.queue.enqueueWriteBuffer(buffer.internal_buffer, CL_TRUE, 0, buffer.data_handle.data_byte_size, buffer.data_handle.data_ptr));
    
    kernel->cl_kernel.setArg(arg_count, buffer.internal_buffer);

    arg_count++;
    return *this;
}

ComputeOperation& ComputeOperation::global_dispatch(glm::ivec3 size)
{
    global_dispatch_size = size;
    return *this;
}

void ComputeOperation::execute()
{
    if(!kernel->is_valid())
        return;

    cl::NDRange global = cl::NDRange(global_dispatch_size.x, global_dispatch_size.y, global_dispatch_size.z);
    cl::NDRange local = cl::NullRange;

    CHECKCL(compute.queue.enqueueNDRangeKernel(kernel->cl_kernel, cl::NullRange, global, local));

    CHECKCL(compute.queue.finish());

    for(auto& buffer : read_buffers)
    {
         CHECKCL(compute.queue.enqueueReadBuffer(buffer->internal_buffer, CL_TRUE, 0, buffer->data_handle.data_byte_size, buffer->data_handle.data_ptr));
    }
    for(auto& buffer : readwrite_buffers)
    {
         CHECKCL(compute.queue.enqueueReadBuffer(buffer->internal_buffer, CL_TRUE, 0, buffer->data_handle.data_byte_size, buffer->data_handle.data_ptr));
    }
}

void Compute::create_kernel(const std::string& path, const std::string& entry_point)
{
    std::string file_name_with_extension = path.substr(path.find_last_of("\\/") + 1);
    if(compute.kernels.find(file_name_with_extension) != compute.kernels.end())
    {
        LOGERROR(std::format("Kernel {} already exists, ignoring", file_name_with_extension));
        return;
    }

    auto ref = compute.kernels.insert({file_name_with_extension, ComputeKernel(path, entry_point)});

    // Attempt to compile the newly created kernel
    ref.first->second.compile();
}

void select_platform()
{
    //get all platforms (drivers)
    std::vector<cl::Platform> all_platforms;
    cl::Platform::get(&all_platforms);

    if(all_platforms.empty())
    {   
        LOGERROR("No platforms found. Check OpenCL installation!");
        exit(1);
    }
    else
    {
        compute.platform = all_platforms[0];
        LOGDEBUG(std::format("Using platform: {}", compute.platform.getInfo<CL_PLATFORM_NAME>()));
    }
}

void select_device()
{
    std::vector<cl::Device> all_devices;
    compute.platform.getDevices(CL_DEVICE_TYPE_GPU, &all_devices);

    if(all_devices.empty())
    {
        LOGERROR("No suitable devices found. Check OpenCL installation!");
        exit(1);
    }
    else
    {
        compute.device = all_devices[0];
        LOGDEBUG(std::format("Using device: {}", compute.device.getInfo<CL_DEVICE_NAME>()));
    }
}

void get_context_and_command_queue()
{
    compute.context = (compute.device);
    compute.queue = cl::CommandQueue(compute.context, compute.device);
}

void Compute::init()
{
    select_platform();
    select_device();
    get_context_and_command_queue();
    load_common_shader_source();
}

// Returns true if any kernels have been recompiled
bool Compute::recompile_kernels(ComputeKernelRecompilationCondition condition)
{
    bool recompiled_any = false;

    for(auto& [key, kernel] : compute.kernels)
    {
        bool recompile_kernel = false;

        switch(condition)
        {
        case ComputeKernelRecompilationCondition::Force:
            recompile_kernel = true;
            break;
        case ComputeKernelRecompilationCondition::SourceChanged:
            recompile_kernel = kernel.has_been_changed();
            break;
        }

        if(recompile_kernel)
        {
            kernel.compile();
            recompiled_any = true;
        }
    }
    return recompiled_any;
}

bool Compute::kernel_exists(const std::string& kernel_name)
{
    return compute.kernels.find(kernel_name) != compute.kernels.end();
}