#pragma once
#include <cstring>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#define MAX_BUF_SIZE 64

namespace strulib {
inline namespace {
using namespace std::string_literals;
template <typename T>
  requires requires(T it) {
    { *it };
    { ++it };
    { it != std::declval<T>() };
  }
struct nextable_itor {
  T begin, end;
  std::optional<std::remove_reference_t<decltype(*std::declval<T>())>>
  next() noexcept {
    if (begin != end)
      return *begin++;
    else
      return std::nullopt;
  }
};
} // namespace
struct fatal_inner_error : std::exception {
  const char *position;
  const char *what() const noexcept override {
    return "strulib: fatal inner error occurred at "s.append(position).c_str();
  }
  fatal_inner_error(const char *pos) : position(pos) {}
};
struct broken_format : std::exception {
  size_t index;
  broken_format(size_t index) noexcept : index(index) {}
};
struct broken_utf8 : broken_format {
  broken_utf8(size_t index) noexcept : broken_format(index) {}
  const char *what() const noexcept override {
    return "utf8 bad formatted at "s.append(std::to_string(index)).c_str();
  }
};
struct broken_utf32 : broken_format {
  broken_utf32(size_t index) noexcept : broken_format(index) {}
  const char *what() const noexcept override {
    return "utf32 bad formatted at "s.append(std::to_string(index)).c_str();
  }
};
/**
 *  @param count Number of UTF-32 characters to be written
 *  @return Number of UTF-8 characters read
 */
template <typename I, typename S>
  requires std::forward_iterator<I> &&
           std::is_convertible_v<decltype(*std::declval<I>()), const char8_t> &&
           requires(S s) {
             { s.push_back(std::declval<char32_t>()) };
           }
constexpr size_t from_u8(I from, I end, S &container, size_t count = -1) {
  return from_u8(nextable_itor<I>{from, end}, container, count);
}
template <typename T>
  requires std::same_as<T, std::remove_all_extents_t<T>>
struct pointer_container_wrapper {
  T *p;
  pointer_container_wrapper<T> &push_back(const T &x) noexcept {
    *p++ = x;
    return *this;
  }
};
/**
 *  @return Number of UTF-8 characters read
 */
template <typename I>
  requires std::forward_iterator<I> &&
           std::is_convertible_v<decltype(*std::declval<I>()), const char8_t>
constexpr size_t from_u8(I from, I end, char32_t *dst, size_t len) {
  auto pcon = pointer_container_wrapper<char32_t>{dst};
  return from_u8(nextable_itor<I>{from, end}, pcon, len);
}
/**
 *  @param count Number of UTF-32 characters to be read
 *  @return Number of UTF-8 characters written
 */
template <typename I, typename S>
  requires std::forward_iterator<I> &&
           std::is_convertible_v<decltype(*std::declval<I>()), const char8_t> &&
           requires(S s) {
             { s.push_back(std::declval<char8_t>()) };
           }
constexpr size_t to_u8(I from, I end, S &container, size_t count = -1) {
  return to_u8(nextable_itor<I>{from, end}, container, count);
}
inline namespace {
#define read_one(name)                                                         \
  auto t = src.next();                                                         \
  if (!t)                                                                      \
    break;                                                                     \
  auto name = *t
template <typename T>
inline std::remove_reference_t<decltype(*std::declval<T>().next())>
read_one_f(T &src, size_t ncast) {
  auto t = src.next();
  if (!t)
    throw broken_utf8(ncast);
  return *t;
}
#define check10__(c)                                                           \
  if (c & 0b01000000)                                                          \
    throw broken_utf8(ncast);                                                  \
  c &= 0b00111111
template <typename T, typename S>
constexpr size_t from_u8(nextable_itor<T> src, S &dst, size_t count) {
  size_t u8_n = 0;
  for (size_t ncast = 0; ncast != count; ++ncast) {
    read_one(cur);
    ++u8_n;
    if (cur < 0b10000000) {
      dst.push_back(cur);
      continue;
    }
    char8_t sec = read_one_f(src, ncast);
    check10__(sec);
    ++u8_n;
    if (cur < 0b11100000) {
      dst.push_back((cur & 0b00011111) << 6 | sec);
      continue;
    }
    char8_t trd = read_one_f(src, ncast);
    check10__(trd);
    ++u8_n;
    if (cur < 0b11110000) {
      dst.push_back((cur & 0b00001111) << 12 | sec << 6 | trd);
      continue;
    }
    char8_t fth = read_one_f(src, ncast);
    check10__(fth);
    ++u8_n;
    if (cur < 0b11111000) {
      dst.push_back((cur & 0b00000111) << 18 | sec << 12 | trd << 6 | fth);
      continue;
    }
    throw broken_utf8(ncast);
  }
  return u8_n;
}
template <typename T, typename S>
constexpr size_t to_u8(nextable_itor<T> src, S &dst, size_t count) {
  size_t ret = 0;
  for (size_t ncast = 0; ncast != count; ++ncast) {
    read_one(cur);
    if (cur < 1 << 7) {
      dst.push_back(cur);
      ++ret;
      continue;
    }
    char8_t sec = 1 << 7 | (cur & 0b00111111);
    cur >>= 6;
    if (cur < 1 << 5) {
      dst.push_back(0b11000000 | cur);
      dst.push_back(sec);
      ret += 2;
      continue;
    }
    char8_t trd = 1 << 7 | (cur & 0b00111111);
    cur >>= 6;
    if (cur < 1 << 4) {
      dst.push_back(0b11100000 | cur);
      dst.push_back(trd);
      dst.push_back(sec);
      ret += 3;
      continue;
    }
    char8_t fth = 1 << 7 | (cur & 0b00111111);
    cur >>= 6;
    if (cur < 1 << 3) {
      dst.push_back(0b11110000 | cur);
      dst.push_back(fth);
      dst.push_back(trd);
      dst.push_back(sec);
      ret += 4;
      continue;
    } else
      throw broken_utf32(ncast);
  }
  return ret;
}
} // namespace
struct stru {
  std::u8string s;
  template <typename... Ts>
  constexpr stru(Ts &&...args) : s(std::forward<Ts>(args)...) {}
  constexpr operator std::u8string() const noexcept { return s; }
  template <typename... Ts> constexpr stru &append(Ts &&...args) noexcept {
    s.append(std::forward<Ts>(args)...);
    return *this;
  }
  template <typename... Ts> constexpr stru &push_back(Ts &&...args) noexcept {
    s.push_back(std::forward<Ts>(args)...);
    return *this;
  }
  class u8itor;
  constexpr u8itor begin() const;
  constexpr const u8itor end() const noexcept;

