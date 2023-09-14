#include "Compute.h"
#include "LogUtility.h"
#include "IOUtility.h"

#include <GLM/glm.hpp>

#include <vector>
#include <format>

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

    std::unordered_map<std::string, ComputeKernel> kernels;

} compute;

#ifdef _DEBUG
#define CHECKCL(func) \
{\
    cl_int function_result = func;\
    if(function_result != CL_SUCCESS) LOGMSG(Log::MessageType::Error, get_cl_error_string(function_result));\
}
#else
#define CHECKCL(func) func;
#endif

ComputeKernel::ComputeKernel(const std::string& path, const std::string& entry_point)
    : path(path)
    , entry_point(entry_point)
{
}

void ComputeKernel::compile()
{
    state = ComputeKernelState::Empty;

    cl::Program::Sources sources;
    sources.push_back(read_file_to_string(path));
    state = ComputeKernelState::Source;

    cl::Program created_program(compute.context, sources);
    state = ComputeKernelState::Program;

    cl_int error = created_program.build({compute.device}, "-w");

    if(error != CL_SUCCESS)
    {
        LOGMSG(Log::MessageType::Error, std::format("Failed to create kernel: {} \n {}", path, created_program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(compute.device)));
        return;
    }
    state = ComputeKernelState::Built;

    LOGMSG(Log::MessageType::Debug, std::format("Created kernel: {}", path));

    cl_kernel = cl::Kernel(created_program, entry_point.c_str(), &error);

    if(error != CL_SUCCESS)
    {
        LOGMSG(Log::MessageType::Error, std::format("Failed to create kernel: {} \n {} error", path, get_cl_error_string(error)))     
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

ComputeOperation::ComputeOperation(const std::string& kernel_name)
    : kernel(&compute.kernels.find(kernel_name)->second)
{

}

ComputeOperation& ComputeOperation::write(const ComputeDataHandle& data)
{
    cl::Buffer new_write_buffer = cl::Buffer(compute.context, CL_MEM_WRITE_ONLY, data.data_byte_size);
   
    CHECKCL(compute.queue.enqueueWriteBuffer(new_write_buffer, CL_TRUE, 0, data.data_byte_size, data.data_ptr));

    write_buffers.push_back(std::move(new_write_buffer));
    
    kernel->cl_kernel.setArg(arg_count, write_buffers.back());

    arg_count++;
    return *this;
}

ComputeOperation& ComputeOperation::read(const ComputeDataHandle& data)
{
    // Create and push new buffer
    cl::Buffer new_read_buffer = cl::Buffer(compute.context, CL_MEM_READ_ONLY, data.data_byte_size);

    read_buffers.push_back({data.data_ptr, data.data_byte_size,std::move(new_read_buffer)});

    kernel->cl_kernel.setArg(arg_count, read_buffers.back().buffer);

    arg_count++;
    return *this;
}

ComputeOperation& ComputeOperation::read_write(const ComputeDataHandle& data)
{
    // Create and push new buffer
    cl::Buffer new_read_write_buffer = cl::Buffer(compute.context, CL_MEM_READ_WRITE, data.data_byte_size);

    write_buffers.push_back(new_read_write_buffer);
    read_buffers.push_back({data.data_ptr, data.data_byte_size, new_read_write_buffer});

    CHECKCL(compute.queue.enqueueWriteBuffer(new_read_write_buffer, CL_TRUE, 0, data.data_byte_size, data.data_ptr));
    
    kernel->cl_kernel.setArg(arg_count, new_read_write_buffer);

    arg_count++;
    return *this;
}

ComputeOperation& ComputeOperation::local_dispatch(glm::ivec3 size)
{
    local_dispatch_size = size;
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

    CHECKCL(compute.queue.enqueueNDRangeKernel(kernel->cl_kernel, cl::NullRange, 
    cl::NDRange(global_dispatch_size.x, global_dispatch_size.y, global_dispatch_size.z), 
    cl::NullRange));

    CHECKCL(compute.queue.finish());

    for(auto& buffer : read_buffers)
    {
         CHECKCL(compute.queue.enqueueReadBuffer(buffer.buffer, CL_TRUE, 0, buffer.data_byte_count, buffer.data_destination));
    }
}

void Compute::create_kernel(const std::string& path, const std::string& entry_point)
{
    std::string file_name_with_extension = path.substr(path.find_last_of("\\/") + 1);
    if(compute.kernels.find(file_name_with_extension) != compute.kernels.end())
    {
        LOGMSG(Log::MessageType::Error, std::format("Kernel {} already exists, ignoring", file_name_with_extension));
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
        LOGMSG(Log::MessageType::Error, "No platforms found. Check OpenCL installation!");
        exit(1);
    }
    else
    {
        compute.platform = all_platforms[0];
        LOGMSG(Log::MessageType::Debug, std::format("Using platform: {}", compute.platform.getInfo<CL_PLATFORM_NAME>()));
    }
}

void select_device()
{
    std::vector<cl::Device> all_devices;
    compute.platform.getDevices(CL_DEVICE_TYPE_GPU, &all_devices);

    if(all_devices.empty())
    {
        LOGMSG(Log::MessageType::Error, "No suitable devices found. Check OpenCL installation!");
        exit(1);
    }
    else
    {
        compute.device = all_devices[0];
        LOGMSG(Log::MessageType::Debug, std::format("Using device: {}", compute.device.getInfo<CL_DEVICE_NAME>()));
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
  
    // Example
    /*
    create_kernel("C:/Users/Matt/Desktop/AdvGfx/AdvGfx/compute/kernel.cl", "simple_add");

    std::vector<int> A = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<int> B = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0};
    std::vector<int> C;
    C.resize(10);

    ComputeOperation("kernel.cl")
        .write(A)
        .write(B)
        .read(C)
        .global_dispatch({10, 1, 1})
        .execute();

    for(int i=0; i<10; i++)
    {
        printf("got: %i \n", C[i]);
    }
    */
}

void Compute::recompile_kernels(ComputeKernelRecompilationCondition condition)
{
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
        }
    }
}