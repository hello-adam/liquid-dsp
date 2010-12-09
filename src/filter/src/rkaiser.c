/*
 * Copyright (c) 2007, 2008, 2009, 2010 Joseph Gaeddert
 * Copyright (c) 2007, 2008, 2009, 2010 Virginia Polytechnic
 *                                      Institute & State University
 *
 * This file is part of liquid.
 *
 * liquid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * liquid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with liquid.  If not, see <http://www.gnu.org/licenses/>.
 */

//
// Design root-Nyquist Kaiser filter
//
// References
//  [Vaidyanathan:1993] Vaidyanathan, P. P., "Multirate Systems and
//      Filter Banks," 1993, Prentice Hall, Section 3.2.1
//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "liquid.internal.h"

#define DEBUG_RKAISER 1

// design_rkaiser_filter()
//
// Design frequency-shifted root-Nyquist filter based on
// the Kaiser-windowed sinc.
//
//  _k      :   filter over-sampling rate (samples/symbol)
//  _m      :   filter delay (symbols)
//  _beta   :   filter excess bandwidth factor (0,1)
//  _dt     :   filter fractional sample delay
//  _h      :   resulting filter [size: 2*_k*_m+1]
void design_rkaiser_filter(unsigned int _k,
                           unsigned int _m,
                           float _beta,
                           float _dt,
                           float * _h)
{
    // validate input
    if (_k < 2) {
        fprintf(stderr,"error: design_rkaiser_filter(), k must be at least 2\n");
        exit(1);
    } else if (_m < 1) {
        fprintf(stderr,"error: design_rkaiser_filter(), m must be at least 1\n");
        exit(1);
    } else if (_beta <= 0.0f || _beta >= 1.0f) {
        fprintf(stderr,"error: design_rkaiser_filter(), beta must be in (0,1)\n");
        exit(1);
    } else if (_dt < -1.0f || _dt > 1.0f) {
        fprintf(stderr,"error: design_rkaiser_filter(), dt must be in [-1,1]\n");
        exit(1);
    }

    // simply call internal method and ignore output rho value
    float rho;
    design_rkaiser_filter_internal(_k,_m,_beta,_dt,_h,&rho);
}

// design_arkaiser_filter()
//
// Design frequency-shifted root-Nyquist filter based on
// the Kaiser-windowed sinc using approximation for rho.
//
//  _k      :   filter over-sampling rate (samples/symbol)
//  _m      :   filter delay (symbols)
//  _beta   :   filter excess bandwidth factor (0,1)
//  _dt     :   filter fractional sample delay
//  _h      :   resulting filter [size: 2*_k*_m+1]
void design_arkaiser_filter(unsigned int _k,
                            unsigned int _m,
                            float _beta,
                            float _dt,
                            float * _h)
{
    // validate input
    if (_k < 2) {
        fprintf(stderr,"error: design_arkaiser_filter(), k must be at least 2\n");
        exit(1);
    } else if (_m < 1) {
        fprintf(stderr,"error: design_arkaiser_filter(), m must be at least 1\n");
        exit(1);
    } else if (_beta <= 0.0f || _beta >= 1.0f) {
        fprintf(stderr,"error: design_arkaiser_filter(), beta must be in (0,1)\n");
        exit(1);
    } else if (_dt < -1.0f || _dt > 1.0f) {
        fprintf(stderr,"error: design_arkaiser_filter(), dt must be in [-1,1]\n");
        exit(1);
    }

    // compute bandwidth adjustment estimate
    float rho_hat = rkaiser_approximate_rho(_m,_beta);
    float gamma_hat = rho_hat*_beta;                // un-normalized correction factor

    unsigned int n=2*_k*_m+1;                       // filter length
    float del = gamma_hat / (float)_k;              // transition bandwidth
    float As  = 14.26f*del*n + 7.95f;               // sidelobe attenuation
    float fc  = (1 + _beta - gamma_hat)/(float)_k;  // filter cutoff

    // compute filter coefficients
    fir_kaiser_window(n,fc,As,_dt,_h);

    // normalize coefficients
    float e2 = 0.0f;
    unsigned int i;
    for (i=0; i<n; i++) e2 += _h[i]*_h[i];
    for (i=0; i<n; i++) _h[i] *= sqrtf(_k/e2);
}

// Find approximate bandwidth adjustment factor rho based on
// filter delay and desired excess bandwdith factor.
//
//  _m      :   filter delay (symbols)
//  _beta   :   filter excess bandwidth factor (0,1)
float rkaiser_approximate_rho(unsigned int _m,
                              float _beta)
{
    if ( _m < 1 ) {
        fprintf(stderr,"error: rkaiser_approximate_rho(): m must be greater than 0\n");
        exit(0);
    } else if ( (_beta < 0.0f) || (_beta > 1.0f) ) {
        fprintf(stderr,"error: rkaiser_approximate_rho(): beta must be in [0,1]\n");
        exit(0);
    } else;

    // compute bandwidth adjustment estimate
    float c0=0.0f, c1=0.0f, c2=0.0f;
    switch (_m) {
    case 1:  c0=0.78583556; c1=0.05439958; c2=0.37818679; break;
    case 2:  c0=0.82194722; c1=0.06170731; c2=0.16362774; break;
    case 3:  c0=0.84686762; c1=0.07475776; c2=0.05263769; break;
    case 4:  c0=0.86538726; c1=0.07374587; c2=0.03491642; break;
    case 5:  c0=0.87861007; c1=0.06981039; c2=0.03553645; break;
    case 6:  c0=0.88901162; c1=0.06708569; c2=0.03459680; break;
    default:
             c0 = 0.057918*logf(_m) + 0.784313;
             c1 = _m <= 3 ?
                     0.0099427*_m + 0.0447250 :
                    -0.0026685*_m + 0.0835030;
             c2 = 0.03373 + expf((-0.30382*_m*_m -0.19451*_m -0.56171));
    }
    // ensure no invalid log taken
    if (c2 >= _beta)
        c2 = 0.999f*_beta;

    float rho_hat = c0 + c1*logf(_beta - c2);

    // ensure estimate is in [0,1]
    if (rho_hat < 0.0f) {
        rho_hat = 0.0f;
    } else if (rho_hat > 1.0f) {
        rho_hat = 1.0f;
    }

    return rho_hat;
}

