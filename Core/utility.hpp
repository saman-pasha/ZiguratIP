
#ifndef __UTILITY_HPP__
#define __UTILITY_HPP__

#include <ctime>
#include <list>
#include <tuple>
#include <string>
#include <sstream>
#include <ostream>
#include <vector>
#include <cstdint>
#include <algorithm>

// BSD/Darwin declare the byte order conversions as macros in <sys/_endian.h>,
// which would rewrite the member declarations below. Utility provides its own
// implementations, so the macros are not needed anywhere in ZiguratIP.
#undef htons
#undef ntohs
#undef htonl
#undef ntohl
#undef htonll
#undef ntohll

namespace Zigurat
{

  class Utility
  {
  public:
    static std::string os_name();
    static std::string user_home();
    static std::string env_var(std::string);

    // The IEEE bits of a double, as the integer a text page can carry
    // without rounding: the default decimal rendering keeps six digits and
    // a model weight does not survive that, where the bit pattern loses
    // nothing and reads back with one memcpy.
    static int64_t double_bits(double);

    template <typename B, typename E> static B constexpr pow(B base, E exp)
    {
      return (exp == 0) ? 1 : Utility::pow<B, E>(base, exp - 1) * base;
    }
    
    static bool in(char, const std::string);

    template <typename I> static bool in(I &&item, const std::vector<I> &item_array) {
      for (I &iitem : item_array) {
	if (item == iitem)
	  return true;
      }
      return false;
    }

    template <typename I> static bool in(const I &item, const std::vector<I> &item_array) {
      for (const I &iitem : item_array) {
	if (item == iitem)
	  return true;
      }
      return false;
    }

    template <typename T> static std::list<T>* set_intersection(std::list<T>* first, std::list<T>* second) {
      std::list<T>* _intersection = new std::list<T>();
      std::set_intersection(first->begin(), first->end(), second->begin(), second->end(), std::back_inserter(&_intersection));
      return _intersection;
    }

    template <typename T> static std::list<T>* set_union(std::list<T>* first, std::list<T>* second) {
      std::list<T>* _union = new std::list<T>();
      std::set_union(first->begin(), first->end(), second->begin(), second->end(), std::back_inserter(&_union));
      return _union;
    }

    template <typename T> static std::list<T>* set_difference(std::list<T>* first, std::list<T>* second) {
      std::list<T>* _difference = new std::list<T>();
      std::set_union(first->begin(), first->end(), second->begin(), second->end(), std::back_inserter(&_difference));
      return _difference;
    }

    static std::string trim_left(const std::string&, char = ' ');
    static std::string trim_right(const std::string&, char = ' ');
    static std::string trim(const std::string&, char = ' ');
    static std::vector<std::string> split(const std::string&);
    static std::vector<std::string> split(const std::string&, char);
    static std::string to_lower(const std::string&);
    static std::string to_upper(const std::string&);
    static std::string config_path(std::string);

    // A compiler diagnostic with the source file in front of it, in the form
    // every editor already parses:
    //
    //     demo/01-schema.parsi:4:3: syntax error at line 4 column 3 near 'RETRN'
    //
    // The parser says "at line 4 column 3" and has no idea what the caller
    // called the file, so the two halves only meet here. THE ORIGINAL TEXT IS
    // KEPT WHOLE and the position is added rather than substituted: rewording a
    // diagnostic to fit a format is how the detail that made it useful gets
    // lost.
    //
    // A message with no position in it gets "file: " and no numbers, because
    // 0:0 sends the reader to the top of a file that is not the problem.
    //
    // It is here, and not in one of the two programs that need it, because both
    // parsi and parsic answer an editor and their answers have to look the
    // same -- the same command parses both.
    static std::string diagnostic(const std::string&, const std::string&);

    // Enough of a filesystem to keep a directory of small files. C++11 has no
    // <filesystem>, and pulling one in for four calls would cost more than it
    // saves.
    static bool file_exists(const std::string&);
    static bool make_directory(const std::string&);
    static bool remove_file(const std::string&);

    // The plain names in a directory, without "." and "..", in no useful
    // order. An unreadable or missing directory yields an empty list rather
    // than an error: nothing registered and nowhere to register are the same
    // answer to everyone who asks.
    static std::vector<std::string> directory_files(const std::string&);

    template <typename ..._Args>
    void ignore_pack(_Args&&...) { }

    template <std::size_t ..._Indices> 
    struct index_sequence { };

