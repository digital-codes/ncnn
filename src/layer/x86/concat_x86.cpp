// Copyright 2019 Tencent
// SPDX-License-Identifier: BSD-3-Clause

#include "concat_x86.h"

namespace ncnn {

Concat_x86::Concat_x86()
{
#if __SSE2__
    support_packing = true;
#endif // __SSE2__
}

int Concat_x86::forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt) const
{
    int dims = bottom_blobs[0].dims;
    int positive_axis = axis < 0 ? dims + axis : axis;

    if (dims == 1) // positive_axis == 0
    {
        // concat vector
        // total length
        size_t elemsize = bottom_blobs[0].elemsize;
        int elempack = bottom_blobs[0].elempack;
        int top_w = 0;
        for (size_t b = 0; b < bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            top_w += bottom_blob.w * bottom_blob.elempack;
        }

        int out_elempack = 1;
#if __SSE2__
        if (opt.use_packing_layout)
        {
#if __AVX512F__
            out_elempack = top_w % 16 == 0 ? 16 : top_w % 8 == 0 ? 8 : top_w % 4 == 0 ? 4 : 1;
#elif __AVX__
            out_elempack = top_w % 8 == 0 ? 8 : top_w % 4 == 0 ? 4 : 1;
#else
            out_elempack = top_w % 4 == 0 ? 4 : 1;
#endif
        }
#endif // __SSE2__
        size_t out_elemsize = elemsize / elempack * out_elempack;

        Mat& top_blob = top_blobs[0];
        top_blob.create(top_w / out_elempack, out_elemsize, out_elempack, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        float* outptr = top_blob;
        for (size_t b = 0; b < bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];

            const float* ptr = bottom_blob;
            memcpy(outptr, ptr, bottom_blob.w * bottom_blob.elemsize);

            outptr += bottom_blob.w * bottom_blob.elempack;
        }
    }

    if (dims == 2 && positive_axis == 0)
    {
        // concat image
        int w = bottom_blobs[0].w;

        // total height
        size_t elemsize = bottom_blobs[0].elemsize;
        int elempack = bottom_blobs[0].elempack;
        int top_h = 0;
        for (size_t b = 0; b < bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            elemsize = std::min(elemsize, bottom_blob.elemsize);
            elempack = std::min(elempack, bottom_blob.elempack);
            top_h += bottom_blob.h * bottom_blob.elempack;
        }

        int out_elempack = 1;
#if __SSE2__
        if (opt.use_packing_layout)
        {
#if __AVX512F__
            out_elempack = top_h % 16 == 0 ? 16 : top_h % 8 == 0 ? 8 : top_h % 4 == 0 ? 4 : 1;
#elif __AVX__
            out_elempack = top_h % 8 == 0 ? 8 : top_h % 4 == 0 ? 4 : 1;
#else
            out_elempack = top_h % 4 == 0 ? 4 : 1;
#endif
        }
#endif // __SSE2__
        size_t out_elemsize = elemsize / elempack * out_elempack;

        Mat& top_blob = top_blobs[0];
        top_blob.create(w, top_h / out_elempack, out_elemsize, out_elempack, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        Mat top_blob_unpacked = top_blob;
        if (elempack < out_elempack)
        {
            top_blob_unpacked.create(w, top_h / elempack, elemsize, elempack, opt.workspace_allocator);
            if (top_blob_unpacked.empty())
                return -100;
        }

        float* outptr = top_blob_unpacked;
        for (size_t b = 0; b < bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];

#if __AVX__
#if __AVX512F__
            if (bottom_blob.elempack == 16 && elempack == 8)
            {
                for (int i = 0; i < bottom_blob.h; i++)
                {
                    const float* r0 = bottom_blob.row(i);

                    float* outptr0 = outptr;
                    float* outptr1 = outptr + w * 8;

                    for (int j = 0; j < w; j++)
                    {
                        outptr0[0] = r0[0];
                        outptr0[1] = r0[1];
                        outptr0[2] = r0[2];
                        outptr0[3] = r0[3];
                        outptr0[4] = r0[4];
                        outptr0[5] = r0[5];
                        outptr0[6] = r0[6];
                        outptr0[7] = r0[7];
                        outptr1[0] = r0[8];
                        outptr1[1] = r0[9];
                        outptr1[2] = r0[10];
                        outptr1[3] = r0[11];
                        outptr1[4] = r0[12];
                        outptr1[5] = r0[13];
                        outptr1[6] = r0[14];
                        outptr1[7] = r0[15];

                        outptr0 += 8;
                        outptr1 += 8;
                        r0 += 16;
                    }

                    outptr += w * 16;
                }
            }
            if (bottom_blob.elempack == 16 && elempack == 4)
            {
                for (int i = 0; i < bottom_blob.h; i++)
                {
                    const float* r0 = bottom_blob.row(i);

                    float* outptr0 = outptr;
                    float* outptr1 = outptr + w * 4;
                    float* outptr2 = outptr + w * 8;
                    float* outptr3 = outptr + w * 12;

                    for (int j = 0; j < w; j++)
                    {
                        outptr0[0] = r0[0];
                        outptr0[1] = r0[1];
                        outptr0[2] = r0[2];
                        outptr0[3] = r0[3];
                        outptr1[0] = r0[4];
                        outptr1[1] = r0[5];
                        outptr1[2] = r0[6];
                        outptr1[3] = r0[7];
                        outptr2[0] = r0[8];
                        outptr2[1] = r0[9];
                        outptr2[2] = r0[10];
                        outptr2[3] = r0[11];
                        outptr3[0] = r0[12];
                        outptr3[1] = r0[13];
                        outptr3[2] = r0[14];
                        outptr3[3] = r0[15];

                        outptr0 += 4;
                        outptr1 += 4;
                        outptr2 += 4;
                        outptr3 += 4;
                        r0 += 16;
                    }

                    outptr += w * 16;
                }
            }
            if (bottom_blob.elempack == 16 && elempack == 1)
            {
                for (int i = 0; i < bottom_blob.h; i++)
                {
                    const float* r0 = bottom_blob.row(i);

                    float* outptr0 = outptr;
                    float* outptr1 = outptr + w;
                    float* outptr2 = outptr + w * 2;
                    float* outptr3 = outptr + w * 3;
                    float* outptr4 = outptr + w * 4;
                    float* outptr5 = outptr + w * 5;
                    float* outptr6 = outptr + w * 6;
                    float* outptr7 = outptr + w * 7;
                    float* outptr8 = outptr + w * 8;
                    float* outptr9 = outptr + w * 9;
                    float* outptra = outptr + w * 10;
                    float* outptrb = outptr + w * 11;
                    float* outptrc = outptr + w * 12;
                    float* outptrd = outptr + w * 13;
                    float* outptre = outptr + w * 14;
                    float* outptrf = outptr + w * 15;

                    for (int j = 0; j < w; j++)
                    {
                        *outptr0++ = r0[0];
                        *outptr1++ = r0[1];
                        *outptr2++ = r0[2];
                        *outptr3++ = r0[3];
                        *outptr4++ = r0[4];
                        *outptr5++ = r0[5];
                        *outptr6++ = r0[6];
                        *outptr7++ = r0[7];
                        *outptr8++ = r0[8];
                        *outptr9++ = r0[9];
                        *outptra++ = r0[10];
                        *outptrb++ = r0[11];
                        *outptrc++ = r0[12];
                        *outptrd++ = r0[13];
                        *outptre++ = r0[14];
                        *outptrf++ = r0[15];

                        r0 += 16;
                    }

                    outptr += w * 16;
                }
            }
#endif // __AVX512F__
            if (bottom_blob.elempack == 8 && elempack == 4)
            {
                for (int i = 0; i < bottom_blob.h; i++)
                {
                    const float* r0 = bottom_blob.row(i);

                    float* outptr0 = outptr;
                    float* outptr1 = outptr + w * 4;

                    for (int j = 0; j < w; j++)
                    {
                        outptr0[0] = r0[0];
                        outptr0[1] = r0[1];
                        outptr0[2] = r0[2];
                        outptr0[3] = r0[3];
                        outptr1[0] = r0[4];
                        outptr1[1] = r0[5];
                        outptr1[2] = r0[6];
                        outptr1[3] = r0[7];

                        outptr0 += 4;
                        outptr1 += 4;
                        r0 += 8;
                    }

                    outptr += w * 8;
                }
            }
            if (bottom_blob.elempack == 8 && elempack == 1)
            {
                for (int i = 0; i < bottom_blob.h; i++)
                {
                    const float* r0 = bottom_blob.row(i);

                    float* outptr0 = outptr;
                    float* outptr1 = outptr + w;
                    float* outptr2 = outptr + w * 2;
                    float* outptr3 = outptr + w * 3;
                    float* outptr4 = outptr + w * 4;
                    float* outptr5 = outptr + w * 5;
                    float* outptr6 = outptr + w * 6;
                    float* outptr7 = outptr + w * 7;

                    for (int j = 0; j < w; j++)
                    {
                        *outptr0++ = r0[0];
                        *outptr1++ = r0[1];
                        *outptr2++ = r0[2];
                        *outptr3++ = r0[3];
                        *outptr4++ = r0[4];
                        *outptr5++ = r0[5];
                        *outptr6++ = r0[6];
                        *outptr7++ = r0[7];

                        r0 += 8;
                    }

                    outptr += w * 8;
                }
            }
#endif // __AVX__
            if (bottom_blob.elempack == 4 && elempack == 1)
            {
                for (int i = 0; i < bottom_blob.h; i++)
                {
                    const float* r0 = bottom_blob.row(i);

                    float* outptr0 = outptr;
                    float* outptr1 = outptr + w;
                    float* outptr2 = outptr + w * 2;
                    float* outptr3 = outptr + w * 3;

                    for (int j = 0; j < w; j++)
                    {
                        *outptr0++ = r0[0];
                        *outptr1++ = r0[1];
                        *outptr2++ = r0[2];
                        *outptr3++ = r0[3];

                        r0 += 4;
                    }

                    outptr += w * 4;
                }
            }
            if (bottom_blob.elempack == elempack) // 1-1 4-4 8-8 16-16
            {
                int size = w * bottom_blob.h;

                const float* ptr = bottom_blob;
                memcpy(outptr, ptr, size * bottom_blob.elemsize);

                outptr += size * bottom_blob.elempack;
            }
        }

        // packing
        if (elempack < out_elempack)
        {
            convert_packing(top_blob_unpacked, top_blob, out_elempack, opt);
        }
    }

    if (dims == 2 && positive_axis == 1)
    {
        // interleave image row
        int h = bottom_blobs[0].h;
        size_t elemsize = bottom_blobs[0].elemsize;
        int elempack = bottom_blobs[0].elempack;

        // total width
        int top_w = 0;
        for (size_t b = 0; b < bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            top_w += bottom_blob.w;
        }

        Mat& top_blob = top_blobs[0];
        top_blob.create(top_w, h, elemsize, elempack, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int i = 0; i < h; i++)
        {
            float* outptr = top_blob.row(i);
            for (size_t b = 0; b < bottom_blobs.size(); b++)
            {
                const Mat& bottom_blob = bottom_blobs[b];

                const float* ptr = bottom_blob.row(i);
                memcpy(outptr, ptr, bottom_blob.w * elemsize);

                outptr += bottom_blob.w * elempack;
            }
        }
    }

    if ((dims == 3 || dims == 4) && positive_axis == 0)
    {
        // concat dim
        int w = bottom_blobs[0].w;
        int h = bottom_blobs[0].h;
        int d = bottom_blobs[0].d;

        // total channels
        size_t elemsize = bottom_blobs[0].elemsize;
        int elempack = bottom_blobs[0].elempack;
        int top_channels = 0;
        for (size_t b = 0; b < bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            elemsize = std::min(elemsize, bottom_blob.elemsize);
            elempack = std::min(elempack, bottom_blob.elempack);
            top_channels += bottom_blob.c * bottom_blob.elempack;
        }

        int out_elempack = 1;
#if __SSE2__
        if (opt.use_packing_layout)
        {
#if __AVX512F__
            out_elempack = top_channels % 16 == 0 ? 16 : top_channels % 8 == 0 ? 8 : top_channels % 4 == 0 ? 4 : 1;
#elif __AVX__
            out_elempack = top_channels % 8 == 0 ? 8 : top_channels % 4 == 0 ? 4 : 1;
#else
            out_elempack = top_channels % 4 == 0 ? 4 : 1;
#endif
        }
#endif // __SSE2__
        size_t out_elemsize = elemsize / elempack * out_elempack;

        Mat& top_blob = top_blobs[0];
        top_blob.create(w, h, d, top_channels / out_elempack, out_elemsize, out_elempack, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        top_blob.dims = dims;

        Mat top_blob_unpacked = top_blob;
        if (elempack < out_elempack)
        {
            top_blob_unpacked.create(w, h, d, top_channels / elempack, elemsize, elempack, opt.workspace_allocator);
            if (top_blob_unpacked.empty())
                return -100;

            top_blob_unpacked.dims = dims;
        }

        int p = 0;
        for (size_t b = 0; b < bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];

#if __AVX__
#if __AVX512F__
            if (bottom_blob.elempack == 16 && elempack == 8)
            {
                int size = bottom_blob.w * bottom_blob.h * bottom_blob.d;

                for (int q = 0; q < bottom_blob.c; q++)
                {
                    const float* r0 = bottom_blob.channel(q);

                    float* outptr0 = top_blob_unpacked.channel(p);
                    float* outptr1 = top_blob_unpacked.channel(p + 1);

                    for (int i = 0; i < size; i++)
                    {
                        outptr0[0] = r0[0];
                        outptr0[1] = r0[1];
                        outptr0[2] = r0[2];
                        outptr0[3] = r0[3];
                        outptr0[4] = r0[4];
                        outptr0[5] = r0[5];
                        outptr0[6] = r0[6];
                        outptr0[7] = r0[7];
                        outptr1[0] = r0[8];
                        outptr1[1] = r0[9];
                        outptr1[2] = r0[10];
                        outptr1[3] = r0[11];
                        outptr1[4] = r0[12];
                        outptr1[5] = r0[13];
                        outptr1[6] = r0[14];
                        outptr1[7] = r0[15];

                        outptr0 += 8;
                        outptr1 += 8;
                        r0 += 16;
                    }

                    p += 2;
                }
            }
            if (bottom_blob.elempack == 16 && elempack == 4)
            {
                int size = bottom_blob.w * bottom_blob.h * bottom_blob.d;

                for (int q = 0; q < bottom_blob.c; q++)
                {
                    const float* r0 = bottom_blob.channel(q);

                    float* outptr0 = top_blob_unpacked.channel(p);
                    float* outptr1 = top_blob_unpacked.channel(p + 1);
                    float* outptr2 = top_blob_unpacked.channel(p + 2);
                    float* outptr3 = top_blob_unpacked.channel(p + 3);

                    for (int i = 0; i < size; i++)
                    {
                        outptr0[0] = r0[0];
                        outptr0[1] = r0[1];
                        outptr0[2] = r0[2];
                        outptr0[3] = r0[3];
                        outptr1[0] = r0[4];
                        outptr1[1] = r0[5];
                        outptr1[2] = r0[6];
                        outptr1[3] = r0[7];
                        outptr2[0] = r0[8];
                        outptr2[1] = r0[9];
                        outptr2[2] = r0[10];
                        outptr2[3] = r0[11];
                        outptr3[0] = r0[12];
                        outptr3[1] = r0[13];
                        outptr3[2] = r0[14];
                        outptr3[3] = r0[15];

                        outptr0 += 4;
                        outptr1 += 4;
                        outptr2 += 4;
                        outptr3 += 4;
                        r0 += 16;
                    }

                    p += 4;
                }
            }
            if (bottom_blob.elempack == 16 && elempack == 1)
            {
                int size = bottom_blob.w * bottom_blob.h * bottom_blob.d;

                for (int q = 0; q < bottom_blob.c; q++)
                {
                    const float* r0 = bottom_blob.channel(q);

                    float* outptr0 = top_blob_unpacked.channel(p);
                    float* outptr1 = top_blob_unpacked.channel(p + 1);
                    float* outptr2 = top_blob_unpacked.channel(p + 2);
                    float* outptr3 = top_blob_unpacked.channel(p + 3);
                    float* outptr4 = top_blob_unpacked.channel(p + 4);
                    float* outptr5 = top_blob_unpacked.channel(p + 5);
                    float* outptr6 = top_blob_unpacked.channel(p + 6);
                    float* outptr7 = top_blob_unpacked.channel(p + 7);
                    float* outptr8 = top_blob_unpacked.channel(p + 8);
                    float* outptr9 = top_blob_unpacked.channel(p + 9);
                    float* outptra = top_blob_unpacked.channel(p + 10);
                    float* outptrb = top_blob_unpacked.channel(p + 11);
                    float* outptrc = top_blob_unpacked.channel(p + 12);
                    float* outptrd = top_blob_unpacked.channel(p + 13);
                    float* outptre = top_blob_unpacked.channel(p + 14);
                    float* outptrf = top_blob_unpacked.channel(p + 15);

                    for (int i = 0; i < size; i++)
                    {
                        *outptr0++ = r0[0];
                        *outptr1++ = r0[1];
                        *outptr2++ = r0[2];
                        *outptr3++ = r0[3];
                        *outptr4++ = r0[4];
                        *outptr5++ = r0[5];
                        *outptr6++ = r0[6];
                        *outptr7++ = r0[7];
                        *outptr8++ = r0[8];
                        *outptr9++ = r0[9];
                        *outptra++ = r0[10];
                        *outptrb++ = r0[11];
                        *outptrc++ = r0[12];
                        *outptrd++ = r0[13];
                        *outptre++ = r0[14];
                        *outptrf++ = r0[15];

                        r0 += 16;
                    }

                    p += 16;
                }
            }
#endif // __AVX512F__
            if (bottom_blob.elempack == 8 && elempack == 4)
            {
                int size = bottom_blob.w * bottom_blob.h * bottom_blob.d;

                for (int q = 0; q < bottom_blob.c; q++)
                {
                    const float* r0 = bottom_blob.channel(q);

                    float* outptr0 = top_blob_unpacked.channel(p);
                    float* outptr1 = top_blob_unpacked.channel(p + 1);

                    for (int i = 0; i < size; i++)
                    {
                        outptr0[0] = r0[0];
                        outptr0[1] = r0[1];
                        outptr0[2] = r0[2];
                        outptr0[3] = r0[3];
                        outptr1[0] = r0[4];
                        outptr1[1] = r0[5];
                        outptr1[2] = r0[6];
                        outptr1[3] = r0[7];

                        outptr0 += 4;
                        outptr1 += 4;
                        r0 += 8;
                    }

                    p += 2;
                }
            }
            if (bottom_blob.elempack == 8 && elempack == 1)
            {
                int size = bottom_blob.w * bottom_blob.h * bottom_blob.d;

                for (int q = 0; q < bottom_blob.c; q++)
                {
                    const float* r0 = bottom_blob.channel(q);

                    float* outptr0 = top_blob_unpacked.channel(p);
                    float* outptr1 = top_blob_unpacked.channel(p + 1);
                    float* outptr2 = top_blob_unpacked.channel(p + 2);
                    float* outptr3 = top_blob_unpacked.channel(p + 3);
                    float* outptr4 = top_blob_unpacked.channel(p + 4);
                    float* outptr5 = top_blob_unpacked.channel(p + 5);
                    float* outptr6 = top_blob_unpacked.channel(p + 6);
                    float* outptr7 = top_blob_unpacked.channel(p + 7);

                    for (int i = 0; i < size; i++)
                    {
                        *outptr0++ = r0[0];
                        *outptr1++ = r0[1];
                        *outptr2++ = r0[2];
                        *outptr3++ = r0[3];
                        *outptr4++ = r0[4];
                        *outptr5++ = r0[5];
                        *outptr6++ = r0[6];
                        *outptr7++ = r0[7];

                        r0 += 8;
                    }

                    p += 8;
                }
            }
#endif // __AVX__
            if (bottom_blob.elempack == 4 && elempack == 1)
            {
                int size = bottom_blob.w * bottom_blob.h * bottom_blob.d;

                for (int q = 0; q < bottom_blob.c; q++)
                {
                    const float* r0 = bottom_blob.channel(q);

                    float* outptr0 = top_blob_unpacked.channel(p);
                    float* outptr1 = top_blob_unpacked.channel(p + 1);
                    float* outptr2 = top_blob_unpacked.channel(p + 2);
                    float* outptr3 = top_blob_unpacked.channel(p + 3);

                    for (int i = 0; i < size; i++)
                    {
                        *outptr0++ = r0[0];
                        *outptr1++ = r0[1];
                        *outptr2++ = r0[2];
                        *outptr3++ = r0[3];

                        r0 += 4;
                    }

                    p += 4;
                }
            }
            if (bottom_blob.elempack == elempack) // 1-1 4-4 8-8
            {
                int size = bottom_blob.total();

                const float* ptr = bottom_blob;
                float* outptr = top_blob_unpacked.channel(p);
                memcpy(outptr, ptr, size * bottom_blob.elemsize);

                p += bottom_blob.c;
            }
        }

        // packing
        if (elempack < out_elempack)
        {
            convert_packing(top_blob_unpacked, top_blob, out_elempack, opt);
        }
    }

    if ((dims == 3 && positive_axis == 1) || (dims == 4 && positive_axis == 2))
    {
        // interleave dim height
        int w = bottom_blobs[0].w;
        int d = bottom_blobs[0].d;
        int channels = bottom_blobs[0].c;
        size_t elemsize = bottom_blobs[0].elemsize;
        int elempack = bottom_blobs[0].elempack;

        // total height
        int top_h = 0;
        for (size_t b = 0; b < bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            top_h += bottom_blob.h;
        }

        Mat& top_blob = top_blobs[0];
        top_blob.create(w, top_h, d, channels, elemsize, elempack, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        top_blob.dims = dims;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            float* outptr = top_blob.channel(q);

            for (int i = 0; i < d; i++)
            {
                for (size_t b = 0; b < bottom_blobs.size(); b++)
                {
                    const Mat& bottom_blob = bottom_blobs[b];

                    int size = bottom_blob.w * bottom_blob.h;

                    const float* ptr = bottom_blob.channel(q).depth(i);
                    memcpy(outptr, ptr, size * elemsize);

                    outptr += size * elempack;
                }
            }
        }
    }

    if ((dims == 3 && positive_axis == 2) || (dims == 4 && positive_axis == 3))
    {
        // interleave dim width
        int h = bottom_blobs[0].h;
        int d = bottom_blobs[0].d;
        int channels = bottom_blobs[0].c;
        size_t elemsize = bottom_blobs[0].elemsize;
        int elempack = bottom_blobs[0].elempack;

        // total height
        int top_w = 0;
        for (size_t b = 0; b < bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            top_w += bottom_blob.w;
        }

        Mat& top_blob = top_blobs[0];
        top_blob.create(top_w, h, d, channels, elemsize, elempack, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        top_blob.dims = dims;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            float* outptr = top_blob.channel(q);

            for (int i = 0; i < d; i++)
            {
                for (int j = 0; j < h; j++)
                {
                    for (size_t b = 0; b < bottom_blobs.size(); b++)
                    {
                        const Mat& bottom_blob = bottom_blobs[b];

                        const float* ptr = bottom_blob.channel(q).depth(i).row(j);
                        memcpy(outptr, ptr, bottom_blob.w * elemsize);

                        outptr += bottom_blob.w * elempack;
                    }
                }
            }
        }
    }

    if (dims == 4 && positive_axis == 1)
    {
        // interleave dim depth
        int w = bottom_blobs[0].w;
        int h = bottom_blobs[0].h;
        int channels = bottom_blobs[0].c;
        size_t elemsize = bottom_blobs[0].elemsize;
        int elempack = bottom_blobs[0].elempack;

        // total depth
        int top_d = 0;
        for (size_t b = 0; b < bottom_blobs.size(); b++)
        {
            const Mat& bottom_blob = bottom_blobs[b];
            top_d += bottom_blob.d;
        }

        Mat& top_blob = top_blobs[0];
        top_blob.create(w, h, top_d, channels, elemsize, elempack, opt.blob_allocator);
        if (top_blob.empty())
            return -100;

        #pragma omp parallel for num_threads(opt.num_threads)
        for (int q = 0; q < channels; q++)
        {
            float* outptr = top_blob.channel(q);

            for (size_t b = 0; b < bottom_blobs.size(); b++)
            {
                const Mat& bottom_blob = bottom_blobs[b];

                int size = bottom_blob.w * bottom_blob.h * bottom_blob.d;

                const float* ptr = bottom_blob.channel(q);
                memcpy(outptr, ptr, size * elemsize);

                outptr += size * elempack;
            }
        }
    }

    return 0;
}

} // namespace ncnn
