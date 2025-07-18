// Copyright 2020 Tencent
// SPDX-License-Identifier: BSD-3-Clause

#include "testutil.h"

static int cast_cpu_naive(const ncnn::Mat& a, ncnn::Mat& b, int type_from, int type_to)
{
    ncnn::ParamDict pd;
    pd.set(0, type_from);
    pd.set(1, type_to);

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;

    ncnn::Layer* op = ncnn::create_layer_naive("Cast");

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    op->forward(a, b, opt);

    op->destroy_pipeline(opt);

    delete op;

    return 0;
}

static int test_cast_cpu(const ncnn::Mat& a, int type_from, int type_to)
{
    ncnn::ParamDict pd;
    pd.set(0, type_from);
    pd.set(1, type_to);

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;
    opt.use_vulkan_compute = false;
    opt.use_int8_inference = false;
    opt.use_packing_layout = false;

    ncnn::Layer* op = ncnn::create_layer_cpu("Cast");

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    ncnn::Mat a_fp16;
    cast_cpu_naive(a, a_fp16, 1, type_from);

    ncnn::Mat b;
    cast_cpu_naive(a_fp16, b, type_from, type_to);

    ncnn::Mat c;
    op->forward(a_fp16, c, opt);

    op->destroy_pipeline(opt);

    delete op;

    if (CompareMat(b, c, 0.001) != 0)
    {
        fprintf(stderr, "test_cast_cpu failed a.dims=%d a=(%d %d %d %d) type_from=%d type_to=%d\n", a.dims, a.w, a.h, a.d, a.c, type_from, type_to);
        return -1;
    }

    return 0;
}

static int test_cast_cpu_packed(const ncnn::Mat& a, int type_from, int type_to)
{
    ncnn::ParamDict pd;
    pd.set(0, type_from);
    pd.set(1, type_to);

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;
    opt.use_vulkan_compute = false;
    opt.use_packing_layout = false;

    ncnn::Layer* op = ncnn::create_layer_cpu("Cast");

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    ncnn::Mat a_fp16;
    cast_cpu_naive(a, a_fp16, 1, type_from);

    ncnn::Mat b;
    cast_cpu_naive(a_fp16, b, type_from, type_to);

    ncnn::Mat a4;
    ncnn::convert_packing(a, a4, 4, opt);

    ncnn::Mat a4_fp16;
    cast_cpu_naive(a4, a4_fp16, 1, type_from);

    ncnn::Mat c;
    op->forward(a4_fp16, c, opt);

    op->destroy_pipeline(opt);

    delete op;

    if (CompareMat(b, c, 0.001) != 0)
    {
        fprintf(stderr, "test_cast_cpu_packed failed a.dims=%d a=(%d %d %d %d) type_from=%d type_to=%d\n", a.dims, a.w, a.h, a.d, a.c, type_from, type_to);
        return -1;
    }

    return 0;
}

