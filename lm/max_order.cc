#include <lm/max_order.hh>
#include <stdexcept>

namespace lm {
namespace ngram {

Order kLibraryMaxOrder = KENLM_MAX_ORDER;

#define KENLM_MAKESTR(x) #x

void VerifyMaxOrder::AssertOrder(Order headerOrder) {
  if (headerOrder != KENLM_MAX_ORDER)
    throw std::length_error("library has KENLM_MAX_ORDER=" KENLM_MAKESTR(KENLM_MAX_ORDER)
                            ", so your code must also have -DKENLM_MAX_ORDER=" KENLM_MAKESTR(KENLM_MAX_ORDER));
}


}}
