/*
# Using Re:Zero(Bachlechner et Al), in order to ensure gradients propagate
# https://arxiv.org/abs/2003.04887
# As Re:Zero stars off with the identity function only we use Xavier initilisation(Glorot et Al)
# https://proceedings.mlr.press/v9/glorot10a/glorot10a.pdf
# ReLU(Agarap) actiation function is also employed 
# https://arxiv.org/abs/1803.08375


# eventually will implement jagged array to replace all std::vector<std::vector<T>>
*/
#pragma once

#include<vector>
#include<set>
#include<cmath>
#include<iostream>
#include<cstdlib>
#include<string>
#include<random>
#include<fstream>
#include<limits>
#include<iomanip>
#include<algorithm>
#include<filesystem>
#include<variant>
#include<omp.h>
#include<array>

const float max_val = 10000000000;
const float n_zero = 0.0001; //try and stop things from freaking out 
const float leak = 0.01;
const float memboost = 3.14159265f;

std::uniform_real_distribution<float> zero_to_one(0.0,1.0);

// keyboard mashed the name for these because standard names like r_dev to avoid clashing
std::random_device rsksksksksks;                          
std::mt19937 ttttt(rsksksksksks());

inline float sign_of(float x){
    return((std::signbit(x) * -2.0f) + 1.0f);
}

float quadractic_increase(float x){
    if (std::abs(x) > 1)
    {
        int sign = sign_of(x);
        x = std::sqrt(std::abs(x)) * sign;
        return x;
    }
    else{
        return x;
        }
}

float log_increase(float x){
    if (std::abs(x) > 1)
    {
        int sign = sign_of(x);
        x = std::log(std::abs(x) + 1) * sign;
        return x;
    }
    else{
        return x;
        }
}
//based on "A handy approximation for the error function and its inverse" by Sergei Winitzki.,
//https://www.academia.edu/9730974/A_handy_approximation_for_the_error_function_and_its_inverse
float approx_erfinv(float x){
    static const double a = 0.6802721088435374;         //this is 1/1.47
    static const float pita_inv = 0.4330746750799873;  //this is 1/1.47 times 1/pi times 2
    float w = 1-x*x;
    w = (w == 1) ? 0.999999:w;
    w = std::log(w);
    float u = pita_inv+(w*0.5);
    float val = std::sqrt(-u + std::sqrt((u * u) - (a * w)));
    return(val * sign_of(x));
}

/*
# activation functions and their derivatives
*/

inline float relu(float x){
    return ((x>0.0f) ? x:0.0f);
}
inline float drelu(float fx){
    return (fx>0.0f);
}

// it may be useful to utilise log space 
// so this function is simply ln(relu(x)+1)
inline float log_relu(float x){
    return std::logf((x>0.0f) ? (x+1.0f):1.0f);
}
inline float dlog_relu(float fx){
    return ((fx>0.0f) ? std::expf(-fx):0.0f);
}

// function to compute cos(x) from sin(x)
// i.e. the derivative of sin(x) activation function
inline float cos_sinx(float sinx){
    return std::sqrtf(1 - (sinx*sinx));
}

bool broken_float(float x){
    if (std::isnan(x)){
        return true;
    }
    else if (std::isinf(x)){
        return true;
    }
    else{
        return false;
    }
}

inline float lrelu(float x){
    return((x>0) ? x:leak*x);
}

inline float sigmoid(float x){
    x = (std::abs(x)<70) ? x:70*sign_of(x);     //to prevent underflow and overflow
    return 1.0f/(std::exp(-x)+1);
}

void soft_max(std::vector<float> &output){
    double denominator = 0;
    std::vector<float> expout(output.size());
    for (int i = 0; i < output.size(); i++)
    {
        expout[i] = std::exp(output[i]);
        if (broken_float(expout[i]))
        {
            expout[i] = 0;
            std::cout<<"error"<<std::endl;
        }
        denominator += expout[i]; 
    }
    denominator = (std::abs(denominator) < 0.001) ? 0.001:denominator;
    denominator = 1 / denominator; 
    for (int i = 0; i < output.size(); i++)
    {
        output[i] = expout[i] * denominator;
    }
}

int prediction(std::vector<float> &output){
    int guess = std::distance(output.begin(),std::max_element(output.begin(),output.end()));
    return guess;
}

void dsoft_max(std::vector<float> &output, std::vector<float> &target, std::vector<float> &loss){
    for (int i = 0; i < output.size(); i++)
    {
        loss[i] = output[i] - target[i];
    }
}

float cross_entrophy_loss(std::vector<float> &target, std::vector<float> & output){
    float loss = 0;
    for (int i = 0; i < output.size(); i++)
    {
        output[i] = (output[i] > n_zero) ? output[i]:n_zero;
        loss -= (target[i] * std::log(output[i]));
    }
    return loss;
}

// struct for holding gradients for individual neurons before update
// neurons will have 1 input 1 output and 2 hidden layers of 8 units
struct neuron_gradients
{
    float bias[16] = {0};
    float alpha[16] = {0};
    float weights[9][7] = {0};
    float padding;
    inline void valclear(){    //wrapper function for memset
        memset(alpha,0,sizeof(alpha));
        memset(bias,0,sizeof(bias));
        memset(weights,0,sizeof(weights));
    }   
    template <typename neuron>
    inline void sgd_with_momentum(neuron &n, float learning_rate, float momentum, neuron_gradients current_grad){    
        #pragma omp simd
        for (uint8_t i = 0; i < 16; i++)
        {
            bias[i] *= momentum;
            current_grad.bias[i] *= learning_rate;
            bias[i] += current_grad.bias[i];
            n.bias[i] -= bias[i];
        }
        
        #pragma omp simd
        for (uint8_t i = 0; i < 16; i++)
        {
            alpha[i] *= momentum;
            current_grad.alpha[i] *= learning_rate;
            alpha[i] += current_grad.alpha[i];
            n.alpha[i] -= alpha[i];
        }

        for (uint8_t i = 0; i < 9; i++)
        {
            #pragma omp simd
            for (uint8_t j = 0; j < 7; j++)
            {
                weights[i][j] *= momentum;
                current_grad.weights[i][j] *= learning_rate;
                weights[i][j] += current_grad.weights[i][j];
                n.weights[i][j] -= weights[i][j];
            }
            
        }
        
    }
    inline void add(neuron_gradients & grad){
        #pragma omp simd
        for (int i = 0; i < 16; i++)
        {
            bias[i] += grad.bias[i];
        }
        #pragma omp simd
        for (int i = 0; i < 16; i++)
        {
            alpha[i] += grad.alpha[i];
        }
        #pragma omp simd collapse(2)
        for (int i = 0; i < 9; i++)
        {
            for (int j = 0; j < 7; j++)
            {
                weights[i][j] += grad.weights[i][j];
            }
            
        }
        
    }
};

/*
# each neuron is basically a simple 2 hidden layer nn 
# Re:Zero is used to allow the gradients to pass through 
# 1 input 7 hidden units in the 2 layers then 1 output
# each struct implements a neuron with a different activation function
*/


struct neuron_unit
{
    float units[16] = {0};     // 0 will be input and 15 will be output
    float bias[16]  = {0};   
    float alpha[16] = {0};     // from Re:Zero is all you need 
    float weights[9][7];
    uint16_t isinput = 0;         
    int16_t a_f = 0;          //activation function defaults to ReLU
    //tags for tag dispatching
    struct relu_neuron{relu_neuron(){}};            // a_f = 0
    struct sine_neuron{sine_neuron(){}};            // a_f = 1
    struct log_relu_neuron{log_relu_neuron(){}};    // a_f = 2
    struct memory_neuron{memory_neuron(){}};        // a_f = 3 (input unit is mrelu, other units relu)
    
    neuron_unit(){
        //  Xavier initialisation
        std::normal_distribution<float> a (0,0.5);            // input to 1st hidden layer
        std::normal_distribution<float> b (0,0.377964473);    // input to 2nd hidden layer
        for (uint8_t i = 0; i < 7; i++)
        {
            weights[0][i] = a(ttttt);
            weights[8][i] = a(ttttt);
        }
        for (uint8_t i = 1; i < 8; i++)
        {
            for (uint8_t j = 0; j < 8; j++)
            {
                weights[i][j] = b(ttttt);
            }
        }
    }
    
    inline void valclear(){    //wrapper function for memset on units
        memset(units,0,sizeof(units));
    };

    // wrapper function for relu and drelu
    inline float act_func(float x, relu_neuron){
        return relu(x);
    }
    inline float dact_func(float fx, relu_neuron){
        return drelu(fx);
    }

    // wrapper function for sine and cos(arcsin(x))
    inline float act_func(float x, sine_neuron){
        return std::sinf(x);
    }
    inline float dact_func(float fx, sine_neuron){
        return cos_sinx(fx);
    }

    // wrapper function for log_relu and dlog_relu
    inline float act_func(float x, log_relu_neuron){
        return log_relu(x);
    }
    inline float dact_func(float fx, log_relu_neuron){
        return dlog_relu(fx);
    }
    
