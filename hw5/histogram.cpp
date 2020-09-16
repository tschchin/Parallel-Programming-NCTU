#include <fstream>
#include <iostream>
#include <string>
#include <ios>
#include <vector>
#include <string.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

cl_program load_program(cl_context context, const char* filename)
{
    std::ifstream in(filename, std::ios_base::binary);
    if(!in.good()) {
        return 0;
    }

    // get file length
    in.seekg(0, std::ios_base::end);
    size_t length = in.tellg();
    in.seekg(0, std::ios_base::beg);

    // read program source
    std::vector<char> data(length + 1);
    in.read(&data[0], length);
    data[length] = 0;

    // create and build program 
    const char* source = &data[0];
    cl_program program = clCreateProgramWithSource(context, 1, &source, 0, 0);
    if(program == 0) {
        return 0;
    }

    if(clBuildProgram(program, 0, 0, 0, 0, 0) != CL_SUCCESS) {
        return 0;
    }

    return program;
}

typedef struct
{
    uint8_t R;
    uint8_t G;
    uint8_t B;
    uint8_t align;
} RGB;

typedef struct
{
    bool type;
    uint32_t size;
    uint32_t height;
    uint32_t weight;
    uint16_t depth;
    RGB *data;
    uint8_t *pixel;
} Image;

Image *readbmp(const char *filename)
{
    FILE *bmp;
    bmp = fopen(filename, "rb");
    char header[54];
    fread(header, sizeof(char), 54, bmp);
    uint32_t size = *(int *)&header[2];
    uint32_t offset = *(int *)&header[10];
    uint32_t w = *(int *)&header[18];
    uint32_t h = *(int *)&header[22];
    uint16_t depth = *(uint16_t *)&header[28];
    if (depth != 24 && depth != 32)
    {
        printf("we don't suppot depth with %d\n", depth);
        exit(0);
    }
    fseek(bmp, offset, SEEK_SET);

    Image *ret = new Image();
    ret->type = 1;
    ret->height = h;
    ret->weight = w;
    ret->size = w * h;
    ret->depth = depth;
    if (depth == 32) {
        ret->pixel = new uint8_t[w * h * 4]{};
        fread(ret->pixel, sizeof(uint8_t), ret->size*4, bmp);
    }
    else if (depth == 24) {
        ret->pixel = new uint8_t[w * h * 3]{};
        fread(ret->pixel, sizeof(uint8_t), ret->size*3, bmp);
    }
    fclose(bmp);
    return ret;
}

int writebmp(const char *filename, Image *img)
{

    uint8_t header[54] = {
        0x42,        // identity : B
        0x4d,        // identity : M
        0, 0, 0, 0,  // file size
        0, 0,        // reserved1
        0, 0,        // reserved2
        54, 0, 0, 0, // RGB data offset
        40, 0, 0, 0, // struct BITMAPINFOHEADER size
        0, 0, 0, 0,  // bmp width
        0, 0, 0, 0,  // bmp height
        1, 0,        // planes
        32, 0,       // bit per pixel
        0, 0, 0, 0,  // compression
        0, 0, 0, 0,  // data size
        0, 0, 0, 0,  // h resolution
        0, 0, 0, 0,  // v resolution
        0, 0, 0, 0,  // used colors
        0, 0, 0, 0   // important colors
    };

    // file size
    uint32_t file_size = img->size * 4 + 54;
    header[2] = (unsigned char)(file_size & 0x000000ff);
    header[3] = (file_size >> 8) & 0x000000ff;
    header[4] = (file_size >> 16) & 0x000000ff;
    header[5] = (file_size >> 24) & 0x000000ff;

    // width
    uint32_t width = img->weight;
    header[18] = width & 0x000000ff;
    header[19] = (width >> 8) & 0x000000ff;
    header[20] = (width >> 16) & 0x000000ff;
    header[21] = (width >> 24) & 0x000000ff;

    // height
    uint32_t height = img->height;
    header[22] = height & 0x000000ff;
    header[23] = (height >> 8) & 0x000000ff;
    header[24] = (height >> 16) & 0x000000ff;
    header[25] = (height >> 24) & 0x000000ff;

    std::ofstream fout;
    fout.open(filename, std::ios::binary);
    fout.write((char *)header, 54);
    fout.write((char *)img->data, img->size * 4);
    fout.close();
}

