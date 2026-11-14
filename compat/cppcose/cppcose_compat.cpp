// SPDX-License-Identifier: Apache-2.0
#include <keymaster/cppcose/cppcose.h>

namespace cppcose {

// The stock KeyMint client uses the overload removed in system/keymaster ec8df6d.
// Preserve its empty protected map and EdDSA algorithm using the current API.
ErrMsgOr<cppbor::Array> constructCoseSign1(const bytevec& key, const bytevec& payload,
                                        const bytevec& aad) {
    return constructCoseSign1(key, {}, payload, aad, EDDSA);
}

}  // namespace cppcose