    template <typename T>
    inline void f_p(std::array<float,16> &pacts, T af){ //pacts here refers to values obtained after applying activation function
        units[0] += bias[0];
        pacts[0] = act_func(units[0],af);
        units[0] += (pacts[0] * alpha[0]);
        #pragma omp simd
        for (uint8_t i = 1; i < 8; i++)
        {
            units[i] = units[0] * weights[0][i-1];
            units[i] += bias[i];
            pacts[i] = act_func(units[i],af);
            units[i] += pacts[i] * alpha[i];
        }
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            units[i] = bias[i];        
        }
        #pragma omp simd collapse(2)
        for (uint8_t i = 8; i < 15; i++)
        {
            for (uint8_t j = 0; j < 7; j++)
            {
                units[i] += units[j+1] * weights[i-7][j];
            }    
        }
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            pacts[i] = act_func(units[i],af);
            units[i] += pacts[i] * alpha[i];
        }
        units[15] = bias[15];
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            units[15] += units[i] * weights[8][i-8];
        }
        pacts[15] = act_func(units[15],af);
        units[15] += pacts[15] * alpha[15];
    }

    inline void f_p(std::array<float,16> &pacts,  memory_neuron &){ //pacts here refers to values obtained after applying activation function
        units[0] += bias[0];
        pacts[0] = relu(units[0]);
        units[0] += (pacts[0] * alpha[0]);
        #pragma omp simd
        for (uint8_t i = 1; i < 8; i++)
        {
            units[i] = units[0] * weights[0][i-1];
            units[i] += bias[i];
            pacts[i] = relu(units[i]);
            units[i] += pacts[i] * alpha[i];
        }
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            units[i] = bias[i];        
        }
        #pragma omp simd collapse(2)
        for (uint8_t i = 8; i < 15; i++)
        {
            for (uint8_t j = 0; j < 7; j++)
            {
                units[i] += units[j+1] * weights[i-7][j];
            }    
        }
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            pacts[i] = relu(units[i]);
            units[i] += pacts[i] * alpha[i];
        }
        // the output will be determined elsewhere
    }

    inline void mem_update(float ht_m1){
        float a = alpha[15];
        float b = bias[15];
        #pragma omp simd
        for (uint8_t i = 8; i < 11; i++)
        {
            a += units[i] * weights[8][i-8];
        }
        #pragma omp simd
        for (uint8_t i = 11; i < 15; i++)
        {
            b += units[i] * weights[8][i-8];
        }
        units[15] = ht_m1 + std::tanh(b)*sigmoid(a);
    }

    inline void forward_pass(std::array<float,16> &pacts,float ht_m1=0){
        sine_neuron s;
        log_relu_neuron l;
        relu_neuron r;
        memory_neuron m;
        switch (a_f)
        {
        case 1:
            f_p(pacts,s);
            return;
        case 2:
            f_p(pacts,l);
            return;
        case 3:
            f_p(pacts,m);
            mem_update(ht_m1);
            return;
        default:
            f_p(pacts,r);
            return;
        }
    }

    // forwardpass without recording post activations for each unit
    template <typename T>
    inline void f_p(T af){
        units[0] += bias[0];
        units[0] += (act_func(units[0],af) * alpha[0]);
        #pragma omp simd
        for (uint8_t i = 1; i < 8; i++)
        {
            units[i] = units[0] * weights[0][i-1];
            units[i] += bias[i];
            units[i] += act_func(units[i],af) * alpha[i];
        }
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            units[i] = bias[i];        
        }
        #pragma omp simd collapse(2)
        for (uint8_t i = 8; i < 15; i++)
        {
            for (uint8_t j = 0; j < 7; j++)
            {
                units[i] += units[j+1] * weights[i-7][j];
            }    
        }
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            units[i] += act_func(units[i],af) * alpha[i];
        }
        units[15] = bias[15];
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            units[15] += units[i] * weights[8][i-8];
        }
        units[15] += act_func(units[15],af) * alpha[15];
    }

    inline void forward_pass(){
        sine_neuron s;
        log_relu_neuron l;
        relu_neuron r;
        switch (a_f)
        {
        case 1:
            f_p(s);
            return;
        case 2:
            f_p(l);
            return;
        default:
            f_p(r);
            return;
        }
    }

    inline float backprop(float dldz, std::array<float,16> &past_unit, std::array<float,16> &pacts, neuron_gradients &gradients, memory_neuron&, float ht_m1, float &tm1grad)
    {   
        float a = alpha[15];
        float b = bias[15];
        #pragma omp simd
        for (uint8_t i = 8; i < 11; i++)
        {
            a += past_unit[i] * weights[8][i-8];
        }
        #pragma omp simd
        for (uint8_t i = 11; i < 15; i++)
        {
            b += past_unit[i] * weights[8][i-8];
        }
        tm1grad += dldz;
        float da = dldz * (sigmoid(a)*(1-sigmoid(a))*(std::tanh(b)));
        float db = dldz * sigmoid(a)*(1-(std::tanh(b))*(std::tanh(b)));
        gradients.alpha[15] += da;
        gradients.bias[15] += db;
        units[0] = 0;
        units[1] = 0;
        units[2] = 0;
        units[3] = 0;
        units[4] = 0;
        units[5] = 0;
        units[6] = 0;
        units[7] = 0;
        #pragma omp simd collapse(2)
        for (uint8_t i = 8; i < 11; i++){
            units[i] = da*weights[8][i-8];
            gradients.weights[8][i-8] += da*past_unit[i];
            gradients.alpha[i] += units[i] * pacts[i];
            units[i] = units[i]*(1+(alpha[i]*drelu(pacts[i])));
            gradients.bias[i] += units[i];
            for (int j = 0; j < 7; j++)
            {
                units[j+1] += units[i] * weights[i-7][j];
                gradients.weights[i-7][j] += units[i]*past_unit[j+1];
            }
        }
        #pragma omp simd collapse(2)
        for (uint8_t i = 11; i < 15; i++){
            units[i] = db*weights[8][i-8];
            gradients.weights[8][i-8] += db*past_unit[i];
            gradients.alpha[i] += units[i] * pacts[i];
            units[i] = units[i]*(1+(alpha[i]*drelu(pacts[i])));
            gradients.bias[i] += units[i];
            for (int j = 0; j < 7; j++)
            {
                units[j+1] += units[i] * weights[i-7][j];
                gradients.weights[i-7][j] += units[i]*past_unit[j+1];
            }
        }
        #pragma omp simd
        for (int i = 1; i < 8; i++)
        {
            gradients.alpha[i] += units[i] * pacts[i];
            units[i] = units[i]*(1+(alpha[i]*drelu(pacts[i])));
            gradients.bias[i] += units[i];
            units[0] += units[i] * weights[0][i-1];
        }
        gradients.alpha[0] += units[0] * pacts[0];
        units[0] *= (1 + (alpha[0] * drelu(pacts[0])));
        gradients.bias[0] += units[0];
        return units[0];
    }    

    // note the units array will be used to store gradients
    template <typename T>
    inline float backprop(float dldz, std::array<float,16> &past_unit, std::array<float,16> &pacts, neuron_gradients &gradients, T af)
    {   
        gradients.alpha[15] += dldz * pacts[15];
        dldz = dldz * (1 + (alpha[15] * dact_func(pacts[15],af)));
        gradients.bias[15] += dldz;
        units[0] = 0;
        units[1] = 0;
        units[2] = 0;
        units[3] = 0;
        units[4] = 0;
        units[5] = 0;
        units[6] = 0;
        units[7] = 0;
        #pragma omp simd collapse(2)
        for(int i = 8 ; i < 15; i++){
            units[i] = dldz*weights[8][i-8];
            gradients.weights[8][i-8] += dldz*past_unit[i];
            gradients.alpha[i] += units[i] * pacts[i];
            units[i] = units[i]*(1+(alpha[i]*dact_func(pacts[i],af)));
            gradients.bias[i] += units[i];
            for (int j = 0; j < 7; j++)
            {
                units[j+1] += units[i] * weights[i-7][j];
                gradients.weights[i-7][j] += units[i]*past_unit[j+1];
            }
        }
        #pragma omp simd
        for (int i = 1; i < 8; i++)
        {
            gradients.alpha[i] += units[i] * pacts[i];
            units[i] = units[i]*(1+(alpha[i]*dact_func(pacts[i],af)));
            gradients.bias[i] += units[i];
            units[0] += units[i] * weights[0][i-1];
        }
        gradients.alpha[0] += units[0] * pacts[0];
        units[0] *= (1 + (alpha[0] * dact_func(pacts[0],af)));
        gradients.bias[0] += units[0];
        return units[0];
    }    
    
    inline float backpropagation(float dldz, std::array<float,16> &past_unit, std::array<float,16> &pacts, neuron_gradients &gradients,float &tm1grad,float ht_m1=0){
        sine_neuron s;
        log_relu_neuron l;
        relu_neuron r;
        memory_neuron m;
        switch (a_f)
        {
        case 1:
            return backprop(dldz, past_unit, pacts, gradients, s);
        case 2:
            return backprop(dldz, past_unit, pacts, gradients, l);
        case 3:
            return backprop(dldz,past_unit, pacts,gradients, m,ht_m1,tm1grad);
        default:
            return backprop(dldz, past_unit, pacts, gradients, r);
        }
    }
};