#if NCNN_VULKAN
static int test_cast_gpu_fp16p(const ncnn::Mat& a, int type_from, int type_to)
{
    if (type_to == 4 || type_from == 4)
        return 0;
    ncnn::ParamDict pd;
    pd.set(0, type_from);
    pd.set(1, type_to);

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;
    opt.use_vulkan_compute = true;
    opt.use_int8_inference = false;
    opt.use_fp16_packed = true;
    opt.use_fp16_storage = false;
    opt.use_fp16_arithmetic = false;
    opt.use_int8_storage = false;
    opt.use_int8_arithmetic = false;
    opt.use_packing_layout = true;

    ncnn::VulkanDevice* vkdev = ncnn::get_gpu_device();

    ncnn::VkAllocator* blob_vkallocator = vkdev->acquire_blob_allocator();
    ncnn::VkAllocator* staging_vkallocator = vkdev->acquire_staging_allocator();

    opt.blob_vkallocator = blob_vkallocator;
    opt.workspace_vkallocator = blob_vkallocator;
    opt.staging_vkallocator = staging_vkallocator;

    if (!vkdev->info.support_fp16_packed()) opt.use_fp16_packed = false;
    if (!vkdev->info.support_fp16_storage()) opt.use_fp16_storage = false;

    ncnn::Layer* op = ncnn::create_layer_vulkan("Cast");

    op->vkdev = vkdev;

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    ncnn::Mat a_fp16;
    if (type_from == 2)
    {
        ncnn::cast_float32_to_float16(a, a_fp16, opt);
    }
    else
    {
        a_fp16 = a;
    }

    ncnn::Mat b;
    cast_cpu_naive(a_fp16, b, type_from, type_to);

    ncnn::Mat d;

    // pack
    ncnn::Mat a4;
    ncnn::convert_packing(a, a4, 4, opt);

    ncnn::Mat a4_fp16;
    if (type_from == 2)
    {
        ncnn::cast_float32_to_float16(a4, a4_fp16, opt);
    }
    else
    {
        a4_fp16 = a4;
    }

    // forward
    ncnn::VkCompute cmd(vkdev);

    // upload
    ncnn::VkMat a4_gpu;
    cmd.record_clone(a4_fp16, a4_gpu, opt);

    ncnn::VkMat d4_gpu;
    if (op->support_inplace)
    {
        op->forward_inplace(a4_gpu, cmd, opt);

        d4_gpu = a4_gpu;
    }
    else
    {
        op->forward(a4_gpu, d4_gpu, cmd, opt);
    }

    // download
    cmd.record_clone(d4_gpu, d, opt);

    cmd.submit_and_wait();

    op->destroy_pipeline(opt);

    delete op;

    vkdev->reclaim_blob_allocator(blob_vkallocator);
    vkdev->reclaim_staging_allocator(staging_vkallocator);

    if (CompareMat(b, d, 0.001) != 0)
    {
        fprintf(stderr, "test_cast_gpu_fp16p failed a.dims=%d a=(%d %d %d %d) type_from=%d type_to=%d\n", a.dims, a.w, a.h, a.d, a.c, type_from, type_to);
        return -1;
    }

    return 0;
}

static int test_cast_gpu_fp16p_pack8(const ncnn::Mat& a, int type_from, int type_to)
{
    if (type_to == 4 || type_from == 4)
        return 0;
    ncnn::ParamDict pd;
    pd.set(0, type_from);
    pd.set(1, type_to);

    std::vector<ncnn::Mat> weights(0);

    ncnn::Option opt;
    opt.num_threads = 1;
    opt.use_vulkan_compute = true;
    opt.use_int8_inference = false;
    opt.use_fp16_packed = true;
    opt.use_fp16_storage = false;
    opt.use_fp16_arithmetic = false;
    opt.use_int8_storage = false;
    opt.use_int8_arithmetic = false;
    opt.use_packing_layout = true;
    opt.use_shader_pack8 = true;

    ncnn::VulkanDevice* vkdev = ncnn::get_gpu_device();

    ncnn::VkAllocator* blob_vkallocator = vkdev->acquire_blob_allocator();
    ncnn::VkAllocator* staging_vkallocator = vkdev->acquire_staging_allocator();

    opt.blob_vkallocator = blob_vkallocator;
    opt.workspace_vkallocator = blob_vkallocator;
    opt.staging_vkallocator = staging_vkallocator;

    if (!vkdev->info.support_fp16_packed()) opt.use_fp16_packed = false;
    if (!vkdev->info.support_fp16_storage()) opt.use_fp16_storage = false;

    ncnn::Layer* op = ncnn::create_layer_vulkan("Cast");

    op->vkdev = vkdev;

    op->load_param(pd);

    ncnn::ModelBinFromMatArray mb(weights.data());

    op->load_model(mb);

    op->create_pipeline(opt);

    ncnn::Mat a_fp16;
    if (type_from == 2)
    {
        ncnn::cast_float32_to_float16(a, a_fp16, opt);
    }
    else
    {
        a_fp16 = a;
    }

    ncnn::Mat b;
    cast_cpu_naive(a_fp16, b, type_from, type_to);

    ncnn::Mat d;

    // pack
    ncnn::Mat a4;
    ncnn::convert_packing(a, a4, 8, opt);
    if (a4.elempack != 8)
        ncnn::convert_packing(a, a4, 4, opt);

    ncnn::Mat a4_fp16;
    if (type_from == 2)
    {
        ncnn::cast_float32_to_float16(a4, a4_fp16, opt);
    }
    else
    {
        a4_fp16 = a4;
    }

    // forward
    ncnn::VkCompute cmd(vkdev);

    // upload
    ncnn::VkMat a4_gpu;
    cmd.record_clone(a4_fp16, a4_gpu, opt);

    ncnn::VkMat d4_gpu;
    if (op->support_inplace)
    {
        op->forward_inplace(a4_gpu, cmd, opt);

        d4_gpu = a4_gpu;
    }
    else
    {
        op->forward(a4_gpu, d4_gpu, cmd, opt);
    }

    // download
    cmd.record_clone(d4_gpu, d, opt);

    cmd.submit_and_wait();

    op->destroy_pipeline(opt);

    delete op;

    vkdev->reclaim_blob_allocator(blob_vkallocator);
    vkdev->reclaim_staging_allocator(staging_vkallocator);

    if (CompareMat(b, d, 0.001) != 0)
    {
        fprintf(stderr, "test_cast_gpu_fp16p_pack8 failed a.dims=%d a=(%d %d %d %d) type_from=%d type_to=%d\n", a.dims, a.w, a.h, a.d, a.c, type_from, type_to);
        return -1;
    }

    return 0;
}
#endif // NCNN_VULKAN

