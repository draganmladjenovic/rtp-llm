/*
 * Copyright (c) 2020-2023, NVIDIA CORPORATION.  All rights reserved.
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
#pragma once

#include "src/fastertransformer/kernels/kv_cache_utils.h"
#include "src/fastertransformer/core/Tensor.h"
#include "src/fastertransformer/utils/compiler_config.h"

namespace fastertransformer {

template<typename T>
void invokeAddQKVBiasIA3Transpose(T*           q_buf,
                                  T*           k_buf,
                                  T*           v_buf,
                                  T*           Q,
                                  const T*     bias_Q,
                                  T*           K,
                                  const T*     bias_K,
                                  T*           V,
                                  const T*     bias_V,
                                  const int    batch_size,
                                  const int    seq_len,
                                  const int    head_num,
                                  const int    size_per_head,
                                  const int*   ia3_tasks,
                                  const T*     ia3_key_weights,
                                  const T*     ia3_value_weights,
                                  cudaStream_t stream);

template<typename T, typename T_IN>
struct MaskedSoftmaxParam {
    // Common parameters.
    T*          attention_score = nullptr;  // (batch_size, head_num, q_length, k_length)
    const T_IN* qk              = nullptr;  // (batch_size, head_num, q_length, k_length)
    const T*    attention_mask  = nullptr;  // (batch_size, q_length, k_length)
    int         batch_size      = 0;
    int         q_length        = 0;
    int         k_length        = 0;
    int         num_heads       = 0;
    T           qk_scale        = T(0.0f);

    // Optional parameters that depend on the type of attention.
    // The slopes of the linear position bias of ALiBi.
    const T* linear_bias_slopes = nullptr;  // (head_num,), optional
};

enum class KvCacheDataType : int8_t {
    BASE = 0,
    INT8 = 1,
    FP8  = 2
};

template<typename T, typename T_IN>
void invokeMaskedSoftmax(MaskedSoftmaxParam<T, T_IN>& param, cudaStream_t stream);

template<typename T>
void invokeTransposeQKV(T*           dst,
                        T*           src,
                        const int    batch_size,
                        const int    seq_len,
                        const int    head_num,
                        const int    size_per_head,
                        const float* scale,
                        const int    int8_mode,
                        cudaStream_t stream);

template<typename T>
void invokeAddQKVBiasIA3RebuildPadding(T*           Q,
                                       const T*     bias_Q,
                                       T*           K,
                                       const T*     bias_K,
                                       T*           V,
                                       const T*     bias_V,
                                       T*           q_buf,
                                       T*           k_buf,
                                       T*           v_buf,
                                       const int    batch_size,
                                       const int    seq_len,
                                       const int    head_num,
                                       const int    size_per_head,
                                       const int    valid_word_num,
                                       const int*   mask_offset,
                                       const int*   ia3_tasks,
                                       const T*     ia3_key_weights,
                                       const T*     ia3_value_weights,
                                       cudaStream_t stream);

template<typename T>
void invokeTransposeAttentionOutRemovePadding(T*           src,
                                              T*           dst,
                                              const int    valid_word_num,
                                              const int    batch_size,
                                              const int    seq_len,
                                              const int    head_num,
                                              const int    size_per_head,
                                              const int*   mask_offset,
                                              const float* scale,
                                              const int    int8_mode,
                                              cudaStream_t stream);

struct PrefixPromptBatchWeightsParam {
    const int*              d_prefix_prompt_lengths  = nullptr;
    const int               max_prefix_prompt_length = 0;
    bool                    count_length             = false;
    KVBlockArray            kv_block_array           = KVBlockArray(0, 0, 0, 0);
};

void float_to_half(const void* input, void* output, int size);
void half_to_float(const void* input, void* output, const int num_elements);

template<typename T>
void invokeSplitQKV(T*           q_buf,
                    T*           k_buf,
                    T*           v_buf,
                    T*           QKV,
                    const int    token_num,
                    const int    head_num,
                    const int    head_num_kv,
                    const int    size_per_head,
                    cudaStream_t stream);

template<typename T>
void invokeAddFusedQKVBiasTranspose(T*                               q_buf,
                                    T*                               k_buf,
                                    T*                               v_buf,
                                    PrefixPromptBatchWeightsParam*   param,
                                    T*                               QKV,
                                    const int*                       position_ids,
                                    const T*                         qkv_bias,
                                    const int*                       padding_offset,
                                    const int*                       cu_seqlens,
                                    const int                        batch_size,
                                    const int                        seq_len,
                                    const int                        token_num,
                                    const int                        head_num,
                                    const int                        head_num_kv,
                                    const int                        size_per_head,
                                    const int                        rotary_embedding_dim,
                                    const int                        rotary_embedding_style,
                                    const float                      rotary_embedding_base,
                                    const float                      rotary_embedding_scale,
                                    const int                        dynamic_embedding_max_pos,
                                    const int                        org_embedding_max_pos,
                                    const int                        base_scale,
                                    const int                        logn_seq_len,
                                    const bool                       use_logn_attention,
                                    const float*                     scale,
                                    const int                        int8_mode,
                                    const bool                       use_paged_fmha,
                                    cudaStream_t                     stream);
template<typename T>
void invokeTranspose4d(T*           dst,
                       T*           src,
                       const int    local_batch_size,
                       const int    seq_len,
                       const int    size_per_head,
                       const int    local_hidden_units,
                       const int    local_head_num,
                       const int    batch_size,
                       const int    ite,
                       cudaStream_t stream);

template<typename T, typename KVCacheBuffer>
void invokeTranspose4dBatchMajor(const T*              kSrc,
                                 const T*              vSrc,
                                 KVCacheBuffer&        kvTable,
                                 const int             localBatchSize,
                                 const int             seqLen,
                                 const int             sizePerHead,
                                 const int             localHeadNum,
                                 const KvCacheDataType cache_type,
                                 const float*          kvScaleOrigQuant,
                                 const int*            sequence_lengths,
                                 const int*            prefix_lengths,
                                 cudaStream_t          stream);

template<typename T>
void invokeAddRelativeAttentionBias(T*           qk_buf,
                                    const T*     relative_attention_bias,
                                    const int    batch_size,
                                    const int    head_num,
                                    const int    seq_len,
                                    cudaStream_t stream);

template<typename T>
void invokeAddHead3SizeQKVBias(const T*     mm_qkv,
                               const T*     bias_qkv,
                               T*           q_buf_,
                               T*           k_buf_,
                               T*           v_buf_,
                               const int    batch,
                               const int    window_num,
                               const int    window_len,
                               const int    head_num,
                               const int    size_per_head,
                               cudaStream_t stream);

template<typename T>
void invokeMaskedSoftMaxWithRelPosBias(T*           qk_buf,
                                       const T*     attn_mask,
                                       const T*     relative_pos_bias,
                                       const int    batch_size,
                                       const int    num_head,
                                       const int    window_num,
                                       const int    window_len,
                                       const float  qk_scale,
                                       cudaStream_t stream);

template<typename T>
void invokeTransposeAttentions(Tensor& attentions_out, const Tensor& attentions_in, cudaStream_t stream = 0);

}  // namespace fastertransformer