// design_rkaiser_filter_internal()
//
// Design frequency-shifted root-Nyquist filter based on
// the Kaiser-windowed sinc.
//
//  _k      :   filter over-sampling rate (samples/symbol)
//  _m      :   filter delay (symbols)
//  _beta   :   filter excess bandwidth factor (0,1)
//  _dt     :   filter fractional sample delay
//  _h      :   resulting filter [size: 2*_k*_m+1]
//  _rho    :   transition bandwidth adjustment, 0 < _rho < 1
void design_rkaiser_filter_internal(unsigned int _k,
                                    unsigned int _m,
                                    float _beta,
                                    float _dt,
                                    float * _h,
                                    float * _rho)
{
    if ( _k < 1 ) {
        fprintf(stderr,"error: design_rkaiser_filter_internal(): k must be greater than 0\n");
        exit(1);
    } else if ( _m < 1 ) {
        fprintf(stderr,"error: design_rkaiser_filter_internal(): m must be greater than 0\n");
        exit(1);
    } else if ( (_beta < 0.0f) || (_beta > 1.0f) ) {
        fprintf(stderr,"error: design_rkaiser_filter_internal(): beta must be in [0,1]\n");
        exit(1);
    } else;

    unsigned int i;

    unsigned int n=2*_k*_m+1;   // filter length

    // compute bandwidth adjustment estimate
    float rho_hat = rkaiser_approximate_rho(_m,_beta);

    // bandwidth adjustment array (3 points makes a parabola)
    float x0 = rho_hat*0.9f;
    float x1;
    float x2 = rho_hat*1.1f;

    // evaluate performance (ISI) of each bandwidth adjustment
    float y0 = design_rkaiser_filter_internal_isi(_k,_m,_beta,_dt,x0,_h);
    float y1;
    float y2 = design_rkaiser_filter_internal_isi(_k,_m,_beta,_dt,x2,_h);

    // run parabolic search to find bandwidth adjustment x_hat which
    // minimizes the inter-symbol interference of the filter
    unsigned int p, pmax=10;
    float t0, t1;
    float x_hat = rho_hat;
    float y_hat;
    for (p=0; p<pmax; p++) {
        // choose center point of [x0,x2]
        x1 = 0.5f*(x0 + x2);
        y1 = design_rkaiser_filter_internal_isi(_k,_m,_beta,_dt,x1,_h);

        // numerator
        t0 = y0 * (x1*x1 - x2*x2) +
             y1 * (x2*x2 - x0*x0) +
             y2 * (x0*x0 - x1*x1);

        // denominator
        t1 = y0 * (x1 - x2) +
             y1 * (x2 - x0) +
             y2 * (x0 - x1);

        // break if denominator is sufficiently small
        if (fabsf(t1) < 1e-9f) break;

        // compute new estimate
        x_hat = 0.5f * t0 / t1;

        // search index of maximum
        if (x_hat > x1) {
            // new minimum
            x0 = x1;
            y0 = y1;
        } else {
            // new maximum
            x2 = x1;
            y2 = y1;
        }

#if DEBUG_RKAISER
        y_hat = design_rkaiser_filter_internal_isi(_k,_m,_beta,_dt,x_hat,_h);
        printf("  %4u : rho=%12.8f, isi=%12.6f dB\n", p+1, x_hat, 20*log10f(y_hat));
#endif
    };

    // re-design filter with optimal value for rho
    y_hat = design_rkaiser_filter_internal_isi(_k,_m,_beta,_dt,x_hat,_h);

    // normalize filter magnitude
    float e2 = 0.0f;
    for (i=0; i<n; i++) e2 += _h[i]*_h[i];
    for (i=0; i<n; i++) _h[i] *= sqrtf(_k/e2);

    // save trasition bandwidth adjustment
    *_rho = rho_hat;
}

// compute filter coefficients and determine resulting ISI
//  
//  _k      :   filter over-sampling rate (samples/symbol)
//  _m      :   filter delay (symbols)
//  _beta   :   filter excess bandwidth factor (0,1)
//  _dt     :   filter fractional sample delay
//  _rho    :   transition bandwidth adjustment, 0 < _rho < 1
//  _h      :   filter buffer [size: 2*_k*_m+1]
float design_rkaiser_filter_internal_isi(unsigned int _k,
                                         unsigned int _m,
                                         float _beta,
                                         float _dt,
                                         float _rho,
                                         float * _h)
{
    unsigned int n=2*_k*_m+1;           // filter length
    float gamma = _rho * _beta;         // un-normalized correction factor
    float kf = (float)_k;               // samples/symbol (float)
    float del = gamma / kf;             // transition bandwidth
    float As = 14.26f*del*n + 7.95f;    // sidelobe attenuation
    float fc = (1 + _beta - gamma)/kf;  // filter cutoff

    // evaluate performance (ISI)
    float isi_max;
    float isi_mse;

    // compute filter
    fir_kaiser_window(n,fc,As,_dt,_h);

    // compute filter ISI
    liquid_filter_isi(_h,_k,_m,&isi_mse,&isi_max);

    // return RMS of ISI
    return isi_mse;
}