// no parameters only serves to store unit values
struct np_neuron_unit
{
    float units[16] = {0};     // 0 will be input and 15 will be output
    //tags for tag dispatching
    struct relu_neuron{relu_neuron(){}};            // a_f = 0
    struct sine_neuron{sine_neuron(){}};            // a_f = 1
    struct log_relu_neuron{log_relu_neuron(){}};    // a_f = 2
    struct memory_neuron{memory_neuron(){}};        // a_f = 3 (input unit is mrelu, other units relu)
    
    inline void valclear(){    //wrapper function for memset on units
        memset(units,0,sizeof(units));
    };

    // wrapper function for relu and drelu
    inline float act_func(float x, relu_neuron){
        return relu(x);
    }
    inline float dact_func(float fx, relu_neuron){
        return drelu(fx);
    }

    // wrapper function for sine and cos(arcsin(x))
    inline float act_func(float x, sine_neuron){
        return std::sinf(x);
    }
    inline float dact_func(float fx, sine_neuron){
        return cos_sinx(fx);
    }

    // wrapper function for log_relu and dlog_relu
    inline float act_func(float x, log_relu_neuron){
        return log_relu(x);
    }
    inline float dact_func(float fx, log_relu_neuron){
        return dlog_relu(fx);
    }
    
    template <typename T>
    inline void f_p(neuron_unit & nu, std::array<float,16> &pacts, T af){ //pacts here refers to values obtained after applying activation function
        units[0] += nu.bias[0];
        pacts[0] = act_func(units[0],af);
        units[0] += (pacts[0] * nu.alpha[0]);
        #pragma omp simd
        for (uint8_t i = 1; i < 8; i++)
        {
            units[i] = units[0] * nu.weights[0][i-1];
            units[i] += nu.bias[i];
            pacts[i] = act_func(units[i],af);
            units[i] += pacts[i] * nu.alpha[i];
        }
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            units[i] = nu.bias[i];        
        }
        #pragma omp simd collapse(2)
        for (uint8_t i = 8; i < 15; i++)
        {
            for (uint8_t j = 0; j < 7; j++)
            {
                units[i] += units[j+1] * nu.weights[i-7][j];
            }    
        }
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            pacts[i] = act_func(units[i],af);
            units[i] += pacts[i] * nu.alpha[i];
        }
        units[15] = nu.bias[15];
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            units[15] += units[i] * nu.weights[8][i-8];
        }
        pacts[15] = act_func(units[15],af);
        units[15] += pacts[15] * nu.alpha[15];
    }

    inline void f_p(neuron_unit & nu, std::array<float,16> &pacts,  memory_neuron &){ //pacts here refers to values obtained after applying activation function
        units[0] += nu.bias[0];
        pacts[0] = relu(units[0]);
        units[0] += (pacts[0] * nu.alpha[0]);
        #pragma omp simd
        for (uint8_t i = 1; i < 8; i++)
        {
            units[i] = units[0] * nu.weights[0][i-1];
            units[i] += nu.bias[i];
            pacts[i] = relu(units[i]);
            units[i] += pacts[i] * nu.alpha[i];
        }
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            units[i] = nu.bias[i];        
        }
        #pragma omp simd collapse(2)
        for (uint8_t i = 8; i < 15; i++)
        {
            for (uint8_t j = 0; j < 7; j++)
            {
                units[i] += units[j+1] * nu.weights[i-7][j];
            }    
        }
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            pacts[i] = relu(units[i]);
            units[i] += pacts[i] * nu.alpha[i];
        }
        // the output will be determined elsewhere
    }

    inline void mem_update(neuron_unit & nu, float ht_m1){
        float a = nu.alpha[15];
        float b = nu.bias[15];
        #pragma omp simd
        for (uint8_t i = 8; i < 11; i++)
        {
            a += units[i] * nu.weights[8][i-8];
        }
        #pragma omp simd
        for (uint8_t i = 11; i < 15; i++)
        {
            b += units[i] * nu.weights[8][i-8];
        }
        units[15] = ht_m1 + std::tanh(b)*sigmoid(a);
    }

    inline void forward_pass(neuron_unit & nu,std::array<float,16> &pacts,float ht_m1=0){
        sine_neuron s;
        log_relu_neuron l;
        relu_neuron r;
        memory_neuron m;
        switch (nu.a_f)
        {
        case 1:
            f_p(nu,pacts,s);
            return;
        case 2:
            f_p(nu,pacts,l);
            return;
        case 3:
            f_p(nu,pacts,m);
            mem_update(nu,ht_m1);
            return;
        default:
            f_p(nu,pacts,r);
            return;
        }
    }

    // forwardpass without recording post activations for each unit
    template <typename T>
    inline void f_p(neuron_unit & nu, T af){
        units[0] += nu.bias[0];
        units[0] += (act_func(units[0],af) * nu.alpha[0]);
        #pragma omp simd
        for (uint8_t i = 1; i < 8; i++)
        {
            units[i] = units[0] * nu.weights[0][i-1];
            units[i] += nu.bias[i];
            units[i] += act_func(units[i],af) * nu.alpha[i];
        }
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            units[i] = nu.bias[i];        
        }
        #pragma omp simd collapse(2)
        for (uint8_t i = 8; i < 15; i++)
        {
            for (uint8_t j = 0; j < 7; j++)
            {
                units[i] += units[j+1] * nu.weights[i-7][j];
            }    
        }
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            units[i] += act_func(units[i],af) * nu.alpha[i];
        }
        units[15] = nu.bias[15];
        #pragma omp simd
        for (uint8_t i = 8; i < 15; i++)
        {
            units[15] += units[i] * nu.weights[8][i-8];
        }
        units[15] += act_func(units[15],af) * nu.alpha[15];
    }

    inline void forward_pass(neuron_unit & nu){
        sine_neuron s;
        log_relu_neuron l;
        relu_neuron r;
        switch (nu.a_f)
        {
        case 1:
            f_p(nu,s);
            return;
        case 2:
            f_p(nu,l);
            return;
        default:
            f_p(nu,r);
            return;
        }
    }

    inline float backprop(neuron_unit & nu, float dldz, std::array<float,16> &past_unit, std::array<float,16> &pacts, neuron_gradients &gradients, memory_neuron&, float ht_m1, float &tm1grad)
    {   
        float a = nu.alpha[15];
        float b = nu.bias[15];
        #pragma omp simd
        for (uint8_t i = 8; i < 11; i++)
        {
            a += past_unit[i] * nu.weights[8][i-8];
        }
        #pragma omp simd
        for (uint8_t i = 11; i < 15; i++)
        {
            b += past_unit[i] * nu.weights[8][i-8];
        }
        tm1grad += dldz;
        float da = dldz * (sigmoid(a)*(1-sigmoid(a))*(std::tanh(b)));
        float db = dldz * sigmoid(a)*(1-(std::tanh(b))*(std::tanh(b)));
        gradients.alpha[15] += da;
        gradients.bias[15] += db;
        units[0] = 0;
        units[1] = 0;
        units[2] = 0;
        units[3] = 0;
        units[4] = 0;
        units[5] = 0;
        units[6] = 0;
        units[7] = 0;
        #pragma omp simd collapse(2)
        for (uint8_t i = 8; i < 11; i++){
            units[i] = da*nu.weights[8][i-8];
            gradients.weights[8][i-8] += da*past_unit[i];
            gradients.alpha[i] += units[i] * pacts[i];
            units[i] = units[i]*(1+(nu.alpha[i]*drelu(pacts[i])));
            gradients.bias[i] += units[i];
            for (int j = 0; j < 7; j++)
            {
                units[j+1] += units[i] * nu.weights[i-7][j];
                gradients.weights[i-7][j] += units[i]*past_unit[j+1];
            }
        }
        #pragma omp simd collapse(2)
        for (uint8_t i = 11; i < 15; i++){
            units[i] = db*nu.weights[8][i-8];
            gradients.weights[8][i-8] += db*past_unit[i];
            gradients.alpha[i] += units[i] * pacts[i];
            units[i] = units[i]*(1+(nu.alpha[i]*drelu(pacts[i])));
            gradients.bias[i] += units[i];
            for (int j = 0; j < 7; j++)
            {
                units[j+1] += units[i] * nu.weights[i-7][j];
                gradients.weights[i-7][j] += units[i]*past_unit[j+1];
            }
        }
        #pragma omp simd
        for (int i = 1; i < 8; i++)
        {
            gradients.alpha[i] += units[i] * pacts[i];
            units[i] = units[i]*(1+(nu.alpha[i]*drelu(pacts[i])));
            gradients.bias[i] += units[i];
            units[0] += units[i] * nu.weights[0][i-1];
        }
        gradients.alpha[0] += units[0] * pacts[0];
        units[0] *= (1 + (nu.alpha[0] * drelu(pacts[0])));
        gradients.bias[0] += units[0];
        return units[0];
    }    

    // note the units array will be used to store gradients
    template <typename T>
    inline float backprop(neuron_unit & nu, float dldz, std::array<float,16> &past_unit, std::array<float,16> &pacts, neuron_gradients &gradients, T af)
    {   
        gradients.alpha[15] += dldz * pacts[15];
        dldz = dldz * (1 + (nu.alpha[15] * dact_func(pacts[15],af)));
        gradients.bias[15] += dldz;
        units[0] = 0;
        units[1] = 0;
        units[2] = 0;
        units[3] = 0;
        units[4] = 0;
        units[5] = 0;
        units[6] = 0;
        units[7] = 0;
        #pragma omp simd collapse(2)
        for(int i = 8 ; i < 15; i++){
            units[i] = dldz*nu.weights[8][i-8];
            gradients.weights[8][i-8] += dldz*past_unit[i];
            gradients.alpha[i] += units[i] * pacts[i];
            units[i] = units[i]*(1+(nu.alpha[i]*dact_func(pacts[i],af)));
            gradients.bias[i] += units[i];
            for (int j = 0; j < 7; j++)
            {
                units[j+1] += units[i] * nu.weights[i-7][j];
                gradients.weights[i-7][j] += units[i]*past_unit[j+1];
            }
        }
        #pragma omp simd
        for (int i = 1; i < 8; i++)
        {
            gradients.alpha[i] += units[i] * pacts[i];
            units[i] = units[i]*(1+(nu.alpha[i]*dact_func(pacts[i],af)));
            gradients.bias[i] += units[i];
            units[0] += units[i] * nu.weights[0][i-1];
        }
        gradients.alpha[0] += units[0] * pacts[0];
        units[0] *= (1 + (nu.alpha[0] * dact_func(pacts[0],af)));
        gradients.bias[0] += units[0];
        return units[0];
    }    
    
    inline float backpropagation(neuron_unit & nu,float dldz, std::array<float,16> &past_unit, std::array<float,16> &pacts, neuron_gradients &gradients,float &tm1grad,float ht_m1=0){
        sine_neuron s;
        log_relu_neuron l;
        relu_neuron r;
        memory_neuron m;
        switch (nu.a_f)
        {
        case 1:
            return backprop(nu,dldz, past_unit, pacts, gradients, s);
        case 2:
            return backprop(nu,dldz, past_unit, pacts, gradients, l);
        case 3:
            return backprop(nu,dldz,past_unit, pacts,gradients, m,ht_m1,tm1grad);
        default:
            return backprop(nu,dldz, past_unit, pacts, gradients, r);
        }
    }
};

