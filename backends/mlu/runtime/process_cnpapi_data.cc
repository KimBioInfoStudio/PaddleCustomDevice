// Copyright (c) 2023 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "runtime/process_cnpapi_data.h"

#include <sys/syscall.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <sstream>
#include <thread>

pid_t gettid() { return syscall(SYS_gettid); }

// Get system-wide realtime clock in nanoseconds
inline uint64_t PosixInNsec() {
#ifdef _POSIX_C_SOURCE
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return tp.tv_sec * 1000 * 1000 * 1000 + tp.tv_nsec;
#else
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return 1000 * (static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec);
#endif
}

inline uint64_t GetTimeGap() {
  static uint64_t time_gap = []() -> uint64_t {
    uint64_t cpu_time = PosixInNsec();
    uint64_t mlu_time = cnpapiGetTimestamp();
    return (cpu_time - mlu_time);
  }();
  return time_gap;
}

inline std::string demangle(std::string name) { return name; }

void AddKernelRecord(const cnpapiActivityKernel *kernel,
                     uint64_t start_ns,
                     C_Profiler collector) {
  static uint64_t time_gap = GetTimeGap();
  if (kernel->start + time_gap < start_ns) {
    return;
  }
  phi::DeviceTraceEvent event;
  event.name = demangle(kernel->name);
  event.type = phi::TracerEventType::Kernel;
  event.start_ns = kernel->start + time_gap;
  event.end_ns = kernel->end + time_gap;
  event.device_id = kernel->device_id;
  event.context_id = kernel->context_id;
  event.stream_id = kernel->queue_id;
  event.correlation_id = kernel->correlation_id;
  event.kernel_info.block_x = kernel->dimx;
  event.kernel_info.block_y = kernel->dimy;
  event.kernel_info.block_z = kernel->dimz;
  event.kernel_info.grid_x = kernel->kernel_type;
  event.kernel_info.grid_y = 0;
  event.kernel_info.grid_z = 0;
  event.kernel_info.queued = kernel->queued;
  event.kernel_info.submitted = kernel->submitted;
  event.kernel_info.completed = kernel->received;

  profiler_add_device_trace_event(collector, &event);
}

const char *MemcpyKind(cnpapiActivityMemcpyType kind) {
  switch (kind) {
    case CNPAPI_ACTIVITY_MEMCPY_TYPE_HTOD:
      return "MEMCPY_HtoD";
    case CNPAPI_ACTIVITY_MEMCPY_TYPE_DTOH:
      return "MEMCPY_DtoH";
    case CNPAPI_ACTIVITY_MEMCPY_TYPE_DTOD:
      return "MEMCPY_DtoD";
    case CNPAPI_ACTIVITY_MEMCPY_TYPE_HTOH:
      return "MEMCPY_HtoH";
    case CNPAPI_ACTIVITY_MEMCPY_TYPE_PTOP:
      return "MEMCPY_PtoP";
    default:
      break;
  }
  return "MEMCPY";
}

void AddMemcpyRecord(const cnpapiActivityMemcpy *memcpy,
                     uint64_t start_ns,
                     C_Profiler collector) {
  static uint64_t time_gap = GetTimeGap();
  if (memcpy->start + time_gap < start_ns) {
    return;
  }
  phi::DeviceTraceEvent event;
  event.name = MemcpyKind(memcpy->copy_type);
  event.type = phi::TracerEventType::Memcpy;
  event.start_ns = memcpy->start + time_gap;
  event.end_ns = memcpy->end + time_gap;
  event.device_id = memcpy->device_id;
  event.context_id = memcpy->context_id;
  event.stream_id = memcpy->queue_id;
  event.correlation_id = memcpy->correlation_id;
  event.memcpy_info.num_bytes = memcpy->bytes;
  snprintf(event.memcpy_info.copy_kind,
           phi::kMemKindMaxLen,
           "%s",
           MemcpyKind(memcpy->copy_type));
  profiler_add_device_trace_event(collector, &event);
}

void AddMemcpy2Record(const cnpapiActivityMemcpyPtoP *memcpy2,
                      uint64_t start_ns,
                      C_Profiler collector) {
  static uint64_t time_gap = GetTimeGap();
  if (memcpy2->start + time_gap < start_ns) {
    return;
  }
  phi::DeviceTraceEvent event;
  event.name = MemcpyKind(memcpy2->copy_type);
  event.type = phi::TracerEventType::Memcpy;
  event.start_ns = memcpy2->start + time_gap;
  event.end_ns = memcpy2->end + time_gap;
  event.device_id = memcpy2->device_id;
  event.context_id = memcpy2->context_id;
  event.stream_id = memcpy2->queue_id;
  event.correlation_id = memcpy2->correlation_id;
  event.memcpy_info.num_bytes = memcpy2->bytes;
  snprintf(event.memcpy_info.copy_kind,
           phi::kMemKindMaxLen,
           "%s",
           MemcpyKind(memcpy2->copy_type));
  profiler_add_device_trace_event(collector, &event);
}

