/*
 * Copyright (C) 1993-2016 David Rowe
 *
 * All rights reserved
 * 
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#include "defines.h"
#include "kiss_fftr.h"
#include "codec2_fft.h"
#include "quantise.h"

int lsp_bits_decode(int i) {
    return lsp_cb[i].log2m;
}

void lsp_to_lpc(float *lsp, float *ak, int order) {
    float xout1, xout2, xin1, xin2;
    float *pw, *n1, *n2, *n3, *n4 = 0;
    float freq[order];
    float Wp[(order * 4) + 2];
    int i, j;

    for (i = 0; i < order; i++)
        freq[i] = cosf(lsp[i]);

    pw = Wp;

    for (i = 0; i <= (4 * (order / 2) + 1); i++) {
        *pw++ = 0.0f;
    }

    pw = Wp;
    xin1 = 1.0f;
    xin2 = 1.0f;

    for (j = 0; j <= order; j++) {
        for (i = 0; i < (order / 2); i++) {
            n1 = pw + (i * 4);
            n2 = n1 + 1;
            n3 = n2 + 1;
            n4 = n3 + 1;

            xout1 = xin1 - 2.0f * (freq[2 * i]) * *n1 + *n2;
            xout2 = xin2 - 2.0f * (freq[2 * i + 1]) * *n3 + *n4;

            *n2 = *n1;
            *n4 = *n3;
            *n1 = xin1;
            *n3 = xin2;
            xin1 = xout1;
            xin2 = xout2;
        }

        xout1 = xin1 + *(n4 + 1);
        xout2 = xin2 - *(n4 + 2);

        ak[j] = (xout1 + xout2) * 0.5f;

        *(n4 + 1) = xin1;
        *(n4 + 2) = xin2;

        xin1 = 0.0f;
        xin2 = 0.0f;
    }
}

void check_lsp_order(float lsp[], int order) {
    float tmp;
    int i;

    for (i = 1; i < order; i++) {
        if (lsp[i] < lsp[i - 1]) {
            tmp = lsp[i - 1];
            lsp[i - 1] = lsp[i] - 0.1f;
            lsp[i] = tmp + 0.1f;
            i = 1;
        }
    }
}

static void lpc_post_filter(kiss_fftr_cfg fft_fwd_cfg, float Pw[], float ak[],
        int order, float beta, float gamma, int bass_boost, float E) {
    COMP Ww[FFT_SIZE / 2 + 1];
    float x[FFT_SIZE];
    float Rw[FFT_SIZE / 2 + 1];
    float e_before, e_after, gain;
    float Pfw;
    float max_Rw, min_Rw;
    float coeff;
    int i;

    for (i = 0; i < FFT_SIZE; i++) {
        x[i] = 0.0f;
    }

    x[0] = ak[0];
    coeff = gamma;

    for (i = 1; i <= order; i++) {
        x[i] = ak[i] * coeff;
        coeff *= gamma;
    }

    codec2_fftr(fft_fwd_cfg, x, Ww);

    for (i = 0; i < (FFT_SIZE / 2); i++) {
        Ww[i].real = Ww[i].real * Ww[i].real + Ww[i].imag * Ww[i].imag;
    }

    max_Rw = 0.0f;
    min_Rw = 1E32f;

    for (i = 0; i < (FFT_SIZE / 2); i++) {
        Rw[i] = sqrtf(Ww[i].real * Pw[i]);

        if (Rw[i] > max_Rw)
            max_Rw = Rw[i];

        if (Rw[i] < min_Rw)
            min_Rw = Rw[i];

    }

    e_before = 1E-4f;

    for (i = 0; i < (FFT_SIZE / 2); i++) {
        e_before += Pw[i];
    }

    e_after = 1E-4f;

    for (i = 0; i < (FFT_SIZE / 2); i++) {
        Pfw = powf(Rw[i], beta);
        Pw[i] *= Pfw * Pfw;
        e_after += Pw[i];
    }

    gain = e_before / e_after;
    gain *= E;

    for (i = 0; i < (FFT_SIZE / 2); i++) {
        Pw[i] *= gain;
    }

    if (bass_boost) {
        for (i = 0; i < FFT_SIZE / 8; i++) {
            Pw[i] *= 1.4f * 1.4f;
        }
    }
}

void aks_to_M2(codec2_fftr_cfg fftr_fwd_cfg, float ak[], int order, MODEL *model,
        float E, float *snr, int sim_pf, int pf, int bass_boost,
        float beta, float gamma, COMP Aw[]) {
    float Pw[FFT_SIZE / 2];
    float a[FFT_SIZE];
    float Em, Am;
    int am, bm, i, m;

    float r = TAU / FFT_SIZE;

    for (i = 0; i < FFT_SIZE; i++) {
        a[i] = 0.0f;
    }

    for (i = 0; i < (FFT_SIZE / 2); i++) {
        Pw[i] = 0.0f;
    }

    for (i = 0; i <= order; i++) {
        a[i] = ak[i];
    }
    
    codec2_fftr(fftr_fwd_cfg, a, Aw);

    for (i = 0; i < (FFT_SIZE / 2); i++) {
        Pw[i] = 1.0f / (Aw[i].real * Aw[i].real + Aw[i].imag * Aw[i].imag + 1E-6f);
    }

    if (pf) {
        lpc_post_filter(fftr_fwd_cfg, Pw, ak, order, beta, gamma, bass_boost, E);
    } else {
        for (i = 0; i < (FFT_SIZE / 2); i++) {
            Pw[i] *= E;
        }
    }

    float signal = 1E-30f;
    float noise = 1E-32f;

    for (m = 1; m <= model->L; m++) {
        am = (int) ((m - 0.5f) * model->Wo / r + 0.5f);
        bm = (int) ((m + 0.5f) * model->Wo / r + 0.5f);
        Em = 0.0f;

        for (i = am; i < bm; i++)
            Em += Pw[i];

        Am = sqrtf(Em);

        signal += (model->A[m] * model->A[m]);
        noise += ((model->A[m] - Am) * (model->A[m] - Am));

        if (sim_pf) {
            if (Am > model->A[m])
                Am *= 0.7f;
            
            if (Am < model->A[m])
                Am *= 1.4f;
        }

        model->A[m] = Am;
    }

    *snr = 10.0f * log10f(signal / noise);
}

float decode_Wo(int index, int bits) {
    return (TAU / P_MAX) + (((TAU / P_MIN) - (TAU / P_MAX)) / (1 << bits)) * index;
}

float decode_energy(int index, int bits) {
    return powf(10.0f, (E_MIN_DB + ((E_MAX_DB - E_MIN_DB) / (1 << bits)) * index) / 10.0f);
}

void decode_lsps_scalar(float lsp[], int indexes[], int order) {
    float lsp_hz[order];
    const float *cb;
    int i, k;

    for (i = 0; i < order; i++) {
        k = lsp_cb[i].k;
        cb = lsp_cb[i].cb;
        lsp_hz[i] = cb[indexes[i] * k];
    }

    for (i = 0; i < order; i++) {
        lsp[i] = (M_PI / 4000.0f) * lsp_hz[i];
    }
}

void bw_expand_lsps(float lsp[], int order, float min_sep_low, float min_sep_high) {
    int i;

    for (i = 1; i < 4; i++) {
        if ((lsp[i] - lsp[i - 1]) < min_sep_low * (M_PI / 4000.0f)) {
            lsp[i] = lsp[i - 1] + min_sep_low * (M_PI / 4000.0f);
        }
    }

    for (i = 4; i < order; i++) {
        if (lsp[i] - lsp[i - 1] < min_sep_high * (M_PI / 4000.0f)) {
            lsp[i] = lsp[i - 1] + min_sep_high * (M_PI / 4000.0f);
        }
    }
}

void apply_lpc_correction(MODEL *model) {
    if (model->Wo < (M_PI * 150.0f / 4000.0f)) {
        model->A[1] *= 0.032f;
    }
}

