#pragma once

#include "absl/status/statusor.h"
#include "maga_transformer/cpp/dataclass/GenerateStream.h"
#include "maga_transformer/cpp/models/GptModel.h"
#include "src/fastertransformer/devices/DeviceBase.h"
#include <memory>

namespace rtp_llm {

class Executor {
public:
    Executor(ft::DeviceBase* device): device_(device){};
    virtual absl::Status
    addLoRA(const int64_t                                                           lora_id,
            const std::vector<std::unordered_map<std::string, ft::ConstBufferPtr>>& lora_a_weights,
            const std::vector<std::unordered_map<std::string, ft::ConstBufferPtr>>& lora_b_weights) = 0;

    virtual absl::Status removeLoRA(const int64_t lora_id)                    = 0;
    virtual absl::Status process(const std::list<GenerateStreamPtr>& streams) = 0;

    static GptModelDescription genModelDescription(const ft::GptInitParameter& params) {
        ft::RopeConfig       rope_config{(ft::RopeType)params.rotary_embedding_style_,
                                   (size_t)params.rotary_embedding_dim_,
                                   (size_t)params.rotary_embedding_base_,
                                   (float)params.rotary_embedding_scale_,
                                   (int)params.dynamic_embedding_max_pos_,
                                   (float)params.base_scale_,
                                   (bool)params.use_logn_attn_,
                                   (int)params.logn_seq_len_};
        ft::AttentionConfigs attention_config{
            params.head_num_ > 1 ? (size_t)params.head_num_ / params.tp_size_ : 1,
            params.head_num_kv_ > 1 ? (size_t)params.head_num_kv_ / params.tp_size_ : 1,
            (size_t)params.size_per_head_,
            rope_config,
            (size_t)params.seq_size_per_block_,
            params.is_causal_ ? fastertransformer::AttentionMaskType::causalMask :
                                fastertransformer::AttentionMaskType::noMask};
        return {attention_config,
                ft::getActivationType(params.activation_type_str_),
                ft::getNormType(params.norm_type_str_),
                params.layernorm_eps_,
                (size_t)params.vocab_size_,
                params.layernorm_type_ == ft::LayerNormType::post_layernorm,
                params.input_embedding_scalar_};
    }

    virtual ~Executor(){};

public:
    ft::DeviceBase* device_;
};

}  // namespace rtp_llm