// pdf is f(x) = m(a^2*e^(a^2)) where a(x) = 10(x-0.5), m is a constant approx =11.28379...., in the interval 0 < x < 1
// has a shape with 2 humps around 0.5
float custom_dist(){
    float x = zero_to_one(ttttt);
    x = 1 - 2*x;
    x = 0.1 * (5 - approx_erfinv(x));
    return x;
}

// vector accessible as vector of arrays
// more work needed on this to eliminate the need for vector of vectors
template<typename T>
struct vec_of_arr
{
    std::vector<T> vec;
    int arr_size;
    int vec_size;
    T & operator ()(int i, int j) const {
        return vec[arr_size*i + j];
    }
    T & operator ()(int i, int j){
        return vec[arr_size*i + j];
    }
    vec_of_arr(){}
    vec_of_arr(int vec_len, int arr_len):vec(vec_len*arr_len,0){arr_size = arr_len;vec_size=vec_len;}
    //appending 'array' to this vector, assumes T is POD
    void app_arr(std::vector<T> &arr){
        vec.reserve(vec.size() + arr_size);
        vec.resize(vec.size() + arr_size);
        for (int i = 0; i < arr_size; i++)
        {
            vec[arr_size*vec_size + i] = arr[i];
        }
        vec_size++;
    }
    void inline resize(int a){
        int b = vec_size;
        vec_size = a;
        vec.reserve(a*arr_size);
        vec.resize(a*arr_size);
        for (int i = b; i < a-b; i++)
        {
            vec[i] = 0;
        }
    }
};

struct int_float
{
    int x;
    float y;
    inline int_float(int a, float b){x=a;y=b;}
};


struct NN
{
    std::vector<neuron_unit> neural_net;
    std::vector<std::vector<int_float>> weights;

