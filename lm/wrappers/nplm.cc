#include "lm/wrappers/nplm.hh"
#include "util/exception.hh"
#include "util/file.hh"

#include <algorithm>
#include <cstring>
#include <cassert>

#include "neuralLM.h"

namespace lm {
namespace np {

Vocabulary::Vocabulary(const nplm::vocabulary &vocab) : vocab_(vocab) {}

Vocabulary::~Vocabulary() {}

void Vocabulary::SetSpecials() {
  null_word_ = vocab_.lookup_word("<null>");
  base::Vocabulary::SetSpecial(vocab_.lookup_word("<s>"),
                               vocab_.lookup_word("</s>"),
                               vocab_.lookup_word("<unk>"));
}

WordIndex Vocabulary::Index(const std::string &str) const {
  return vocab_.lookup_word(str);
}

WordIndex Vocabulary::Index(const StringPiece &str) const {
#if NPLM_HAVE_FIND_STRING_PIECE
  return vocab_.lookup_word(std::pair<char const*, char const*>(str.begin(), str.end()));
#else
  return vocab_.lookup_word(std::string(str.data(), str.size()));
#endif
}

WordIndex Vocabulary::Index(char const* key) const {
#if NPLM_HAVE_FIND_STRING_PIECE
  return vocab_.lookup_word(std::pair<char const*, char const*>(key, key + std::strlen(key)));
#else
  return vocab_.lookup_word(std::string(key));
#endif
}


class Backend {
  public:
    Backend(const nplm::neuralLM &from, const std::size_t cache_size) : lm_(from), ngram_(from.get_order()) {
      lm_.set_cache(cache_size);
    }

    nplm::neuralLM &LM() { return lm_; }
    const nplm::neuralLM &LM() const { return lm_; }

    Eigen::Matrix<int,Eigen::Dynamic,1> &staging_ngram() { return ngram_; }

    double lookup_from_staging() { return lm_.lookup_ngram(ngram_); }

    int order() const { return lm_.get_order(); }

  private:
    nplm::neuralLM lm_;
    Eigen::Matrix<int,Eigen::Dynamic,1> ngram_;
};

bool Model::Recognize(const std::string &name) {
  try {
    util::scoped_fd file(util::OpenReadOrThrow(name.c_str()));
    char magic_check[16];
    util::ReadOrThrow(file.get(), magic_check, sizeof(magic_check));
    const char nnlm_magic[] = "\\config\nversion ";
    return !memcmp(magic_check, nnlm_magic, 16);
  } catch (const util::Exception &) {
    return false;
  }
}

Model::Model(const std::string &file, std::size_t cache)
    : base_instance_(new nplm::neuralLM()), vocab_(base_instance_->get_vocabulary()), cache_size_(cache) {
  base_instance_->read(file);
  vocab_.SetSpecials();
  null_word_ = vocab_.NullWord();
  assert(null_word_ == base_instance_->lookup_word("<null>"));
  assert(vocab_.BeginSentence() == base_instance_->lookup_word("<s>"));
  UTIL_THROW_IF(base_instance_->get_order() > NPLM_MAX_ORDER, util::Exception, "This NPLM has order " << (unsigned int)base_instance_->get_order() << " but the KenLM wrapper was compiled with " << NPLM_MAX_ORDER << ".  Change the defintion of NPLM_MAX_ORDER and recompile.");
  // log10 compatible with backoff models.
  base_instance_->set_log_base(10.0);
  State begin_sentence, null_context;
  std::fill(begin_sentence.words, begin_sentence.words + NPLM_MAX_ORDER - 1, vocab_.BeginSentence());
  std::fill(null_context.words, null_context.words + NPLM_MAX_ORDER - 1, null_word_);
  Init(begin_sentence, null_context, vocab_, base_instance_->get_order());
}

Model::~Model() {
  backend_.reset(0); // per-thread pointers are destroyed on thread exit already
}

Backend *Model::GetThreadSpecificBackend() const {
  Backend *backend = backend_.get();
  if (!backend) {
    backend = new Backend(*base_instance_, cache_size_);
    backend_.reset(backend);
  }
  return backend;
}

FullScoreReturn Model::FullScore(const State &from, const WordIndex new_word, State &out_state) const {
  Backend *backend = GetThreadSpecificBackend();
  // State is in natural word order.
  FullScoreReturn ret;
  // Always say full order:
  assert(backend->order() == Order());
  ret.ngram_length = Order();
  for (int i = 0; i < ret.ngram_length - 1; ++i)
    backend->staging_ngram()(i) = from.words[i];  // staging_ngram is thread specific only because backend_ is.
  backend->staging_ngram()(ret.ngram_length - 1) = new_word;
  ret.prob = backend->lookup_from_staging();
  // Shift everything down by one.
  memcpy(out_state.words, from.words + 1, sizeof(WordIndex) * (ret.ngram_length - 2));
  out_state.words[ret.ngram_length - 2] = new_word;
  // Fill in trailing words with zeros so state comparison works. //TODO: template nplm on order?
  memset(out_state.words + ret.ngram_length - 1, 0, sizeof(WordIndex) * (NPLM_MAX_ORDER - ret.ngram_length));
  return ret;
}

// TODO: inline FullScore call without building explicit reversed state
FullScoreReturn Model::FullScoreForgotState(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state) const {
  State state;
  GetState(context_rbegin, context_rend, state);
  return FullScore(state, new_word, out_state);
}

void Model::GetState(const WordIndex *context_rbegin, const WordIndex *context_rend, State &state) const
{
  // State is in natural word order.  The API here specifies reverse order.
  unsigned const fullctxlen = Order() - 1;
  unsigned const relevant_context_length = std::min(fullctxlen, (unsigned)(context_rend - context_rbegin));
  // don't use a single byte for the context length (needs to be large enough to
  // grab the full part of hypothetical 256 word contexts)

  WordIndex *fullend = state.words + fullctxlen, *nullend = fullend - relevant_context_length;
  // null word padding, reversed [rbegin, rend), i.e. normal order, 0 padding:
  std::fill(state.words, nullend, null_word_);
  std::reverse_copy(context_rbegin, context_rbegin + relevant_context_length, nullend);
  assert(state.words + NPLM_MAX_ORDER == fullend + (NPLM_MAX_ORDER - 1 - fullctxlen));
  memset(fullend, 0, sizeof(WordIndex) * (NPLM_MAX_ORDER - 1 - fullctxlen));
}

} // namespace np
} // namespace lm