void AddMemsetRecord(const cnpapiActivityMemset *memset,
                     uint64_t start_ns,
                     C_Profiler collector) {
  static uint64_t time_gap = GetTimeGap();
  if (memset->start + time_gap < start_ns) {
    return;
  }
  phi::DeviceTraceEvent event;
  event.name = "MEMSET";
  event.type = phi::TracerEventType::Memset;
  event.start_ns = memset->start + time_gap;
  event.end_ns = memset->end + time_gap;
  event.device_id = memset->device_id;
  event.context_id = memset->context_id;
  event.stream_id = memset->queue_id;
  event.correlation_id = memset->correlation_id;
  event.memset_info.num_bytes = memset->bytes;
  event.memset_info.value = memset->value;
  profiler_add_device_trace_event(collector, &event);
}

class CnpapiRuntimeCbidStr {
 public:
  static const CnpapiRuntimeCbidStr &GetInstance() {
    static CnpapiRuntimeCbidStr inst;
    return inst;
  }

  std::string RuntimeKind(cnpapi_CallbackId cbid) const {
    auto iter = cbid_str_.find(cbid);
    if (iter == cbid_str_.end()) {
      return "MLU Runtime API " + std::to_string(cbid);
    }
    return iter->second;
  }

 private:
  CnpapiRuntimeCbidStr();

  std::unordered_map<cnpapi_CallbackId, std::string> cbid_str_;
};

