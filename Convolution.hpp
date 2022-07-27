#ifndef _CONVOLUTION_
#define _CONVOLUTION_

#include "WeTensor.hpp"
#include "im2winSIMD.hpp"

template<class dataType>
class Convolution{
    public:
    WeTensor<dataType> *input, *filter, *output;
    size_t stride;
    virtual long double get_gflops() = 0;
    virtual void conv_implement() = 0;
};

// template<class dataType>
//  double Convolution<dataType>::get_gflops(){
//     std::cout<<"start"<<std::endl;
//      size_t filter_batch = filter->batch_size;
//      size_t filter_channel = filter->channel;
//      size_t filter_height = filter->height;
//      size_t filter_width = filter->width;

//      size_t output_batch = output->batch_size;
//      size_t output_channel = output->channel;
//      size_t output_height = output->height;
//      size_t output_width = output->width;
//      std::cout<<"filter_batch: "<<filter_batch<<std::endl;    
// std::cout<<"run_before"<<std::endl;
// static  double gflops = 0;
// // static  double gflops = static_cast<int>(filter_batch);
//     // static long double gflops = 1e-9 * output_height * output_width * output_batch 
//     //                          * output_channel * filter_channel * filter_height * filter_width * 2;
//     // std::cout<<"flops"<<std::endl;
//     return gflops;
// }

template<class dataType>
class Im2colConv :public Convolution<dataType>{
    public:
    WeTensor<dataType> *input, *filter, *output;
    size_t stride;
    Im2colConv(WeTensor<dataType> *input_, WeTensor<dataType> *filter_, WeTensor<dataType> *output_, size_t stride_){
    input = input_;
    filter = filter_;
    output = output_;
    stride = stride_;};
    long double get_gflops() override;
    void conv_implement() override{output->getDataTensor() = at::conv2d(input->getDataTensor(), filter->getDataTensor(), {}, stride);return;};
};

template<class dataType>
long double Im2colConv<dataType>::get_gflops(){
     size_t filter_batch = filter->batch_size;
     size_t filter_channel = filter->channel;
     size_t filter_height = filter->height;
     size_t filter_width = filter->width;

     size_t output_batch = output->batch_size;
     size_t output_channel = output->channel;
     size_t output_height = output->height;
     size_t output_width = output->width;

    static long double gflops = 1e-9 * output_height * output_width * output_batch 
                              * output_channel * filter_channel * filter_height * filter_width * 2;
    return gflops;
}

template<class dataType>
class DirectConv :public Convolution<dataType>{
    public:
    WeTensor<dataType> *input, *filter, *output;
    size_t stride;
    DirectConv(WeTensor<dataType>* input_, WeTensor<dataType> *filter_, WeTensor<dataType> *output_, size_t stride_);
    long double get_gflops() override;
    void conv_implement() override;
};

template<class dataType>
DirectConv<dataType>::DirectConv(WeTensor<dataType>* input_, WeTensor<dataType> *filter_, WeTensor<dataType> *output_, size_t stride_){
    input = input_;
    filter = filter_;
    output = output_;
    stride = stride_;
}

template<class dataType>
long double DirectConv<dataType>::get_gflops(){
     size_t filter_batch = filter->batch_size;
     size_t filter_channel = filter->channel;
     size_t filter_height = filter->height;
     size_t filter_width = filter->width;

     size_t output_batch = output->batch_size;
     size_t output_channel = output->channel;
     size_t output_height = output->height;
     size_t output_width = output->width;

    static long double gflops = 1e-9 * output_height * output_width * output_batch 
                              * output_channel * filter_channel * filter_height * filter_width * 2;
    return gflops;
}

template<class dataType>
void DirectConv<dataType>::conv_implement(){
    dataType *inptr, *filptr, *outptr;
    inptr = input->getDataPtr();
    filptr = filter->getDataPtr();
    outptr = output->getDataPtr();

    const size_t input_batch = input->batch_size;
    const size_t input_channel = input->channel;
    const size_t input_height = input->height;
    const size_t input_width = input->width;

    const size_t filter_batch = filter->batch_size;
    const size_t filter_channel = filter->channel;
    const size_t filter_height = filter->height;
    const size_t filter_width = filter->width;

    const size_t output_batch = output->batch_size;
    const size_t output_channel = output->channel;
    const size_t output_height = output->height;
    const size_t output_width = output->width;    

    const size_t stride_height = stride;
    const size_t stride_width = stride;

    #ifdef omp_flag
    #pragma omp parallel for schedule(omp_flag)
    #endif
    for(size_t i = 0; i < output_batch; i++){
        size_t iY = i * output_channel;
        size_t iD = i * input_channel;
        for(size_t j = 0; j < output_channel; j++){
            size_t jiY = (iY + j) * output_height;
            size_t jW = j * input_channel;
            for(size_t k = 0; k < output_height; k++) {
                size_t kjiY = (jiY + k) * output_width;
                size_t kD =  k * input_width * stride_height;
                for(size_t l = 0; l < output_width; l++){
                    size_t indexY = kjiY + l;
                    double tmp = *(outptr+indexY);
                    size_t headD = kD + l * stride_width;
                    for (size_t c = 0; c < input_channel; c++) {
                        size_t ciD = (iD + c) * input_height * input_width + headD;
                        size_t cjW = (jW + c) * filter_width;
                        for (size_t h = 0; h < filter_height; h++) {
                            size_t hciD = ciD + h * input_width;
                            size_t hcjW = (cjW + h) * filter_width;
                            for (size_t w = 0; w < filter_width; w++) {
                                size_t whcD = hciD + w;
                                size_t whcW = hcjW + w;
                                tmp += inptr[whcD] * filptr[whcW];
                            }
                        }
                    }
                    *(outptr+indexY) = tmp;
                }
            }
        }
    }
    return;
}

