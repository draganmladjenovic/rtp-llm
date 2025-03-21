import torch
import copy
from typing import List, Dict, Any

import torch.nn as nn
from transformers import PreTrainedTokenizerBase

from maga_transformer.config.gpt_init_model_parameters import GptInitModelParameters
from maga_transformer.async_decoder_engine.embedding.interface import EngineInputs, EngineOutputs
from maga_transformer.models.downstream_modules.custom_module import CustomModule, CustomHandler
from maga_transformer.models.downstream_modules.embedding.misc import EmbeddingRendererBase, hidden_combo_to_batch
from maga_transformer.models.downstream_modules.embedding.api_datatype import OpenAIEmbeddingRequest, \
    Usage, ALLEmbeddingResponseFormat, ALLEmbeddingResponse, EmbeddingResponseType, OpenAIEmbeddingResponse
    
class ALLEmbeddingModule(CustomModule):
    def __init__(self, config: GptInitModelParameters, tokenizer: PreTrainedTokenizerBase):
        super().__init__(config, tokenizer)
        self.renderer = ALLEmbeddingRenderer(config, tokenizer)
        self.handler = NormalHandler(config)


class ALLEmbeddingRenderer(EmbeddingRendererBase):
    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, ** kwargs)
        self.embedding_type = EmbeddingResponseType.DENSE
    
    async def render_response(self, request: OpenAIEmbeddingRequest, inputs: EngineInputs, outputs: EngineOutputs) -> Dict[str, Any]:
        usage = Usage(prompt_tokens=outputs.input_length, total_tokens=outputs.input_length)
        data: List[ALLEmbeddingResponseFormat] = []
        bias = 0
        for i in range(len(outputs.outputs)):
            data.append(ALLEmbeddingResponseFormat(
                object=self.embedding_type,
                embedding=outputs.outputs[i][:inputs.input_lengths[i]].tolist(),
                token_ids=inputs.token_ids[bias: bias + inputs.input_lengths[i]].tolist(),
                index=i))
            bias += inputs.input_lengths[i]
        return ALLEmbeddingResponse(data=data, usage=usage).model_dump()

    async def render_log_response(self, response: Dict[str, Any]):
        log_response = copy.copy(response)
        if 'data' in log_response:
            del log_response['data']
        return log_response


class NormalHandler(CustomHandler):
    def __init__(self, config: GptInitModelParameters):
        super().__init__(config)

    def forward(self, input_ids: torch.Tensor, hidden_states: torch.Tensor, input_lengths: torch.Tensor) -> torch.Tensor:
        return hidden_combo_to_batch(torch.nn.functional.normalize(hidden_states, dim=1), input_lengths)