int main(int argc, char *argv[])
{
    char *filename;
    if (argc >= 2)
    {
        // -- Setup OpenCL --- 
        cl_int err;
        cl_uint num;
        
        err = clGetPlatformIDs(0, 0, &num);
        if(err != CL_SUCCESS) {
            std::cerr << "Unable to get platforms\n";
            return 0;
        }

        std::vector<cl_platform_id> platforms(num);
        err = clGetPlatformIDs(num, &platforms[0], &num);
        if(err != CL_SUCCESS) {
            std::cerr << "Unable to get platform ID\n";
            return 0;
        }

        cl_context_properties prop[] = { CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(platforms[0]), 0 };
        cl_context context = clCreateContextFromType(prop, CL_DEVICE_TYPE_DEFAULT, NULL, NULL, NULL);
        if(context == 0) {
            std::cerr << "Can't create OpenCL context\n";
            return 0;
        }

        size_t cb;
        clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, NULL, &cb);
        std::vector<cl_device_id> devices(cb / sizeof(cl_device_id));
        clGetContextInfo(context, CL_CONTEXT_DEVICES, cb, &devices[0], 0);

        clGetDeviceInfo(devices[0], CL_DEVICE_NAME, 0, NULL, &cb);
        std::string devname;
        devname.resize(cb);
        clGetDeviceInfo(devices[0], CL_DEVICE_NAME, cb, &devname[0], 0);
        //std::cout << "Device: " << devname.c_str() << "\n";

        cl_command_queue queue = clCreateCommandQueue(context, devices[0], 0, 0);
        if(queue == 0) {
            std::cerr << "Can't create command queue\n";
            clReleaseContext(context);
            return 0;
        }

        
        cl_program program = load_program(context, "histogram.cl");
        if(program == 0) {
            std::cerr << "Can't load or build program\n";
            clReleaseCommandQueue(queue);
            clReleaseContext(context);
            return 0;
        }

        cl_kernel kernel_core = clCreateKernel(program, "histogram", 0);
        if(kernel_core == 0) {
            std::cerr << "Can't load kernel\n";
            clReleaseProgram(program);
            clReleaseCommandQueue(queue);
            clReleaseContext(context);
            return 0;
        }

        // -- End Setup OpenCL --

        uint32_t R[256] = {0};
        uint32_t G[256] = {0};
        uint32_t B[256] = {0};

        int many_img = argc - 1;
        for (int i = 0; i < many_img; i++)
        {
            filename = argv[i + 1];
            Image *img = readbmp(filename);

            std::cout << img->weight << ":" << img->height << "\n";

            std::fill(R, R+256, 0);
            std::fill(G, G+256, 0);
            std::fill(B, B+256, 0);

            int data_size = img->size;

            int rgb_len;
            if (img->depth == 24) {
                rgb_len = 3;
            }
            else if (img->depth == 32) {
                rgb_len = 4;
            }

            
            // -- Setup openCL memory -- 
            cl_mem cl_img = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(uint8_t) * data_size * rgb_len, NULL, NULL);
            clEnqueueWriteBuffer(queue, cl_img, CL_FALSE, 0, sizeof(uint8_t) * data_size * rgb_len, img->pixel, 0, NULL, NULL);
           
            cl_mem cl_R = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(uint32_t) * 256, NULL, NULL);
            cl_mem cl_G = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(uint32_t) * 256, NULL, NULL);
            cl_mem cl_B = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(uint32_t) * 256, NULL, NULL);
            clEnqueueWriteBuffer(queue, cl_R, CL_FALSE, 0, sizeof(uint32_t) * 256, R, 0, NULL, NULL);
            clEnqueueWriteBuffer(queue, cl_G, CL_FALSE, 0, sizeof(uint32_t) * 256, G, 0, NULL, NULL);
            clEnqueueWriteBuffer(queue, cl_B, CL_FALSE, 0, sizeof(uint32_t) * 256, B, 0, NULL, NULL);
            clFlush(queue);
            clFinish(queue);
            if (cl_R==0 || cl_G==0 || cl_B==0 || cl_img==0) {
                std::cerr << "Can't create Buffer\n";
                clReleaseCommandQueue(queue);
                clReleaseContext(context);
                return 0;
            }
            // -- End Setup openCL memory --

            // -- Setup Kernel param --
            //clSetKernelArg(kernel_core, 0, sizeof(cl_mem), &cl_img);
            clSetKernelArg(kernel_core, 0, sizeof(cl_mem), &cl_img);
            clSetKernelArg(kernel_core, 1, sizeof(cl_mem), &cl_R);
            clSetKernelArg(kernel_core, 2, sizeof(cl_mem), &cl_G);
            clSetKernelArg(kernel_core, 3, sizeof(cl_mem), &cl_B);
            clSetKernelArg(kernel_core, 4, sizeof(int), &rgb_len);
            // -- End Setup Kernel param --

            // -- run kernel --
            size_t work_size = data_size*rgb_len;
            err = clEnqueueNDRangeKernel(queue, kernel_core, 1, 0, &work_size, 0, 0, 0, 0);

            cl_int errR, errG, errB, errRGB;
            //RGB a[img->size];
            if(err == CL_SUCCESS) {
                //errRGB = clEnqueueReadBuffer(queue, cl_img, CL_TRUE, 0, img->size * sizeof(RGB), a, 0, 0, 0);
                errR = clEnqueueReadBuffer(queue, cl_R, CL_TRUE, 0, 256 * sizeof(uint32_t), R, 0, 0, 0);
                errG = clEnqueueReadBuffer(queue, cl_G, CL_TRUE, 0, 256 * sizeof(uint32_t), G, 0, 0, 0);
                errB = clEnqueueReadBuffer(queue, cl_B, CL_TRUE, 0, 256 * sizeof(uint32_t), B, 0, 0, 0);
                clFlush(queue);
                clFinish(queue);
            } else {
                std::cout << "run kernel failed" << err <<std::endl; 
            }

            if (!(errR==CL_SUCCESS && errG==CL_SUCCESS && errB==CL_SUCCESS)) {
                std::cout << "memory read failed" << errR << " " << errG << " " << errB <<std::endl; 
            } else {
            }
            // -- End run kernel -- 

            int max = 0;
            for(int i=0;i<256;i++){
                max = R[i] > max ? R[i] : max;
                max = G[i] > max ? G[i] : max;
                max = B[i] > max ? B[i] : max;
            }

            Image *ret = new Image();
            ret->type = 1;
            ret->height = 256;
            ret->weight = 256;
            ret->size = 256 * 256;
            ret->data = new RGB[256 * 256];

            for(int i=0;i<ret->height;i++){
                for(int j=0;j<256;j++){
                    if(R[j]*256/max > i)
                        ret->data[256*i+j].R = 255;
                    if(G[j]*256/max > i)
                        ret->data[256*i+j].G = 255;
                    if(B[j]*256/max > i)
                        ret->data[256*i+j].B = 255;
                }
            }

            std::string newfile = "hist_" + std::string(filename); 
            writebmp(newfile.c_str(), ret);
        }
        clFlush(queue);
        clFinish(queue);
        clReleaseKernel(kernel_core);
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
    }else{
        printf("Usage: ./hist <img.bmp> [img2.bmp ...]\n");
    }
    return 0;
}