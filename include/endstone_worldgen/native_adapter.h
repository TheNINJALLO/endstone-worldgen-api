#pragma once
#include "endstone_worldgen/generation_scheduler.h"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace endstone_worldgen {
enum class NativeChunkRequestKind { CreateNewChunk, GetOrLoadChunk };

struct NativeChunkRequest {
    std::string dimension;
    ChunkPos position;
    NativeChunkRequestKind kind{NativeChunkRequestKind::CreateNewChunk};
    bool read_only{};
    std::uint64_t sequence{};
};

struct NativeCapabilities {
    bool capture_chunk{};
    bool commit_chunk{};
    bool vanilla_post_processing{};
    bool vanilla_hybrid{};
    bool vanilla_native_stages{};
    bool chunk_source_probe{};
    bool bds_task_launch_visible{};
    bool chunk_request_interception{};
    bool exact_vtable_hooks{};
    bool detached_worker_dispatch{};
    bool primary_thread_commit_gate{};
};

struct NativeDiagnostics {
    std::string adapter;
    std::string runtime_build;
    bool exact_build_match{};
    bool primary_thread{};
    bool chunk_source_available{};
    bool chunk_source_can_launch_tasks{};
    bool interception_installed{};
    std::size_t hooked_chunk_sources{};
    std::uint64_t intercepted_requests{};
    std::uint64_t dropped_requests{};
};

class IVanillaGenerationAdapter {
public:
    virtual ~IVanillaGenerationAdapter()=default;
    virtual std::string bedrockBuild()const=0;
    virtual bool verifySymbols()noexcept=0;
    virtual NativeCapabilities capabilities()const noexcept=0;
    virtual NativeDiagnostics diagnostics()const=0;
    virtual std::optional<ChunkBuffer> captureChunk(const std::string&,ChunkPos)=0;
    virtual bool commitChunk(const std::string&,const ChunkBuffer&)=0;
    virtual bool flushThreadBatch(const std::string&)=0;
    virtual bool installChunkRequestInterception(bool include_get_or_load = false)=0;
    virtual void disableChunkRequestInterception() noexcept=0;
    virtual std::vector<NativeChunkRequest> drainInterceptedRequests(std::size_t maximum)=0;
};
}