CnpapiRuntimeCbidStr::CnpapiRuntimeCbidStr() {
#define REGISTER_RUNTIME_CBID_STR(cbid) \
  cbid_str_[CNPAPI_CNDRV_TRACE_CBID_##cbid] = #cbid

  REGISTER_RUNTIME_CBID_STR(cnMalloc);
  REGISTER_RUNTIME_CBID_STR(cnMallocHost);
  REGISTER_RUNTIME_CBID_STR(cnFree);
  REGISTER_RUNTIME_CBID_STR(cnFreeHost);
  REGISTER_RUNTIME_CBID_STR(cnMemcpy);
  REGISTER_RUNTIME_CBID_STR(cnMemcpyPeer);
  REGISTER_RUNTIME_CBID_STR(cnMemcpyHtoD);
  REGISTER_RUNTIME_CBID_STR(cnMemcpyDtoH);
  REGISTER_RUNTIME_CBID_STR(cnMemcpyDtoD);
  REGISTER_RUNTIME_CBID_STR(cnMemcpyAsync);
  REGISTER_RUNTIME_CBID_STR(cnMemcpyHtoDAsync);
  REGISTER_RUNTIME_CBID_STR(cnMemcpyDtoHAsync);
  REGISTER_RUNTIME_CBID_STR(cnMemcpyDtoDAsync);
  REGISTER_RUNTIME_CBID_STR(cnMemcpyDtoD2D);
  REGISTER_RUNTIME_CBID_STR(cnMemcpyDtoD3D);
  REGISTER_RUNTIME_CBID_STR(cnMemcpy2D);
  REGISTER_RUNTIME_CBID_STR(cnMemcpy3D);
  REGISTER_RUNTIME_CBID_STR(cnMemsetD8);
  REGISTER_RUNTIME_CBID_STR(cnMemsetD16);
  REGISTER_RUNTIME_CBID_STR(cnMemsetD32);
  REGISTER_RUNTIME_CBID_STR(cnMemsetD8Async);
  REGISTER_RUNTIME_CBID_STR(cnMemsetD16Async);
  REGISTER_RUNTIME_CBID_STR(cnMemsetD32Async);
  REGISTER_RUNTIME_CBID_STR(cnInvokeKernel);
  REGISTER_RUNTIME_CBID_STR(cnCreateQueue);
  REGISTER_RUNTIME_CBID_STR(cnDestroyQueue);
  REGISTER_RUNTIME_CBID_STR(cnQueueSync);
  REGISTER_RUNTIME_CBID_STR(cnQueueWaitNotifier);
  REGISTER_RUNTIME_CBID_STR(cnWaitNotifier);
  REGISTER_RUNTIME_CBID_STR(cnCreateNotifier);
  REGISTER_RUNTIME_CBID_STR(cnDestroyNotifier);
  REGISTER_RUNTIME_CBID_STR(cnPlaceNotifier);
  REGISTER_RUNTIME_CBID_STR(cnCtxCreate);
  REGISTER_RUNTIME_CBID_STR(cnCtxDestroy);
  REGISTER_RUNTIME_CBID_STR(cnCtxGetCurrent);
  REGISTER_RUNTIME_CBID_STR(cnCtxSetCurrent);
  REGISTER_RUNTIME_CBID_STR(cnCtxGetDevice);
  REGISTER_RUNTIME_CBID_STR(cnCtxSync);
  REGISTER_RUNTIME_CBID_STR(cnInvokeHostFunc);
#undef REGISTER_RUNTIME_CBID_STR
}

void AddApiRecord(const cnpapiActivityAPI *api,
                  uint64_t start_ns,
                  const std::unordered_map<uint32_t, uint64_t> tid_mapping,
                  C_Profiler collector) {
  static uint64_t time_gap = GetTimeGap();
  if (api->start + time_gap < start_ns) {
    return;
  }
  phi::RuntimeTraceEvent event;
  event.name = CnpapiRuntimeCbidStr::GetInstance().RuntimeKind(api->cbid);
  event.start_ns = api->start + time_gap;
  event.end_ns = api->end + time_gap;
  event.process_id = api->process_id;
  event.thread_id = api->thread_id;
  event.correlation_id = api->correlation_id;
  event.callback_id = api->cbid;
  event.type = phi::TracerEventType::CudaRuntime;
  profiler_add_runtime_trace_event(collector, &event);
}

void ProcessCnpapiActivityRecord(
    const cnpapiActivity *record,
    uint64_t start_ns,
    const std::unordered_map<uint32_t, uint64_t> tid_mapping,
    C_Profiler collector) {
  switch (record->type) {
    case CNPAPI_ACTIVITY_TYPE_KERNEL:
      AddKernelRecord(reinterpret_cast<const cnpapiActivityKernel *>(record),
                      start_ns,
                      collector);
      break;
    case CNPAPI_ACTIVITY_TYPE_MEMCPY:
      AddMemcpyRecord(reinterpret_cast<const cnpapiActivityMemcpy *>(record),
                      start_ns,
                      collector);
      break;
    case CNPAPI_ACTIVITY_TYPE_MEMCPY_PTOP:
      AddMemcpy2Record(
          reinterpret_cast<const cnpapiActivityMemcpyPtoP *>(record),
          start_ns,
          collector);
      break;
    case CNPAPI_ACTIVITY_TYPE_MEMSET:
      AddMemsetRecord(reinterpret_cast<const cnpapiActivityMemset *>(record),
                      start_ns,
                      collector);
      break;
    case CNPAPI_ACTIVITY_TYPE_CNDRV_API:
      AddApiRecord(reinterpret_cast<const cnpapiActivityAPI *>(record),
                   start_ns,
                   tid_mapping,
                   collector);
      break;
    default:
      break;
  }
}

void *AlignedMalloc(size_t size, size_t alignment) {
  assert(alignment >= sizeof(void *) && (alignment & (alignment - 1)) == 0);
  size = (size + alignment - 1) / alignment * alignment;
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
  void *aligned_mem = nullptr;
  if (posix_memalign(&aligned_mem, alignment, size) != 0) {
    aligned_mem = nullptr;
  }
  return aligned_mem;
#elif defined(_WIN32)
  return _aligned_malloc(size, alignment);
#else
  void *mem = malloc(size + alignment);
  if (mem == nullptr) {
    return nullptr;
  }
  size_t adjust = alignment - reinterpret_cast<uint64_t>(mem) % alignment;
  void *aligned_mem = reinterpret_cast<char *>(mem) + adjust;
  *(reinterpret_cast<void **>(aligned_mem) - 1) = mem;
  assert(reinterpret_cast<uint64_t>(aligned_mem) % alignment == 0);
  return aligned_mem;
#endif
}

void AlignedFree(void *mem_ptr) {
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
  free(mem_ptr);
#elif defined(_WIN32)
  _aligned_free(mem_ptr);
#else
  if (mem_ptr) {
    free(*(reinterpret_cast<void **>(mem_ptr) - 1));
  }
#endif
}

void Tracer::AllocateBuffer(uint64_t **buffer, size_t *size) {
  constexpr size_t kBufferSize = 1 << 23;  // 8 MB
  constexpr size_t kBufferAlignSize = 8;
  *buffer = reinterpret_cast<uint64_t *>(
      AlignedMalloc(kBufferSize, kBufferAlignSize));
  *size = kBufferSize;
}

void Tracer::ProduceBuffer(uint64_t *buffer, size_t valid_size) {
  std::lock_guard<std::mutex> guard(activity_buffer_lock_);
  activity_buffers_.emplace_back(buffer, valid_size);
}

std::vector<ActivityBuffer> Tracer::ConsumeBuffers() {
  std::vector<ActivityBuffer> buffers;
  {
    std::lock_guard<std::mutex> guard(activity_buffer_lock_);
    buffers.swap(activity_buffers_);
  }
  return buffers;
}

void Tracer::ReleaseBuffer(uint64_t *buffer) { AlignedFree(buffer); }
