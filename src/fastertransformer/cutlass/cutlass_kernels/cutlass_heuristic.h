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

#include "src/fastertransformer/cutlass/cutlass_kernels/gemm_configs.h"
#include "src/fastertransformer/cuda/cuda_utils.h"

namespace tensorrt_llm
{
namespace kernels
{
namespace cutlass_kernels
{

std::vector<tensorrt_llm::cutlass_extensions::CutlassGemmConfig> get_candidate_configs(int sm,
    const bool is_weight_only, const bool simt_configs_only, const bool int8_configs_only = false,
    const int max_split_k = 1);

std::vector<tensorrt_llm::cutlass_extensions::CutlassGemmConfig> get_valid_config_from_occupancies(
    const std::vector<tensorrt_llm::cutlass_extensions::CutlassGemmConfig>& candidate_configs,
    const std::vector<int>& occupancies);

tensorrt_llm::cutlass_extensions::CutlassGemmConfig estimate_best_config_from_occupancies(
    const std::vector<tensorrt_llm::cutlass_extensions::CutlassGemmConfig>& candidate_configs,
    const std::vector<int>& occupancies, const int64_t m, const int64_t n, const int64_t k,
    const int multi_processor_count);

struct TileConfig{
    int block_m;
    int block_n;
    int block_k;
    int warp_m;
    int warp_n;
    int warp_k;
};

void print_config(tensorrt_llm::cutlass_extensions::CutlassGemmConfig config);
bool is_valid_split_k_factor(const int64_t m, const int64_t n, const int64_t k, const tensorrt_llm::cutlass_extensions::CutlassGemmConfig gemm_config,
    const size_t workspace_bytes, const bool is_weight_only);
TileConfig get_tile_config_from_config(tensorrt_llm::cutlass_extensions::CutlassTileConfig tile_config);

} // namespace cutlass_kernels
} // namespace kernels
} // namespace tensorrt_llm
