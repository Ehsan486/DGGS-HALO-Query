#include "bafe/bafe.hpp"

#include <stdexcept>

namespace bafe {

std::unique_ptr<HaloEngine> make_cuda_engine(int) {
    throw std::runtime_error(
        "This BAFE build does not contain the CUDA backend; "
        "configure with -DBAFE_ENABLE_CUDA=ON and a CUDA toolkit");
}

}  // namespace bafe

