#pragma once

#include "src/fastertransformer/devices/DeviceBase.h"
#include "src/fastertransformer/cuda/cuda_utils.h"
#include "src/fastertransformer/cuda/cublas/cublas.h"
#include "src/fastertransformer/cuda/cufmha/cufmha.h"
#include "src/fastertransformer/cuda/nccl/nccl_utils.h"
#include "src/fastertransformer/trt_plugins/weightOnlyQuantMatmulPlugin/weightOnlyQuantMatmulPlugin.h"
#include "src/fastertransformer/cutlass/interface.h"

#include <nvml.h>
namespace trt_plugins = tensorrt_llm::plugins;

namespace fastertransformer {

class CudaDevice : public DeviceBase {
public:
    CudaDevice(const DeviceInitParams& params);
    ~CudaDevice();

public:
    void init() override;
    DeviceProperties getDeviceProperties() override;
    DeviceStatus getDeviceStatus() override;
    IAllocator* getAllocator() override { return allocator_.get(); }
    IAllocator* getHostAllocator() override { return host_allocator_.get(); }

    void syncAndCheck() override;
    void syncCommunication(bool timeout = true) override;

private:
    void checkUseOpenSourceFMHA();
    void checkUseTrtV1FMHA();
    void checkUseTrtV2FMHA();
    void checkUseMultiBlockMode();


public:
    cudaStream_t getStream() {return stream_;}
    NcclParam getNcclParam() {return nccl_param_;}
public:
    void copy(const CopyParams& params);
    TransposeOutput transpose(const TransposeParams& params);
    ConvertOutput convert(const ConvertParams& params);
    SelectOutput select(const SelectParams& params);
    LayernormOutput layernorm(const LayernormParams& params);
    BufferPtr gemm(const GemmParams& params);
    BufferPtr embeddingLookup(const EmbeddingLookupParams& params);
    void activation(const ActivationParams& params);
    BufferPtr softmax(const SoftmaxParams& params);
    AttentionModuleOutput contextAttention(const AttentionModuleParams& params);
    AttentionModuleOutput decoderSelfAttention(const AttentionModuleParams& params);
    void sampleGreedy(const GreedyParams& params);
    void broadcast(const BroadcastParams& params);
    void allReduce(const AllReduceParams& params);
    void allGather(const AllGatherParams& params);

    BufferPtr quantize(const QuantizeParams& params);

// TODO: @xinglai delelte this
public:
    cudaStream_t stream() const {return stream_;}
    cublasMMWrapper* cublasMMWrapperPtr() const {return cublas_mm_wrapper_.get();}

private:
    std::unique_ptr<IAllocator> allocator_;
    std::unique_ptr<IAllocator> host_allocator_;

    cudaStream_t stream_;
    cublasHandle_t cublas_handle_;
    cublasLtHandle_t cublaslt_handle_;
    cudaDeviceProp device_prop_;

    std::mutex cublas_wrapper_mutex_;
    std::unique_ptr<cublasAlgoMap> cublas_algo_map_;
    std::unique_ptr<cublasMMWrapper> cublas_mm_wrapper_;
    
    std::unique_ptr<trt_plugins::WeightOnlyQuantMatmulPlugin> weight_only_matmul_plguin_;

    nvmlDevice_t nvml_device_;
    NcclParam nccl_param_;

    BufferPtr curandstate_buf_; // for sampler use.

    std::unique_ptr<cufmha> cufmha_runner_;
    bool use_trtv1_fmha         = false;
    bool use_trtv2_fmha         = false;
    bool use_openSource_fmha    = false;
    bool use_multi_block_mode   = false;
};

} // namespace fastertransformer

