/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/fastertransformer/layers/FfnLayer.h"
#include "src/fastertransformer/kernels/layernorm_kernels.h"
#include "src/fastertransformer/kernels/transpose_int8_kernels.h"
#include "src/fastertransformer/cuda/nvtx/nvtx_utils.h"

namespace fastertransformer {

template<typename T>
void FfnLayer<T>::preAllocate() {
    if (max_token_num_ > 0) {
        allocateBuffer(max_token_num_, false);
    }
}

template<typename T>
void FfnLayer<T>::forward(std::vector<fastertransformer::Tensor>*       output_tensors,
                          const std::vector<fastertransformer::Tensor>* input_tensors,
                          const FfnWeight<T>*                           ffn_weights,
                          const bool                                    use_moe) {
    TensorMap input_tensor({{"ffn_input", input_tensors->at(0)}});
    TensorMap output_tensor({{"ffn_output", output_tensors->at(0)}});
    forward(&output_tensor, &input_tensor, ffn_weights, use_moe);
}

template<typename T>
void FfnLayer<T>::forward(TensorMap* output_tensors, TensorMap* input_tensors, const FfnWeight<T>* ffn_weights, const bool use_moe) {
    // input tensors:
    //      ffn_input [token_num, hidden_dimension],
    //      ia3_tasks [batch_size] (optional)
    //      moe_k     [1], uint64 (optional)
    //      padding_offset [token_num] (optional)
    //      seq_len [1], int32, (optional), only used for ia3

    // output tensors:
    //      ffn_output [token_num, hidden_dimension]
    //      expert_scales [token_num, moe_k] (optional)
    //      expanded_source_row_to_expanded_dest_row [token_num, moe_k] (optional)
    //      expert_for_source_row [token_num, moe_k] (optional)

    FT_LOG_DEBUG(__PRETTY_FUNCTION__);
    FT_CHECK(input_tensors->size() >= 1 && input_tensors->size() <= 9);
    FT_CHECK(output_tensors->size() >= 1 || output_tensors->size() <= 4);

    allocateBuffer(input_tensors->at("ffn_input").shape()[0], use_moe);

    const int m             = input_tensors->at("ffn_input").shape()[0];
    T*        output_tensor = output_tensors->at("ffn_output").getPtr<T>();
    const T*  input_tensor  = input_tensors->at("ffn_input").getPtr<const T>();
    const int layer_id      = input_tensors->getVal<int>("layer_id");

    const int  batch_size         = input_tensors->getVal<int>("batch_size", 1);
    const int* lora_input_lengths = input_tensors->getPtr<int>("lora_input_lengths", nullptr);
    const int* ia3_tasks = input_tensors->getPtr<const int>("ia3_tasks", nullptr);
    // lora
    const int* lora_ids = input_tensors->getPtr<int>("lora_ids", nullptr);
    const float* ffn_dynamic_scale = quant_algo_.smoothQuantInt8() ? input_tensors->at("ffn_dynamic_scale").getPtr<float>() : nullptr;

    //  for moe output
    T*   expert_scales    = nullptr;
    int* permuted_rows    = nullptr;

    T*  fc2_result = nullptr;
    int* permuted_experts = nullptr;

    // moe outputs should exist or not together
    FT_CHECK((use_moe && output_tensors->isExist("expert_scales")
              && output_tensors->isExist("expanded_source_row_to_expanded_dest_row")
              && output_tensors->isExist("expert_for_source_row"))
             || (!use_moe && !output_tensors->isExist("expert_scales")
                 && !output_tensors->isExist("expanded_source_row_to_expanded_dest_row")
                 && !output_tensors->isExist("expert_for_source_row")));

    if (use_moe) {
        fc2_result    = output_tensors->at("fc2_result").getPtr<T>();
        expert_scales    = output_tensors->at("expert_scales").getPtr<T>();
        permuted_rows    = output_tensors->at("expanded_source_row_to_expanded_dest_row").getPtr<int>();
        permuted_experts = output_tensors->at("expert_for_source_row").getPtr<int>();
    }

    // moe can't be used with use_gated_activation currently
    auto activation_type = getActivationType();

    if (use_moe) {
        FT_CHECK(ffn_weights->gating_weight.kernel != nullptr);
        print_bsd(layer_id, "moe input", input_tensor, 1, m, hidden_units_);
        float alpha = 1.0f;
        float beta  = 0.0f;

        cublas_wrapper_->Gemm(CUBLAS_OP_N,
                              CUBLAS_OP_N,
                              expert_num_,
                              m,
                              hidden_units_,
                              &alpha,
                              ffn_weights->gating_weight.kernel,
                              CUDA_R_16F,
                              expert_num_,
                              input_tensor,
                              CUDA_R_16F,
                              hidden_units_,
                              &beta,
                              moe_gates_buf_,
                              CUDA_R_32F,
                              expert_num_,
                              CUDA_R_32F,
                              cublasGemmAlgo_t(-1));

        print_bsd(layer_id, "moe gate", moe_gates_buf_, 1, m, expert_num_);

        if (quant_algo_.weightOnly()) {
            FT_CHECK_WITH_INFO(quant_algo_.getGroupSize() == 0, "moe only support scale_per_col");
            moe_plugin_->enqueue(input_tensor,
                                 moe_gates_buf_,
                                 ffn_weights->intermediate_weight.quant_kernel,
                                 ffn_weights->intermediate_weight.weight_only_quant_scale,
                                 ffn_weights->intermediate_weight.bias,
                                 ffn_weights->output_weight.quant_kernel,
                                 ffn_weights->output_weight.weight_only_quant_scale,
                                 ffn_weights->output_weight.bias,
                                 ffn_weights->intermediate_weight2.quant_kernel,
                                 ffn_weights->intermediate_weight2.weight_only_quant_scale,
                                 ffn_weights->intermediate_weight2.bias,
                                 m,
                                 moe_fc_workspace_,
                                 output_tensor,
                                 fc2_result,
                                 nullptr,
                                 expert_scales,
                                 permuted_rows,
                                 permuted_experts,
                                 stream_);
        } else {
            moe_plugin_->enqueue(input_tensor,
                                 moe_gates_buf_,
                                 ffn_weights->intermediate_weight.kernel,
                                 nullptr,
                                 ffn_weights->intermediate_weight.bias,
                                 ffn_weights->output_weight.kernel,
                                 nullptr,
                                 ffn_weights->output_weight.bias,
                                 ffn_weights->intermediate_weight2.kernel,
                                 nullptr,
                                 ffn_weights->intermediate_weight2.bias,
                                 m,
                                 moe_fc_workspace_,
                                 output_tensor,
                                 fc2_result,
                                 nullptr,
                                 expert_scales,
                                 permuted_rows,
                                 permuted_experts,
                                 stream_);
        }
        sync_check_cuda_error();
        return;
    }

    const int64_t inter_size = is_sparse_head_ ? local_layer_inter_size_[layer_id] : inter_size_;
    const int64_t inter_padding_size =
        is_sparse_head_ ? local_layer_inter_padding_size_[layer_id] : inter_padding_size_;

    PUSH_RANGE(stream_, "ffn_gemm_1");
    int m_tmp = input_tensors->at("ffn_input").shape()[0];
    if (m_tmp % 8 != 0) {
        m_tmp = (m_tmp / 8 + 1) * 8;
    }
#ifdef SPARSITY_ENABLED
    const int m_padded        = m_tmp;
    bool      use_sparse_gemm = sparse_ && cublas_wrapper_->isUseSparse(1, inter_size, m, hidden_units_);
#else
    constexpr bool use_sparse_gemm = false;
    constexpr int  m_padded        = 0;
#endif
    const int cur_inter_size = quant_algo_.weightOnly() ? inter_padding_size : inter_size;
    gemm_runner_->Gemm(m,
                       cur_inter_size,
                       hidden_units_,
                       input_tensor,
                       &ffn_weights->intermediate_weight2,
                       inter_buf_,
                       ffn_dynamic_scale);

    // lora
    lora_gemm_->applyLoRA(m,
                          batch_size,
                          lora_input_lengths,
                          hidden_units_,
                          cur_inter_size,
                          lora_ids,
                          ffn_weights->intermediate_weight2.lora_weights,
                          input_tensor,
                          inter_buf_);

    print_bsd(layer_id, "ffn_up", inter_buf_, 1, m, cur_inter_size);

    if (use_gated_activation_) {
        // gemm used inter_size, int8 use inter_padding_size
        gemm_runner_->Gemm(m,
                           cur_inter_size,
                           hidden_units_,
                           input_tensor,
                           &ffn_weights->intermediate_weight,
                           inter_buf_2_,
                           ffn_dynamic_scale);
        // lora
        lora_gemm_->applyLoRA(m,
                              batch_size,
                              lora_input_lengths,
                              hidden_units_,
                              cur_inter_size,
                              lora_ids,
                              ffn_weights->intermediate_weight.lora_weights,
                              input_tensor,
                              inter_buf_2_);
        print_bsd(layer_id, "ffn_gate", inter_buf_2_, 1, m, cur_inter_size);
    }
    POP_RANGE;  // End for NVTX Range: FFN gemm 1

    PUSH_RANGE(stream_, "add_bias_act");
    genericActivation(layer_id,
                      m,
                      ffn_weights->intermediate_weight2.bias,
                      use_gated_activation_ ? ffn_weights->intermediate_weight.bias : nullptr,
                      input_tensors->at("ia3_tasks", {MEMORY_GPU, TYPE_INT32, {}, nullptr}).getPtr<const int>(),
                      ffn_weights->ia3_weight.kernel,
                      (float*)nullptr,
                      (float*)nullptr,
                      ffn_weights->output_weight.act_scale,
                      input_tensors->getPtr<int>("padding_offset", nullptr),
                      input_tensors->getVal<int>("seq_len", 1));
    POP_RANGE;

    sync_check_cuda_error();

    print_bsd(layer_id, "ffn act", inter_buf_, 1, m, inter_padding_size);

    T* inter_buf_normed_output = nullptr;
    if (ffn_weights->dense_layernorm.gamma && ffn_weights->dense_layernorm.beta) {
        invokeGeneralLayerNormWithPadding(inter_buf_normed_,
                                          inter_buf_,
                                          ffn_weights->dense_layernorm.gamma,
                                          ffn_weights->dense_layernorm.beta,
                                          layernorm_eps_,
                                          m,
                                          inter_size,
                                          inter_padding_size,
                                          nullptr,
                                          nullptr,
                                          0,
                                          stream_);

        inter_buf_normed_output = inter_buf_normed_;
        sync_check_cuda_error();
    } else {
        inter_buf_normed_output = inter_buf_;
    }
    print_bsd(layer_id, "ffn ln", inter_buf_normed_output, 1, m, cur_inter_size);

    if (quant_algo_.smoothQuantInt8()) {
        invokePerTokenQuantization(reinterpret_cast<int8_t*>(inter_buf_normed_),
                                   inter_buf_,
                                   m,
                                   cur_inter_size,
                                   ffn_dynamic_scale_2_,
                                   ffn_weights->output_weight.smoother,
                                   nullptr,
                                   stream_);
        inter_buf_normed_output = (inter_buf_normed_);
        sync_check_cuda_error();
        print_bsd(layer_id, "ffn quant tensor", reinterpret_cast<int8_t*>(inter_buf_normed_output), 1, m, cur_inter_size);

    }

    PUSH_RANGE(stream_, "ffn_gemm_2");
    gemm_runner_->Gemm(m,
                       hidden_units_,
                       cur_inter_size,
                       inter_buf_normed_output,
                       &ffn_weights->output_weight,
                       output_tensor,
                       ffn_dynamic_scale_2_);

    // lora

    lora_gemm_->applyLoRA(m,
                          batch_size,
                          lora_input_lengths,
                          cur_inter_size,
                          hidden_units_,
                          lora_ids,
                          ffn_weights->output_weight.lora_weights,
                          inter_buf_normed_output,
                          output_tensor);
    print_bsd(layer_id, "before in share", output_tensor, 1, m, hidden_units_);

    if (ffn_weights->shared_expert_gating_weight.kernel) {
        float alpha = 1.0f;
        float beta  = 0.0f;

        cublas_wrapper_->Gemm(CUBLAS_OP_N,
                              CUBLAS_OP_N,
                              1,
                              m,
                              hidden_units_,
                              &alpha,
                              ffn_weights->shared_expert_gating_weight.kernel,
                              CUDA_R_16F,
                              1,
                              input_tensor,
                              CUDA_R_16F,
                              hidden_units_,
                              &beta,
                              shared_gating_scale_buf_,
                              CUDA_R_16F,
                              1,
                              CUDA_R_32F,
                              cublasGemmAlgo_t(-1));

        // gemm_runner_->Gemm(m, 1, hidden_units_, input_tensor, &ffn_weights->shared_expert_gating_weight, shared_gating_scale_buf_);
        print_bsd(layer_id, "shared scale buf", shared_gating_scale_buf_, 1, m, 1, 0, 1);
        invokeSigmoid(shared_gating_scale_buf_, m, 1, stream_);
        print_bsd(layer_id, "after sigmoid", shared_gating_scale_buf_, 1, m, 1, 0, 1);
        invokeScaledDot(output_tensor, output_tensor, shared_gating_scale_buf_, m, hidden_units_, stream_);
    }

    print_bsd(layer_id, "ffn out layer", output_tensor, 1, m, hidden_units_);

    sync_check_cuda_error();
    POP_RANGE;

    if (is_free_buffer_after_forward_ == true) {
        freeBuffer();
    }
    sync_check_cuda_error();
}

template<typename T>
FfnLayer<T>::FfnLayer(size_t                            max_batch_size,
                      size_t                            max_seq_len,
                      size_t                            hidden_units,
                      size_t                            expert_num,
                      size_t                            moe_k,
                      bool                              moe_normalize_expert_scale,
                      size_t                            moe_style,
                      size_t                            inter_size,
                      size_t                            inter_padding_size,
                      size_t                            moe_inter_padding_size,
                      std::vector<int64_t>              local_layer_inter_size,
                      std::vector<int64_t>              local_layer_inter_padding_size,
                      cudaStream_t                      stream,
                      cublasMMWrapper*                  cublas_wrapper,
                      tc::QuantAlgo                     quant_algo,
                      IAllocator*                       allocator,
                      bool                              is_free_buffer_after_forward,
                      bool                              sparse,
                      bool                              is_sparse_head,
                      fastertransformer::ActivationType activation_type,
                      bool                              has_moe_norm,
                      float                             layernorm_eps):
    BaseLayer(stream, cublas_wrapper, allocator, is_free_buffer_after_forward, nullptr, sparse),
    max_token_num_(max_batch_size * max_seq_len),
    expert_num_(expert_num),
    moe_k_(moe_k),
    moe_normalize_expert_scale_(moe_normalize_expert_scale),
    moe_style_(moe_style),
    hidden_units_(hidden_units),
    inter_size_(inter_size),
    inter_padding_size_(inter_padding_size),
    moe_inter_padding_size_(moe_inter_padding_size),
    local_layer_inter_size_(local_layer_inter_size),
    local_layer_inter_padding_size_(local_layer_inter_padding_size),
    is_sparse_head_(is_sparse_head),
    activation_type_(activation_type),
    quant_algo_(quant_algo),
    has_moe_norm_(has_moe_norm),
    layernorm_eps_(layernorm_eps) {
    use_gated_activation_ = isGatedActivation(activation_type_);
    FT_LOG_DEBUG(__PRETTY_FUNCTION__);
    gemm_runner_ = std::make_shared<GemmRunner<T>>(stream, allocator, cublas_wrapper, quant_algo_);

    if (expert_num > 0) {
        nvinfer1::DataType data_type, weight_type;
        if (std::is_same<T, half>::value) {
            data_type = nvinfer1::DataType::kHALF;
        } else if (std::is_same<T, __nv_bfloat16>::value) {
            data_type = nvinfer1::DataType::kBF16;
        } else if (std::is_same<T, float>::value) {
            data_type = nvinfer1::DataType::kFLOAT;
        } else {
            FT_LOG_ERROR("not supported yet");
        }

        if (quant_algo.getWeightBits() == 8) {
            weight_type = nvinfer1::DataType::kINT8;
        } else if (quant_algo.getWeightBits() != 0) {
            FT_LOG_ERROR("MOE only support int8");
        } else {
            weight_type = data_type;
        }

        tensorrt_llm::kernels::MOEExpertScaleNormalizationMode moe_norm_mode =
            tensorrt_llm::kernels::MOEExpertScaleNormalizationMode::NONE;
        if (has_moe_norm_) {
            moe_norm_mode = tensorrt_llm::kernels::MOEExpertScaleNormalizationMode::RENORMALIZE;
        }

        moe_plugin_ = std::make_shared<tensorrt_llm::plugins::MixtureOfExpertsPlugin>(expert_num,
                                                                                      moe_k,
                                                                                      moe_normalize_expert_scale,
                                                                                      hidden_units_,
                                                                                      moe_style == 2 ? moe_inter_padding_size : inter_padding_size,
                                                                                      activation_type,
                                                                                      data_type,
                                                                                      weight_type,
                                                                                      moe_norm_mode);
    }
    lora_gemm_ = std::make_shared<LoraGemm<T>>(stream, allocator, cublas_wrapper);
}

template<typename T>
FfnLayer<T>::~FfnLayer() {
    FT_LOG_DEBUG(__PRETTY_FUNCTION__);
    cublas_wrapper_ = nullptr;
    freeBuffer();
}

template<typename T>
void FfnLayer<T>::allocateBuffer() {
    FT_CHECK_WITH_INFO(false,
                       "FfnLayer::allocateBuffer() is deprecated. Use `allocateBuffer(size_t token_num, ...)` instead");
}

template<typename T>
void FfnLayer<T>::allocateBuffer(size_t token_num, bool use_moe) {
    FT_LOG_DEBUG(__PRETTY_FUNCTION__);
    if (use_moe) {
        moe_gates_buf_ =
            (float*)allocator_->reMalloc(moe_gates_buf_, sizeof(float) * pad_to_multiple_of_16(token_num * expert_num_));
        size_t moe_workspace_size = moe_plugin_->getWorkspaceSize(token_num);
        moe_fc_workspace_         = (char*)allocator_->reMalloc(moe_fc_workspace_, moe_workspace_size);
    } else {
        shared_gating_scale_buf_ =  (T*)allocator_->reMalloc(shared_gating_scale_buf_, sizeof(T) * token_num);
        const auto type_size = sizeof(T);
        inter_buf_ =
            (T*)allocator_->reMalloc(inter_buf_, type_size * token_num * inter_padding_size_ + token_num * 4);
        if (use_gated_activation_) {
            inter_buf_2_ = (T*)allocator_->reMalloc(
                inter_buf_2_, sizeof(T) * token_num * inter_padding_size_ + token_num * 4);
        }
        inter_buf_normed_ = (T*)(allocator_->reMalloc(
            inter_buf_normed_, sizeof(T) * token_num * inter_padding_size_ + token_num * 4));
        cudaMemsetAsync(inter_buf_normed_, 0, sizeof(T) * token_num * inter_padding_size_ + token_num * 4, stream_);
    }

    if(quant_algo_.smoothQuantInt8()){
        ffn_dynamic_scale_2_ = (float*)(allocator_->reMalloc(ffn_dynamic_scale_2_, sizeof(float)* token_num));
    }
    is_allocate_buffer_ = true;
}

template<typename T>
void FfnLayer<T>::freeBuffer() {
    FT_LOG_DEBUG(__PRETTY_FUNCTION__);
    if (is_allocate_buffer_) {
        allocator_->free((void**)(&inter_buf_));
        if (use_gated_activation_) {
            allocator_->free((void**)(&inter_buf_2_));
        }
        allocator_->free((void**)(&inter_buf_normed_));
        allocator_->free((void**)(&shared_gating_scale_buf_));

        if (expert_num_ != 0) {
            allocator_->free((void**)(&moe_gates_buf_));
            allocator_->free((void**)(&moe_fc_workspace_));
        }

        if (mixed_gemm_workspace_) {
            allocator_->free((void**)(&mixed_gemm_workspace_));
            mixed_gemm_ws_bytes_ = 0;
        }

        is_allocate_buffer_ = false;
    }
}

#define INVOKE_GENERIC_ACT(ACT)                                                                                        \
    invokeGenericActivation<ACT>(inter_buf_,                                                                           \
                                 bias1,                                                                                \
                                 inter_buf_2_,                                                                         \
                                 bias2,                                                                                \
                                 ia3_tasks,                                                                            \
                                 ia3_weights,                                                                          \
                                 m,                                                                                    \
                                 inter_padding_size,                                                                   \
                                 0,                                                                                    \
                                 activation_in,                                                                        \
                                 activation_out,                                                                       \
                                 activation_scale,                                                                     \
                                 padding_offset,                                                                       \
                                 seq_len,                                                                              \
                                 stream_);

template<typename T>
void FfnLayer<T>::genericActivation(int          layer_id,
                                    int          m,
                                    const T*     bias1,
                                    const T*     bias2,
                                    const int*   ia3_tasks,
                                    const T*     ia3_weights,
                                    const float* activation_in,
                                    const float* activation_out,
                                    const T*     activation_scale,
                                    const int*   padding_offset,
                                    const int    seq_len) {
    if (ia3_tasks != nullptr) {
        FT_CHECK(seq_len > 0);
    }

    const int64_t inter_padding_size =
        is_sparse_head_ ? local_layer_inter_padding_size_[layer_id] : inter_padding_size_;

    // dispatch according to actual activation
    switch (getActivationType()) {
        case ActivationType::Gelu:
        case ActivationType::Geglu:
            INVOKE_GENERIC_ACT(GeluActivation);
            break;
        case ActivationType::Relu:
            INVOKE_GENERIC_ACT(ReluActivation);
            break;
        case ActivationType::Silu:
        case ActivationType::Swiglu:
            INVOKE_GENERIC_ACT(SiluActivation);
            break;
        case ActivationType::Identity:
            INVOKE_GENERIC_ACT(IdentityActivation);
            break;
        case ActivationType::GeluNoneApproximate:
        case ActivationType::GeGluNoneApproximate:
            INVOKE_GENERIC_ACT(GeluActivationNoneApproximate);
            break;
        default:
            FT_CHECK_WITH_INFO(false, "not support activation type");
            break;
    }
}

#undef INVOKE_GENERIC_ACT

template class FfnLayer<float>;
template class FfnLayer<half>;
#ifdef ENABLE_BF16
template class FfnLayer<__nv_bfloat16>;
#endif

}  // namespace fastertransformer
