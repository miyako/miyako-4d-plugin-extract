// ─────────────────────────────────────────────
//  llama_chunker.cpp
//  Depends on: libllama.a, llama.h, llama-cpp.h
//  Build (macOS arm64):
//    clang++ -std=c++17 -O2 -arch arm64 \
//            -I/path/to/llama.cpp \
//            chunker.cpp main_example.cpp \
//            /path/to/libllama.a \
//            -framework Metal -framework Foundation \
//            -o chunker
//  Build (Windows x64, MSVC):
//    cl /std:c++17 /O2 chunker.cpp main_example.cpp
//       /I C:\path\to\llama.cpp
//       /link libllama.lib
// ─────────────────────────────────────────────

#include "chunker.h"

// llama.h declares the C API; llama-cpp.h declares C++ helpers.
// We include them only in this translation unit so clients need
// not touch them.
#include "llama.h"
#include "llama-cpp.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
//  Platform helpers
// ─────────────────────────────────────────────────────────────────────────────
#if defined(_WIN32)
#  define CHUNKER_WINDOWS 1
#else
#  define CHUNKER_WINDOWS 0
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  ModelDeleter – called by unique_ptr when the chunker is destroyed
// ─────────────────────────────────────────────────────────────────────────────
namespace chunker {

void LlamaChunker::ModelDeleter::operator()(void* p) const noexcept {
    if (p) {
        llama_model_free(reinterpret_cast<llama_model*>(p));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────
LlamaChunker::LlamaChunker(const ModelConfig& cfg)
    : cfg_(cfg)
{
    // ── 1. Init backend (idempotent after first call) ──────────────────
    llama_backend_init();

    // ── 2. Build model params: load metadata only, skip weights ────────
    llama_model_params mparams = llama_model_default_params();
    mparams.vocab_only = true;   // <── key flag: no weights loaded

    // ── 3. Load the model ──────────────────────────────────────────────
    llama_model* raw = llama_model_load_from_file(cfg_.model_path.c_str(),
                                                  mparams);
    if (!raw) {
        throw std::runtime_error(
            "chunker: failed to load model from: " + cfg_.model_path);
    }

    // Transfer ownership into the unique_ptr with our custom deleter.
    model_owner_.reset(raw);
    model_ = raw;

    // ── 4. Grab vocab alias (no ownership) ────────────────────────────
    vocab_ = reinterpret_cast<void*>(
        const_cast<llama_vocab*>(llama_model_get_vocab(raw)));

    if (!vocab_) {
        throw std::runtime_error("chunker: model has no vocab");
    }

    // ── 5. Cache n_ctx_train for later per-call validation ──────────────
    // llama_model_n_ctx_train() returns 0 when vocab_only=true in some
    // llama.cpp builds because tensors are not loaded.  Fall back to
    // reading the value directly from the GGUF metadata key-value store.
    n_ctx_train_ = static_cast<uint32_t>(llama_model_n_ctx_train(raw));
    if (n_ctx_train_ == 0) {
        // Try architecture-specific key first, then generic fallback
        char meta_buf[32] = {};
        const char* const kCtxKeys[] = {
            "qwen3.context_length",
            "qwen2.context_length",
            "llama.context_length",
            "phi3.context_length",
            "gemma.context_length",
            "context_length",
            nullptr
        };
        for (int ki = 0; kCtxKeys[ki] && n_ctx_train_ == 0; ++ki) {
            if (llama_model_meta_val_str(raw, kCtxKeys[ki],
                                         meta_buf, sizeof(meta_buf)) >= 0) {
                long v = std::strtol(meta_buf, nullptr, 10);
                if (v > 0) n_ctx_train_ = static_cast<uint32_t>(v);
            }
        }
        if (n_ctx_train_ == 0) {
            // Last resort: assume a safe conservative value
            n_ctx_train_ = 4096;
            std::fprintf(stderr,
                "[chunker] WARNING: could not read context_length from "
                "metadata; assuming %u\n", n_ctx_train_);
        }
    }

    // ── 6. Find a reliable pad token ──────────────────────────────────
    discover_pad_token();

    if (cfg_.verbosity >= 1) {
        std::fprintf(stderr,
            "[chunker] model: %s\n"
            "          vocab size: %d\n"
            "          n_ctx_train: %u\n"
            "          pad string: \"%s\"  (token id %d, %u tok/unit)\n",
            cfg_.model_path.c_str(),
            llama_vocab_n_tokens(
                reinterpret_cast<const llama_vocab*>(vocab_)),
            n_ctx_train_,
            pad_string_.c_str(), pad_token_id_, pad_unit_tokens_);
    }
}

LlamaChunker::~LlamaChunker() {
    // model_owner_ RAII handles llama_model_free.
    // llama_backend_free() is intentionally NOT called here: it is a
    // process-global shutdown that conflicts with multiple instances.
    // Call it explicitly in main() if desired.
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pad-token discovery
// ─────────────────────────────────────────────────────────────────────────────
/*
  Strategy:
  1. Iterate candidate strings known to tokenise to exactly 1 token in
     SentencePiece / BPE / tiktoken-style vocabs.
  2. Verify round-trip: tokenise → detokenise → retokenise gives same id.
  3. Reject BOS, EOS, UNK, and any id that appears in common English prose.
  4. Fall back to EOS if nothing suitable found (acceptable: EOS at non-final
     positions is ignored by most embedding pooling strategies).
*/
void LlamaChunker::discover_pad_token() {
    const llama_vocab* vp = reinterpret_cast<const llama_vocab*>(vocab_);
    const llama_model* mp = reinterpret_cast<const llama_model*>(model_);
    (void)mp; // only used by llama_token_to_piece

    const int32_t bos_id = llama_vocab_bos(vp);
    const int32_t eos_id = llama_vocab_eos(vp);

    // ── Prefer the vocab's own dedicated PAD token ────────────────────
    // The GGUF for Harrier/Qwen3 explicitly sets tokenizer.ggml.padding_token_id
    // which llama.cpp exposes via llama_vocab_pad().  Using it is more
    // semantically correct than probing candidates.
    {
        const int32_t pad_id = llama_vocab_pad(vp);
        // pad_id == eos_id is intentional for this model (PAD = <|endoftext|>).
        // Accept it as long as it is a real token (>= 0).
        if (pad_id >= 0) {
            char buf[64] = {};
            int  n = llama_token_to_piece(vp, pad_id, buf, sizeof(buf) - 1,
                                          0, /*special=*/true);
            pad_token_id_    = pad_id;
            pad_string_      = (n > 0) ? std::string(buf, n) : "<pad>";
            pad_unit_tokens_ = 1;
            if (cfg_.verbosity >= 1) {
                std::fprintf(stderr,
                    "[chunker] pad token: using vocab PAD token "
                    "id=%d \"%s\"\n", pad_id, pad_string_.c_str());
            }
            return;
        }
    }

    // llama_vocab_unk() was removed in a recent llama.cpp refactor.
    // Detect UNK via token attribute flags instead.
    // LLAMA_TOKEN_ATTR_UNKNOWN (0x1) is set on the unknown/OOV placeholder.
    auto is_unk = [&](int32_t id) -> bool {
        if (id < 0) return false;
        return (llama_vocab_get_attr(vp, id) & LLAMA_TOKEN_ATTR_UNKNOWN) != 0;
    };

    // Candidates ordered by preference.
    // These are short, typographically neutral, rarely in prose.
    static const char* const kCandidates[] = {
        "▁",    // SentencePiece word-start marker (U+2581)
        "Ġ",    // GPT-2 space marker
        "Ċ",    // GPT-2 newline marker
        "⁺",    // superscript plus  – very rare in prose
        "·",    // middle dot
        "•",    // bullet
        "–",    // en-dash
        "_",    // underscore
        ".",    // period (common but 1-token in most vocabs)
        " ",    // space
        nullptr
    };

    auto try_candidate = [&](const char* s) -> bool {
        // Tokenise with no BOS/EOS addition
        std::vector<int32_t> toks = tokenize(s, /*add_special=*/false);
        if (toks.size() != 1) return false;

        int32_t id = toks[0];
        if (id < 0)      return false;
        if (id == bos_id) return false;
        if (id == eos_id) return false;
        if (is_unk(id))    return false;

        // Round-trip: detokenise and re-tokenise
        char buf[64] = {};
        int  n = llama_token_to_piece(vp, id, buf, sizeof(buf) - 1,
                                      /*lstrip=*/0, /*special=*/false);
        if (n <= 0) return false;
        buf[n] = '\0';

        std::vector<int32_t> rt = tokenize(buf, /*add_special=*/false);
        if (rt.size() != 1 || rt[0] != id) return false;

        pad_token_id_    = id;
        pad_string_      = s;
        pad_unit_tokens_ = 1;
        return true;
    };

    for (int i = 0; kCandidates[i]; ++i) {
        if (try_candidate(kCandidates[i])) return;
    }

    // Last resort: use EOS.  This is semantically awkward but structurally
    // safe because embedding models pool over all positions and many ignore
    // the EOS token's embedding when it appears mid-sequence.
    if (eos_id >= 0) {
        pad_token_id_    = eos_id;
        pad_string_      = "<EOS-pad>";
        pad_unit_tokens_ = 1;
        if (cfg_.verbosity >= 1) {
            std::fprintf(stderr,
                "[chunker] WARNING: no neutral pad token found; "
                "using EOS (%d) as pad.\n", eos_id);
        }
        return;
    }

    throw std::runtime_error(
        "chunker: cannot find any usable pad token in this vocab.");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tokenise helper
// ─────────────────────────────────────────────────────────────────────────────
std::vector<int32_t>
LlamaChunker::tokenize(const std::string& text, bool add_special) const {
    const llama_vocab* vp = reinterpret_cast<const llama_vocab*>(vocab_);

    // First call: measure required buffer size
    int n = llama_tokenize(vp,
                           text.c_str(),
                           static_cast<int32_t>(text.size()),
                           nullptr, 0,
                           add_special,
                           /*parse_special=*/false);
    if (n < 0) {
        // llama_tokenize returns negative count on "buffer too small".
        // Re-call with the right size.
        n = -n;
    }
    if (n == 0) return {};

    std::vector<int32_t> out(static_cast<size_t>(n));
    int actual = llama_tokenize(vp,
                                text.c_str(),
                                static_cast<int32_t>(text.size()),
                                out.data(),
                                static_cast<int32_t>(out.size()),
                                add_special,
                                /*parse_special=*/false);
    if (actual < 0) {
        throw std::runtime_error("chunker: tokenize failed (buffer error)");
    }
    out.resize(static_cast<size_t>(actual));
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pad to target length
// ─────────────────────────────────────────────────────────────────────────────
void LlamaChunker::pad_to_target(std::vector<int32_t>& out,
                                  uint32_t target_len) const {
    assert(pad_token_id_ >= 0);
    const size_t content = out.size();
    if (content >= target_len) {
        out.resize(target_len); // trim if overlap pushed us over
        return;
    }
    const size_t n_pad = target_len - content;
    // Prepend pad tokens so content ends at the last position.
    // Decoder-only models pool the last token's hidden state;
    // prepending keeps the final attended token as real content.
    out.insert(out.begin(),
               n_pad, static_cast<int32_t>(pad_token_id_));
}

// ─────────────────────────────────────────────────────────────────────────────
//  chunk() – main entry point
// ─────────────────────────────────────────────────────────────────────────────
/*
  Algorithm: Greedy sentence-aware bucket packing
  ─────────────────────────────────────────────────
  1. Tokenise the full text (no BOS/EOS to keep token counts accurate).
  2. Walk tokens; track sentence boundaries heuristically (period / '!'/ '?'
     followed by whitespace).  More robust than splitting on whitespace first
     because we work in token space, not character space.
  3. For each bucket:
     a. Start with `overlap_tokens` tokens from the tail of the previous bucket.
     b. Greedily append sentence-terminal runs until we hit
        (target_len - pad_reserve) tokens.
     c. If we exhaust all tokens, stop.
     d. Pad the bucket tail to exactly target_len.
  4. If the final bucket's fill ratio < min_fill_ratio, discard it.
*/
std::vector<Chunk>
LlamaChunker::chunk(const std::string& text,
                    const ChunkParams& params,
                    ChunkStats* stats_out) const {
    validate_params(params);
    const llama_vocab* vp = reinterpret_cast<const llama_vocab*>(vocab_);
    const llama_model* mp = reinterpret_cast<const llama_model*>(model_);

    // ── tokenise ──────────────────────────────────────────────────────
    std::vector<int32_t> all_toks = tokenize(text, /*add_special=*/false);
    if (all_toks.empty()) return {};

    const uint32_t capacity = params.target_len - params.pad_reserve;

    // ── helper: find next sentence boundary after position i ──────────
    // Returns the index ONE PAST the boundary token, or all_toks.size()
    // if none found.
    auto find_sentence_end = [&](size_t start, size_t limit) -> size_t {
        // Detokenise a small window to look for punctuation.
        // We work character-level on the decoded string.
        for (size_t i = start; i < std::min(limit, all_toks.size()); ++i) {
            char buf[8] = {};
            int n = llama_token_to_piece(vp, all_toks[i], buf, sizeof(buf) - 1,
                                         0, false);
            if (n <= 0) continue;
            buf[n] = '\0';
            // Look for sentence-ending punctuation in the decoded piece
            for (int c = 0; c < n; ++c) {
                if (buf[c] == '.' || buf[c] == '!' || buf[c] == '?') {
                    return i + 1; // one past this token
                }
            }
        }
        return std::min(limit, all_toks.size());
    };

    // ── main bucketing loop ───────────────────────────────────────────
    std::vector<Chunk> result;
    size_t cursor   = 0;
    size_t prev_end = 0;  // for overlap
    uint32_t seq    = 0;

    const size_t N = all_toks.size();

    while (cursor < N) {
        std::vector<int32_t> bucket;
        bucket.reserve(params.target_len);

        // a. Prepend overlap from previous bucket
        if (seq > 0 && params.overlap_tokens > 0 && prev_end > params.overlap_tokens) {
            size_t ov_start = prev_end - params.overlap_tokens;
            bucket.insert(bucket.end(),
                          all_toks.begin() + static_cast<ptrdiff_t>(ov_start),
                          all_toks.begin() + static_cast<ptrdiff_t>(prev_end));
        }

        // b. Greedily fill to capacity, preferring sentence boundaries
        size_t fill_cursor = cursor;
        while (fill_cursor < N &&
               static_cast<uint32_t>(bucket.size()) < capacity)
        {
            // How many tokens we can still add
            uint32_t room = capacity - static_cast<uint32_t>(bucket.size());

            // Find a sentence boundary within `room` tokens
            size_t bound = find_sentence_end(fill_cursor,
                                             fill_cursor + room);
            if (bound == fill_cursor) {
                // No boundary found: take as many as fit
                bound = std::min(N, fill_cursor + room);
            }

            bucket.insert(bucket.end(),
                          all_toks.begin() + static_cast<ptrdiff_t>(fill_cursor),
                          all_toks.begin() + static_cast<ptrdiff_t>(bound));
            fill_cursor = bound;
        }

        prev_end = fill_cursor;
        cursor   = fill_cursor;

        // c. Check fill ratio
        uint32_t content_toks = static_cast<uint32_t>(bucket.size());
        float    fill_ratio   = static_cast<float>(content_toks)
                              / static_cast<float>(params.target_len);

        bool mostly_pad = fill_ratio < params.min_fill_ratio;
        if (mostly_pad && params.min_fill_ratio > 0.0f && seq > 0) {
            // Discard underfull final bucket
            if (cfg_.verbosity >= 2) {
                std::fprintf(stderr,
                    "[chunker] discarding underfull bucket %u "
                    "(fill %.1f%%)\n", seq, fill_ratio * 100.0f);
            }
            break;
        }

        // d. Pad to exact target length
        pad_to_target(bucket, params.target_len);
        assert(bucket.size() == params.target_len);

        // e. Build debug text (optional but cheap for small buckets)
        std::string chunk_text;
        chunk_text.reserve(content_toks * 5);
        for (uint32_t t = 0; t < content_toks; ++t) {
            char buf[16] = {};
            int  n = llama_token_to_piece(vp, bucket[t], buf, sizeof(buf) - 1,
                                          0, false);
            if (n > 0) { buf[n] = '\0'; chunk_text += buf; }
        }

        Chunk ch;
        ch.tokens   = std::move(bucket);
        ch.text     = std::move(chunk_text);
        ch.seq_id   = seq;
        ch.is_pad   = mostly_pad;
        result.push_back(std::move(ch));

        if (cfg_.verbosity >= 2) {
            std::fprintf(stderr,
                "[chunker] chunk %u: %u content toks, fill %.1f%% (target %u)\n",
                seq, content_toks, fill_ratio * 100.0f, params.target_len);
        }

        ++seq;
    }

    // ── fill stats ────────────────────────────────────────────────────
    if (stats_out) {
        stats_out->total_tokens    = N;
        stats_out->num_chunks      = result.size();
        size_t pad_total = 0;
        double fill_sum  = 0.0;
        for (auto& c : result) {
            size_t content = 0;
            for (auto t : c.tokens) {
                if (t != pad_token_id_) ++content;
            }
            pad_total += params.target_len - content;
            fill_sum  += static_cast<double>(content) / params.target_len;
        }
        stats_out->pad_tokens_total = pad_total;
        stats_out->avg_fill_ratio   =
            result.empty() ? 0.0 : fill_sum / static_cast<double>(result.size());
        stats_out->target_len       = params.target_len;
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  chunk_batch()
// ─────────────────────────────────────────────────────────────────────────────
std::vector<Chunk>
LlamaChunker::chunk_batch(const std::vector<std::string>& texts,
                          const ChunkParams& params,
                          ChunkStats* stats_out) const {
    std::vector<Chunk> all;
    ChunkStats combined{};

    uint32_t seq_offset = 0;
    for (auto& text : texts) {
        ChunkStats s{};
        auto chunks = chunk(text, params, &s);

        // Offset seq_ids so they're unique across the batch
        for (auto& c : chunks) c.seq_id += seq_offset;
        seq_offset += static_cast<uint32_t>(chunks.size());

        combined.total_tokens    += s.total_tokens;
        combined.num_chunks      += s.num_chunks;
        combined.pad_tokens_total+= s.pad_tokens_total;
        combined.avg_fill_ratio  += s.avg_fill_ratio;

        all.insert(all.end(),
                   std::make_move_iterator(chunks.begin()),
                   std::make_move_iterator(chunks.end()));
    }
    if (!texts.empty()) {
        combined.avg_fill_ratio /= static_cast<double>(texts.size());
    }
    if (stats_out) *stats_out = combined;
    return all;
}


// ─────────────────────────────────────────────────────────────────────────────
//  detokenize
// ─────────────────────────────────────────────────────────────────────────────
std::string LlamaChunker::detokenize(const std::vector<int32_t>& tokens,
                                      bool skip_pad) const {
    const llama_vocab* vp = reinterpret_cast<const llama_vocab*>(vocab_);
    std::string out;
    out.reserve(tokens.size() * 4);
    for (int32_t tok : tokens) {
        if (skip_pad && tok == pad_token_id_) continue;
        char buf[256] = {};
        int n = llama_token_to_piece(vp, tok, buf, sizeof(buf) - 1,
                                     /*lstrip=*/0, /*special=*/true);
        if (n > 0) out.append(buf, static_cast<size_t>(n));
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  validate_params
// ─────────────────────────────────────────────────────────────────────────────
void LlamaChunker::validate_params(const ChunkParams& p) const {
    if (p.target_len == 0)
        throw std::invalid_argument("chunker: target_len must be > 0");
    if (p.target_len > n_ctx_train_)
        throw std::invalid_argument(
            "chunker: target_len " + std::to_string(p.target_len) +
            " exceeds model n_ctx_train " + std::to_string(n_ctx_train_));
    if (p.pad_reserve >= p.target_len)
        throw std::invalid_argument("chunker: pad_reserve must be < target_len");
    if (p.overlap_tokens >= p.target_len - p.pad_reserve)
        throw std::invalid_argument("chunker: overlap_tokens too large for target_len");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Factory
// ─────────────────────────────────────────────────────────────────────────────

// We need a pointer-stable address for the atexit lambda.
// Store as a raw pointer; atexit resets it.
static LlamaChunker* g_instance_ptr = nullptr;

ChunkerPtr make_chunker(const ModelConfig& cfg) {
    auto ptr = std::make_unique<LlamaChunker>(cfg);
    g_instance_ptr = ptr.get();

    // Register cleanup: reset the global alias (the unique_ptr in main
    // will be reset first during normal scope exit, but atexit is the
    // belt-and-suspenders guarantee for abnormal termination paths).
    std::atexit([]() {
        // If the unique_ptr in the caller's scope has already been
        // destroyed, g_instance_ptr is dangling — set to null only.
        g_instance_ptr = nullptr;
        llama_backend_free();
    });

    return ptr;
}

} // namespace chunker