  class recons;
  constexpr recons reconstruct();
};
inline stru operator"" _s(const char8_t *str, size_t len) noexcept {
  return stru(str, len);
}
inline std::basic_ostream<char8_t> &&operator<<(std::basic_ostream<char8_t> &os,
                                                const stru &str) noexcept {
  return std::move(os << str.s);
}
inline std::ostream &&operator<<(std::ostream &os, const stru &str) noexcept {
  return std::move(os << reinterpret_cast<const char *>(str.s.c_str()));
}
class stru::u8itor {
  std::optional<std::reference_wrapper<const std::u8string>> end_itor;
  struct data_t {
    const std::u8string &source_str;
    std::vector<char32_t> buf;
    size_t vernier;
    decltype(source_str.begin()) source_itor;
  };
  std::optional<data_t> data;
  constexpr u8itor(const std::u8string &s, bool is_end_itor) {
    if (is_end_itor)
      end_itor.emplace(s);
    else {
      auto buf = std::vector<char32_t>(
          std::min(static_cast<size_t>(MAX_BUF_SIZE), s.size()));
      data.emplace(data_t{s, buf, 0, s.cbegin()});
      buffing();
    }
  }
  constexpr void buffing() {
    try {
      data->buf.clear();
      auto offset = from_u8(data->source_itor, data->source_str.cend(),
                            data->buf, data->buf.capacity());
      data->source_itor += offset;
    } catch (broken_utf8 &e) {
      e.index += data->source_itor - data->source_str.begin();
      throw e;
    }
  }

public:
  static constexpr u8itor begin(const std::u8string &s) {
    return u8itor(s, false);
  }
  static constexpr const u8itor end(const std::u8string &s) noexcept {
    return u8itor(s, true);
  }
  constexpr char32_t operator*() {
    while (data->vernier >= data->buf.size()) {
      data->vernier -= data->buf.size();
      buffing();
    }
    return data->buf[data->vernier];
  }
  constexpr u8itor &operator++() noexcept {
    ++data->vernier;
    return *this;
  }
  constexpr u8itor operator++(int) noexcept {
    u8itor ret(*this);
    ++*this;
    return ret;
  }
  constexpr u8itor &operator+=(size_t n) noexcept {
    data->vernier += n;
    return *this;
  }
  constexpr bool operator==(const u8itor &x) const noexcept {
    if (x.end_itor)
      return &data->source_str == &x.end_itor->get() &&
             data->source_itor == data->source_str.end() &&
             data->vernier == data->buf.size();
    else
      return data->source_itor == x.data->source_itor &&
             data->vernier == x.data->vernier;
  }
  constexpr bool operator!=(const u8itor &x) const noexcept {
    return !(*this == x);
  }
};
constexpr stru::u8itor stru::begin() const { return u8itor::begin(s); }
constexpr const stru::u8itor stru::end() const noexcept {
  return u8itor::end(s);
}
class stru::recons {
  stru &old, cons;

public:
  class itor;
  constexpr recons(stru &x) noexcept : old(x), cons() {}
  constexpr itor begin();
  constexpr const itor end() noexcept;
  constexpr ~recons() noexcept { std::swap(cons.s, old.s); }
};
class stru::recons::itor {
public:
  class pseudo_pointer;
  constexpr pseudo_pointer operator*();
  constexpr itor &operator++() noexcept {
    ++data->vernier;
    ++data->old_itor;
    return *this;
  }
  constexpr itor operator++(int) noexcept {
    itor t(*this);
    ++*this;
    return t;
  }
  constexpr bool operator==(const itor &x) const noexcept {
    if (x.is_end_itor)
      return &data->rec == &x.is_end_itor->get() &&
             data->old_itor == data->rec.old.end();
    else
      return data->old_itor == x.data->old_itor;
  }
  constexpr bool operator!=(const itor &x) const noexcept {
    return !(*this == x);
  }
  constexpr itor(recons &rec, bool is_enditor) {
    if (is_enditor)
      is_end_itor.emplace(rec);
    else {
      auto buf = std::vector<char32_t>(
          std::min(static_cast<size_t>(MAX_BUF_SIZE), rec.old.s.size()));
      buf.clear();
      data.emplace(data_t{rec, buf, 0, rec.old.begin()});
    }
  }
  constexpr ~itor() noexcept {
    if (!is_end_itor)
      asyn();
  }

private:
  std::optional<std::reference_wrapper<const stru::recons>> is_end_itor;
  struct data_t {
    stru::recons &rec;
    std::vector<char32_t> buf;
    size_t vernier;
    decltype(rec.old.begin()) old_itor;
  };
  std::optional<data_t> data;
  constexpr void asyn() {
    try {
      to_u8(data->buf.cbegin(), data->buf.cend(), data->rec.cons.s);
      data->buf.clear();
    } catch (broken_utf32 &e) {
      fatal_inner_error(__func__);
    }
  }
};
class stru::recons::itor::pseudo_pointer {
  bool modified;
  char32_t &c;
  char32_t old_c;
  size_t &vernier;
  std::vector<char32_t> &buf;

public:
  constexpr pseudo_pointer(char32_t &c, char32_t old_c, size_t &vernier,
                           std::vector<char32_t> &buf)
      : modified(false), c(c), old_c(old_c), vernier(vernier), buf(buf) {}
  constexpr char32_t &operator*() noexcept {
    modified = true;
    return c;
  }
  constexpr char32_t operator&() noexcept { return old_c; }
  constexpr ~pseudo_pointer() noexcept {
    if (!modified)
      --vernier;
  }
  constexpr pseudo_pointer &push_back(char32_t c) {
    buf.push_back(c);
    ++vernier;
    return *this;
  }
};
constexpr stru::recons stru::reconstruct() { return recons(*this); }
constexpr stru::recons::itor stru::recons::begin() {
  return itor(*this, false);
}
constexpr const stru::recons::itor stru::recons::end() noexcept {
  return itor(*this, true);
}
constexpr stru::recons::itor::pseudo_pointer stru::recons::itor::operator*() {
  if (data->vernier >= data->buf.capacity()) {
    data->vernier = 0;
    asyn();
  }
  data->buf.push_back('\0');
  return pseudo_pointer(data->buf.back(), *data->old_itor, data->vernier,
                        data->buf);
}

} // namespace strulib
