#include "maga_transformer/cpp/dataclass/EngineInitParameter.h"
#include "src/fastertransformer/devices/DeviceFactory.h"
#include "src/fastertransformer/core/BufferHelper.h"
#include "src/fastertransformer/core/torch_utils/BufferTorchUtils.h"
#include "src/fastertransformer/models/W.h"
#include "src/fastertransformer/utils/py_utils/pybind_utils.h"
#include <memory>

using namespace std;
using namespace fastertransformer;

namespace rtp_llm {

ft::ConstBufferPtr WeightsConverter::CopyTensorToBufferPtr(const torch::Tensor& tensor) {
    auto buffer = torchTensor2Buffer(tensor);
    if (need_copy_) {
        auto new_buffer = device_->allocateBuffer({buffer->type(),
                                                   buffer->shape(),
                                                   AllocationType::DEVICE});
        device_->copy({*new_buffer, *buffer});
        return new_buffer;
    } else {
        return buffer;
    }
}

ft::ConstBufferPtr
WeightsConverter::mayFindBuffer(const ConstBufferPtrMap& map,
                                const std::string& key)
{
    auto it = map.find(key);
    if (it != map.end()) {
        return it->second;
    }
    return nullptr;
}

ft::LayerNormWeightsPtr
WeightsConverter::mayCreateLayerNormWeights(const ConstBufferPtrMap& map,
                                            const std::string& gamma_key,
                                            const std::string& beta_key)   
{
    if (map.count(gamma_key) > 0) {
        const auto layer_norm_weights = new LayerNormWeights();
        layer_norm_weights->gamma     = mayFindBuffer(map, gamma_key);
        layer_norm_weights->beta      = mayFindBuffer(map, beta_key);
        return unique_ptr<const LayerNormWeights>(layer_norm_weights);
    }
    return nullptr;
}

ft::DenseWeightsPtr
WeightsConverter::mayCreateDenseWeights(const ConstBufferPtrMap& map,
                                        const std::string& kernel_key,
                                        const std::string& bias_key,
                                        const std::string& scales_key)
{
    if (map.count(kernel_key) > 0) {
        const auto dense_weights = new DenseWeights();
        if (!bias_key.empty()) {
            dense_weights->bias = mayFindBuffer(map, bias_key);
        }
        if (map.count(scales_key) <= 0) {
            dense_weights->kernel = mayFindBuffer(map, kernel_key);
        } else {
            auto kernel = mayFindBuffer(map, kernel_key);
            auto scales = mayFindBuffer(map, scales_key);
            // construct qbuffer need kernel and scales has no ref.
            FT_LOG_DEBUG("load qbuffer weight [%s] ", scales_key.c_str());
            dense_weights->kernel = ConstBufferPtr(
                new ft::QBuffer(BufferPtr(new Buffer(kernel->where(),
                                                     kernel->type(),
                                                     kernel->shape(),
                                                     kernel->data())),
                                BufferPtr(new Buffer(scales->where(),
                                                     scales->type(),
                                                     scales->shape(),
                                                     scales->data())),
                                BufferPtr(new Buffer(scales->where(),
                                                     scales->type(),
                                                     {0},
                                                     nullptr))));
        }
        return unique_ptr<const DenseWeights>(dense_weights);
        
    }
    return nullptr;
}

ft::FfnLayerWeights
WeightsConverter::createFfnWeights(const ConstBufferPtrMap& map) {
    ft::FfnLayerWeights ffn_weights;
    ffn_weights.up_weight = mayCreateDenseWeights(map,
                                                  W::ffn_w3,
                                                  W::ffn_b3,
                                                  W::ffn_s3);

    ffn_weights.gate_weight = mayCreateDenseWeights(map,
                                                    W::ffn_w1,
                                                    W::ffn_b1,
                                                    W::ffn_s1);

    ffn_weights.down_weight = mayCreateDenseWeights(map,
                                                    W::ffn_w2,
                                                    W::ffn_b2,
                                                    W::ffn_s2);

    ffn_weights.dense_layernorm = mayCreateLayerNormWeights(map,
                                                            W::ffn_ln_gamma,
                                                            W::ffn_ln_beta);

    ffn_weights.moe_gating_weight = mayCreateDenseWeights(map,
                                                          W::moe_gate);

    return ffn_weights;
}

ft::AttentionLayerWeights
WeightsConverter::createAttentionWeights(const ConstBufferPtrMap& map) {
    ft::AttentionLayerWeights attention_weights;
    attention_weights.pre_attention_layernorm = mayCreateLayerNormWeights(map,
                                                                          W::pre_attn_ln_gamma,
                                                                          W::pre_attn_ln_beta);

    attention_weights.qkv_weight = mayCreateDenseWeights(map,
                                                         W::attn_qkv_w,
                                                         W::attn_qkv_b,
                                                         W::attn_qkv_s);
    
    attention_weights.attention_layernorm = mayCreateLayerNormWeights(map,
                                                                      W::attn_ln_gamma,
                                                                      W::attn_ln_beta);
    attention_weights.output_weight = mayCreateDenseWeights(map,
                                                            W::attn_o_w,
                                                            W::attn_o_b,
                                                            W::attn_o_s);

    return attention_weights;
}

std::unique_ptr<TensorMaps>
WeightsConverter::convertLayerWeights(py::object py_layer_weights) {
    TensorMaps tensor_layer_weights;
    auto layers_weights_vec = ft::convertPyObjectToVec(py_layer_weights);
    for (auto& layer_weights : layers_weights_vec) {
        TensorMap weights;
        for (auto& it : convertPyObjectToDict(layer_weights)) {
            weights.emplace(it.first, ft::convertPyObjectToTensor(it.second));
        }
        tensor_layer_weights.emplace_back(std::move(weights));
    }
    return std::make_unique<TensorMaps>(std::move(tensor_layer_weights));
}

std::unique_ptr<TensorMap>
WeightsConverter::convertGlobalWeight(py::object py_global_weight) {
    TensorMap global_weights;
    auto global_weights_dict = ft::convertPyObjectToDict(py_global_weight);
    for (auto& it : global_weights_dict) {
        global_weights.emplace(it.first, ft::convertPyObjectToTensor(it.second));
    }
    return std::make_unique<TensorMap>(std::move(global_weights));
}

std::unique_ptr<ConstBufferPtrMaps>
WeightsConverter::convertLayerWeights(std::unique_ptr<TensorMaps> tensor_layer_weights) {
    ConstBufferPtrMaps layer_weights;
    for (auto& layer_weight : *tensor_layer_weights) {
        ConstBufferPtrMap weights;
        for (auto& it : layer_weight) {
            weights.emplace(it.first, CopyTensorToBufferPtr(it.second));
        }
        layer_weights.emplace_back(std::move(weights));
    }
    return std::make_unique<ConstBufferPtrMaps>(std::move(layer_weights));
}

std::unique_ptr<ConstBufferPtrMap> 
WeightsConverter::convertGlobalWeight(std::unique_ptr<TensorMap> tensor_global_weight) {
    ConstBufferPtrMap global_weights;
    for (auto& it : *tensor_global_weight) {
        global_weights.emplace(it.first, CopyTensorToBufferPtr(it.second));
    }
    return std::make_unique<ConstBufferPtrMap>(std::move(global_weights));
}

std::unique_ptr<ft::Weights>
WeightsConverter::createGptWeights(py::object layer_weights,
                                   py::object global_weight)
{
    return std::move(createGptWeights(std::move(convertLayerWeights(layer_weights)),
                                      std::move(convertGlobalWeight(global_weight))));
}

std::unique_ptr<ft::Weights>
WeightsConverter::createGptWeights(std::unique_ptr<TensorMaps> layer_weights,
                                   std::unique_ptr<TensorMap>  global_weight)
{
    return std::move(createGptWeights(std::move(convertLayerWeights(std::move(layer_weights))),
                                      std::move(convertGlobalWeight(std::move(global_weight)))));
}

std::unique_ptr<ft::Weights>
WeightsConverter::createGptWeights(std::unique_ptr<ConstBufferPtrMaps> layer_weights,
                                   std::unique_ptr<ConstBufferPtrMap>  global_weight)
{
    auto        layers_weights = *layer_weights;
    ft::Weights gpt_weights;
    // make global weight
    gpt_weights.embedding = mayCreateDenseWeights(*global_weight,
                                                   W::embedding);
    gpt_weights.prefix_encoder_embedding = mayCreateDenseWeights(*global_weight,
                                                                  W::prefix_w);
    gpt_weights.pre_decoder_layernorm = mayCreateLayerNormWeights(*global_weight,
                                                                   W::pre_decoder_ln_gamma,
                                                                   W::pre_decoder_ln_beta);
    gpt_weights.position_encoding = mayCreateDenseWeights(*global_weight,
                                                           W::wpe);
    gpt_weights.token_type_embedding = mayCreateDenseWeights(*global_weight,
                                                              W::token_type_embedding);
    gpt_weights.final_layernorm = mayCreateLayerNormWeights(*global_weight,
                                                             W::final_ln_gamma,
                                                             W::final_ln_beta);
    gpt_weights.lm_head = mayCreateDenseWeights(*global_weight,
                                                 W::lm_head);

    for (auto& layer_weights : layers_weights) {
        ft::LayerWeights layer_ws;
        layer_ws.pre_layernorm = mayCreateLayerNormWeights(layer_weights,
                                                               W::pre_ln_gamma,
                                                               W::pre_ln_beta);

        layer_ws.post_ffn_layernorm = mayCreateLayerNormWeights(layer_weights,
                                                                    W::post_ffn_ln_gamma,
                                                                    W::post_ffn_ln_beta);

        layer_ws.post_layernorm = mayCreateLayerNormWeights(layer_weights,
                                                                W::post_ln_gamma,
                                                                W::post_ln_beta);

        layer_ws.self_attention_weights = createAttentionWeights(layer_weights);
        layer_ws.ffn_weights = createFfnWeights(layer_weights);
        gpt_weights.layers.emplace_back(std::move(layer_ws));
    }
    return std::make_unique<ft::Weights>(gpt_weights);
}




/////////////////////////////////deprected///////////////////////////


std::unique_ptr<ConstBufferPtrMaps> WeightsConverter::convertLayerWeights_(py::object py_layer_weights) {
    return convertLayerWeights(std::move(convertLayerWeights(py_layer_weights)));
}
    
std::unique_ptr<ConstBufferPtrMap>  WeightsConverter::convertGlobalWeight_(py::object py_global_weight) {
    return convertGlobalWeight(std::move(convertGlobalWeight(py_global_weight)));
}


}  // namespace rtp_llm