static int test_cast(const ncnn::Mat& a, int type_from, int type_to)
{
    return 0
           || test_cast_cpu(a, type_from, type_to)
           || test_cast_cpu_packed(a, type_from, type_to)
#if NCNN_VULKAN
           || test_cast_gpu_fp16p(a, type_from, type_to)
           || test_cast_gpu_fp16p_pack8(a, type_from, type_to)
#endif // NCNN_VULKAN
           ;
}

static int test_cast_0()
{
    return 0
           || test_cast(RandomMat(5, 6, 7, 16), 1, 2)
           || test_cast(RandomMat(3, 4, 5, 13), 1, 2)
           || test_cast(RandomMat(5, 6, 7, 16), 2, 1)
           || test_cast(RandomMat(3, 4, 5, 13), 2, 1)
           || test_cast(RandomMat(5, 6, 7, 16), 1, 4)
           || test_cast(RandomMat(3, 4, 5, 13), 1, 4)
           || test_cast(RandomMat(5, 6, 7, 16), 4, 1)
           || test_cast(RandomMat(3, 4, 5, 13), 4, 1);
}

static int test_cast_1()
{
    return 0
           || test_cast(RandomMat(5, 7, 16), 1, 2)
           || test_cast(RandomMat(3, 5, 13), 1, 2)
           || test_cast(RandomMat(5, 7, 16), 2, 1)
           || test_cast(RandomMat(3, 5, 13), 2, 1)
           || test_cast(RandomMat(5, 7, 16), 1, 4)
           || test_cast(RandomMat(3, 5, 13), 1, 4)
           || test_cast(RandomMat(5, 7, 16), 4, 1)
           || test_cast(RandomMat(3, 5, 13), 4, 1);
}

static int test_cast_2()
{
    return 0
           || test_cast(RandomMat(6, 16), 1, 2)
           || test_cast(RandomMat(7, 15), 1, 2)
           || test_cast(RandomMat(6, 16), 2, 1)
           || test_cast(RandomMat(7, 15), 2, 1)
           || test_cast(RandomMat(6, 16), 1, 4)
           || test_cast(RandomMat(7, 15), 1, 4)
           || test_cast(RandomMat(6, 16), 4, 1)
           || test_cast(RandomMat(7, 15), 4, 1);
}

static int test_cast_3()
{
    return 0
           || test_cast(RandomMat(128), 1, 2)
           || test_cast(RandomMat(127), 1, 2)
           || test_cast(RandomMat(128), 2, 1)
           || test_cast(RandomMat(127), 2, 1)
           || test_cast(RandomMat(128), 1, 4)
           || test_cast(RandomMat(127), 1, 4)
           || test_cast(RandomMat(128), 4, 1)
           || test_cast(RandomMat(127), 4, 1);
}

int main()
{
    SRAND(7767517);

    return 0
           || test_cast_0()
           || test_cast_1()
           || test_cast_2()
           || test_cast_3();
}
