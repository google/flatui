// Copyright 2015 Google Inc. All rights reserved.
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

#ifndef FPL_FLATUI_UTIL_H
#define FPL_FLATUI_UTIL_H

namespace flatui {

/// @cond FLATUI_INTERNAL
///
/// @brief The hash ID related definitions and implementations.

/// @typedef HashedId
///
/// @brief A typedef to represent the ID of a hash.
typedef uint32_t HashedId;

/// @typedef SequenceId
///
/// @brief A typedef to represent the ID of a sequence.
typedef uint64_t SequenceId;

/// @var kNullHash
///
/// @brief A sentinel value to demarcate a `null` or invalid hash.
static HashedId kNullHash = 0;

/// @var kInitialHashValue
///
/// @brief An initial value of hash calculation.
static HashedId kInitialHashValue = 0x84222325;

/// @brief Hash a UTF8 string with a length into a `HashId`.
///
/// @warning This function asserts if the `id` collides with `kNullHash`.
/// If you hit this assert, you will need to change your `id`.
///
/// @param[in] id A UTF8 string representing the ID to hash.
/// @param[in] length a length of the string.
///
/// @return Returns the HashId corresponding to the `id`.
inline HashedId HashId(const char *id, int32_t length,
                       HashedId hash = kInitialHashValue) {
  // A quick good hash, from:
  // https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
  for (auto i = 0; i < length; ++i) {
    hash = (hash ^ static_cast<uint8_t>(*id++)) * 0x000001b3;
  }
  // We use kNullHash for special checks, so make sure it doesn't collide.
  // If you hit this assert, sorry, please change your id :)
  assert(hash != kNullHash);
  return hash;
}

/// @brief Hash a C-string into a `HashId`.
///
/// @warning This function asserts if the `id` collides with `kNullHash`.
/// If you hit this assert, you will need to change your `id`.
///
/// @param[in] id A C-string representing the ID to hash.
///
/// @return Returns the HashId corresponding to the `id`.
inline HashedId HashId(const char *id, HashedId hash = kInitialHashValue) {
  auto length = strlen(id);
  return HashId(id, static_cast<int32_t>(length), hash);
}

/// @brief Returns a hash that XORs the high and low bits of seq.
///
/// @param[in] seq A number that could start from 0 and increase with every
///                unique instance of id.
inline HashedId HashIdFromSequence(SequenceId seq) {
  return static_cast<HashedId>((seq >> 32) ^ seq);
}

/// @brief Returns a HashedId of the passed in sequence and id.
///
/// @param[in] id A C-string representing the ID to hash.
/// @param[in] seq A number that could start from 0 and increase with every
///                unique instance of id.
inline HashedId HashedSequenceId(const char *id, SequenceId seq) {
  return HashId(id, HashIdFromSequence(seq));
}

/// @brief Hash a pointer to an object, of which there is guaranteed to be only
/// one (e.g. a texture).
///
/// @param[in] ptr A const void pointer to an object, of which there is
/// guranteed to be only one, that should be hashed.
///
/// @return Returns the HashId corresponding to the `ptr`.
inline HashedId HashPointer(const void *ptr) {
  // This method of integer hashing simply randomizes the integer space given,
  // in case there is an uneven distribution in the input (like is often the
  // case with pointers due to memory allocator implementations causing
  // higher and lower bits to be similar).
  // Knuth: "The Art of Computer Programming", section 6.4
  auto hash = static_cast<HashedId>(reinterpret_cast<size_t>(ptr) * 2654435761);
  assert(hash != kNullHash);
  return hash;
}

/// @brief Hash a value.
///
/// @param[in] i An integer value to be hashed.
///
/// @return Returns the HashId corresponding to the `i`.
inline HashedId HashValue(int32_t i) {
  // This method of integer hashing simply randomizes the integer space given,
  // in case there is an uneven distribution in the input (like is often the
  // case with pointers due to memory allocator implementations causing
  // higher and lower bits to be similar).
  // Knuth: "The Art of Computer Programming", section 6.4
  auto hash = static_cast<HashedId>(static_cast<size_t>(i) * 2654435761);
  assert(hash != kNullHash);
  return hash;
}

/// @brief Hash an array of C-string into a `HashId`.
///
/// @warning This function asserts if the `id` collides with `kNullHash`.
/// If you hit this assert, you will need to change your `id`.
///
/// @param[in] id An array of C-string representing the ID to hash.
/// @param[in] count A size of array.
///
/// @return Returns the HashId corresponding to the `id`.
inline HashedId HashId(const char *id[], int32_t count) {
  HashedId hash = kInitialHashValue;
  for (auto i = 0; i < count; ++i) {
    hash = HashId(id[i], hash);
  }
  // We use kNullHash for special checks, so make sure it doesn't collide.
  // If you hit this assert, sorry, please change your id :)
  assert(hash != kNullHash);
  return hash;
}

/// @brief Compare two hashes for equality.
///
/// @param[in] hash1 The first HashedId to compare for equality.
/// @param[in] hash2 The second HashedId to compare for equality.
///
/// @return Returns `true` if the two hashes were equal. Otherwise, it returns
/// `false`.
inline bool EqualId(HashedId hash1, HashedId hash2) {
  // We use hashes for comparison (rather that strcmp or pointer compare)
  // so the user can feel free to generate ids in temporary strings.
  // Not only are collisions rare because we use the entire 32bit space,
  // they are even more rare in practice, since noticable effects can only
  // occur between two adjacent elements, and then only when one of the two
  // elements was just inserted, etc.
  return hash1 == hash2;
}
/// @endcond

/// @brief A hash combine function.
/// Using a similar algorithm to the boost implementation.
/// http://www.boost.org/doc/libs/1_37_0/doc/html/hash/reference.html#boost.hash_combine
template <class T>
inline size_t HashCombine(size_t seed, const T &v) {
  return static_cast<size_t>(HashValue(*reinterpret_cast<const int32_t *>(&v)) +
                             0x9e3779b9 + (seed << 6) + (seed >> 2));
}

}  // namespace flatui

#endif  // FPL_FLATUI_UTIL_H
