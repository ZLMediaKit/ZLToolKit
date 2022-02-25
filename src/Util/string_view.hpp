/**
* Formatting library for C++ - the core API for char/UTF-8
*
* Copyright (c) 2012 - present, Victor Zverovich
* All rights reserved.
*
* For the license information refer to format.h.
* 
* 
*  An implementation of ``std::basic_string_view`` for pre-C++17. It provides a
*  subset of the API. ``fmt::basic_string_view`` is used for format strings even
*  if ``std::string_view`` is available to prevent issues when a library is
*  compiled with a different ``-std`` option than the client code (which is not
*  recommended).
*/
#ifndef OAHO_BASIC_STRING_VIEW_FROM_FMT
#define OAHO_BASIC_STRING_VIEW_FROM_FMT
#include <type_traits>
#include <string>


#ifdef _has_cpp_version_14
    #define BASIC_STRING_VIEW_CONSTEXPR constexpr
#else
    #define BASIC_STRING_VIEW_CONSTEXPR
#endif // _has_cpp_version_14



namespace toolkit{
    template <typename Char> class basic_string_view {
    private:
        const Char* data_;
        size_t size_;

    public:
        using value_type = Char;
        using iterator = const Char*;

        BASIC_STRING_VIEW_CONSTEXPR basic_string_view() noexcept : data_(nullptr), size_(0) {}

        /** Constructs a string reference object from a C string and a size. */
        BASIC_STRING_VIEW_CONSTEXPR basic_string_view(const Char* s, size_t count) noexcept
            : data_(s),
              size_(count) {}

        /**
      \rst
      Constructs a string reference object from a C string computing
      the size with ``std::char_traits<Char>::length``.
      \endrst
     */
        BASIC_STRING_VIEW_CONSTEXPR inline basic_string_view(const Char* s) : data_(s), size_(std::char_traits<Char>::length(s)){
        }

        /** Constructs a string reference from a ``std::basic_string`` object. */
        template <typename Traits, typename Alloc>
        BASIC_STRING_VIEW_CONSTEXPR basic_string_view(
                const std::basic_string<Char, Traits, Alloc>& s) noexcept
            : data_(s.data()),
              size_(s.size()) {}

        template <typename S, typename = typename std::enable_if<std::is_same<S, basic_string_view<Char>>::value>::type>
        BASIC_STRING_VIEW_CONSTEXPR basic_string_view(S s) noexcept : data_(s.data()),size_(s.size()) {}

        /** Returns a pointer to the string data. */
        BASIC_STRING_VIEW_CONSTEXPR auto data() const -> const Char* { return data_; }
        /** Returns the string size. */
        BASIC_STRING_VIEW_CONSTEXPR auto size() const -> size_t { return size_; }
        BASIC_STRING_VIEW_CONSTEXPR auto begin() const -> iterator { return data_; }
        BASIC_STRING_VIEW_CONSTEXPR auto end() const -> iterator { return data_ + size_; }
        BASIC_STRING_VIEW_CONSTEXPR auto operator[](size_t pos) const -> const Char& {
            return data_[pos];
        }
        BASIC_STRING_VIEW_CONSTEXPR void remove_prefix(size_t n) {
            data_ += n;
            size_ -= n;
        }
        // Lexicographically compare this string reference to other.
        BASIC_STRING_VIEW_CONSTEXPR auto compare(basic_string_view<Char> other) const -> int {
            size_t str_size = size_ < other.size_ ? size_ : other.size_;
            int result = std::char_traits<Char>::compare(data_, other.data_, str_size);
            if (result == 0)
                result = size_ == other.size_ ? 0 : (size_ < other.size_ ? -1 : 1);
            return result;
        }

        BASIC_STRING_VIEW_CONSTEXPR friend auto operator == (basic_string_view<Char> lhs,
                                                           basic_string_view<Char> rhs)
                -> bool {
            return lhs.compare(rhs) == 0;
        }
        friend auto operator!=(basic_string_view<Char> lhs, basic_string_view<Char> rhs) -> bool {
            return lhs.compare(rhs) != 0;
        }
        friend auto operator<(basic_string_view<Char> lhs, basic_string_view<Char> rhs) -> bool {
            return lhs.compare(rhs) < 0;
        }
        friend auto operator<=(basic_string_view<Char> lhs, basic_string_view<Char> rhs) -> bool {
            return lhs.compare(rhs) <= 0;
        }
        friend auto operator>(basic_string_view<Char> lhs, basic_string_view<Char> rhs) -> bool {
            return lhs.compare(rhs) > 0;
        }
        friend auto operator>=(basic_string_view<Char> lhs, basic_string_view<Char> rhs) -> bool {
            return lhs.compare(rhs) >= 0;
        }
    };

    using string_view = basic_string_view<char>;
}

#endif