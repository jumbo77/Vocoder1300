/*
 * Copyright (C) 1993-2016 David Rowe
 *
 * All rights reserved
 * 
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#include "defines.h"
#include "sine.h"
#include "codec2_fft.h"

void synthesise(codec2_fftr_cfg fft_inv_cfg, float Sn_[], MODEL *model, float Pn[], int shift) {
    COMP Sw_[FFT_SIZE];
    float sw_[FFT_SIZE];
    int i, l, j, b;

    if (shift) {
        /* Update memories */
        for (i = 0; i < (N - 1); i++) {
            Sn_[i] = Sn_[i + N];
        }
        
        Sn_[N - 1] = 0.0f;
    }

    for (i = 0; i < FFT_SIZE; i++) {
        Sw_[i].real = 0.0f;
        Sw_[i].imag = 0.0f;
    }

    /* Now set up frequency domain synthesized speech */
    for (l = 1; l <= model->L; l++) {
        b = (int) (l * model->Wo * FFT_SIZE / TAU + 0.5f);
        
        if (b > ((FFT_SIZE / 2) - 1)) {
            b = (FFT_SIZE / 2) - 1;
        }
        
        Sw_[b].real = model->A[l] * cosf(model->phi[l]);
        Sw_[b].imag = model->A[l] * sinf(model->phi[l]);
        
        Sw_[FFT_SIZE - b].real = Sw_[b].real;
        Sw_[FFT_SIZE - b].imag = -Sw_[b].imag;
    }

    /* Perform inverse DFT */
    
    codec2_fftri(fft_inv_cfg, Sw_, sw_);

    /* Overlap add to previous samples */

    for (i = 0; i < (N - 1); i++) {
        Sn_[i] += sw_[FFT_SIZE - N + 1 + i] * Pn[i];
    }

    if (shift) {
        for (i = (N - 1), j = 0; i < (2 * N); i++, j++) {
            Sn_[i] = sw_[j] * Pn[i];
        }
    } else {
        for (i = (N - 1), j = 0; i < (2 * N); i++, j++) {
            Sn_[i] += sw_[j] * Pn[i];
        }
    }
}
