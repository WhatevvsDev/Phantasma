#include "Compute.h"
#include "LogUtility.h"

#define CL_HPP_TARGET_OPENCL_VERSION 300
#include <CL/opencl.hpp>

#include <vector>
#include <format>
#include <iostream>
#include <fstream>

struct
{
    cl::Context context;
    cl::Device device;
    cl::Platform platform; // Driver
    cl::CommandQueue queue;

    std::unordered_map<std::string, cl::Kernel> kernels;

} compute;

// Init should look for them, and make them automagically
void Compute::create_kernel(const std::string& path)
{
    std::string source_string;
    std::ifstream program_source;
    program_source.open (path);
    program_source >> source_string;
    program_source.close();
    
    const char* source_string_ptr[] = { source_string.c_str() };

    cl_int error;

    cl_program created_program = clCreateProgramWithSource(compute.context.get(), 1, source_string_ptr, nullptr, &error);

    if(error != CL_SUCCESS)
    {
        LOGMSG(Log::MessageType::Error, std::format("Failed to create kernel: {}", path))
    }
    else
    {
        LOGMSG(Log::MessageType::Debug, std::format("Created kernel: {}", path))

        std::string name = path.substr(path.find_last_of("\\/"));
        cl::Kernel kernel = cl::Kernel(cl::Program(created_program), name.c_str());

        compute.kernels.insert({name, kernel});
    }
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

void Compute::init()
{
    select_platform();
    select_device();

    compute.context = (compute.device);
     
    cl::Program::Sources sources;
    
    create_kernel("C:/Users/Matt/Desktop/AdvGfx/AdvGfx/compute/kernel.cl");
     
    //// create buffers on the device
    //cl::Buffer buffer_A(compute.context,CL_MEM_READ_WRITE,sizeof(int)*10);
    //cl::Buffer buffer_B(compute.context,CL_MEM_READ_WRITE,sizeof(int)*10);
    //cl::Buffer buffer_C(compute.context,CL_MEM_READ_WRITE,sizeof(int)*10);
    // 
    //int A[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    //int B[] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0};
    // 
    ////create queue to which we will push commands for the device.
    //compute.queue = cl::CommandQueue(compute.context, compute.device);
    // 
    ////write arrays A and B to the device
    //compute.queue.enqueueWriteBuffer(buffer_A,CL_TRUE,0,sizeof(int)*10,A);
    //kernel_add.setArg(0,buffer_A);

    //compute.queue.enqueueWriteBuffer(buffer_B,CL_TRUE,0,sizeof(int)*10,B);
    // 
    // 
    ////run the kernel
    //cl::Kernel kernel_add = cl::Kernel(program, "simple_add");
    //kernel_add.setArg(0,buffer_A);
    //kernel_add.setArg(1,buffer_B);
    //kernel_add.setArg(2,buffer_C);
    //compute.queue.enqueueNDRangeKernel(kernel_add,cl::NullRange,cl::NDRange(10),cl::NullRange);
    //compute.queue.finish();
    // 
    //int C[10];

    ////read result C from the device to array C
    //compute.queue.enqueueReadBuffer(buffer_C,CL_TRUE,0,sizeof(int)*10,C);
}