#include "eden_all.hpp"
#include <vector>

using namespace eden;

#define putsln(x) std::cout << (x) << '\n'
#define putstab(x) std::cout << '\t' << (x) << '\n'

static constexpr
void test_bitset() {

}

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
void test_stack_vector() {

}

static constexpr
void test_releasing_vector() {

  {
    std::vector<u64_t> stdvec;
    releasing_vector<u64_t> edenvec;
    if (stdvec.size() not_eq edenvec.size())
    {putstab("Failed: mismatching default sizes");}

    if (stdvec.capacity() not_eq edenvec.capacity())
    {putstab("Failed: mismatching default capacities");}

    if (edenvec.release() not_eq nullptr)
    {putstab("Failed: release on empty vector returns non-null");}

    if (not ((std::span<u64_t>) edenvec).empty())
    {putstab("Failed: span created from empty vector not empty");}

    const releasing_vector<u64_t> empty_other;
    if (edenvec not_eq empty_other)
    {putstab("Failed: empty vectors compare differently");}

    if (edenvec not_eq edenvec)
    {putstab("Failed: empty vector compares not equal to itself");}

    try {
      [[maybe_unused]] auto should_throw = edenvec.at(0);
      [[maybe_unused]] auto should_throw2 = edenvec.at(9);
      putstab("Failed: at method on empty vector did not throw");
    }
    catch (std::exception& e) {}
  }

  {
    std::vector<u64_t> stdvec;
    releasing_vector<u64_t> edenvec;
    releasing_vector<u64_t> edenvec_extra;
    for (u64_t i{}; i<256; ++i) {
      stdvec.push_back(i);
      edenvec.push_back(i);
      edenvec_extra.emplace_back(i);
      if (stdvec.size() not_eq edenvec.size()) {
        putstab("Failed: differing vector sizes.");
        break;
      }

      const bool same = (edenvec[i] == edenvec_extra[i]) and (edenvec[i] == stdvec[i]);
      if (not same) {
        putstab("Failed: vectors contain dissimilar elements.");
        break;
      }
    }

    if (edenvec not_eq edenvec_extra)
      putstab("Failed: identical vectors compare differently.");

    if (edenvec.front() not_eq edenvec[0])
      putstab("Failed: vector.front() doesn't equal first element");

    if (edenvec.back() not_eq edenvec[edenvec.size() - 1])
      putstab("Failed: vector.back() doesn't equal last element");

    std::reverse(stdvec.begin(), stdvec.end());
    std::reverse(edenvec.begin(), edenvec.end());

    std::sort(stdvec.begin(), stdvec.end());
    std::sort(edenvec.begin(), edenvec.end());

    for (auto i{0uz}; i<edenvec.size(); ++i) {
      if (stdvec[i] not_eq edenvec[i]) {
        putstab("Failed: vector failed to reverse.");
        break;
      }
    }

    if (*edenvec.begin() not_eq edenvec.front())
      putstab("Failed: front element not equal to begin iterator.");
    if (*(edenvec.rend() - 1) not_eq edenvec.front())
      putstab("Failed: front element not equal to reverse end iterator plus 1.");
    if (*(edenvec.end() - 1) not_eq edenvec.back())
      putstab("Failed: last element not equal to end iterator minus 1.");
    if (*edenvec.rbegin() not_eq edenvec.back())
      putstab("Failed: last element not equal to reverse begin iterator.");

    stdvec.clear(); edenvec.clear(); edenvec_extra.clear();
    if (not(edenvec.empty() and edenvec_extra.empty()))
      putstab("Failed: cleared vector does not report as empty.");

    if (edenvec.capacity() == 0)
      putstab("Failed: cleared vector reports an empty capacity");

  }


}


int main() {
  putsln("---- Releasing Vector ----");
  test_releasing_vector();
  putsln("---- Passed! ----");

  putsln("");

  putsln("---- Stack Vector ----");
  test_stack_vector();
  putsln("---- Passed! ----");

  putsln("");

  putsln("---- Owned Pointer ----");
  test_owned_ptr();
  putsln("---- Passed! ----");

  putsln("");

  putsln("---- Concepts ----");
  test_concepts();
  putsln("---- Passed! ----");

  putsln("");

  putsln("---- Type Traits ----");
  test_type_traits();
  putsln("---- Passed! ----");

  putsln("");

  putsln("---- Bitset ----");
  test_bitset();
  putsln("---- Passed! ----");
}