    template <std::size_t N, std::size_t ..._Indices>
    struct build_sequence : public build_sequence<N - 1, N - 1, _Indices...> { };

    template <std::size_t ..._Indices>
    struct build_sequence<0, _Indices...>
    {
      using type = index_sequence<_Indices...>;
    };

    template <std::size_t N>
    using make_index_sequence = typename build_sequence<N>::type;

    template <typename ..._Args>
    using index_sequence_for = typename build_sequence<sizeof(_Args)...>::type;

    template <typename _First, typename ..._Rest, std::size_t ..._Indices>
    std::tuple<_Rest...> tuple_cdr(std::tuple<_First, _Rest...> tuple)
    {
      return std::make_tuple(std::get<_Indices + 1>(tuple)...);
    }

    template <typename _First, typename ..._Rest>
    std::tuple<_Rest...> tuple_cdr(std::tuple<_First, _Rest...> tuple)
    {
      return tuple_cdr<_First, _Rest..., index_sequence_for<_Rest...> >(tuple);
    }

    static std::size_t generate_id();    
    static std::string octet_as_hex(const uint8_t*, size_t);

    template <typename T>
    static T min(T lhs, T rhs)
    {
      if (lhs < rhs)
	return lhs;
      return rhs;
    }

    template <typename T>
    static T max(T lhs, T rhs)
    {
      if (lhs > rhs)
	return lhs;
      return rhs;
    }

    template <typename T>
    static T gcd(const T& m, const T& n)
    {
      if (m < n) return Utility::gcd(n, m);
      T r = m % n;
      if (r.is_zero()) return n;
      return Utility::gcd(n, r);
    }

    template <typename T>
    static T lcm(const T& a, const T& b)
    {
      return (a * b) / Utility::gcd(a, b);
    }

    // %Y rather than %G: the pair has to round trip, and strptime does not
    // accept the ISO week-based year on every platform.
    static std::string time_to_string(time_t, std::string = "%Y/%m/%d %H:%M:%S", bool = true);
    static time_t string_to_time(std::string, std::string = "%Y/%m/%d %H:%M:%S", bool = true);

    static std::string pad_left(std::string, size_t, char = ' ');
    static std::string pad_right(std::string, size_t, char = ' ');

    static uint16_t htons(uint16_t);
    static uint16_t ntohs(uint16_t);
    static uint32_t htonl(uint32_t);
    static uint32_t ntohl(uint32_t);
    static uint64_t htonll(uint64_t);
    static uint64_t ntohll(uint64_t);

    // Unpredictable octets, straight from the kernel pool. Everything that
    // needs to be unguessable -- prime candidates, padding, seeds, salts,
    // session identifiers -- comes through here, so there is one place to be
    // right about it rather than a std::rand() in each of them.
    //
    // Throws rather than returning short or falling back. A caller that cannot
    // be given entropy must not be handed predictable bytes and told they are
    // random.
    static void random_bytes(uint8_t*, size_t);

    // Text that is safe to put inside an HTML document.
    //
    // & < > " ' become entities. Everything else is left alone, so this is safe
    // to apply to text that needs no escaping and cheap enough to apply to all
    // of it.
    static std::string escape_html(const std::string&);

    // Markup, deliberately.
    //
    // ECHO escapes what it is given unless the page wrote it as a literal, so a
    // page that has built markup in a variable has to say so. Wrapping it in
    // this is that saying: it is one word, it is greppable, and it is the only
    // way past the escaping.
    struct Raw
    {
      std::string text;
    };

    static Raw raw(const std::string& text) { return Raw{text}; }

    // What ECHO calls for anything that is not a literal.
    //
    // The value is rendered the way it would have been written straight to the
    // stream, and then escaped. Numbers come through unchanged; a String
    // carrying <script> does not.
    // Rendered into a stream of the destination's own type, so whatever
    // operator<< the value has for that stream is the one used -- the Type
    // library defines them per stream type, not for std::ostream.
    template <typename S, typename T>
    static void echo_escaped(S& out, const T& value)
    {
      S rendered;
      rendered << value;
      out << Utility::escape_html(rendered.str());
    }

    // The overload that lets Raw past, and the only one that does.
    template <typename S>
    static void echo_escaped(S& out, const Raw& markup)
    {
      out << markup.text;
    }
    static uint64_t random_uint64();
  };

}

#endif // __UTILITY_HPP__
