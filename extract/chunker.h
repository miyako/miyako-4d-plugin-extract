#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

// ─────────────────────────────────────────────────────────────────────────────
//  chunker.h
//  Fixed-shape token chunker for llama.cpp
//  Targets: macOS arm64, Windows x64
//
//  Two-level configuration:
//    ModelConfig  – passed once at construction (model path, verbosity)
//    ChunkParams  – passed per chunk() call (target_len, overlap, …)
//
//  This lets a single LlamaChunker instance serve texts that need different
//  bucket sizes or overlap widths without reloading the vocab.
// ─────────────────────────────────────────────────────────────────────────────

namespace chunker {

// ── Per-call parameters ───────────────────────────────────────────────────────
//
// target_len    : exact token count each output chunk must have (≤ model n_ctx)
// pad_reserve   : slots kept free for padding at the tail of each bucket;
//                 content fills up to (target_len - pad_reserve) tokens.
//                 Rule of thumb: max(4, target_len / 64)
// overlap_tokens: tokens copied from the tail of the previous chunk to the
//                 head of the next one, preserving cross-boundary context.
//                 Rule of thumb: ~6-10 % of target_len.  Set 0 to disable.
// min_fill_ratio: discard the final (underfull) chunk if its content token
//                 count is below this fraction of target_len.  0.0 = keep all.
//
struct ChunkParams {
    uint32_t target_len     = 512;
    uint32_t pad_reserve    = 8;
    uint32_t overlap_tokens = 32;
    float    min_fill_ratio = 0.20f;

    // Convenience ctor: derive sensible defaults from target_len alone.
    static ChunkParams from_target(uint32_t target_len,
                                   float    overlap_pct    = 0.06f,
                                   float    min_fill_ratio = 0.20f)
    {
        ChunkParams p;
        p.target_len     = target_len;
        p.pad_reserve    = std::max(4u, target_len / 64u);
        p.overlap_tokens = static_cast<uint32_t>(target_len * overlap_pct);
        p.min_fill_ratio = min_fill_ratio;
        return p;
    }
};

// ── Construction-time configuration ──────────────────────────────────────────
//
// model_path : GGUF file; loaded vocab-only (no weights in RAM)
// verbosity  : 0 = silent, 1 = one-line summary per chunk() call, 2 = per-chunk
//
struct ModelConfig {
    std::string model_path;
    int         verbosity = 1;
};

// ── Output types ──────────────────────────────────────────────────────────────

struct Chunk {
    std::vector<int32_t> tokens;  // exactly params.target_len tokens
    std::string          text;    // reconstructed content (pre-pad portion)
    uint32_t             seq_id;  // monotone index within this chunk() call
    bool                 is_pad;  // true if content < min_fill_ratio * target_len
};

struct ChunkStats {
    size_t   total_tokens;      // source tokens (no pad)
    size_t   num_chunks;        // buckets emitted
    size_t   pad_tokens_total;  // padding tokens inserted across all chunks
    double   avg_fill_ratio;    // mean (content / target_len) across chunks
    uint32_t target_len;        // echoes the params used
};

// ── LlamaChunker ──────────────────────────────────────────────────────────────
//
//  Construction: load vocab once.
//  Usage:        call chunk() or chunk_batch() with any ChunkParams you like.
//
class LlamaChunker {
public:
    explicit LlamaChunker(const ModelConfig& cfg);
    ~LlamaChunker();

    LlamaChunker(const LlamaChunker&)            = delete;
    LlamaChunker& operator=(const LlamaChunker&) = delete;

    // Tokenise `text`, split into fixed-length padded buckets.
    // Every returned Chunk has exactly `params.target_len` tokens.
    std::vector<Chunk> chunk(const std::string& text,
                             const ChunkParams& params,
                             ChunkStats*        stats_out = nullptr) const;

    // Chunk multiple texts; seq_ids are unique across the whole batch.
    std::vector<Chunk> chunk_batch(const std::vector<std::string>& texts,
                                   const ChunkParams&               params,
                                   ChunkStats*                      stats_out = nullptr) const;

    // ── Vocab introspection ───────────────────────────────────────────────
    // Reconstruct a string from a token id sequence.
    // Pass the full chunk.tokens vector to see content + pad,
    // or a slice to see just the content portion.
    std::string detokenize(const std::vector<int32_t>& tokens,
                           bool skip_pad = false) const;

    const std::string& pad_string()      const { return pad_string_; }
    int32_t            pad_token_id_internal() const { return pad_token_id_; }
    uint32_t           pad_unit_tokens() const { return pad_unit_tokens_; }
    uint32_t           n_ctx_train()     const { return n_ctx_train_; }
    bool               ready()           const { return model_ != nullptr; }

    std::vector<int32_t> tokenize(const std::string& text,
                                  bool add_special = false) const;
private:
    
    void discover_pad_token();
    void pad_to_target(std::vector<int32_t>& out, uint32_t target_len) const;
    void validate_params(const ChunkParams& p) const;

    struct ModelDeleter { void operator()(void* p) const noexcept; };
    std::unique_ptr<void, ModelDeleter> model_owner_;
    void*    model_      = nullptr;
    void*    vocab_      = nullptr;

    ModelConfig  cfg_;
    uint32_t     n_ctx_train_    = 0;
    std::string  pad_string_;
    int32_t      pad_token_id_   = -1;
    uint32_t     pad_unit_tokens_ = 1;
};

// ── Factory ───────────────────────────────────────────────────────────────────
using ChunkerPtr = std::unique_ptr<LlamaChunker>;
ChunkerPtr make_chunker(const ModelConfig& cfg);

} // namespace chunker
