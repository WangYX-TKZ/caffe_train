#include <algorithm>
#include <csignal>
#include <ctime>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "boost/iterator/counting_iterator.hpp"

#include "caffe/util/bbox_util.hpp"
#include "caffe/util/center_util.hpp"


namespace caffe {
float Yoloverlap(float x1, float w1, float x2, float w2)
{
    float l1 = x1 - w1/2;
    float l2 = x2 - w2/2;
    float left = l1 > l2 ? l1 : l2;
    float r1 = x1 + w1/2;
    float r2 = x2 + w2/2;
    float right = r1 < r2 ? r1 : r2;
    return right - left;
}

float boxIntersection(NormalizedBBox a, NormalizedBBox b)
{
    float a_center_x = (float)(a.xmin() + a.xmax()) / 2;
    float a_center_y = (float)(a.ymin() + a.ymax()) / 2;
    float a_w = (float)(a.xmax() - a.xmin());
    float a_h = (float)(a.ymax() - a.ymin());
    float b_center_x = (float)(b.xmin() + b.xmax()) / 2;
    float b_center_y = (float)(b.ymin() + b.ymax()) / 2;
    float b_w = (float)(b.xmax() - b.xmin());
    float b_h = (float)(b.ymax() - b.ymin());
    float w = Yoloverlap(a_center_x, a_w, b_center_x, b_w);
    float h = Yoloverlap(a_center_y, a_h, b_center_y, b_h);
    if(w < 0 || h < 0) return 0;
    float area = w*h;
    return area;
}

float boxUnion(NormalizedBBox a, NormalizedBBox b)
{
    float i = boxIntersection(a, b);
    float a_w = (float)(a.xmax() - a.xmin());
    float a_h = (float)(a.ymax() - a.ymin());
    float b_w = (float)(b.xmax() - b.xmin());
    float b_h = (float)(b.ymax() - b.ymin());
    float u = a_h*a_w + b_w*b_h - i;
    return u;
}

float YoloBBoxIou(NormalizedBBox a, NormalizedBBox b){
    return (float)boxIntersection(a, b)/boxUnion(a, b);
}

int int_index(std::vector<int>a, int val, int n)
{
    int i;
    for(i = 0; i < n; ++i){
        if(a[i] == val) return i;
    }
    return -1;
}

template <typename Dtype>
Dtype CenterSigmoid(Dtype x){
	return 1. / (1. + exp(-x));
}

template double CenterSigmoid(double x);
template float CenterSigmoid(float x);

template <typename Dtype>
Dtype SingleSoftmaxLoss(Dtype bg_score, Dtype face_score, Dtype lable_value){
    Dtype MaxVaule = bg_score;
    Dtype sumValue = Dtype(0.f);
    Dtype pred_data_value = Dtype(0.f);
    // 求出每组的最大值, 计算出概率值
    MaxVaule = std::max(MaxVaule, face_score);
    sumValue = std::exp(bg_score - MaxVaule) + std::exp(face_score - MaxVaule);
    if(lable_value == 1.){
        pred_data_value = std::exp(face_score - MaxVaule) / sumValue;
    }else{
        pred_data_value = std::exp(bg_score - MaxVaule) / sumValue;
    }
    Dtype loss = (-1) * log(std::max(pred_data_value,  Dtype(FLT_MIN)));
    return loss;
}

template float SingleSoftmaxLoss(float bg_score, float face_score, float lable_value);
template double SingleSoftmaxLoss(double bg_score, double face_score, double lable_value);

template <typename T>
bool SortScorePairDescendCenter(const pair<T, float>& pair1,
                          const pair<T, float>& pair2) {
    return pair1.second > pair2.second;
}

template <typename Dtype>
Dtype smoothL1_Loss(Dtype x, Dtype* x_diff){
    Dtype loss = Dtype(0.);
    Dtype fabs_x_value = std::fabs(x);
    if(fabs_x_value < 1){
        loss = 0.5 * x * x;
        *x_diff = x;
    }else{
        loss = fabs_x_value - 0.5;
        *x_diff = (Dtype(0) < x) - (x < Dtype(0));
    }
    return loss;
}
template float smoothL1_Loss(float x, float* x_diff);
template double smoothL1_Loss(double x, double* x_diff);

template <typename Dtype>
Dtype L2_Loss(Dtype x, Dtype* x_diff){
    Dtype loss = Dtype(0.);
    loss = x * x;
    *x_diff =2 * x;
    return loss;
}

template float L2_Loss(float x, float* x_diff);
template double L2_Loss(double x, double* x_diff);

template <typename Dtype>
Dtype Object_L2_Loss(Dtype x, Dtype* x_diff){
    Dtype loss = Dtype(0.);
    loss =0.5 * x * x;
    *x_diff =x;
    return loss;
}

template float Object_L2_Loss(float x, float* x_diff);
template double Object_L2_Loss(double x, double* x_diff);


template<typename Dtype>
Dtype gaussian_radius(const Dtype heatmap_height, const Dtype heatmap_width, const Dtype min_overlap){

    Dtype a1  = Dtype(1.0);
    Dtype b1  = (heatmap_width + heatmap_height);
    Dtype c1  = Dtype( heatmap_width * heatmap_height * (1 - min_overlap) / (1 + min_overlap));
    Dtype sq1 = std::sqrt(b1 * b1 - 4 * a1 * c1);
    Dtype r1  = Dtype((b1 + sq1) / 2);

    Dtype a2  = Dtype(4.0);
    Dtype b2  = 2 * (heatmap_height + heatmap_width);
    Dtype c2  = (1 - min_overlap) * heatmap_width * heatmap_height;
    Dtype sq2 = std::sqrt(b2 * b2 - 4 * a2 * c2);
    Dtype r2  = Dtype((b2 + sq2) / 2);

    Dtype a3  = Dtype(4 * min_overlap);
    Dtype b3  = -2 * min_overlap * (heatmap_height + heatmap_width);
    Dtype c3  = (min_overlap - 1) * heatmap_width * heatmap_height;
    Dtype sq3 = std::sqrt(b3 * b3 - 4 * a3 * c3);
    Dtype r3  = Dtype((b3 + sq3) / 2);
    return std::min(std::min(r1, r2), r3);
}

template float gaussian_radius(const float heatmap_width, const float heatmap_height, const float min_overlap);
template double gaussian_radius(const double heatmap_width, const double heatmap_height, const double min_overlap);


template<typename Dtype>
std::vector<Dtype> gaussian2D(const int height, const int width, Dtype sigma){
    int half_width = (width - 1) / 2;
    int half_height = (height - 1) / 2;
    std::vector<Dtype> heatmap((width *height), Dtype(0.));
    for(int i = 0; i < height; i++){
        int x = i - half_height;
        for(int j = 0; j < width; j++){
            int y = j - half_width;
            heatmap[i * width + j] = std::exp(float(-(x*x + y*y) / (2* sigma * sigma)));
            if(heatmap[i * width + j] < 0.00000000005)
                heatmap[i * width + j] = Dtype(0.);
        }
    }
    return heatmap;
}

template std::vector<float> gaussian2D(const int height, const int width, float sigma);
template std::vector<double> gaussian2D(const int height, const int width, double sigma);

template<typename Dtype>
void draw_umich_gaussian(std::vector<Dtype>& heatmap, int center_x, int center_y, float radius
                              , const int height, const int width){
    float diameter = 2 * radius + 1;
    std::vector<Dtype> gaussian = gaussian2D(int(diameter), int(diameter), Dtype(diameter / 6));
    int left = std::min(int(center_x), int(radius)), right = std::min(int(width - center_x), int(radius) + 1);
    int top = std::min(int(center_y), int(radius)), bottom = std::min(int(height - center_y), int(radius) + 1);
    if((left + right) > 0 && (top + bottom) > 0){
        for(int row = 0; row < (top + bottom); row++){
            for(int col = 0; col < (right + left); col++){
                int heatmap_index = (int(center_y) -top + row) * width + int(center_x) -left + col;
                int gaussian_index = (int(radius) - top + row) * int(diameter) + int(radius) - left + col;
                heatmap[heatmap_index] = heatmap[heatmap_index] >= gaussian[gaussian_index]  ? heatmap[heatmap_index]:
                                            gaussian[gaussian_index];
            }
        }
    }
}

template void draw_umich_gaussian(std::vector<float>& heatmap, int center_x, int center_y, float radius, const int height, const int width);
template void draw_umich_gaussian(std::vector<double>& heatmap, int center_x, int center_y, float radius, const int height, const int width);

template <typename Dtype>
void transferCVMatToBlobData(std::vector<Dtype> heatmap, Dtype* buffer_heat){
  for(unsigned ii = 0; ii < heatmap.size(); ii++){
        buffer_heat[ii] = buffer_heat[ii] > heatmap[ii] ? 
                                              buffer_heat[ii] : heatmap[ii];
  }
}
template void transferCVMatToBlobData(std::vector<float> heatmap, float* buffer_heat);
template void transferCVMatToBlobData(std::vector<double> heatmap, double* buffer_heat);



template <typename Dtype> 
Dtype FocalLossSigmoid(Dtype* label_data, Dtype * pred_data, int dimScale, Dtype *bottom_diff){
    Dtype alpha_ = 0.25;
    Dtype gamma_ = 2.f;
    Dtype loss = Dtype(0.);
    for(int i = 0; i < dimScale; i++){
        if(label_data[i] == 0.5){ // gt_boxes周围的小格子，因为离gt_box较近，所以计算这里的负样本
            loss -= alpha_ * std::pow(pred_data[i], gamma_) * std::log(std::max(1 - pred_data[i], Dtype(FLT_MIN)));
            Dtype diff_elem_ = alpha_ * std::pow(pred_data[i], gamma_);
            Dtype diff_next_ = pred_data[i] - gamma_ * (1 - pred_data[i]) * std::log(std::max(1 - pred_data[i], Dtype(FLT_MIN)));
            bottom_diff[i] = diff_elem_ * diff_next_;
        }else if(label_data[i] == 1){ //gt_boxes包围的都认为是正样本
            loss -= alpha_ * std::pow(1 - pred_data[i], gamma_) * std::log(std::max(pred_data[i], Dtype(FLT_MIN)));
            Dtype diff_elem_ = alpha_ * std::pow(1 - pred_data[i], gamma_);
            Dtype diff_next_ = gamma_ * pred_data[i] * std::log(std::max(pred_data[i], Dtype(FLT_MIN))) + pred_data[i] - 1;
            bottom_diff[i] = diff_elem_ * diff_next_;
        }
    }
    return loss;
}

template float FocalLossSigmoid(float* label_data, float *pred_data, int dimScale,  float *bottom_diff);
template double FocalLossSigmoid(double* label_data, double *pred_data, int dimScale,  double *bottom_diff);


// hard sampling mine postive : negative 1: 5 sigmoid
// 按理来说是需要重新统计负样本的编号，以及获取到他的数值
// label_data : K x H x W
// pred_data : K x H x W x N
template <typename Dtype>
void SelectHardSampleSigmoid(Dtype *label_data, Dtype *pred_data, const int negative_ratio, const int num_postive, 
                          const int output_height, const int output_width, const int num_channels){
    CHECK_EQ(num_channels, 4 + 2) << "x, y, width, height + objectness + label class containing face";
    std::vector<std::pair<int, float> > loss_value_indices;
    loss_value_indices.clear();
    Dtype alpha_ = 0.25;
    Dtype gamma_ = 2.f;
    for(int h = 0; h < output_height; h ++){
        for(int w = 0; w < output_width; w ++){
            if(label_data[h * output_width +w] == 0.){
                int bg_index = h * output_width + w;
                // Focal loss when sample belong to background
                Dtype loss = (-1) * alpha_ * std::pow(pred_data[bg_index], gamma_) * 
                                            std::log(std::max(1 - pred_data[bg_index],  Dtype(FLT_MIN)));
                loss_value_indices.push_back(std::make_pair(bg_index, loss));
            }
        }
    }
    std::sort(loss_value_indices.begin(), loss_value_indices.end(), SortScorePairDescendCenter<int>);
    int num_negative = std::min(int(loss_value_indices.size()), num_postive * negative_ratio);
    for(int ii = 0; ii < num_negative; ii++){
        int h = loss_value_indices[ii].first / output_width;
        int w = loss_value_indices[ii].first % output_width;
        label_data[h * output_width + w] = 0.5;
    }
}
template void SelectHardSampleSigmoid(float *label_data, float *pred_data, const int negative_ratio, const int num_postive, 
                                     const int output_height, const int output_width, const int num_channels);
template void SelectHardSampleSigmoid(double *label_data, double *pred_data, const int negative_ratio, const int num_postive, 
                                     const int output_height, const int output_width, const int num_channels);



// label_data shape N : 1
// pred_data shape N : k (object classes)
// dimScale is the number of what ?? N * K ??
template <typename Dtype>
Dtype SoftmaxLossEntropy(Dtype* label_data, Dtype* pred_data, 
                            const int batch_size, const int output_height, 
                            const int output_width, Dtype *bottom_diff, 
                            const int num_channels){
    Dtype loss = Dtype(0.f);
    int dimScale = output_height * output_width;
    for(int b = 0; b < batch_size; b++){
        for(int h = 0; h < output_height; h++){
            for(int w = 0; w < output_width; w++){
                Dtype label_value = Dtype(label_data[b * dimScale + h * output_width + w]);
                if(label_value == Dtype(-1.) || label_value == Dtype(-2.)){
                    continue;
                }else{
                    int label_idx = 0;
                    if(label_value == 0.5)
                        label_idx = 0;
                    else if(label_value == 1.)
                        label_idx = 1;
                    else{
                        LOG(FATAL)<<"no valid label value";
                    }
                    int bg_index = b * num_channels * dimScale + 4 * dimScale + h * output_width + w;
                    Dtype MaxVaule = pred_data[bg_index + 0 * dimScale];
                    Dtype sumValue = Dtype(0.f);
                    // 求出每组的最大值, 计算出概率值
                    for(int c = 0; c < 2; c++){
                        MaxVaule = std::max(MaxVaule, pred_data[bg_index + c * dimScale]);
                    }
                    for(int c = 0; c< 2; c++){
                        sumValue += std::exp(pred_data[bg_index + c * dimScale] - MaxVaule);
                    }
                    Dtype pred_data_value = std::exp(pred_data[bg_index + label_idx * dimScale] - MaxVaule) / sumValue;
                    Dtype pred_another_data_value = std::exp(pred_data[bg_index + (1 - label_idx) * dimScale] - MaxVaule) / sumValue;
                    loss -= log(std::max(pred_data_value,  Dtype(FLT_MIN)));
                    bottom_diff[bg_index + label_idx * dimScale] = pred_data_value - 1;
                    bottom_diff[bg_index + (1 - label_idx) * dimScale] = pred_another_data_value;
                }
            }
        }
    }
    return loss;
}

template float SoftmaxLossEntropy(float* label_data, float* pred_data, 
                            const int batch_size, const int output_height, 
                            const int output_width, float *bottom_diff, 
                            const int num_channels);
template double SoftmaxLossEntropy(double* label_data, double* pred_data, 
                            const int batch_size, const int output_height, 
                            const int output_width, double *bottom_diff, 
                            const int num_channels);

template <typename Dtype>
void SoftmaxCenterGrid(Dtype * pred_data, const int batch_size,
            const int label_channel, const int num_channels,
            const int outheight, const int outwidth){
    CHECK_EQ(label_channel, 2);
    int dimScale = outheight * outwidth;
    for(int b = 0; b < batch_size; b ++){
        for(int h = 0; h < outheight; h++){
            for(int w = 0; w < outwidth; w++){
                int class_index = b * num_channels * dimScale + 4 * dimScale +  h * outwidth + w;
                Dtype MaxVaule = pred_data[class_index + 0 * dimScale];
                Dtype sumValue = Dtype(0.f);
                // 求出每组的最大值
                for(int c = 0; c< label_channel; c++){
                    MaxVaule = std::max(MaxVaule, pred_data[class_index + c * dimScale]);
                }
                // 每个样本组减去最大值， 计算exp，求和
                for(int c = 0; c< label_channel; c++){
                    pred_data[class_index + c * dimScale] = std::exp(pred_data[class_index + c * dimScale] - MaxVaule);
                    sumValue += pred_data[class_index + c * dimScale];
                }
                // 计算softMax
                for(int c = 0; c< label_channel; c++){
                    pred_data[class_index + c * dimScale] = pred_data[class_index + c * dimScale] / sumValue;
                }
            }
        }
    }
}

template void SoftmaxCenterGrid(float * pred_data, const int batch_size,
            const int label_channel, const int num_channels,
            const int outheight, const int outwidth);
template void SoftmaxCenterGrid(double * pred_data, const int batch_size,
            const int label_channel, const int num_channels,
            const int outheight, const int outwidth);
// hard sampling mine postive : negative 1: 5 softmax
// 按理来说是需要重新统计负样本的编号，以及获取到他的数值
// label_data : K x H x W
template <typename Dtype>
void SelectHardSampleSoftMax(Dtype *label_data, std::vector<Dtype> batch_sample_loss,
                          const int negative_ratio, std::vector<int> postive, 
                          const int output_height, const int output_width,
                          const int num_channels, const int batch_size){
    CHECK_EQ(num_channels, 4 + 2) << "x, y, width, height + label classes containing background + face";
    int dimScale = output_height * output_width;
    std::vector<std::pair<int, float> > loss_value_indices;
    for(int b = 0; b < batch_size; b ++){
        loss_value_indices.clear();
        int num_postive = postive[b];
        for(int h = 0; h < output_height; h ++){
            for(int w = 0; w < output_width; w ++){
                int select_index = h * output_width + w;
                if(label_data[b * dimScale + select_index] == -1.){
                    loss_value_indices.push_back(std::make_pair(select_index, batch_sample_loss[b * dimScale + select_index]));
                }
            }
        }
        std::sort(loss_value_indices.begin(), loss_value_indices.end(), SortScorePairDescendCenter<int>);
        int num_negative = std::min(int(loss_value_indices.size()), num_postive * negative_ratio);
        for(int ii = 0; ii < num_negative; ii++){
            int h = loss_value_indices[ii].first / output_width;
            int w = loss_value_indices[ii].first % output_width;
            label_data[b * dimScale + h * output_width + w] = 0.5;
        }
    }
}

template void SelectHardSampleSoftMax(float *label_data, std::vector<float> batch_sample_loss, 
                          const int negative_ratio, std::vector<int> postive, 
                          const int output_height, const int output_width,
                          const int num_channels, const int batch_size);
template void SelectHardSampleSoftMax(double *label_data, std::vector<double> batch_sample_loss, 
                          const int negative_ratio, std::vector<int> postive, 
                          const int output_height, const int output_width,
                          const int num_channels, const int batch_size);




}  // namespace caffe