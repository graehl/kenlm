#ifndef LM_WRAPPERS_NPLM_H
#define LM_WRAPPERS_NPLM_H

#include "lm/facade.hh"
#include "lm/max_order.hh"
#include "util/string_piece.hh"
#include "util/murmur_hash.hh"

#include <boost/thread/tss.hpp>
#include <boost/scoped_ptr.hpp>

/* Wrapper to NPLM "by Ashish Vaswani, with contributions from David Chiang
 * and Victoria Fossum."
 * http://nlg.isi.edu/software/nplm/

  latest nplm 0.3 e.g. https://github.com/graehl/nplm
 */

/// as always this must be same at kenlm build time and client compile time
#ifndef NPLM_MAX_ORDER
#define NPLM_MAX_ORDER KENLM_MAX_ORDER
#endif

namespace nplm {
class vocabulary;
class neuralLM;
} // namespace nplm

namespace lm {
namespace np {

class Vocabulary : public base::Vocabulary {
  public:
    Vocabulary(nplm::vocabulary const& vocab);

    ~Vocabulary();

    /// call after load on the owning Backend.
    void SetSpecials();

    WordIndex Index(const std::string &str) const;

    WordIndex Index(const StringPiece &str) const;

    WordIndex Index(char const* str) const;

    ::lm::WordIndex NullWord() const { return null_word_; }

  private:
    nplm::vocabulary const& vocab_;
    ::lm::WordIndex null_word_;
};


struct State {
  WordIndex words[NPLM_MAX_ORDER - 1];
  int Compare(const State &other) const {
    return std::memcmp(words, other.words, sizeof(words));
  }
  unsigned char Length() const {
    return NPLM_MAX_ORDER - 1; //TODO: should we detect zero-padding end?
  }
  /// words is constructed zero-padded already
  void ZeroRemaining() {}
};

class Backend;

inline uint64_t hash_value(const State &state, uint64_t seed = 0) {
  return util::MurmurHashNative(state.words, sizeof(State::words), seed);
}

class Model : public ::lm::base::ModelFacade<Model, State, Vocabulary> {
  private:
    typedef ::lm::base::ModelFacade<Model, State, Vocabulary> P;

  public:
    // Does this look like an NPLM?
    static bool Recognize(const std::string &file);

    explicit Model(const std::string &file, std::size_t cache_size = 1 << 20);

    ~Model();

    FullScoreReturn FullScore(const State &from, const WordIndex new_word, State &out_state) const;

    FullScoreReturn FullScoreForgotState(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state) const;

    // Prefer BaseFullScoreForgotState or build state one word at a time.  The context words should be provided in reverse order.
    void GetState(const WordIndex *context_rbegin, const WordIndex *context_rend, State &out_state) const;

  private:
    Backend *GetThreadSpecificBackend() const;
    boost::scoped_ptr<nplm::neuralLM> base_instance_;
    mutable boost::thread_specific_ptr<Backend> backend_;

    Vocabulary vocab_;

    ::lm::WordIndex null_word_;

    const std::size_t cache_size_;
};

} // namespace np
} // namespace lm

#endif // LM_WRAPPERS_NPLM_H