template<class dataType>
class Im2winConvBase :public Convolution<dataType>{
    public:
    WeTensor<dataType> *input, *filter, *output;
    size_t stride;
    Im2winConvBase(WeTensor<dataType>* input_, WeTensor<dataType> *filter_, WeTensor<dataType> *output_, size_t stride_);
    long double get_gflops() override;
    at::Tensor image2window();
    at::Tensor filter2window();
    void conv_implement() override;
};

template<class dataType>
Im2winConvBase<dataType>::Im2winConvBase(WeTensor<dataType> *input_, WeTensor<dataType> *filter_, WeTensor<dataType> *output_, size_t stride_){
    input = input_;
    filter = filter_;
    output = output_;
    stride = stride_;
}

template<class dataType>
long double Im2winConvBase<dataType>::get_gflops(){
     size_t filter_batch = filter->batch_size;
     size_t filter_channel = filter->channel;
     size_t filter_height = filter->height;
     size_t filter_width = filter->width;

     size_t output_batch = output->batch_size;
     size_t output_channel = output->channel;
     size_t output_height = output->height;
     size_t output_width = output->width;

    static long double gflops = 1e-9 * output_height * output_width * output_batch 
                              * output_channel * filter_channel * filter_height * filter_width * 2;
    return gflops;
}

template<class dataType>
at::Tensor Im2winConvBase<dataType>::image2window(){
    const size_t input_batch = input->batch_size;
    const size_t input_channel = input->channel;
    const size_t input_height = input->height;
    const size_t input_width = input->width;

    const size_t filter_batch = filter->batch_size;
    const size_t filter_channel = filter->channel;
    const size_t filter_height = filter->height;
    const size_t filter_width = filter->width;  

    const size_t stride_height = stride;
    const size_t stride_width = stride;

    const size_t window_row = (input_height - filter_height) / stride_height + 1;
    const size_t window_col = (input_width - filter_width) / stride_width + 1;

    at::Tensor output = at::empty({0}, input->getDataTensor().options());
   
    size_t output_size;
    if(filter_height > stride_height && filter_width > stride_width)
        output_size = input_batch * input_channel * window_row * input_width * filter_height;
    else
        output_size = input_batch * input_channel * input_height * input_width;
    output.resize_(output_size);

    dataType *inptr, *outptr;
    inptr = input->getDataPtr();
    outptr = output.data_ptr<dataType>();

    #ifdef omp_flag
    #pragma omp parallel for schedule(omp_flag)
    #endif
    for(size_t i = 0; i < input_batch; ++i){ 
        size_t iY = i * input_channel;
        size_t iX = i * input_channel * input_height * input_width;
        for(size_t j = 0; j < input_channel; ++j){
            size_t jiY =  (iY + j) * window_row;
            size_t jiX =  iX + j * input_height * input_width;
            for(size_t k = 0; k < window_row; ++k){
                size_t kjiY = (jiY + k) * input_width;
                size_t kjiX = jiX + k * input_width * stride_height;
               for(size_t m = 0; m < input_width; ++m){
                   size_t mkjiY = (kjiY + m) * filter_height;
                   size_t mkjiX = kjiX + m;
                    for(size_t n = 0; n < filter_height; ++n){
                        size_t nmkjiY = mkjiY + n;
                        size_t nmkjiX = mkjiX  + n * input_width;
                        *(outptr + nmkjiY) = *(inptr + nmkjiX);
                    }
                }
            }
        }
    }
    return output;
}

