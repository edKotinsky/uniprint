#ifndef UNIPRINT_HPP
#define UNIPRINT_HPP

#include "gfp.hpp"
#include <cstddef>
#include <cstdio>
#include <ostream>
#include <type_traits>
#include <utility>

namespace uniprint {

namespace args {

template <typename T> class argument {
protected:
  T m_value;

public:
  explicit argument(T value) : m_value(value) {}

  argument() = delete;

  T get() { return m_value; }

  T const get() const { return m_value; }

  operator T() { return m_value; }

  operator const T() const { return m_value; }
};

class dummy_base {};

class sep : public argument<char const *>, dummy_base {
public:
  using type = char const *;

  explicit sep(type value) : argument<type>(value) {}
};

class end : public argument<char const *>, dummy_base {
public:
  using type = char const *;

  explicit end(type value) : argument<type>(value) {}
};

class file : public argument<std::ostream &>, dummy_base {
public:
  using type = std::ostream &;

  explicit file(type stream) : argument<type>(stream) {}
};

class flush : public argument<bool>, dummy_base {
public:
  using type = bool;

  explicit flush(type value) : argument<type>(value) {}
};

} // namespace args

namespace details {

using namespace gfp::details;

template <class A>
struct is_argument
    : std::integral_constant<bool,
                             std::is_base_of<args::dummy_base, A>::value> {};

template <typename T, bool Cond, T IfTrue, T IfFalse> struct conditional {
  using type = T;
  static constexpr T value = IfTrue;
};

template <typename T, T IfTrue, T IfFalse>
struct conditional<T, false, IfTrue, IfFalse> {
  using type = T;
  static constexpr T value = IfFalse;
};
} // namespace details

class print {
private:
  struct m_print_args {
    // little trick: store pointer to `std::ostream`, not reference. See
    // `std::reference_wrapper`
    std::ostream *file = nullptr;
    char const *sep = " ";
    char const *end = "\n";
    bool flush = false;

    explicit m_print_args(std::ostream *f) : file(f) {}

    m_print_args() = default;
  };

  m_print_args m_args;

  // SFINAE type-based if condition:
  // if T is not an argument class, print the value
  template <typename T>
  auto check_print(m_print_args &args, T &&value) -> typename std::enable_if<
      !std::is_base_of<args::dummy_base, typename std::decay<T>::type>::value,
      void>::type {
    (*args.file) << value;
  }

  // otherwise do nothing
  template <typename T>
  auto check_print(m_print_args &, T &&) -> typename std::enable_if<
      details::is_argument<typename std::decay<T>::type>::value>::type {}

  // inductive case 1: print separator on each even index
  template <std::size_t Index, typename Head, typename... Rest>
  auto print_impl(m_print_args &args, Head &&head, Rest &&...rest) ->
      typename std::enable_if<Index % 2 == 0>::type {
    // if it is not a last type in a pack or it is not an argument class
    if (sizeof...(Rest) != 0 ||
        !std::is_base_of<args::dummy_base,
                         typename std::decay<Head>::type>::value)
      (*args.file) << args.sep;
    print_impl<Index + 1>(args, std::forward<Head>(head),
                          std::forward<Rest>(rest)...);
  }

  // inductive case 2: on odd index, print head and recursively
  // print the rest
  template <std::size_t Index, typename Head, typename... Rest>
  auto print_impl(m_print_args &args, Head &&head, Rest &&...rest) ->
      typename std::enable_if<Index % 2 != 0, void>::type {
    check_print(args, std::forward<Head>(head));
    if (args.flush)
      (*args.file) << std::flush;

    // check if the Head type is an argument type
    // if so, next index will be equal to Index, otherwise Index + 1
    using head_t = typename std::decay<Head>::type;
    constexpr bool is_base_arg = details::is_argument<head_t>::value;
    constexpr std::size_t next_idx =
        details::conditional<std::size_t, is_base_arg, Index, Index + 1>::value;

    print_impl<next_idx>(args, std::forward<Rest>(rest)...);
  }

  // base case: no values. Just print the end symbol
  template <std::size_t Index> void print_impl(m_print_args &args) {
    (*args.file) << args.end;
    if (args.flush)
      (*args.file) << std::flush;
  }

public:
  explicit print(std::ostream &default_file) : m_args(&default_file) {}

  print() = delete;

  print(print &) = delete;

  print &operator=(print &) = delete;

  template <typename... Types> void operator()(Types &&...args) {
    m_print_args print_args(m_args);

    auto flush =
        gfp::get_from_pack<args::flush>{}(std::forward<Types>(args)...);

    gfp::call_match(
        flush, [&print_args](args::flush a) { print_args.flush = a.get(); },
        [](gfp::none_type) {});

    auto sep = gfp::get_from_pack<args::sep>{}(std::forward<Types>(args)...);

    gfp::call_match(
        sep, [&print_args](args::sep a) { print_args.sep = a.get(); },
        [](gfp::none_type) {});

    auto end = gfp::get_from_pack<args::end>{}(std::forward<Types>(args)...);

    gfp::call_match(
        end, [&print_args](args::end a) { print_args.end = a.get(); },
        [](gfp::none_type) {});

    auto file = gfp::get_from_pack<args::file>{}(std::forward<Types>(args)...);

    gfp::call_match(
        file, [&print_args](args::file a) { print_args.file = &a.get(); },
        [](gfp::none_type) {});

    print_impl<1>(print_args, std::forward<Types>(args)...);
  }
};

} // namespace uniprint

#endif