    struct network_gradient
    {
        std::vector<neuron_gradients> net_grads;
        std::vector<std::vector<float>> weight_gradients;
        network_gradient(){}
        network_gradient(NN & nn)
        :net_grads(nn.neural_net.size(),neuron_gradients())
        ,weight_gradients(nn.neural_net.size())
        {
            for (int i = 0; i < nn.neural_net.size(); i++)
            {
                weight_gradients[i].resize(nn.weights[i].size());
                for (int j = 0; j < weight_gradients[i].size(); j++)
                {
                    weight_gradients[i][j] = 0;
                }
            }
        }
        void sync(NN & nn){
            for (int i = 0; i < nn.neural_net.size(); i++)
            {
                weight_gradients[i].resize(nn.weights[i].size());
                for (int j = 0; j < weight_gradients[i].size(); j++)
                {
                    weight_gradients[i][j] = 0;
                }
            }
        }
        void valclear(){
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < net_grads.size(); i++)
            {
                net_grads[i].valclear();
            }
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < weight_gradients.size(); i++)
            {
                #pragma omp simd
                for (int j = 0; j < weight_gradients[i].size(); j++)
                {
                    weight_gradients[i][j] = 0;
                }
            }
        }

        //neuron &n, float learning_rate, float momentum, neuron_gradients current_grad
        inline void sgd_with_momentum(NN &n, float learning_rate, float momentum, network_gradient &current_gradient){
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < n.neural_net.size(); i++)
            {
                net_grads[i].sgd_with_momentum(n.neural_net[i],learning_rate,momentum,current_gradient.net_grads[i]);
            }
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < weight_gradients.size(); i++)
            {
                #pragma omp simd
                for (int j = 0; j < weight_gradients[i].size(); j++)
                {
                    weight_gradients[i][j] *= momentum;
                    current_gradient.weight_gradients[i][j] *= learning_rate;
                    weight_gradients[i][j] += current_gradient.weight_gradients[i][j];
                    n.weights[i][j].y -= weight_gradients[i][j];
                }   
            }
        }

        //restrict gradients from -1 to 1
        inline void restrict_tanh(){
            for (int i = 0; i < net_grads.size(); i++)
            {
                #pragma omp simd
                for (int j = 0; j < 16; j++)
                {
                    net_grads[i].bias[j] = std::tanh(net_grads[i].bias[j]);
                }
                #pragma omp simd
                for (int j = 0; j < 16; j++)
                {
                    net_grads[i].alpha[j] = std::tanh(net_grads[i].alpha[j]);
                }
                for (int j = 0; j < 9; j++)
                {
                    for (int k = 0; k < 7; k++)
                    {
                        net_grads[i].weights[j][k] = std::tanh(net_grads[i].weights[j][k]);
                    }   
                }
            }
            for (int i = 0; i < weight_gradients.size(); i++)
            {
                for (int j = 0; j < weight_gradients[i].size(); j++)
                {
                    weight_gradients[i][j] = std::tanh(weight_gradients[i][j]);
                }
            }
        }

        inline void restrict_log(){
            #pragma omp for
            for (int i = 0; i < net_grads.size(); i++)
            {
                for (int j = 0; j < 16; j++)
                {
                    net_grads[i].bias[j] = log_increase(net_grads[i].bias[j]);
                }
                for (int j = 0; j < 16; j++)
                {
                    net_grads[i].alpha[j] = log_increase(net_grads[i].alpha[j]);
                }
                for (int j = 0; j < 9; j++)
                {
                    for (int k = 0; k < 7; k++)
                    {
                        net_grads[i].weights[j][k] = log_increase(net_grads[i].weights[j][k]);
                    }   
                }
            }
            #pragma omp for
            for (int i = 0; i < weight_gradients.size(); i++)
            {
                for (int j = 0; j < weight_gradients[i].size(); j++)
                {
                    weight_gradients[i][j] = log_increase(weight_gradients[i][j]);
                }
            }
        }

        inline void restrict_quadratic(){
            #pragma omp for
            for (int i = 0; i < net_grads.size(); i++)
            {
                for (int j = 0; j < 16; j++)
                {
                    net_grads[i].bias[j] = quadractic_increase(net_grads[i].bias[j]);
                }
                for (int j = 0; j < 16; j++)
                {
                    net_grads[i].alpha[j] = quadractic_increase(net_grads[i].alpha[j]);
                }
                for (int j = 0; j < 9; j++)
                {
                    for (int k = 0; k < 7; k++)
                    {
                        net_grads[i].weights[j][k] = quadractic_increase(net_grads[i].weights[j][k]);
                    }   
                }
            }
            #pragma omp for
            for (int i = 0; i < weight_gradients.size(); i++)
            {
                #pragma omp simd
                for (int j = 0; j < weight_gradients[i].size(); j++)
                {
                    weight_gradients[i][j] = quadractic_increase(weight_gradients[i][j]);
                }
            }
        }

        inline float clip(float x, float max){
            return ((std::abs(x)<max) ? x : (sign_of(x)*max));
        }
        inline void clip(float max){
            #pragma omp for schedule(static)
            for (int i = 0; i < net_grads.size(); i++)
            {
                #pragma omp simd
                for (int j = 0; j < 16; j++)
                {
                    net_grads[i].bias[j] = clip(net_grads[i].bias[j],max);
                }
                #pragma omp simd
                for (int j = 0; j < 16; j++)
                {
                    net_grads[i].alpha[j] = clip(net_grads[i].alpha[j],max);
                }
                #pragma omp simd collapse(2)
                for (int j = 0; j < 9; j++)
                {
                    for (int k = 0; k < 7; k++)
                    {
                        net_grads[i].weights[j][k] = clip(net_grads[i].weights[j][k],max);
                    }   
                }
            }
            #pragma omp for schedule(static)
            for (int i = 0; i < weight_gradients.size(); i++)
            {
                #pragma omp simd
                for (int j = 0; j < weight_gradients[i].size(); j++)
                {
                    weight_gradients[i][j] = clip(weight_gradients[i][j],max);
                }
            }
        }

        inline void norm_clip(float max){
            float gradient_l2_norm = 0;
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < net_grads.size(); i++)
            {
                #pragma omp simd
                for (int j = 0; j < 16; j++)
                {
                    gradient_l2_norm+= net_grads[i].bias[j]*net_grads[i].bias[j];
                }
                #pragma omp simd
                for (int j = 0; j < 16; j++)
                {
                    gradient_l2_norm+= net_grads[i].alpha[j] * net_grads[i].alpha[j];
                }
                #pragma omp simd collapse(2)
                for (int j = 0; j < 9; j++)
                {
                    for (int k = 0; k < 7; k++)
                    {
                        gradient_l2_norm+=net_grads[i].weights[j][k]*net_grads[i].weights[j][k];
                    }   
                }
            }
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < weight_gradients.size(); i++)
            {
                #pragma omp simd
                for (int j = 0; j < weight_gradients[i].size(); j++)
                {
                    gradient_l2_norm+=weight_gradients[i][j]*weight_gradients[i][j];
                }
            }
            if (gradient_l2_norm < max)
            {
                return;
            }
            gradient_l2_norm = (max/gradient_l2_norm);
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < net_grads.size(); i++)
            {
                #pragma omp simd
                for (int j = 0; j < 16; j++)
                {
                    net_grads[i].bias[j] *= gradient_l2_norm;
                }
                #pragma omp simd
                for (int j = 0; j < 16; j++)
                {
                    net_grads[i].alpha[j] *= gradient_l2_norm;
                }
                #pragma omp simd collapse(2)
                for (int j = 0; j < 9; j++)
                {
                    for (int k = 0; k < 7; k++)
                    {
                      net_grads[i].weights[j][k] *= gradient_l2_norm;
                    }   
                }
            }
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < weight_gradients.size(); i++)
            {
                #pragma omp simd
                for (int j = 0; j < weight_gradients[i].size(); j++)
                {
                    weight_gradients[i][j] *= gradient_l2_norm;
                }
            }
        }
        inline void condense(std::vector<network_gradient> & multi_grad){
            for (int i = 0; i < multi_grad.size(); i++)
            {
                #pragma omp for schedule(static)
                for (int j = 0; j < multi_grad[i].net_grads.size(); j++)
                {
                    net_grads[j].add(multi_grad[i].net_grads[j]);
                }
                #pragma omp for schedule(static)
                for (int j = 0; j < multi_grad[i].weight_gradients.size(); j++)
                {
                    #pragma omp simd
                    for (int k = 0; k < multi_grad[i].weight_gradients[j].size(); k++)
                    {
                        weight_gradients[j][k] += multi_grad[i].weight_gradients[j][k];
                    }
                    
                }
            }
        }
        
        inline void condense(network_gradient & grad){
            #pragma omp parallel for schedule(static) 
            for (int j = 0; j < grad.net_grads.size(); j++)
            {
                net_grads[j].add(grad.net_grads[j]);
            }
            #pragma omp parallel for schedule(static)
            for (int j = 0; j < grad.weight_gradients.size(); j++)
            {
                #pragma omp simd
                for (int k = 0; k < grad.weight_gradients[j].size(); k++)
                {
                    weight_gradients[j][k] += grad.weight_gradients[j][k];
                }
                    
            }
        }
    };

    std::vector<int> input_index;           //indexing recording input neurons
    std::vector<int> output_index;          //indexing recording output neurons
    std::vector<std::vector<int>> layermap;
    std::vector<int> depth;                    // for layermap purposes

    // sort asecending by index, should always be almost sorted at least thus use insertion sort
    void weight_index_sort(){
        //this layer of parallelization should be enough
        //#pragma omp parallel for
        for (int i = 0; i < weights.size(); i++)
        {
            for(int j = 1; j < weights[i].size();j++){
                int_float p = weights[i][j];
                int k = j - 1;
                while( (k >= 0)&&(weights[i][k-1].x > p.x))
                {
                    weights[i][k+1] = weights[i][k];
                    k--;
                }
                weights[i][k+1] = p;
            }
        }
    }
    void layermap_sync(){
        weight_index_sort();
        layermap.clear();
        depth.clear();
        depth.reserve(neural_net.size());
        depth.resize(neural_net.size());
        std::fill(depth.begin(),depth.end(),0);
        layermap.reserve(neural_net.size());
        layermap.resize(1,{});
        layermap[0].reserve(input_index.size());
        for (int i = 0; i < input_index.size(); i++)
        {
            layermap[0].emplace_back(input_index[i]);
        }
        for (int i = 0; i < depth.size(); i++)
        {
            if (neural_net[i].isinput)
            {
                continue;
            }
            for (int j = 0; j < weights[i].size(); j++)
            {
                if (weights[i][j].x > i)
                {
                    break;
                }
                depth[i] = std::max(depth[i],depth[weights[i][j].x] + 1);
            }
            if (layermap.size() < depth[i]+1)
            {
                layermap.resize(depth[i]+1,{});
            }
            layermap[depth[i]].emplace_back(i);
        }
        layermap.shrink_to_fit();
    } 
    
    NN(int size, std::vector<int> input_neurons, std::vector<int> output_neurons, float connection_density, float connection_sd)
    :neural_net(size,neuron_unit())
    ,input_index(input_neurons)
    ,output_index(output_neurons)
    ,weights(size)
    {
        std::normal_distribution<float> connection_dist(connection_density, connection_sd);
        for (int i = 0; i < input_index.size(); i++)
        {
            neural_net[input_index[i]].isinput = 1;
        }
        for (int i = 0; i < size; i++)
        {
            if (neural_net[i].isinput)
            {
                continue;
            }
            float density = connection_dist(ttttt);         //number of connections for each neuron is distributed normally around connection_density with standard deviation connection_sd
            if(density < 0){
                density = 0;
            }
            else if (density >= 1.0f)
            {
                density = 1.0f;
            }
            int connect_n =  std::floor(density * (size - 1));          //this is the number of input connections
            std::set<int> input_i = {};
            for (int j = 0; j < connect_n; j++)
            {
                int remaining = size - 1 - weights[i].size();
                int input_neuron_index = std::floor((remaining) * (custom_dist() - 0.5));
                int wrap_around  = (input_neuron_index < 0) * (remaining + input_neuron_index);
                wrap_around = (wrap_around > input_neuron_index) ? wrap_around:input_neuron_index;
                input_neuron_index = (wrap_around % (remaining));
                int pos = 0;
                bool gapped = true;
                for(int k = 0; k < weights[i].size(); k++)
                {
                    if ((input_neuron_index >= i) && gapped)
                    {
                        gapped = false;
                        input_neuron_index++;
                    }
                    if (input_neuron_index >= weights[i][k].x)
                    {
                        input_neuron_index++;
                        pos++;
                        continue;
                    }
                    if ((input_neuron_index >= i) && gapped)
                    {
                        gapped = false;
                        input_neuron_index++;
                        continue;
                    }
                    break;
                }
                if ((input_neuron_index >= i) && gapped)
                {
                    input_neuron_index++;
                }
                weights[i].insert(weights[i].begin() + pos,int_float(input_neuron_index,0.0f));    
            }
        }
        layermap_sync();
        for (int i = 0; i < size; i++)
        {   //not really the correct way to use He initialisation but eh
            float input_layer_size = weights[i].size();
            int recurrent_connections = 0;
            #pragma omp simd
            for (int j = 0; j < input_layer_size; j++)
            {
                recurrent_connections += (weights[i][j].x > i);
            }
            
            //Xavier initialisation
            std::uniform_real_distribution<float> weight_init_dist(-std::sqrt(1/(input_layer_size)),std::sqrt(1/(input_layer_size)));
            for(int j = 0; j < input_layer_size; j++)
            {
                //if (weights[i][j].x > i)
                //{
                    //weights[i][j].y = 0; /*this seems to stop gradient explosion*/
                //}
                //else{
                    weights[i][j].y = weight_init_dist(ttttt) /*multiplier to tune? prevent gradient exploding*/;
                //}
            } 
        }
    }

    inline void valclear(){
        for (int i = 0; i < neural_net.size(); i++)
        {
            neural_net[i].valclear();
        }
    }
    struct state{
        std::vector<std::array<float,16>> values;
        void valclear(){
            for (int i = 0; i < values.size(); i++)
            {
                #pragma omp simd
                for (int j = 0; j < 16; j++)
                {
                    values[i][j] = 0;
                }
            }
        }
        state(){}
        state(int size)
        : values(size)
        {
            valclear();
        }
    };
    // post for post activation values
    inline void record_state(state &post){
        for (int i = 0; i < neural_net.size(); i++)
        {
            #pragma omp simd
            for (int j = 0; j < 16; j++)
            {
                post.values[i][j] = neural_net[i].units[j];
            }   
        }
    }
    //from now pre refers to pre Re:Zero and Post past_unit
    //zeros out values for consistency
    template<typename T>
    inline void sforwardpass(T &inputs, state &pre, state &post){
        #pragma omp simd
        for (int i = 0; i < input_index.size(); i++)
        {
            neural_net[input_index[i]].units[0] = inputs[i];
        }
        for (int i = 0; i < layermap.size(); i++)
        {
            for (int j = 0; j < layermap[i].size(); j++)
            {   
                neural_net[layermap[i][j]].units[0] *= neural_net[layermap[i][j]].isinput;
                #pragma omp simd
                for (int l = 0; l < weights[layermap[i][j]].size(); l++)
                {
                    const int in_indx = weights[layermap[i][j]][l].x;
                    const float in = (in_indx > layermap[i][j]) ? 0.0f : neural_net[in_indx].units[15];
                    //apologies for the naming scheme
                    neural_net[layermap[i][j]].units[0] += weights[layermap[i][j]][l].y * in;
                }
                neural_net[layermap[i][j]].forward_pass(pre.values[layermap[i][j]]);
            }
        }
        record_state(post);
    }

    inline void sforwardpass(std::vector<float> &inputs){
        #pragma omp simd
        for (int i = 0; i < input_index.size(); i++)
        {
            neural_net[input_index[i]].units[0] = inputs[i];
        }
        for (int i = 0; i < layermap.size(); i++)
        {
            for (int j = 0; j < layermap[i].size(); j++)
            {   
                neural_net[layermap[i][j]].units[0] *= neural_net[layermap[i][j]].isinput;
                #pragma omp simd
                for (int l = 0; l < weights[layermap[i][j]].size(); l++)
                {
                    const int in_indx = weights[layermap[i][j]][l].x;
                    const float in = (in_indx > layermap[i][j]) ? 0.0f : neural_net[in_indx].units[15];
                    //apologies for the naming scheme
                    neural_net[layermap[i][j]].units[0] += weights[layermap[i][j]][l].y * in;
                }
                neural_net[layermap[i][j]].forward_pass();
            }
        }
    }
    
    // performant code is ugly code, horrible code duplication with switch to avoid inner loop if statement
    template<typename T>
    inline void forward_pass(T &inputs, state &pre, state &post, vec_of_arr<float> & states, int tstep){
        #pragma omp simd
        for (int i = 0; i < input_index.size(); i++)
        {
            neural_net[input_index[i]].units[0] = inputs[i];
        }
        switch (tstep)
        {
            case 0:
                for (int i = 0; i < layermap.size(); i++)
                {
                    for (int j = 0; j < layermap[i].size(); j++)
                    {   
                        neural_net[layermap[i][j]].units[0] *= neural_net[layermap[i][j]].isinput;
                        #pragma omp simd
                        for (int l = 0; l < weights[layermap[i][j]].size(); l++)
                        {               
                            const int in_indx = weights[layermap[i][j]][l].x;
                            const float in = (in_indx > layermap[i][j]) ? 0.0f : neural_net[in_indx].units[15];
                            neural_net[layermap[i][j]].units[0] += weights[layermap[i][j]][l].y * in;
                        }
                        neural_net[layermap[i][j]].forward_pass(pre.values[layermap[i][j]]);
                        states(tstep,layermap[i][j]) = neural_net[layermap[i][j]].units[15];
                    }
                }
                record_state(post);
                return;
            
            default:
                for (int i = 0; i < layermap.size(); i++)
                {
                    for (int j = 0; j < layermap[i].size(); j++)
                    {   
                        neural_net[layermap[i][j]].units[0] *= neural_net[layermap[i][j]].isinput;
                        #pragma omp simd
                        for (int l = 0; l < weights[layermap[i][j]].size(); l++)
                        {
                            const int in_indx = weights[layermap[i][j]][l].x;
                            const float in = (in_indx > layermap[i][j]) ? states(tstep - 1,in_indx) : neural_net[in_indx].units[15];
                            neural_net[layermap[i][j]].units[0] += weights[layermap[i][j]][l].y * in;
                        }
                        neural_net[layermap[i][j]].forward_pass(pre.values[layermap[i][j]],states(tstep-1,layermap[i][j]));
                        states(tstep,layermap[i][j]) = neural_net[layermap[i][j]].units[15];
                    }
                }
                record_state(post);
                return;
        }
    }

    template<typename T>
    inline void forward_pass(T &inputs, vec_of_arr<float> & states,int tstep,state &pre){
        #pragma omp simd
        for (int i = 0; i < input_index.size(); i++)
        {
            neural_net[input_index[i]].units[0] = inputs[i];
        }
        switch (tstep)
        {
            case 0:
                for (int i = 0; i < layermap.size(); i++)
                {
                    for (int j = 0; j < layermap[i].size(); j++)
                    {   
                        neural_net[layermap[i][j]].units[0] *= neural_net[layermap[i][j]].isinput;
                        #pragma omp simd
                        for (int l = 0; l < weights[layermap[i][j]].size(); l++)
                        {               
                            const int in_indx = weights[layermap[i][j]][l].x;
                            const float in = (in_indx > layermap[i][j]) ? 0.0f : neural_net[in_indx].units[15];
                            neural_net[layermap[i][j]].units[0] += weights[layermap[i][j]][l].y * in;
                        }
                        neural_net[layermap[i][j]].forward_pass(pre.values[layermap[i][j]]);
                        states(tstep,layermap[i][j]) = neural_net[layermap[i][j]].units[15];
                    }
                }
                return;
            
            default:
                for (int i = 0; i < layermap.size(); i++)
                {
                    for (int j = 0; j < layermap[i].size(); j++)
                    {   
                        neural_net[layermap[i][j]].units[0] *= neural_net[layermap[i][j]].isinput;
                        #pragma omp simd
                        for (int l = 0; l < weights[layermap[i][j]].size(); l++)
                        {
                            const int in_indx = weights[layermap[i][j]][l].x;
                            const float in = (in_indx > layermap[i][j]) ? states(tstep - 1,in_indx) : neural_net[in_indx].units[15];
                            neural_net[layermap[i][j]].units[0] += weights[layermap[i][j]][l].y * in;
                        }
                        neural_net[layermap[i][j]].forward_pass();
                        states(tstep,layermap[i][j]) = neural_net[layermap[i][j]].units[15];
                    }
                }
                return;
        }
    }

    //back propagation through time, passing arguement gradients to avoid memorry allocation
    inline void bptt(vec_of_arr<float> & dloss, std::vector<state> &pre, std::vector<state> &post, network_gradient &net_grad,vec_of_arr<float> &states, vec_of_arr<float> &gradients){
        std::fill(gradients.vec.begin(),gradients.vec.end(),0);
        for (int i = 0; i < dloss.vec_size; i++)
        {
            for (int j = 0; j < dloss.arr_size; j++)
            {
                gradients(i,output_index[j]) = dloss(i,j);
            }
        }
        for (int i = dloss.vec_size - 1; i > 0; i--)
        {
            for (int j = layermap.size() - 1; j >= 0; j--)
            {
                for (int k = 0; k < layermap[j].size(); k++)
                {
                    const int & n = layermap[j][k];
                    float dldz = neural_net[n].backpropagation(gradients(i,n),post[i].values[n],pre[i].values[n],net_grad.net_grads[n],gradients(i-1,n),post[i-1].values[n][15]);
                    #pragma omp simd
                    for (int l = 0; l < weights[n].size(); l++)
                    {
                        const int t_step = (n > weights[n][l].x) ? (i):(i-1);
                        gradients(t_step,weights[n][l].x) += dldz * weights[n][l].y;
                        net_grad.weight_gradients[n][l] += dldz * post[t_step].values[weights[n][l].x][15]; 
                    }   
                }   
            }
        }
        for (int j = layermap.size() - 1; j >= 0; j--)
        {
            for (int k = 0; k < layermap[j].size(); k++)
            {
                const int & n = layermap[j][k];
                float nothing = 0;
                float dldz = neural_net[n].backpropagation(gradients(0,n),post[0].values[n],pre[0].values[n],net_grad.net_grads[n],nothing);
                #pragma omp simd
                for (int l = 0; l < weights[n].size(); l++)
                {
                    if (weights[n][l].x > n)
                    {
                        continue;
                    }
                    gradients(0,weights[n][l].x) += dldz * weights[n][l].y;
                    net_grad.weight_gradients[n][l] += dldz * post[0].values[weights[n][l].x][15];
                }       
            }           
        }
    }
    // saving to and loading from a text file
    inline void save_to_txt(std::string file_name){
        std::ofstream file(file_name,std::fstream::trunc);
        file << "number_of_neurons:"<<"\n";
        file << neural_net.size() << "\n";
        file << "input_index" <<"\n";   
        for (int i = 0; i < input_index.size(); i++)
        {
            file << input_index[i] << " ";
        }
        file << "\n";
        file << "output_index" << "\n";
        for (int i = 0; i < output_index.size(); i++)
        {
            file << output_index[i] << " ";
        }
        file << "\n";
        file << "number_of_layers" << "\n";
        file << layermap.size() << "\n";
        file << "<layermap>" << "\n";
        for (int i = 0; i < layermap.size(); i++)
        {
            file << "no_of_neurons" << "\n";
            file << layermap[i].size() << "\n";
            for (int j = 0; j < layermap[i].size(); j++)
            {
                file << layermap[i][j] << " ";
            }
            file << "\n";
            if(i != (layermap.size() -1)){
                file << "next_layer" << "\n";
            }
        }
        file << "</layermap>" <<"\n";
        file << "<weights>" << "\n";
        for (int i = 0; i < neural_net.size(); i++)
        {
            file << "no_of_weights" << "\n";
            file << weights[i].size() << "\n";
            for (int j = 0; j < weights[i].size(); j++)
            {
                file << weights[i][j].x << " ";
                file << std::fixed<<std::setprecision(std::numeric_limits<float>::max_digits10)
                << weights[i][j].y << "\n";
            }
            file << "---------" << "\n";
        }
        file << "</weights>" << std::endl;
        for (int i = 0; i < neural_net.size(); i++)
        {
            file << "<neuron>" << "\n";
            file << "<bias>" << "\n";
            for (int j = 0; j < 16; j++)
            {
                file << std::fixed<<std::setprecision(std::numeric_limits<float>::max_digits10)
                << neural_net[i].bias[j] << "\n";
            }
            file << "</bias>" << "\n";

            file << "<alpha>" << "\n";
            for (int j = 0; j < 16; j++)
            {
                file << std::fixed<<std::setprecision(std::numeric_limits<float>::max_digits10)
                << neural_net[i].alpha[j] << "\n";
            }
            file << "</alpha>" << "\n";
            file << "<a_f>" << "\n";
            file << neural_net[i].a_f << "\n";
            file << "</a_f>" << "\n";
            file << "<nweights>" << "\n";
            for (int j = 0; j < 9; j++)
            {
                for (int k = 0; k < 7; k++)
                {
                    file << std::fixed<<std::setprecision(std::numeric_limits<float>::max_digits10)
                    << neural_net[i].weights[j][k] << "\n";
                }
                
            }
            file << "</nweights>" << "\n";
            file << "</neuron>" << "\n";
        }
        file << "<end>" << "\n";
        file.close();
    }
    void cut_recurrent(){
        weight_index_sort();
        for (int i = 0; i < weights.size(); i++)
        {
            for (int j = 0; j < weights[i].size(); j++)
            {
                if (weights[i][j].x > i)
                {
                    weights[i].resize(j,int_float(0,0));
                    break;
                }   
            }   
        }
    }

    NN(std::string file_name){
        std::string str_data;
        std::vector<int> output_in;
        std::vector<int> input_in;
        std::ifstream file(file_name);
        if (file.good()){;}else{std::cout<<"ERROR "<<file_name<<" does not exist"<<std::endl; std::exit(EXIT_FAILURE);}
        file >> str_data;
        file >> str_data;
        neural_net.resize(std::stoi(str_data),neuron_unit());
        weights.resize(std::stoi(str_data));
        file >> str_data;
        while (true)
        {
            std::string data;
            file >> data;
            if(data == "output_index"){
                break;
            }
            input_index.emplace_back(std::stoi(data));
        }
        while (true)
        {
            std::string data;
            file >> data;
            if(data == "number_of_layers"){
                break;
            }
            output_index.emplace_back(std::stoi(data));
        }
        file >> str_data;
        layermap.resize(std::stoi(str_data),{});
        file >> str_data;
        int itr = 0;
        while(true){
            std::string data;
            file >> data;
            if (data == "</layermap>")
            {
                break;
            }
            else if (data == "no_of_neurons")
            {
                file >> data;
                layermap[itr].reserve(std::stoi(data));
            }
            else if(data == "next_layer")
            {
                itr++;
            }
            else{
                layermap[itr].emplace_back(std::stoi(data));
            }
        }
        file >> str_data;
        itr = 0;
        while (true)
        {
            std::string data;
            file >> data;
            if (data == "</weights>")
            {
                break;
            }
            else if (data == "no_of_weights")
            {
                file >> data;
                weights[itr].reserve(std::stoi(data));
            }
            else if(data == "---------")
            {
                itr++;
            }
            else{
                int index = std::stoi(data);
                file >> data;
                float value= std::stof(data);
                weights[itr].emplace_back(int_float(index,value));
            }
        }
        itr = 0;
        while (true)
        {
            std::string data;
            file >> data;
            if (data == "<neuron>")
            {
                continue;
            }else if (data == "<bias>")
            {
                for (int i = 0; i < 16; i++)
                {
                    file >> data;
                    float b = std::stof(data);
                    neural_net[itr].bias[i] = b;
                }
                file >> data;
            }else if (data == "<alpha>")
            {
                for (int i = 0; i < 16; i++)
                {
                    file >> data;
                    float al = std::stof(data);
                    neural_net[itr].alpha[i] = al;
                }
                file >> data;
            }else if (data == "<a_f>")
            {
                file >> data;
                int af = std::stoi(data);
                neural_net[itr].a_f = af;
                file >> data;
            }else if (data == "<nweights>")
            {
                for(int i = 0; i < 9; i++){
                    for (int j = 0; j < 7; j++)
                    {
                        file >> data;
                        float we = std::stof(data);
                        neural_net[itr].weights[i][j] = we;
                    }
                }
                file >> data;
            }
            else if (data == "<end>")
            {
                break;
            }
            else{
                itr++;
            }
        }
        file.close();
        for (int i = 0; i < input_index.size(); i++)
        {
            neural_net[input_index[i]].isinput = true;
        }
        
    }

    // returns true if inputs can appect every output of current timestep
    // flase otherwise
    bool connection_check(){
        std::vector<int> checker(neural_net.size(),0);
        for (int i = 0; i < input_index.size(); i++)
        {
            checker[input_index[i]] = 1;
            for (int j = 0; j < layermap.size(); j++)
            {
                for (int k = 0; k < layermap[j].size(); k++)
                {
                    const int & n=layermap[j][k];
                    for(int l = 0; l < weights[n].size(); l++){
                        if (weights[n][l].x > n)
                        {
                            break;
                        }
                        checker[n] = (checker[n]||checker[weights[n][l].x]);
                    }
                }
            }
            for (int j = 0; j < output_index.size(); j++)
            {
                if (!checker[output_index[j]])
                {
                    return false;
                }
                
            }
            std::fill(checker.begin(),checker.end(),0);    
        }
        return true;
    }

    struct training_essentials{
        vec_of_arr<float> dloss;
        std::vector<state> pre;
        std::vector<state> post;
        network_gradient f;
        vec_of_arr<float> gradients;
        vec_of_arr<float> states;
        training_essentials(NN& n):
            dloss(1,n.output_index.size()),
            pre(1,state(n.neural_net.size())),
            post(1,state(n.neural_net.size())),
            f(n),
            gradients(1,n.neural_net.size()),
            states(1,n.neural_net.size()){}
        void resize(int tsteps){
            state ff(gradients.arr_size);
            gradients.resize(tsteps);
            dloss.resize(tsteps);
            pre.resize(tsteps,ff);
            post.resize(tsteps,ff);
            states.resize(tsteps);
        }
        void append(){
            resize(gradients.vec_size+1);
        }
    };

    int parameter_count(){
        int count = 0;
        #pragma omp simd
        for (int i = 0; i < weights.size(); i++)
        {
            count += weights[i].size();
        }
        count += neural_net.size() * 95;
        return count;
    }
    
    // parameterless shell of units for batch training purposes
    struct npNN
    {
        std::vector<np_neuron_unit> neural_net;
        npNN(NN & nn):neural_net(nn.neural_net.size()){}
        inline void record_state(state &post){
            for (int i = 0; i < neural_net.size(); i++)
            {
                #pragma omp simd
                for (int j = 0; j < 16; j++)
                {
                    post.values[i][j] = neural_net[i].units[j];
                }   
            }
        }
 
        template<typename T>
        inline void sforwardpass(NN & nn,T &inputs, state &pre, state &post){
            #pragma omp simd
            for (int i = 0; i < nn.input_index.size(); i++)
            {
                neural_net[nn.input_index[i]].units[0] = inputs[i];
            }
            for (int i = 0; i < nn.layermap.size(); i++)
            {
                for (int j = 0; j < nn.layermap[i].size(); j++)
                {   
                    neural_net[nn.layermap[i][j]].units[0] *= nn.neural_net[nn.layermap[i][j]].isinput;
                    #pragma omp simd
                    for (int l = 0; l < nn.weights[nn.layermap[i][j]].size(); l++)
                    {
                        const int in_indx = nn.weights[nn.layermap[i][j]][l].x;
                        const float in = (in_indx > nn.layermap[i][j]) ? 0.0f : neural_net[in_indx].units[15];
                        //apologies for the naming scheme
                        neural_net[nn.layermap[i][j]].units[0] += nn.weights[nn.layermap[i][j]][l].y * in;
                    }
                    neural_net[nn.layermap[i][j]].forward_pass(nn.neural_net[nn.layermap[i][j]],pre.values[nn.layermap[i][j]]);
                }
            }
            record_state(post);
        }
        template<typename T>
        inline void sforwardpass(NN & nn,T &inputs){
            #pragma omp simd
            for (int i = 0; i < nn.input_index.size(); i++)
            {
                neural_net[nn.input_index[i]].units[0] = inputs[i];
            }
            for (int i = 0; i < nn.layermap.size(); i++)
            {
                for (int j = 0; j < nn.layermap[i].size(); j++)
                {   
                    neural_net[nn.layermap[i][j]].units[0] *= nn.neural_net[nn.layermap[i][j]].isinput;
                    #pragma omp simd
                    for (int l = 0; l < nn.weights[nn.layermap[i][j]].size(); l++)
                    {
                        const int in_indx = nn.weights[nn.layermap[i][j]][l].x;
                        const float in = (in_indx > nn.layermap[i][j]) ? 0.0f : neural_net[in_indx].units[15];
                        //apologies for the naming scheme
                        neural_net[nn.layermap[i][j]].units[0] += nn.weights[nn.layermap[i][j]][l].y * in;
                    }
                    neural_net[nn.layermap[i][j]].forward_pass(nn.neural_net[nn.layermap[i][j]]);
                }
            }
        }
        
        // performant code is ugly code, horrible code duplication with switch to avoid inner loop if statement
        template<typename T>
        inline void forward_pass(NN & nn,T &inputs, state &pre, state &post, vec_of_arr<float> & states, int tstep){
            #pragma omp simd
            for (int i = 0; i < nn.input_index.size(); i++)
            {
                neural_net[nn.input_index[i]].units[0] = inputs[i];
            }
            switch (tstep)
            {
                case 0:
                    for (int i = 0; i < nn.layermap.size(); i++)
                    {
                        for (int j = 0; j < nn.layermap[i].size(); j++)
                        {   
                            neural_net[nn.layermap[i][j]].units[0] *= nn.neural_net[nn.layermap[i][j]].isinput;
                            #pragma omp simd
                            for (int l = 0; l < nn.weights[nn.layermap[i][j]].size(); l++)
                            {               
                                const int in_indx = nn.weights[nn.layermap[i][j]][l].x;
                                const float in = (in_indx > nn.layermap[i][j]) ? 0.0f : neural_net[in_indx].units[15];
                                neural_net[nn.layermap[i][j]].units[0] += nn.weights[nn.layermap[i][j]][l].y * in;
                            }
                            neural_net[nn.layermap[i][j]].forward_pass(nn.neural_net[nn.layermap[i][j]],pre.values[nn.layermap[i][j]]);
                            states(tstep,nn.layermap[i][j]) = neural_net[nn.layermap[i][j]].units[15];
                        }
                    }
                    record_state(post);
                    return;
                
                default:
                    for (int i = 0; i < nn.layermap.size(); i++)
                    {
                        for (int j = 0; j < nn.layermap[i].size(); j++)
                        {   
                            neural_net[nn.layermap[i][j]].units[0] *= nn.neural_net[nn.layermap[i][j]].isinput;
                            #pragma omp simd
                            for (int l = 0; l < nn.weights[nn.layermap[i][j]].size(); l++)
                            {
                                const int in_indx = nn.weights[nn.layermap[i][j]][l].x;
                                const float in = (in_indx > nn.layermap[i][j]) ? states(tstep - 1,in_indx) : neural_net[in_indx].units[15];
                                neural_net[nn.layermap[i][j]].units[0] += nn.weights[nn.layermap[i][j]][l].y * in;
                            }
                            neural_net[nn.layermap[i][j]].forward_pass(nn.neural_net[nn.layermap[i][j]],pre.values[nn.layermap[i][j]],states(tstep-1,nn.layermap[i][j]));
                            states(tstep,nn.layermap[i][j]) = neural_net[nn.layermap[i][j]].units[15];
                        }
                    }
                    record_state(post);
                    return;
            }
        }
        template<typename T>
        inline void forward_pass(NN &nn,T &inputs, vec_of_arr<float> & states,int tstep,state &pre){
            #pragma omp simd
            for (int i = 0; i < nn.input_index.size(); i++)
            {
                neural_net[nn.input_index[i]].units[0] = inputs[i];
            }
            switch (tstep)
            {
                case 0:
                    for (int i = 0; i < nn.layermap.size(); i++)
                    {
                        for (int j = 0; j < nn.layermap[i].size(); j++)
                        {   
                            neural_net[nn.layermap[i][j]].units[0] *= nn.neural_net[nn.layermap[i][j]].isinput;
                            #pragma omp simd
                            for (int l = 0; l < nn.weights[nn.layermap[i][j]].size(); l++)
                            {               
                                const int in_indx = nn.weights[nn.layermap[i][j]][l].x;
                                const float in = (in_indx > nn.layermap[i][j]) ? 0.0f : neural_net[in_indx].units[15];
                                neural_net[nn.layermap[i][j]].units[0] += nn.weights[nn.layermap[i][j]][l].y * in;
                            }
                            neural_net[nn.layermap[i][j]].forward_pass(nn.neural_net[nn.layermap[i][j]],pre.values[nn.layermap[i][j]]);
                            states(tstep,nn.layermap[i][j]) = neural_net[nn.layermap[i][j]].units[15];
                        }
                    }
                    return;
                
                default:
                    for (int i = 0; i < nn.layermap.size(); i++)
                    {
                        for (int j = 0; j < nn.layermap[i].size(); j++)
                        {   
                            neural_net[nn.layermap[i][j]].units[0] *= nn.neural_net[nn.layermap[i][j]].isinput;
                            #pragma omp simd    
                            for (int l = 0; l < nn.weights[nn.layermap[i][j]].size(); l++)
                            {
                                const int in_indx = nn.weights[nn.layermap[i][j]][l].x;
                                const float in = (in_indx > nn.layermap[i][j]) ? states(tstep - 1,in_indx) : neural_net[in_indx].units[15];
                                neural_net[nn.layermap[i][j]].units[0] += nn.weights[nn.layermap[i][j]][l].y * in;
                            }
                            neural_net[nn.layermap[i][j]].forward_pass(nn.neural_net[nn.layermap[i][j]],pre.values[nn.layermap[i][j]],states(tstep-1,nn.layermap[i][j]));
                            states(tstep,nn.layermap[i][j]) = neural_net[nn.layermap[i][j]].units[15];
                        }
                    }
                    return;
            }
        }

        //back propagation through time, passing arguement gradients to avoid memorry allocation
        inline void bptt(NN &nn,vec_of_arr<float> & dloss, std::vector<state> &pre, std::vector<state> &post, network_gradient &net_grad,vec_of_arr<float> &states, vec_of_arr<float> &gradients){
            std::fill(gradients.vec.begin(),gradients.vec.end(),0);
            #pragma omp simd collapse(2)
            for (int i = 0; i < dloss.vec_size; i++)
            {
                for (int j = 0; j < dloss.arr_size; j++)
                {
                    gradients(i,nn.output_index[j]) = dloss(i,j);
                }
            }
            for (int i = dloss.vec_size - 1; i > 0; i--)
            {
                for (int j = nn.layermap.size() - 1; j >= 0; j--)
                {
                    for (int k = 0; k < nn.layermap[j].size(); k++)
                    {

                        const int & n = nn.layermap[j][k];
                        float dldz = neural_net[n].backpropagation(nn.neural_net[n],gradients(i,n),post[i].values[n],pre[i].values[n],net_grad.net_grads[n],gradients(i-1,n),post[i-1].values[n][15]);    
                        #pragma omp simd
                        for (int l = 0; l < nn.weights[n].size(); l++)
                        {
                            const int t_step = (n > nn.weights[n][l].x) ? (i):(i-1);
                            gradients(t_step,nn.weights[n][l].x) += dldz * nn.weights[n][l].y;
                            net_grad.weight_gradients[n][l] += dldz * post[t_step].values[nn.weights[n][l].x][15]; 
                        }   
                    }   
                }
            }
            for (int j = nn.layermap.size() - 1; j >= 0; j--)
            {
                for (int k = 0; k < nn.layermap[j].size(); k++)
                {
                    const int & n = nn.layermap[j][k];
                    float nothing = 0;
                    float dldz = neural_net[n].backpropagation(nn.neural_net[n],gradients(0,n),post[0].values[n],pre[0].values[n],net_grad.net_grads[n],nothing);
                    #pragma omp simd
                    for (int l = 0; l < nn.weights[n].size(); l++)
                    {
                        if (nn.weights[n][l].x > n)
                        {
                            continue;
                        }
                        gradients(0,nn.weights[n][l].x) += dldz * nn.weights[n][l].y;
                        net_grad.weight_gradients[n][l] += dldz * post[0].values[nn.weights[n][l].x][15];
                    }       
                }           
            }
        }
    };

    //L2 regression for the recurrent weights, as a coutermeasure to keep gradients under control
    void rec_l2_reg(float lambda){
        weight_index_sort();
        lambda = 1.0f-lambda;
        #pragma omp simd
        for (int i = 0; i < weights.size(); i++)
        {
            for (int j = weights[i].size()-1; j >= 0; j--)
            {
                if (weights[i][j].x < i)
                {
                    break;
                }
                weights[i][j].y *= lambda;  
            }
        }
    }
};


// cyclical buffer for attention QKV


