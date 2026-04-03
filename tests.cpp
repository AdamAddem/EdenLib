#include "eden_all.hpp"
#include <vector>
#include <iostream>

using namespace eden;

#define putsln(x) std::cout << (x) << '\n'
#define putstab(x) std::cout << '\t' << (x) << '\n'

#define EXPECT(x, msg) if( not (x) ) putstab(msg);
#define EXPECT_BREAK(x, msg) if( not (x) ) {putstab(msg); break;}
#define REJECT(x, msg) if( x ) putstab(msg);

#include <random>
static std::random_device rd;
static std::mt19937 gen(rd());

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
    EXPECT(stdvec.size() == edenvec.size(), "Failed: mismatching default sizes")
    EXPECT(stdvec.capacity() == edenvec.capacity(), "Failed: mismatching default capacities");
    EXPECT(edenvec.release() == nullptr, "Failed: release on empty vector returns non-null");
    EXPECT(((std::span<u64_t>) edenvec).empty(), "Failed: span created from empty vector not empty");

    const releasing_vector<u64_t> empty_other;
    EXPECT(edenvec == empty_other, "Failed: empty vectors compare differently");
    EXPECT(edenvec == edenvec, "Failed: empty vector compares not equal to itself");

    try {
      [[maybe_unused]] auto should_throw = edenvec.at(0);
      [[maybe_unused]] auto should_throw2 = edenvec.at(9);
      putstab("Failed: at method on empty vector did not throw");
    }
    catch (std::exception& e) {}
  }

  //loop from 0 - 255
  {
    static constexpr sz_t loop_sz{10'000};
    std::vector<u64_t> stdvec;
    releasing_vector<u64_t> edenvec1;
    releasing_vector<u64_t> edenvec2;
    releasing_vector<u64_t> edenvec3;
    u64_t array_version[loop_sz];
    for (u64_t i{}; i<loop_sz; ++i) {
      const auto k = gen();
      stdvec.push_back(k);
      edenvec1.push_back(k);
      edenvec2.emplace_back(k);
      edenvec3.emplace_back(k);
      array_version[i] = k;

      EXPECT_BREAK(stdvec.size() == edenvec1.size(), "Failed: differing vector sizes.");
      EXPECT_BREAK((edenvec1[i] == edenvec2[i]) and (edenvec1[i] == stdvec[i]), "Failed: vectors contain dissimilar elements.");
    }

    EXPECT(edenvec1 == edenvec2, "Failed: identical vectors compare differently.");
    EXPECT(edenvec1.front() == edenvec1[0], "Failed: vector.front() doesn't equal first element");
    EXPECT(edenvec1.back() == edenvec1[edenvec1.size() - 1], "Failed: vector.back() doesn't equal last element");

    std::reverse(stdvec.begin(), stdvec.end());
    std::reverse(edenvec1.begin(), edenvec1.end());

    for (auto i{0uz}; i<edenvec1.size(); ++i) {
      EXPECT_BREAK(stdvec[i] == edenvec1[i], "Failed: vector failed to reverse.");
    }

    std::sort(edenvec1.begin(), edenvec1.end());

    EXPECT(*edenvec1.begin() == edenvec1.front(), "Failed: front element not equal to begin iterator.");
    EXPECT(*(edenvec1.rend() - 1) == edenvec1.front(), "Failed: front element not equal to reverse end iterator plus 1.");
    EXPECT(*(edenvec1.end() - 1) == edenvec1.back(), "Failed: last element not equal to end iterator minus 1.");
    EXPECT(*edenvec1.rbegin() == edenvec1.back(), "Failed: last element not equal to reverse begin iterator.");

    const auto span1 = edenvec1.to_span();
    auto data1 = edenvec1.release();
    auto data2 = edenvec2.release();
    auto data3 = edenvec3.release();
    for (auto i{0uz}; i<loop_sz; ++i) {
      EXPECT_BREAK(data2[i] == array_version[i], "Failed: owned_ptr released data not equivalent to stack array.");
      EXPECT_BREAK(data3[i] == array_version[i], "Failed: data_handle released data not equivalent to stack array.");
    }
    EXPECT(span1.data() == data1, "Failed: span data not equivalent to original data.");

    stdvec.clear(); edenvec1.clear(); edenvec2.clear();
    EXPECT(edenvec1.empty() and edenvec2.empty() and edenvec3.empty(), "Failed: cleared vector does not report as empty.");
    std::allocator<u64_t> alloc;
    releasing_vector<u64_t>::destroy_and_deallocate(std::move(data1));
    releasing_vector<u64_t>::destroy_and_deallocate(std::move(data2));
    edenvec3.destroy_and_deallocate(std::move(data3));
  }

  //string specialization
  {
    std::string std_str("Hello!");
    releasing_string my_str("Hello!");
    EXPECT(my_str == std_str, "Failed: releasing_string compares not equal to identical std::string.");
    EXPECT(std::string(my_str) == std_str, "Failed: operator std::string output compares not equal to identical std::string.");
    EXPECT(my_str.to_stdstring() == std_str, "Failed: to_stdstring() output compares not equal to identical std::string.");
    EXPECT(std::string_view(my_str) == std_str, "Failed: operator std::string_view output compares not equal to identical std::string.");
    EXPECT(my_str.to_stringview() == std_str, "Failed: to_string_view() output compares not equal to identical std::string.");
    EXPECT(my_str == "Hello!", "Failed: releasing_string compares not equal to identical string literal.");

    std_str = "Goodbye!";
    REJECT(my_str == std_str, "Failed: releasing_string compares equal to differing std::string.");
    REJECT(std::string(my_str) == std_str, "Failed: operator std::string output compares equal to differing std::string.");
    REJECT(my_str.to_stdstring() == std_str, "Failed: to_stdstring() output compares equal to differing std::string.");
    REJECT(std::string_view(my_str) == std_str, "Failed: operator std::string_view output compares equal to differing std::string.");
    REJECT(my_str.to_stringview() == std_str, "Failed: to_string_view() output compares equal to differing std::string.");
    REJECT(my_str == "Goodbye!", "Failed: releasing_string compares not equal to identical string literal.");
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