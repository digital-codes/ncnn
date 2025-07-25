// Copyright 2020 Tencent
// SPDX-License-Identifier: BSD-3-Clause

static void linear_coeffs_fp16sa(int w, int outw, int* xofs, __fp16* alpha, int align_corner)
{
    double scale = (double)w / outw;
    if (align_corner)
    {
        scale = (double)(w - 1) / (outw - 1);
    }

    for (int dx = 0; dx < outw; dx++)
    {
        float fx = (float)((dx + 0.5) * scale - 0.5);
        if (align_corner)
        {
            fx = static_cast<float>(dx * scale);
        }

        int sx = floor(fx);
        fx -= sx;

        if (sx < 0)
        {
            sx = 0;
            fx = 0.f;
        }
        if (sx >= w - 1)
        {
            sx = w - 2;
            fx = 1.f;
        }

        xofs[dx] = sx;

        alpha[dx * 2] = (__fp16)(1.f - fx);
        alpha[dx * 2 + 1] = (__fp16)fx;
    }
}

static void resize_bilinear_image_fp16s(const Mat& src, Mat& dst, float* alpha, int* xofs, float* beta, int* yofs)
{
    int w = dst.w;
    int h = dst.h;

    // loop body
    Mat rowsbuf0(w);
    Mat rowsbuf1(w);
    float* rows0 = rowsbuf0;
    float* rows1 = rowsbuf1;

    int prev_sy1 = -2;

    for (int dy = 0; dy < h; dy++)
    {
        int sy = yofs[dy];

        if (sy == prev_sy1)
        {
            // reuse all rows
        }
        else if (sy == prev_sy1 + 1)
        {
            // hresize one row
            float* rows0_old = rows0;
            rows0 = rows1;
            rows1 = rows0_old;
            const __fp16* S1 = src.row<const __fp16>(sy + 1);

            const float* alphap = alpha;
            float* rows1p = rows1;
            int dx = 0;
            for (; dx < w; dx++)
            {
                int sx = xofs[dx];
                const __fp16* S1p = S1 + sx;

                float a0 = alphap[0];
                float a1 = alphap[1];
                rows1p[dx] = (float)S1p[0] * a0 + (float)S1p[1] * a1;

                alphap += 2;
            }
        }
        else
        {
            // hresize two rows
            const __fp16* S0 = src.row<const __fp16>(sy);
            const __fp16* S1 = src.row<const __fp16>(sy + 1);

            const float* alphap = alpha;
            float* rows0p = rows0;
            float* rows1p = rows1;
            int dx = 0;
            for (; dx < w; dx++)
            {
                int sx = xofs[dx];
                const __fp16* S0p = S0 + sx;
                const __fp16* S1p = S1 + sx;

                float a0 = alphap[0];
                float a1 = alphap[1];
                rows0p[dx] = (float)S0p[0] * a0 + (float)S0p[1] * a1;
                rows1p[dx] = (float)S1p[0] * a0 + (float)S1p[1] * a1;

                alphap += 2;
            }
        }

        prev_sy1 = sy;

        // vresize
        float b0 = beta[0];
        float b1 = beta[1];

        float* rows0p = rows0;
        float* rows1p = rows1;
        __fp16* Dp = dst.row<__fp16>(dy);

#if __riscv_zvfh
        int n = w;
        while (n > 0)
        {
            size_t vl = __riscv_vsetvl_e16m4(n);

            vfloat32m8_t _rows0 = __riscv_vle32_v_f32m8(rows0p, vl);
            vfloat32m8_t _rows1 = __riscv_vle32_v_f32m8(rows1p, vl);

            vfloat32m8_t _Dp = __riscv_vfmacc_vf_f32m8(__riscv_vfmul_vf_f32m8(_rows0, b0, vl), b1, _rows1, vl);

            __riscv_vse16_v_f16m4(Dp, __riscv_vfncvt_f_f_w_f16m4(_Dp, vl), vl);

            Dp += vl;
            rows0p += vl;
            rows1p += vl;
            n -= vl;
        }
#else  // __riscv_zvfh
        for (int i = 0; i < w; i++)
        {
            *Dp++ = (__fp16)(*rows0p++ * b0 + *rows1p++ * b1);
        }
#endif // __riscv_zvfh

        beta += 2;
    }
}

static void resize_bilinear_image_fp16sa(const Mat& src, Mat& dst, __fp16* alpha, int* xofs, __fp16* beta, int* yofs)
{
    int w = dst.w;
    int h = dst.h;

    // loop body
    Mat rowsbuf0(w, (size_t)2u);
    Mat rowsbuf1(w, (size_t)2u);
    __fp16* rows0 = rowsbuf0;
    __fp16* rows1 = rowsbuf1;

    int prev_sy1 = -2;

    for (int dy = 0; dy < h; dy++)
    {
        int sy = yofs[dy];

        if (sy == prev_sy1)
        {
            // reuse all rows
        }
        else if (sy == prev_sy1 + 1)
        {
            // hresize one row
            __fp16* rows0_old = rows0;
            rows0 = rows1;
            rows1 = rows0_old;
            const __fp16* S1 = src.row<const __fp16>(sy + 1);

            const __fp16* alphap = alpha;
            __fp16* rows1p = rows1;
            int dx = 0;
            for (; dx < w; dx++)
            {
                int sx = xofs[dx];
                const __fp16* S1p = S1 + sx;

                __fp16 a0 = alphap[0];
                __fp16 a1 = alphap[1];
                rows1p[dx] = S1p[0] * a0 + S1p[1] * a1;

                alphap += 2;
            }
        }
        else
        {
            // hresize two rows
            const __fp16* S0 = src.row<const __fp16>(sy);
            const __fp16* S1 = src.row<const __fp16>(sy + 1);

            const __fp16* alphap = alpha;
            __fp16* rows0p = rows0;
            __fp16* rows1p = rows1;
            int dx = 0;
            for (; dx < w; dx++)
            {
                int sx = xofs[dx];
                const __fp16* S0p = S0 + sx;
                const __fp16* S1p = S1 + sx;

                __fp16 a0 = alphap[0];
                __fp16 a1 = alphap[1];
                rows0p[dx] = S0p[0] * a0 + S0p[1] * a1;
                rows1p[dx] = S1p[0] * a0 + S1p[1] * a1;

                alphap += 2;
            }
        }

        prev_sy1 = sy;

        // vresize
        __fp16 b0 = beta[0];
        __fp16 b1 = beta[1];

        __fp16* rows0p = rows0;
        __fp16* rows1p = rows1;
        __fp16* Dp = dst.row<__fp16>(dy);

#if __riscv_zvfh
        int n = w;
        while (n > 0)
        {
            size_t vl = __riscv_vsetvl_e16m8(n);

            vfloat16m8_t _rows0 = __riscv_vle16_v_f16m8(rows0p, vl);
            vfloat16m8_t _rows1 = __riscv_vle16_v_f16m8(rows1p, vl);

            vfloat16m8_t _Dp = __riscv_vfmacc_vf_f16m8(__riscv_vfmul_vf_f16m8(_rows0, b0, vl), b1, _rows1, vl);

            __riscv_vse16_v_f16m8(Dp, _Dp, vl);

            Dp += vl;
            rows0p += vl;
            rows1p += vl;
            n -= vl;
        }
#else  // __riscv_zvfh
        for (int i = 0; i < w; i++)
        {
            *Dp++ = *rows0p++ * b0 + *rows1p++ * b1;
        }
#endif // __riscv_zvfh

        beta += 2;
    }
}
