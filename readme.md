# Stru

Stru is an exceptionally lightweight wrapper for `std::u8string`.

The program leverages the capabilities of C++20 and has undergone thorough testing under this standard. However, it is designed to seamlessly function in environments below C++20, provided that all template constraints have been removed.

## Features

### Input Iterator
- Efficient iteration over UTF-8 strings, yielding `char32_t` elements.
- Buffering is employed for format conversion, with a default buffer size of 64 `char32_t`s.
```cpp
for (char32_t c : stru_instance) {
    // Perform operations...
}
```

### Output Iterator
- It is essential to note that this operation results in the reconstruction of the string.
- Similar to the input iterator, output iteration is buffered.

```cpp
for (auto pc /*not an actual pointer*/ : stru_instance.reconstruct()) {
    // Perform actions, e.g., somefunc(*pc)

    // If no assignment (actually dereference) is made, the current character
    // will be discarded. Therefore, choose either:
    //     *pc = new_character;
    // or
    //     *pc = &pc /*`&` gets the original character*/;

    // If one character isn't sufficient, you can also do:
    //     pc.push_back(second_char);
    //     pc.push_back(third_char);
    //     ...
    // Note that it may possibly result in the growth of the buffer.
}
```

### Print
- Direct printing via `std::basic_ostream<char>` or `std::basic_ostream<char8_t>`.

```cpp
std::cout << stru_instance ...;
some_basic_ostream_of_char8_t << stru_instance ...;
```

### Format Conversion

Template functions are provided to facilitate format conversion between UTF-8 and UTF-32 for arbitrary containers.
```cpp
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
size_t from_u8(I from, I end, S &container, size_t count = -1);

/**
 *  @return Number of UTF-8 characters read
 */
template <typename I>
requires std::forward_iterator<I> &&
         std::is_convertible_v<decltype(*std::declval<I>()), const char8_t>
size_t from_u8(I from, I end, char32_t *dst, size_t len);

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
size_t to_u8(I from, I end, S &container, size_t count = -1);
```