template<class dataType>
at::Tensor Im2winConvBase<dataType>::filter2window(){
    const size_t filter_batch = filter->batch_size;
    const size_t filter_channel = filter->channel;
    const size_t filter_height = filter->height;
    const size_t filter_width = filter->width; 

    at::Tensor output = at::empty({0}, filter->getDataTensor().options());
    output.resize_(filter_batch * filter_channel * filter_height * filter_width);

    dataType *srcptr, *outptr;
    srcptr = filter->getDataPtr();
    outptr = output.data_ptr<dataType>();

    #ifdef omp_flag
    #pragma omp parallel for schedule(omp_flag)
    #endif
    for(size_t i = 0; i < filter_batch; i++){
        size_t iD = i * filter_channel;
        size_t iY = i * filter_channel * filter_height * filter_width; 
        for(size_t j = 0; j < filter_channel; j++){
            size_t jiD = (iD + j) * filter_height;
            size_t jiY = iY + j * filter_height * filter_width;
            for(size_t m = 0; m < filter_height; m++){
                size_t mjiD = (jiD + m) * filter_width;
                size_t mjiY = jiY + m;
               for(size_t n = 0; n < filter_width; n++){
                   size_t nmjiD = mjiD + n;
                   size_t nmjiY = mjiY + n * filter_height;
                    *(outptr + nmjiY) = *(srcptr + nmjiD);
                }
            }
        }
    }
    return output;
}

template<class dataType>
void Im2winConvBase<dataType>::conv_implement(){
    at::Tensor input_win = image2window();
    at::Tensor filter_win = filter2window();

    dataType *inptr, *filptr, *outptr;
    inptr = input_win.data_ptr<dataType>();
    filptr = filter_win.data_ptr<dataType>();
    outptr = output->getDataPtr();

    const size_t input_batch = input->batch_size;
    const size_t input_channel = input->channel;
    const size_t input_height = input->height;
    const size_t input_width = input->width;

    const size_t filter_batch = filter->batch_size;
    const size_t filter_channel = filter->channel;
    const size_t filter_height = filter->height;
    const size_t filter_width = filter->width;

    const size_t output_batch = output->batch_size;
    const size_t output_channel = output->channel;
    const size_t output_height = output->height;
    const size_t output_width = output->width;    

    const size_t stride_height = stride;
    const size_t stride_width = stride;

    const size_t output_csize = output_batch * output_channel * output_height * output_width;
    const size_t window_area = output_height * output_width;
    const size_t filter_area = filter_height * filter_width;
    const size_t filter_c_area = filter_area * filter_channel;
    const size_t gap_width = stride_width * filter_height;
    const size_t window_store = filter_height * input_width;
    const size_t c_window_store = output_height * window_store;
    const size_t sum_window_store = input_channel * c_window_store;
    const size_t sum_filter_area = filter_batch * window_area;

    #ifdef omp_flag
    #pragma omp parallel for schedule(omp_flag)
    #endif
    for(int b = 0; b < input_batch; ++b){
        int bD = b * sum_window_store;
        int bY = b * sum_filter_area;
        for(int  c = 0; c < input_channel; ++c){
            int cbD = bD + c * c_window_store;
            int cW = c * filter_area;
            for(int n = 0; n < filter_batch; ++n){
                int headW = n * filter_c_area + cW;
                int nbY = bY + n * window_area;
                for(int i = 0; i < output_height; ++i){
                    int iD = i * window_store + cbD;
                    int inbY = nbY + i * output_width;
                    for(int j = 0; j < output_width; ++j){
                        int indexY = inbY + j;
                        int headD = iD + j * gap_width;
                        for(int w = 0; w < filter_width; ++w){
                            int wcD = w * filter_height + headD;
                            int wcW = w * filter_height + headW;
                            for(int h = 0; h < filter_height; ++h){
                                int indexD = wcD + h;
                                int indexW = wcW + h; 
                                *(outptr + indexY) += *(inptr + indexD) * (*(filptr + indexW));
                            }
                        }
                    }
                }
            }
        }
    }
}

template<class dataType>
class Im2winConvSIMD :public Convolution<dataType>{
    public:
    WeTensor<dataType> *input, *filter, *output;
    size_t stride;
    at::Tensor image2window();
    at::Tensor filter2window();
    Im2winConvSIMD(WeTensor<dataType>* input_, WeTensor<dataType> *filter_, WeTensor<dataType> *output_, size_t stride_);
    long double get_gflops() override;
    void conv_implement() override;
};

template<class dataType>
Im2winConvSIMD<dataType>::Im2winConvSIMD(WeTensor<dataType> *input_, WeTensor<dataType> *filter_, WeTensor<dataType> *output_, size_t stride_){
    input = input_;
    filter = filter_;
    output = output_;
    stride = stride_;
}

