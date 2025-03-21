/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (c) 2021, NAVER Corp.  Authored by CLOVA.
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

#include <float.h>
#include <algorithm>

#include "src/fastertransformer/kernels/sampling_topk_kernels.h"
#include "src/fastertransformer/kernels/sampling_topp_kernels.h"
#include "src/fastertransformer/kernels/sampling_penalty_kernels.h"
#include "src/fastertransformer/layers/sampling_layers/TopKSamplingLayer.h"
#include "src/fastertransformer/utils/logger.h"
#include "src/fastertransformer/cuda/memory_utils.h"

namespace fastertransformer {

template<typename T>
void TopKSamplingLayer<T>::allocateBuffer()
{
    FT_CHECK(false);
}

template<typename T>
void TopKSamplingLayer<T>::allocateBuffer(size_t batch_size, Tensor top_k, Tensor top_p)
{
    FT_LOG_DEBUG(__PRETTY_FUNCTION__);
    BaseSamplingLayer<T>::allocateBuffer(batch_size, top_k, top_p);
    uint max_top_k = top_k.size() > 0 ? top_k.max<uint>() : 1;
    if (max_top_k == 0) {
        // for safety. TopKSamplingLayer handles a case of top_k=0 and top_p=0 as
        // a greedy decode, i.e. top_k=1, although such case has max_top_k=0.
        max_top_k = 1;
    }
    invokeTopKSampling<T>(nullptr,
                          sampling_workspace_size_,
                          nullptr,
                          nullptr,
                          nullptr,
                          nullptr,
                          nullptr,
                          nullptr,
                          nullptr,
                          nullptr,
                          nullptr,
                          max_top_k,
                          1.0f,
                          vocab_size_padded_,
                          nullptr,
                          stream_,
                          batch_size,
                          skip_decode_buf_);
    sampling_workspace_ = allocator_->reMalloc(sampling_workspace_, sampling_workspace_size_);
    runtime_top_k_buf_ =
        reinterpret_cast<uint*>(allocator_->reMalloc(runtime_top_k_buf_, sizeof(uint) * batch_size));
    runtime_top_p_buf_ =
        reinterpret_cast<float*>(allocator_->reMalloc(runtime_top_p_buf_, sizeof(float) * batch_size));
    is_allocate_buffer_ = true;
}

template<typename T>
void TopKSamplingLayer<T>::freeBuffer()
{
    FT_LOG_DEBUG(__PRETTY_FUNCTION__);
    if (is_allocate_buffer_) {
        allocator_->free((void**)(&sampling_workspace_));
        allocator_->free((void**)(&runtime_top_k_buf_));
        allocator_->free((void**)(&runtime_top_p_buf_));
    }
    BaseSamplingLayer<T>::freeBuffer();
    is_allocate_buffer_ = false;
}

template<typename T>
void TopKSamplingLayer<T>::setup(const size_t batch_size, const size_t beam_width, TensorMap* runtime_args)
{
    // Setup runtime topk and topp arguments.
    //
    // runtime_args:
    //     runtime_top_k [1] or [batch_size] on cpu, optional, uint.
    //     runtime_top_p [1] or [batch_size] on cpu, optional, float.
    //     temperature [1] or [batch_size] on cpu, optional
    //     repetition_penalty [1] or [batch_size] on cpu, optional
    FT_LOG_DEBUG(__PRETTY_FUNCTION__);
    BaseSamplingLayer<T>::setup(batch_size, beam_width, runtime_args);

    uint         tmp_top_k     = 0;
    const Tensor runtime_top_k = runtime_args->isExist("runtime_top_k") ?
                                     runtime_args->at("runtime_top_k") :
                                     Tensor(MEMORY_CPU, TYPE_UINT32, {1}, &tmp_top_k);
    const Tensor runtime_top_p = runtime_args->isExist("runtime_top_p") ? runtime_args->at("runtime_top_p") : Tensor();
    const size_t runtime_top_k_size = runtime_top_k.size();
    const size_t runtime_top_p_size = runtime_top_p.size();

    uint  top_k = runtime_top_k.max<uint>();
    float top_p = runtime_top_p_size == 0 ? 0.0f : runtime_top_p.getVal<float>();

    if (runtime_top_k_size > 1) {
        FT_CHECK_WITH_INFO(
            runtime_top_k.size() == batch_size,
            fmtstr("runtime_top_k.size() (%d) == batch_size (%d) is not satisfied!", runtime_top_k.size(), batch_size));
        cudaAutoCpy(runtime_top_k_buf_, runtime_top_k.getPtr<uint>(), batch_size, stream_);
    }
    if (runtime_top_p_size > 1) {
        FT_CHECK_WITH_INFO(
            runtime_top_p.size() == batch_size,
            fmtstr("runtime_top_p.size() (%d) == batch_size (%d) is not satisfied!", runtime_top_p.size(), batch_size));
        cudaAutoCpy(runtime_top_p_buf_, runtime_top_p.getPtr<float>(), batch_size, stream_);
    }

    invokeSetupTopKRuntimeArgs(batch_size,
                                top_k,
                                runtime_top_k_buf_,
                                runtime_top_k_size,
                                top_p,
                                runtime_top_p_buf_,
                                runtime_top_p_size,
                                skip_decode_buf_,
                                stream_);

    cudaAutoCpy(skip_decode_, skip_decode_buf_, batch_size, stream_);
    uint* runtime_top_ks = new uint[batch_size];
    cudaAutoCpy(runtime_top_ks, runtime_top_k_buf_, batch_size, stream_);
    runtime_max_top_k_ = static_cast<int>(*std::max_element(runtime_top_ks, runtime_top_ks + batch_size));
    delete[] runtime_top_ks;
}

template<typename T>
void TopKSamplingLayer<T>::runSampling(TensorMap* output_tensors, TensorMap* input_tensors)
{
    // input_tensors:
    //      logits [local_batch_size, vocab_size_padded]
    //      embedding_bias [vocab_size_padded], optional
    //      step [1] on cpu
    //      max_input_length [1] on cpu
    //      input_lengths [local_batch_size], optional
    //      ite [1] on cpu

    // output_tensors:
    //      output_ids [max_seq_len, batch_size]
    //      finished [local_batch_size], optional
    //      sequence_length [local_batch_size], optional
    //      cum_log_probs [batch_size], must be float*, optional
    //          The cumultative log probability of generated tokens.
    //      output_log_probs [local_batch_size], must be float*, optional
    //          The log probs at the current step.

    FT_LOG_DEBUG(__PRETTY_FUNCTION__);
    FT_CHECK(input_tensors->size() >= 4);
    FT_CHECK(output_tensors->size() >= 1);

    const int batch_size       = output_tensors->at("output_ids").shape()[1];
    const int local_batch_size = input_tensors->at("logits").shape()[0];
    const int ite              = input_tensors->at("ite").getVal<int>();
    const int step             = input_tensors->at("step").getVal<int>();

    // in case of skip any, the logit value is already copied and processed.
    T* logits = !skip_any_ ? input_tensors->at("logits").getPtr<T>() : runtime_logits_buf_;

    invokeAddBiasEndMask(logits,
                         (T*)(nullptr),
                         input_tensors->at("end_id").getPtr<const int>(),
                         output_tensors->at("finished", Tensor{MEMORY_GPU, TYPE_INVALID, {}, nullptr}).getPtr<bool>(),
                         local_batch_size,
                         vocab_size_,
                         vocab_size_padded_,
                         stream_);
    sync_check_cuda_error();

    float* cum_log_probs =
        output_tensors->isExist("cum_log_probs") ? output_tensors->at("cum_log_probs").getPtr<float>() : nullptr;
    float* output_log_probs =
        output_tensors->isExist("output_log_probs") ? output_tensors->at("output_log_probs").getPtr<float>() : nullptr;
    float* index_log_probs =
        output_tensors->isExist("index_log_probs") ? output_tensors->at("index_log_probs").getPtr<float>() : nullptr;
    int* token_id_for_index_prob =
        output_tensors->isExist("token_id_for_index_prob") ? output_tensors->at("token_id_for_index_prob").getPtr<int>() : nullptr;

    invokeBatchTopKSampling(
        sampling_workspace_,
        sampling_workspace_size_,
        logits,
        output_tensors->at("output_ids").getPtrWithOffset<int>(step * batch_size + ite * local_batch_size),
        output_tensors->at("sequence_length", Tensor{MEMORY_GPU, TYPE_INVALID, {}, nullptr}).getPtr<int>(),
        output_tensors->at("finished", Tensor{MEMORY_GPU, TYPE_INVALID, {}, nullptr}).getPtr<bool>(),
        cum_log_probs,
        output_log_probs,
        index_log_probs,
        token_id_for_index_prob,
        curandstate_buf_ + ite * local_batch_size,
        (int)runtime_max_top_k_,  // useless because runtime_top_k_buf_ is never nullptr. Keep for legacy.
        (int*)(runtime_top_k_buf_ + ite * local_batch_size),
        1.0f,  // useless because runtime_top_p_buf_ is never nullptr. Keep for legacy.
        runtime_top_p_buf_ + ite * local_batch_size,
        vocab_size_padded_,
        input_tensors->at("end_id").getPtr<int>(),
        stream_,
        local_batch_size,
        skip_decode_buf_ + ite * local_batch_size);
    sync_check_cuda_error();
}

template<typename T>
TopKSamplingLayer<T>::TopKSamplingLayer(size_t             max_batch_size,
                                        size_t             vocab_size,
                                        size_t             vocab_size_padded,
                                        int                end_id,
                                        size_t             top_k,
                                        unsigned long long random_seed,
                                        float              temperature,
                                        float              len_penalty,
                                        float              repetition_penalty,
                                        cudaStream_t       stream,
                                        cublasMMWrapper*   cublas_wrapper,
                                        IAllocator*        allocator,
                                        bool               is_free_buffer_after_forward):
    BaseSamplingLayer<T>(max_batch_size,
                         vocab_size,
                         vocab_size_padded,
                         end_id,
                         top_k,
                         0.0f,
                         random_seed,
                         temperature,
                         len_penalty,
                         repetition_penalty,
                         stream,
                         cublas_wrapper,
                         allocator,
                         is_free_buffer_after_forward,
                         nullptr)
{
}

template<typename T>
TopKSamplingLayer<T>::TopKSamplingLayer(TopKSamplingLayer<T> const& top_k_sampling_layer):
    BaseSamplingLayer<T>(top_k_sampling_layer)
{
}

template<typename T>
TopKSamplingLayer<T>::~TopKSamplingLayer()
{
    FT_LOG_DEBUG(__PRETTY_FUNCTION__);
    freeBuffer();
}

template class TopKSamplingLayer<float>;
template class TopKSamplingLayer<half>;

}  // namespace fastertransformer
