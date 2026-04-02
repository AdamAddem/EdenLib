#include "eden_all.hpp"
#include <vector>
#include <iostream>

using namespace eden;

#define putsln(x) std::cout << (x) << '\n'
#define putstab(x) std::cout << '\t' << (x) << '\n'

static constexpr
void test_type_traits() {

}

static constexpr
void test_concepts() {

}

static constexpr
void test_owned_ptr() {

}

static constexpr
void test_releasing_vector() {

  //default initialzation
  {
    std::vector<u64_t> stdvec;
    releasing_vector<u64_t> edenvec;
    if (stdvec.size() not_eq edenvec.size())
      putstab("Failed: mismatching default sizes");

    if (stdvec.capacity() not_eq edenvec.capacity())
      putstab("Failed: mismatching default capacities");

    if (edenvec.release() not_eq nullptr)
      putstab("Failed: release on empty vector returns non-null");

    if (not ((std::span<u64_t>) edenvec).empty())
      putstab("Failed: span created from empty vector not empty");

    const releasing_vector<u64_t> empty_other;
    if (edenvec not_eq empty_other)
      putstab("Failed: empty vectors compare differently");

    if (edenvec not_eq edenvec)
      putstab("Failed: empty vector compares not equal to itself");

    try {
      [[maybe_unused]] auto should_throw = edenvec.at(0);
      [[maybe_unused]] auto should_throw2 = edenvec.at(9);
      putstab("Failed: at method on empty vector did not throw");
    }
    catch (std::exception& e) {}
  }

  //loop from 0 - 255
  {
    std::vector<u64_t> stdvec;
    releasing_vector<u64_t> edenvec1;
    releasing_vector<u64_t> edenvec2;
    releasing_vector<u64_t> edenvec3;
    u64_t array_version[256];
    for (u64_t i{}; i<256; ++i) {
      stdvec.push_back(i);
      edenvec1.push_back(i);
      edenvec2.emplace_back(i);
      edenvec3.emplace_back(i);
      array_version[i] = i;

      if (stdvec.size() not_eq edenvec1.size()) {
        putstab("Failed: differing vector sizes.");
        break;
      }

      if ((edenvec1[i] not_eq edenvec2[i]) or (edenvec1[i] not_eq stdvec[i])) {
        putstab("Failed: vectors contain dissimilar elements.");
        break;
      }

    }

    if (edenvec1 not_eq edenvec2)
      putstab("Failed: identical vectors compare differently.");

    if (edenvec1.front() not_eq edenvec1[0])
      putstab("Failed: vector.front() doesn't equal first element");

    if (edenvec1.back() not_eq edenvec1[edenvec1.size() - 1])
      putstab("Failed: vector.back() doesn't equal last element");

    std::reverse(stdvec.begin(), stdvec.end());
    std::reverse(edenvec1.begin(), edenvec1.end());

    for (auto i{0uz}; i<edenvec1.size(); ++i) {
      if (stdvec[i] not_eq edenvec1[i]) {
        putstab("Failed: vector failed to reverse.");
        break;
      }
    }

    std::sort(edenvec1.begin(), edenvec1.end());

    if (edenvec1 not_eq edenvec2)
      putstab("Failed: reversed then sorted list not equal to original.");
    if (*edenvec1.begin() not_eq edenvec1.front())
      putstab("Failed: front element not equal to begin iterator.");
    if (*(edenvec1.rend() - 1) not_eq edenvec1.front())
      putstab("Failed: front element not equal to reverse end iterator plus 1.");
    if (*(edenvec1.end() - 1) not_eq edenvec1.back())
      putstab("Failed: last element not equal to end iterator minus 1.");
    if (*edenvec1.rbegin() not_eq edenvec1.back())
      putstab("Failed: last element not equal to reverse begin iterator.");

    const auto span1 = edenvec1.to_span();
    auto data1 = edenvec1.release();
    auto data2 = edenvec2.release();
    auto data3 = edenvec3.release();
    for (auto i{0uz}; i<256; ++i) {
      if (data1[i] not_eq array_version[i]) {
        putstab("Failed: raw released data not equivalent to stack array.");
        break;
      }
      if (data2[i] not_eq array_version[i]) {
        putstab("Failed: owned_ptr released data not equivalent to stack array.");
        break;
      }
      if (data3[i] not_eq array_version[i]) {
        putstab("Failed: data_handle released data not equivalent to stack array.");
        break;
      }
      if (span1[i] not_eq array_version[i]) {
        putstab("Failed: span not equivalent to array version.");
        break;
      }
    }

    if (span1.data() not_eq data1)
      putstab("Failed: span data not equivalent to original data.");

    stdvec.clear(); edenvec1.clear(); edenvec2.clear();
    if (not(edenvec1.empty() and edenvec2.empty() and edenvec3.empty()))
      putstab("Failed: cleared vector does not report as empty.");

    std::allocator<u64_t> alloc;
    releasing_vector<u64_t>::destroy_and_deallocate(data1);
    releasing_vector<u64_t>::destroy_and_deallocate(data2);
    edenvec3.destroy_and_deallocate(data3);
  }

}

int main() {
  putsln("---- Releasing Vector ----");
  test_releasing_vector();
  putsln("---- Releasing Vector ----");

  putsln("");

  putsln("---- Owned Pointer ----");
  test_owned_ptr();
  putsln("---- Owned Pointer ----");

  putsln("");

  putsln("---- Concepts ----");
  test_concepts();
  putsln("---- Concepts ----");

  putsln("");

  putsln("---- Type Traits ----");
  test_type_traits();
  putsln("---- Type Traits ----");
}