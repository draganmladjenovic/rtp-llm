
import functools
import logging
from typing import List
import torch
from typing import List, Any
from einops import rearrange

from maga_transformer.utils.model_weight import (W, WeightInfo, ModelWeightInfo, LoRAModelWeightInfo,
                                                 ModelDeployWeightInfo, CkptWeightInfo, concat_1,
                                                 concat_0, identity, zeros, transpose, merge_qkv_lora_A,
                                                 merge_qkv_lora_B, shift_one, pad, merge_qkv_b)
from maga_transformer.utils.group_quant_weight_util import get_layer_group_quant_weight_info

# permute for sliced rotary
def permute(w, head_num, dim1, dim2):
    return w.view(head_num, dim1 // head_num // 2, 2, dim2).transpose(1, 2).reshape(dim1, dim2)

def merge_qkv(ts, hidden_size, head_num_kv, head_num):
    q, k, v = ts
    q = permute(q, head_num, hidden_size, hidden_size)
    k = permute(k, head_num_kv, head_num_kv * hidden_size // head_num, hidden_size)
    qkv_weight = torch.concat([q.T, k.T, v.T], dim=1).contiguous()
    return qkv_weight

def merge_qkv_hf(ts: List[torch.Tensor], hidden_size, head_num_kv, head_num):
    q, k, v = ts
    qkv_weight = torch.concat([q.T, k.T, v.T], dim=1).contiguous()
    return qkv_weight

def qkv_rerange(ts, hidden_size, head_num_kv, head_num):
    num_key_value_groups = int(head_num // head_num_kv)
    size_per_head = int(hidden_size / head_num)
    w = rearrange(ts[0].T, "q (h gs d) -> q h gs d",
                  gs=2 + num_key_value_groups,
                  d=size_per_head)
    wq = w[..., : num_key_value_groups, :].reshape(w.shape[0], -1)
    wk = w[..., -2, :].reshape(w.shape[0], -1)
    wv = w[..., -1, :].reshape(w.shape[0], -1)
    return torch.concat([wq, wk, wv], dim=1)

def qkv_transpose(ts, hidden_size):
    return ts[0].reshape(hidden_size, -1)

class DefaultWeightNames:
    WQ = 'layers.{i}.attention.wq.weight'
    WK = 'layers.{i}.attention.wk.weight'
    WV = 'layers.{i}.attention.wv.weight'
    WO = 'layers.{i}.attention.wo.weight'
    FFW1 = 'layers.{i}.feed_forward.w1.weight'
    FFW2 = 'layers.{i}.feed_forward.w2.weight'
    FFW3 = 'layers.{i}.feed_forward.w3.weight'
    ATTEN_NORM = 'layers.{i}.attention_norm.weight'
    FFN_NORM = 'layers.{i}.ffn_norm.weight'
    TOKEN_EMBEDDING = 'tok_embeddings.weight'
    NORM = 'norm.weight'
    OUTPUT = 'output.weight'

class HfWeightNames:
    WQ = 'model.layers.{i}.self_attn.q_proj.weight'
    WK = 'model.layers.{i}.self_attn.k_proj.weight'
    WV = 'model.layers.{i}.self_attn.v_proj.weight'
    WO = 'model.layers.{i}.self_attn.o_proj.weight'
    FFW1 = 'model.layers.{i}.mlp.gate_proj.weight'
    FFW2 = 'model.layers.{i}.mlp.down_proj.weight'
    FFW3 = 'model.layers.{i}.mlp.up_proj.weight'
    ATTEN_NORM = 'model.layers.{i}.input_layernorm.weight'
    FFN_NORM = 'model.layers.{i}.post_attention_layernorm.weight'
    TOKEN_EMBEDDING = 'model.embed_tokens.weight'
    NORM = 'model.norm.weight'
    OUTPUT = 'lm_head.weight'

class SQWeightNames(HfWeightNames):
    W_QKV = 'model.layers.{i}.attention.query_key_value.weight.int8.col'
    W_QKV_S = 'model.layers.{i}.attention.query_key_value.scale_w_quant_orig.col'
    WO = 'model.layers.{i}.attention.dense.weight.int8.col'
    WO_S = 'model.layers.{i}.attention.dense.scale_w_quant_orig.col'
    FFW1 = 'model.layers.{i}.mlp.fc.weight.int8.col'
    FFW1_S = 'model.layers.{i}.mlp.fc.scale_w_quant_orig.col'
    FFW2 = 'model.layers.{i}.mlp.proj.weight.int8.col'
    FFW2_S = 'model.layers.{i}.mlp.proj.scale_w_quant_orig.col'
    FFW3 = 'model.layers.{i}.mlp.gate.weight.int8.col'
    FFW3_S = 'model.layers.{i}.mlp.gate.scale_w_quant_orig.col'
    FFNW2_Smoother = 'model.layers.{i}.mlp.proj.smoother'
    WO_Smoother = 'model.layers.{i}.attention.dense.smoother'
    FFN_NORM = 'model.layers.{i}.post_layernorm.weight'

class YiWeightNames(HfWeightNames):
    ATTEN_NORM = 'model.layers.{i}.ln1.weight'
    FFN_NORM = 'model.layers.{i}.ln2.weight'

class BaichuanWeightNames(HfWeightNames):
    W_QKV = 'model.layers.{i}.self_attn.W_pack.weight'

class InternlmWeightNames(HfWeightNames):
    BQ = 'model.layers.{i}.self_attn.q_proj.bias'
    BK = 'model.layers.{i}.self_attn.k_proj.bias'
    BV = 'model.layers.{i}.self_attn.v_proj.bias'
    BO = 'model.layers.{i}.self_attn.o_proj.bias'

class Internlm2WeightNames:
    W_QKV = 'model.layers.{i}.attention.wqkv.weight'
    WO = 'model.layers.{i}.attention.wo.weight'
    FFW1 = 'model.layers.{i}.feed_forward.w1.weight'
    FFW2 = 'model.layers.{i}.feed_forward.w2.weight'
    FFW3 = 'model.layers.{i}.feed_forward.w3.weight'
    ATTEN_NORM = 'model.layers.{i}.attention_norm.weight'
    FFN_NORM = 'model.layers.{i}.ffn_norm.weight'
    TOKEN_EMBEDDING = 'model.tok_embeddings.weight'
    NORM = 'model.norm.weight'
    OUTPUT = 'output.weight'

class GemmaWeightNames(HfWeightNames):
    OUTPUT = 'model.embed_tokens.weight'

class CohereWeightNames:
    WQ = 'model.layers.{i}.self_attn.q_proj.weight'
    WK = 'model.layers.{i}.self_attn.k_proj.weight'
    WV = 'model.layers.{i}.self_attn.v_proj.weight'
    WO = 'model.layers.{i}.self_attn.o_proj.weight'
    FFW1 = 'model.layers.{i}.mlp.gate_proj.weight'
    FFW2 = 'model.layers.{i}.mlp.down_proj.weight'
    FFW3 = 'model.layers.{i}.mlp.up_proj.weight'
    ATTEN_NORM = 'model.layers.{i}.input_layernorm.weight'
    Q_NORM = 'model.layers.{i}.self_attn.q_norm.weight'
    K_NORM = 'model.layers.{i}.self_attn.k_norm.weight'
    NORM = 'model.norm.weight'
    TOKEN_EMBEDDING = 'model.embed_tokens.weight'

class LlamaWeightInfo(ModelDeployWeightInfo):
    def __init__(self, config, tp_size, tp_rank):
        super().__init__(config, tp_size, tp_rank)
        self._names = None
        self._merge_qkv = None
        self._merge_qkv_b = None

    def _process_meta(self, meta_dicts, weight_keys):
        if self._quant_algo.isSmoothQuant():
            logging.info('load hf llama smooth quant weight')
            self._names = SQWeightNames
        elif Internlm2WeightNames.W_QKV.format(i='0') in weight_keys:
            logging.info('load internlm2 style weight')
            self._names = Internlm2WeightNames
            self._merge_qkv = qkv_rerange
        elif YiWeightNames.FFN_NORM.format(i='0') in weight_keys:
            logging.info('load Yi style weight')
            self._names = YiWeightNames
            self._merge_qkv = merge_qkv_hf
        elif BaichuanWeightNames.W_QKV.format(i='0') in weight_keys:
            logging.info('load baichuan style weight')
            self._names = BaichuanWeightNames
            self._merge_qkv = None
        elif InternlmWeightNames.BQ.format(i='0') in weight_keys:
            logging.info('load internlm style weight')
            self._names = InternlmWeightNames
            self._merge_qkv = merge_qkv_hf
            self._merge_qkv_b = merge_qkv_b
        elif DefaultWeightNames.OUTPUT in weight_keys:
            logging.info('load default llama1 style weight')
            self._names = DefaultWeightNames
            self._merge_qkv = merge_qkv
        elif HfWeightNames.OUTPUT in weight_keys:
            logging.info('load hf llama1 style weight')
            self._names = HfWeightNames
            self._merge_qkv = merge_qkv_hf
        elif HfWeightNames.FFN_NORM.format(i='0') not in weight_keys:
            logging.info('load cohere style weight')
            self._names = CohereWeightNames
            self._merge_qkv = merge_qkv_hf
        else:
            raise Exception('unknown weights format')

    def _get_weight_info(self):
        weights = [
            WeightInfo(W.embedding, [CkptWeightInfo(self._names.TOKEN_EMBEDDING, concat_1)], identity),
            WeightInfo(W.final_ln_gamma, [CkptWeightInfo(self._names.NORM, identity)], identity),
            WeightInfo(W.final_ln_beta, [], functools.partial(zeros, shape=[self._hidden_size])),
        ]
        if self._names == CohereWeightNames:
            weights.append(WeightInfo(W.lm_head, [CkptWeightInfo(self._names.TOKEN_EMBEDDING, identity)], identity))
        else:
            weights.append(WeightInfo(W.lm_head, [CkptWeightInfo(self._names.OUTPUT, concat_0)], identity))

        layer_weights = [
            WeightInfo(W.pre_ln_gamma, [CkptWeightInfo(self._names.ATTEN_NORM, identity)], identity),

            WeightInfo(W.post_ln_gamma, [CkptWeightInfo(self._names.FFN_NORM, identity)], identity),
        ]
        if self._names == SQWeightNames:
            layer_weights.extend([
                WeightInfo(W.attn_o_w, [CkptWeightInfo(self._names.WO, identity)], transpose),
                WeightInfo(W.attn_o_s, [CkptWeightInfo(self._names.WO_S, identity)], identity),

                WeightInfo(W.ffn_w1, [CkptWeightInfo(self._names.FFW1, identity)], transpose),
                WeightInfo(W.ffn_s1, [CkptWeightInfo(self._names.FFW1_S, identity)], identity),

                WeightInfo(W.ffn_w3, [CkptWeightInfo(self._names.FFW3, identity)], transpose),
                WeightInfo(W.ffn_s3, [CkptWeightInfo(self._names.FFW3_S, identity)], identity),

                WeightInfo(W.ffn_w2, [CkptWeightInfo(self._names.FFW2, identity)], transpose),
                WeightInfo(W.ffn_s2, [CkptWeightInfo(self._names.FFW2_S, identity)], identity),

                WeightInfo(W.attn_qkv_w, [CkptWeightInfo(self._names.W_QKV,
                                                         functools.partial(qkv_transpose, hidden_size=self._hidden_size))],
                           transpose),
                WeightInfo(W.attn_qkv_s, [CkptWeightInfo(self._names.W_QKV_S, identity)],
                           identity),

                WeightInfo(W.attn_o_smoother, [CkptWeightInfo(self._names.WO_Smoother, identity)], identity),
                WeightInfo(W.ffn_smoother, [CkptWeightInfo(self._names.FFNW2_Smoother, identity)], identity),
            ]
            )
        else:
            layer_weights.extend([
                WeightInfo(W.attn_o_w, [CkptWeightInfo(self._names.WO, concat_1)], transpose),

                WeightInfo(W.ffn_w1, [CkptWeightInfo(self._names.FFW1, concat_0)], transpose),

                WeightInfo(W.ffn_w3, [CkptWeightInfo(self._names.FFW3, concat_0)], transpose),

                WeightInfo(W.ffn_w2, [CkptWeightInfo(self._names.FFW2, concat_1)], transpose),
            ]
            )

            if self._names == CohereWeightNames:
                layer_weights.append(WeightInfo(W.qk_ln_gamma,
                                                [CkptWeightInfo(self._names.Q_NORM, identity),
                                                 CkptWeightInfo(self._names.K_NORM, identity)], concat_0))
            else:
                layer_weights.append(WeightInfo(W.post_ln_gamma, [CkptWeightInfo(self._names.FFN_NORM, identity)], identity))

            if self._names == InternlmWeightNames:
                layer_weights.append(
                    WeightInfo(W.attn_qkv_b,
                                        [CkptWeightInfo(self._names.BQ, identity),
                                        CkptWeightInfo(self._names.BK, identity),
                                        CkptWeightInfo(self._names.BV, identity)],
                                        functools.partial(self._merge_qkv_b))),
                layer_weights.append(WeightInfo(W.attn_o_b, [CkptWeightInfo(self._names.BO, identity)], identity))

            if self._merge_qkv is not None:
                if hasattr(self._names, 'W_QKV'):
                    infos = [CkptWeightInfo(self._names.W_QKV, identity)]
                else:
                    infos = [CkptWeightInfo(self._names.WQ, concat_0),
                             CkptWeightInfo(self._names.WK, concat_0),
                             CkptWeightInfo(self._names.WV, concat_0)]
                layer_weights.append(
                    WeightInfo(W.attn_qkv_w, infos,
                               functools.partial(self._merge_qkv,
                                                 hidden_size=self._hidden_size,
                                                 head_num_kv=self._head_num_kv,
                                                 head_num=self._head_num)))
            else:
                layer_weights.append(
                    WeightInfo(W.attn_qkv_w, [CkptWeightInfo(self._names.W_QKV, identity)], transpose))

        if self._quant_algo.isGptq() or self._quant_algo.isAwq():
            layer_weights = get_layer_group_quant_weight_info(layer_weights, self._quant_algo, self._inter_padding_size)

        lora_base_name = "base_model.model.{}.{}.weight"
        lora_weights = []
        for lora_name in ['lora_A', 'lora_B']:
            lora_weights.append(
                WeightInfo(W.attn_o_w + "." + lora_name, [CkptWeightInfo(lora_base_name.format(HfWeightNames.WO[:-len(".weight")], lora_name), concat_1)], transpose))
            lora_weights.append(
                WeightInfo(W.ffn_w1 + "." + lora_name, [CkptWeightInfo(lora_base_name.format(HfWeightNames.FFW1[:-len(".weight")], lora_name), concat_0)], transpose))

            lora_weights.append(
                WeightInfo(W.ffn_w2 + "." + lora_name, [CkptWeightInfo(lora_base_name.format(HfWeightNames.FFW2[:-len(".weight")], lora_name), concat_1)], transpose))
            lora_weights.append(
                WeightInfo(W.ffn_w3 + "." + lora_name, [CkptWeightInfo(lora_base_name.format(HfWeightNames.FFW3[:-len(".weight")], lora_name), concat_0)], transpose))

        lora_weights.append(WeightInfo(W.attn_qkv_w + "." + 'lora_A',
                        [CkptWeightInfo(lora_base_name.format(HfWeightNames.WQ[:-len(".weight")], 'lora_A'), identity),
                        CkptWeightInfo(lora_base_name.format(HfWeightNames.WK[:-len(".weight")], 'lora_A'), identity),
                        CkptWeightInfo(lora_base_name.format(HfWeightNames.WV[:-len(".weight")], 'lora_A'), identity)],
                        functools.partial(merge_qkv_lora_A)))

        lora_weights.append(WeightInfo(W.attn_qkv_w + "." + 'lora_B',
                        [CkptWeightInfo(lora_base_name.format(HfWeightNames.WQ[:-len(".weight")], 'lora_B'), identity),
                        CkptWeightInfo(lora_base_name.format(HfWeightNames.WK[:-len(".weight")], 'lora_B'), identity),
                        CkptWeightInfo(lora_base_name.format(HfWeightNames.WV[:-len(".weight")], 'lora_B'), identity)],
                        functools.partial(merge_qkv_lora_B)))

        return ModelWeightInfo(layer_weights=layer_weights, weights=weights,
                               tp_strategy=self._get_gpt_style_tp_strategy())
class GemmaWeightInfo(LlamaWeightInfo):
    def __init__(self, config, tp_size, tp_rank):
        super().__init__(config, tp_size, tp_rank)

    def _process_meta(self, meta_dicts, weight_keys):
        logging.info('load gemma style weight')
        self._names = GemmaWeightNames
        self._merge_qkv = merge_qkv_hf

    def _check_layernorm(self, weight):
        if isinstance(weight, list):
            return
        if ("layernorm" in weight.name) and ("gamma" in weight.name):
            logging.info(f"gemma adds shift 1 to {weight.name}")
            weight.process_fun = shift_one

    def _get_weight_info(self):
        weight_info = super()._get_weight_info()
        for layer_weight in weight_info.layer_weights:
            self._check_layernorm(layer_weight)
        for weight in weight_info.weights:
            self._check_layernorm(weight)
        return weight_info
