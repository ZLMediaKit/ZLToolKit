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
#include <iterator>
#include <cstring>
#include <iostream>
#include <utility>
#if defined(__clang__) || defined(__GNUC__)
  #define CPP_STANDARD __cplusplus
#elif defined(_MSC_VER)
  #define CPP_STANDARD _MSVC_LANG
#endif

#ifdef __cplusplus
  #if CPP_STANDARD >= 201402L
    #define BASIC_STRING_VIEW_CONSTEXPR constexpr
  #else
    #define BASIC_STRING_VIEW_CONSTEXPR
  #endif
#endif


namespace toolkit{
    template <typename Char>
    class basic_string_view {
    public:
        using size_type = typename std::basic_string<Char>::size_type;
        using value_type = Char;
        using iterator = const Char*;
        using const_iterator = const iterator;
        using this_type = basic_string_view<value_type>;
        static constexpr const size_type npos = std::basic_string<Char>::npos;
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
        BASIC_STRING_VIEW_CONSTEXPR auto cbegin() const -> const_iterator { return data_;}
        BASIC_STRING_VIEW_CONSTEXPR auto end() const -> iterator { return data_ + size_; }
        BASIC_STRING_VIEW_CONSTEXPR auto cend() const -> const_iterator { return data_ + size_;}
        BASIC_STRING_VIEW_CONSTEXPR auto operator[](size_t pos) const -> const Char& {
            return data_[pos];
        }
        BASIC_STRING_VIEW_CONSTEXPR auto operator = (const basic_string_view<Char>& other) -> this_type& {
            this->data_ = other.data();
            this->size_ = other.size();
            return *this;
        }

        void remove_prefix(size_t n) {
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

        BASIC_STRING_VIEW_CONSTEXPR auto substr(size_type pos = 0, size_type count = std::basic_string<Char>::npos) const -> this_type {
            if( count == std::basic_string<Char>::npos )
              count = size_ - pos;
            if( pos >= size_ || pos + count > size_)
              throw std::out_of_range("substr is out of range");
            return this_type(data_ + pos, count);
        }

        size_type find(basic_string_view<Char> v, size_type pos = 0) const noexcept{
          if( pos >= size_ || v.size() > size_ - pos) return npos;
          const char* find_it = strstr(data_ + pos, v.data());
          if(!find_it) return npos;
          if(find_it < data_ || find_it >= (data_ + size_))return npos;
          return find_it - data_;
        }

        size_type find(Char ch, size_type pos = 0) const {
            if( pos >= size_ ) return npos;
            const Char* find_it = strchr(data_ + pos, ch);
            if(!find_it)return npos;
            if( find_it < data_ || find_it >= data_ + size_)return npos;
            return find_it - data_;
        }

        size_type find(const Char* s, size_type pos, size_type count) const{
            return find(this_type(s, count), pos);
        }

        size_type find(const Char* s, size_type pos = 0) const{
            return find(this_type(s, strlen(s)), pos);
        }

        size_type find_first_of(const basic_string_view& v, size_type pos = 0) const noexcept{
            if(pos >= size_)
              return npos;
            const Char* find_it = strpbrk(data_ + pos, v.data());
            if(!find_it)
              return npos;
            if(find_it < data_ || find_it >= data_ + size_)
              return npos;
            return find_it - data_;
        }

        size_type find_first_of(const Char* s, size_type pos, size_type count) const{
            return find_first_of(this_type(s, count), pos);
        }


        size_type find_first_of(const Char* s, size_type pos = 0) const{
            return find_first_of(this_type(s, strlen(s)), pos);
        }


        BASIC_STRING_VIEW_CONSTEXPR size_type find_first_of(Char c, size_type pos = 0) const noexcept{
            return find(c,  pos);
        }

        friend std::ostream& operator << (std::ostream& os, const this_type& _this){
            os.write(_this.data_, _this.size_);
            return os;
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

      private:
        const Char* data_;
        size_t size_;
    };

    using string_view = basic_string_view<char>;
}


namespace std{
  //ÌØ»¯string_viewµÄhash
  template<> struct hash<toolkit::basic_string_view<char>>{
      size_t operator()(const toolkit::basic_string_view<char>& view){
         constexpr size_t offset_basis = 14695981039346656037ULL;
         constexpr size_t prime        = 1099511628211ULL;
         size_t val = offset_basis;
         for(size_t idx = 0; idx < view.size();++idx){
           val ^= static_cast<size_t>(view[idx]);
           val *= prime;
         }
         return val;
      }
  };
}

#endif