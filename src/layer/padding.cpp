// Copyright 2017 Tencent
// SPDX-License-Identifier: BSD-3-Clause

#include "padding.h"

namespace ncnn {

Padding::Padding()
{
    one_blob_only = true;
    support_inplace = false;
}

int Padding::load_param(const ParamDict& pd)
{
    top = pd.get(0, 0);
    bottom = pd.get(1, 0);
    left = pd.get(2, 0);
    right = pd.get(3, 0);
    type = pd.get(4, 0);
    value = pd.get(5, 0.f);
    per_channel_pad_data_size = pd.get(6, 0);
    front = pd.get(7, 0);
    behind = pd.get(8, 0);

    return 0;
}

int Padding::load_model(const ModelBin& mb)
{
    if (per_channel_pad_data_size)
    {
        per_channel_pad_data = mb.load(per_channel_pad_data_size, 1);
    }

    return 0;
}

template<typename T>
static void copy_make_border_image(const Mat& src, Mat& dst, int top, int left, int type, T v)
{
    int w = dst.w;
    int h = dst.h;

    const T* ptr = src;
    T* outptr = dst;

    if (type == 0)
    {
        int y = 0;
        // fill top
        for (; y < top; y++)
        {
            int x = 0;
            for (; x < w; x++)
            {
                outptr[x] = v;
            }
            outptr += w;
        }
        // fill center
        for (; y < (top + src.h); y++)
        {
            int x = 0;
            for (; x < left; x++)
            {
                outptr[x] = v;
            }
            if (src.w < 12)
            {
                for (; x < (left + src.w); x++)
                {
                    outptr[x] = ptr[x - left];
                }
            }
            else
            {
                memcpy(outptr + left, ptr, src.w * sizeof(T));
                x += src.w;
            }
            for (; x < w; x++)
            {
                outptr[x] = v;
            }
            ptr += src.w;
            outptr += w;
        }
        // fill bottom
        for (; y < h; y++)
        {
            int x = 0;
            for (; x < w; x++)
            {
                outptr[x] = v;
            }
            outptr += w;
        }
    }

    if (type == 1)
    {
        int y = 0;
        // fill top
        for (; y < top; y++)
        {
            int x = 0;
            for (; x < left; x++)
            {
                outptr[x] = ptr[0];
            }
            if (src.w < 12)
            {
                for (; x < (left + src.w); x++)
                {
                    outptr[x] = ptr[x - left];
                }
            }
            else
            {
                memcpy(outptr + left, ptr, src.w * sizeof(T));
                x += src.w;
            }
            for (; x < w; x++)
            {
                outptr[x] = ptr[src.w - 1];
            }
            outptr += w;
        }
        // fill center
        for (; y < (top + src.h); y++)
        {
            int x = 0;
            for (; x < left; x++)
            {
                outptr[x] = ptr[0];
            }
            if (src.w < 12)
            {
                for (; x < (left + src.w); x++)
                {
                    outptr[x] = ptr[x - left];
                }
            }
            else
            {
                memcpy(outptr + left, ptr, src.w * sizeof(T));
                x += src.w;
            }
            for (; x < w; x++)
            {
                outptr[x] = ptr[src.w - 1];
            }
            ptr += src.w;
            outptr += w;
        }
        // fill bottom
        ptr -= src.w;
        for (; y < h; y++)
        {
            int x = 0;
            for (; x < left; x++)
            {
                outptr[x] = ptr[0];
            }
            if (src.w < 12)
            {
                for (; x < (left + src.w); x++)
                {
                    outptr[x] = ptr[x - left];
                }
            }
            else
            {
                memcpy(outptr + left, ptr, src.w * sizeof(T));
                x += src.w;
            }
            for (; x < w; x++)
            {
                outptr[x] = ptr[src.w - 1];
            }
            outptr += w;
        }
    }

    if (type == 2)
    {
        int y = 0;
        // fill top
        ptr += top * src.w;
        for (; y < top; y++)
        {
            int x = 0;
            for (; x < left; x++)
            {
                outptr[x] = ptr[left - x];
            }
            if (src.w < 12)
            {
                for (; x < (left + src.w); x++)
                {
                    outptr[x] = ptr[x - left];
                }
            }
            else
            {
                memcpy(outptr + left, ptr, src.w * sizeof(T));
                x += src.w;
            }
            for (; x < w; x++)
            {
                outptr[x] = ptr[src.w - (x - left - src.w) - 2];
            }
            outptr += w;
            ptr -= src.w;
        }
        // fill center
        for (; y < (top + src.h); y++)
        {
            int x = 0;
            for (; x < left; x++)
            {
                outptr[x] = ptr[left - x];
            }
            if (src.w < 12)
            {
                for (; x < (left + src.w); x++)
                {
                    outptr[x] = ptr[x - left];
                }
            }
            else
            {
                memcpy(outptr + left, ptr, src.w * sizeof(T));
                x += src.w;
            }
            for (; x < w; x++)
            {
                outptr[x] = ptr[src.w - (x - left - src.w) - 2];
            }
            ptr += src.w;
            outptr += w;
        }
        // fill bottom
        ptr -= 2 * src.w;
        for (; y < h; y++)
        {
            int x = 0;
            for (; x < left; x++)
            {
                outptr[x] = ptr[left - x];
            }
            if (src.w < 12)
            {
                for (; x < (left + src.w); x++)
                {
                    outptr[x] = ptr[x - left];
                }
            }
            else
            {
                memcpy(outptr + left, ptr, src.w * sizeof(T));
                x += src.w;
            }
            for (; x < w; x++)
            {
                outptr[x] = ptr[src.w - (x - left - src.w) - 2];
            }
            outptr += w;
            ptr -= src.w;
        }
    }
}

int Padding::forward(const Mat& bottom_blob, Mat& top_blob, const Option& opt) const
{
    if (top == 0 && bottom == 0 && left == 0 && right == 0 && front == 0 && behind == 0)
    {
        top_blob = bottom_blob;
        return 0;
    }

    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int d = bottom_blob.d;
    int channels = bottom_blob.c;
    int dims = bottom_blob.dims;
    size_t elemsize = bottom_blob.elemsize;

    int outw = w + left + right;

    if (dims == 1)
    {
        top_blob.create(outw, elemsize, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        if (elemsize == 1)
            copy_make_border_image<signed char>(bottom_blob, top_blob, 0, left, type, static_cast<signed char>(value));
        if (elemsize == 2)
            copy_make_border_image<unsigned short>(bottom_blob, top_blob, 0, left, type, support_fp16_storage && opt.use_fp16_storage ? float32_to_float16(value) : float32_to_bfloat16(value));
        if (elemsize == 4)
            copy_make_border_image<float>(bottom_blob, top_blob, 0, left, type, value);

        return 0;
    }

    int outh = h + top + bottom;

    if (dims == 2)
    {
        top_blob.create(outw, outh, elemsize, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        if (elemsize == 1)
            copy_make_border_image<signed char>(bottom_blob, top_blob, top, left, type, static_cast<signed char>(value));
        if (elemsize == 2)
            copy_make_border_image<unsigned short>(bottom_blob, top_blob, top, left, type, support_fp16_storage && opt.use_fp16_storage ? float32_to_float16(value) : float32_to_bfloat16(value));
        if (elemsize == 4)
            copy_make_border_image<float>(bottom_blob, top_blob, top, left, type, value);

        return 0;
    }

    if (dims == 3)
    {
        int outc = channels + front + behind;

        top_blob.create(outw, outh, outc, elemsize, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < outc; q++)
        {
            Mat borderm = top_blob.channel(q);

            float pad_value = per_channel_pad_data_size ? per_channel_pad_data[q] : value;

            //Channel padding
            if (((q < front) || (q >= (channels + front))) && type == 0)
            {
                if (elemsize == 1)
                {
                    borderm.fill(static_cast<signed char>(pad_value));
                }
                if (elemsize == 2)
                {
                    borderm.fill(support_fp16_storage && opt.use_fp16_storage ? float32_to_float16(pad_value) : float32_to_bfloat16(pad_value));
                }
                if (elemsize == 4)
                {
                    borderm.fill(pad_value);
                }
            }
            else
            {
                int q_ = q - front;

                if (type == 1)
                {
                    q_ = q_ <= 0 ? 0 : q_;
                    q_ = q_ >= channels - 1 ? channels - 1 : q_;
                }
                if (type == 2)
                {
                    q_ = abs(q_);
                    q_ = (channels - 1) - abs(q_ - (channels - 1));
                }
                const Mat m = bottom_blob.channel(q_);
                if (elemsize == 1)
                    copy_make_border_image<signed char>(m, borderm, top, left, type, static_cast<signed char>(pad_value));
                if (elemsize == 2)
                    copy_make_border_image<unsigned short>(m, borderm, top, left, type, support_fp16_storage && opt.use_fp16_storage ? float32_to_float16(pad_value) : float32_to_bfloat16(pad_value));
                if (elemsize == 4)
                    copy_make_border_image<float>(m, borderm, top, left, type, pad_value);
            }
        }

        return 0;
    }

    if (dims == 4)
    {
        int outd = d + front + behind;

        top_blob.create(outw, outh, outd, channels, elemsize, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            float pad_value = per_channel_pad_data_size ? per_channel_pad_data[q] : value;

            for (int z = 0; z < outd; z++)
            {
                Mat borderm = top_blob.channel(q).depth(z);

                // depth padding
                if (((z < front) || (z >= (d + front))) && type == 0)
                {
                    if (elemsize == 1)
                    {
                        borderm.fill(static_cast<signed char>(pad_value));
                    }
                    if (elemsize == 2)
                    {
                        borderm.fill(support_fp16_storage && opt.use_fp16_storage ? float32_to_float16(pad_value) : float32_to_bfloat16(pad_value));
                    }
                    if (elemsize == 4)
                    {
                        borderm.fill(pad_value);
                    }
                }
                else
                {
                    int z_ = z - front;

                    if (type == 1)
                    {
                        z_ = z_ <= 0 ? 0 : z_;
                        z_ = z_ >= d - 1 ? d - 1 : z_;
                    }
                    if (type == 2)
                    {
                        z_ = abs(z_);
                        z_ = (d - 1) - abs(z_ - (d - 1));
                    }
                    const Mat m = bottom_blob.channel(q).depth(z_);
                    if (elemsize == 1)
                        copy_make_border_image<signed char>(m, borderm, top, left, type, static_cast<signed char>(pad_value));
                    if (elemsize == 2)
                        copy_make_border_image<unsigned short>(m, borderm, top, left, type, support_fp16_storage && opt.use_fp16_storage ? float32_to_float16(pad_value) : float32_to_bfloat16(pad_value));
                    if (elemsize == 4)
                        copy_make_border_image<float>(m, borderm, top, left, type, pad_value);
                }
            }
        }

        return 0;
    }

    return 0;
}

} // namespace ncnn