template<class dataType>
long double Im2winConvSIMD<dataType>::get_gflops(){
     size_t filter_batch = filter->batch_size;
     size_t filter_channel = filter->channel;
     size_t filter_height = filter->height;
     size_t filter_width = filter->width;

     size_t output_batch = output->batch_size;
     size_t output_channel = output->channel;
     size_t output_height = output->height;
     size_t output_width = output->width;

    static long double gflops = 1e-9 * output_height * output_width * output_batch 
                              * output_channel * filter_channel * filter_height * filter_width * 2;
    return gflops;
}

template<class dataType>
at::Tensor Im2winConvSIMD<dataType>::image2window(){
    const size_t input_batch = input->batch_size;
    const size_t input_channel = input->channel;
    const size_t input_height = input->height;
    const size_t input_width = input->width;

    const size_t filter_batch = filter->batch_size;
    const size_t filter_channel = filter->channel;
    const size_t filter_height = filter->height;
    const size_t filter_width = filter->width;  

    const size_t stride_height = stride;
    const size_t stride_width = stride;

    const size_t window_row = (input_height - filter_height) / stride_height + 1;
    const size_t window_col = (input_width - filter_width) / stride_width + 1;

    at::Tensor output = at::empty({0}, input->getDataTensor().options());
   
    size_t output_size;
    if(filter_height > stride_height && filter_width > stride_width)
        output_size = input_batch * input_channel * window_row * input_width * filter_height;
    else
        output_size = input_batch * input_channel * input_height * input_width;
    output.resize_(output_size);

    dataType *inptr, *outptr;
    inptr = input->getDataPtr();
    outptr = output.data_ptr<dataType>();

    #ifdef omp_flag
    #pragma omp parallel for schedule(omp_flag)
    #endif
    for(size_t i = 0; i < input_batch; ++i){ 
        size_t iY = i * input_channel;
        size_t iX = i * input_channel * input_height * input_width;
        for(size_t j = 0; j < input_channel; ++j){
            size_t jiY =  (iY + j) * window_row;
            size_t jiX =  iX + j * input_height * input_width;
            for(size_t k = 0; k < window_row; ++k){
                size_t kjiY = (jiY + k) * input_width;
                size_t kjiX = jiX + k * input_width * stride_height;
               for(size_t m = 0; m < input_width; ++m){
                   size_t mkjiY = (kjiY + m) * filter_height;
                   size_t mkjiX = kjiX + m;
                    for(size_t n = 0; n < filter_height; ++n){
                        size_t nmkjiY = mkjiY + n;
                        size_t nmkjiX = mkjiX  + n * input_width;
                        *(outptr + nmkjiY) = *(inptr + nmkjiX);
                    }
                }
            }
        }
    }
    return output;
}

template<class dataType>
at::Tensor Im2winConvSIMD<dataType>::filter2window(){
    const size_t filter_batch = filter->batch_size;
    const size_t filter_channel = filter->channel;
    const size_t filter_height = filter->height;
    const size_t filter_width = filter->width; 

    at::Tensor output = at::empty({0}, filter->getDataTensor().options());
    output.resize_(filter_batch * filter_channel * filter_height * filter_width);

    dataType *srcptr, *outptr;
    srcptr = filter->getDataPtr();
    outptr = output.data_ptr<dataType>();

    #ifdef omp_flag
    #pragma omp parallel for schedule(omp_flag)
    #endif
    for(size_t i = 0; i < filter_batch; i++){
        size_t iD = i * filter_channel;
        size_t iY = i * filter_channel * filter_height * filter_width; 
        for(size_t j = 0; j < filter_channel; j++){
            size_t jiD = (iD + j) * filter_height;
            size_t jiY = iY + j * filter_height * filter_width;
            for(size_t m = 0; m < filter_height; m++){
                size_t mjiD = (jiD + m) * filter_width;
                size_t mjiY = jiY + m;
               for(size_t n = 0; n < filter_width; n++){
                   size_t nmjiD = mjiD + n;
                   size_t nmjiY = mjiY + n * filter_height;
                    *(outptr + nmjiY) = *(srcptr + nmjiD);
                }
            }
        }
    }
    return output;
}

template<class dataType>
void Im2winConvSIMD<dataType>::conv_implement(){
    at::Tensor input_win = image2window();
    at::Tensor filter_win = filter2window();

    dataType *inptr, *filptr, *outptr;
    inptr = input_win.data_ptr<dataType>();
    filptr = filter_win.data_ptr<dataType>();
    outptr = output->getDataPtr();

    size_t dims_a[4]={input->batch_size, input->channel, input->height, input->width};
    size_t dims_b[4]={filter->batch_size, filter->channel, filter->height, filter->width};
    size_t dims_c[4]={output->batch_size, output->channel, output->height, output->width};
    
    IM2WIN_CONV_SIMD(inptr, filptr, outptr, dims_a, dims_b, dims_c);
}

#endif