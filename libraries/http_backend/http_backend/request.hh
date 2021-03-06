#pragma once

#include <boost/lexical_cast.hpp>
#include <iostream>
#include <microhttpd.h>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <li/metamap/metamap.hh>
#include <li/http_backend/error.hh>
#include <li/http_backend/url_decode.hh>

namespace li {

struct http_request {

  const char* header(const char* k) const;

  // With list of parameters: s::id = int(), s::name = string(), ...
  template <typename S, typename V, typename... T>
  auto url_parameters(assign_exp<S, V> e, T... tail);
  template <typename S, typename V, typename... T>
  auto get_parameters(assign_exp<S, V> e, T... tail);
  template <typename S, typename V, typename... T>
  auto post_parameters(assign_exp<S, V> e, T... tail);

  // Const wrapper.
  template <typename O> auto url_parameters(const O& res);
  template <typename O> auto get_parameters(const O& res);
  template <typename O> auto post_parameters(const O& res);

  // With a metamap.
  template <typename O> auto url_parameters(O& res);
  template <typename O> auto get_parameters(O& res);
  template <typename O> auto post_parameters(O& res);

  MHD_Connection* mhd_connection;
  std::string body;
  std::string url;
  std::string url_spec;
};

template <typename F>
int mhd_keyvalue_iterator(void* cls, enum MHD_ValueKind kind, const char* key, const char* value) {
  F& f = *(F*)cls;
  if (key and value and *key and *value) {
    f(key, value);
    return MHD_YES;
  }
  // else return MHD_NO;
  return MHD_YES;
}

struct url_parser_info_node { int slash_pos; bool is_path; };
using url_parser_info = std::unordered_map<std::string, url_parser_info_node>;

auto make_url_parser_info(const std::string_view url) {

  url_parser_info info;

  auto check_pattern = [](const char* s, char a) { return *s == a and *(s + 1) == a; };

  int slash_pos = -1;
  for (int i = 0; i < int(url.size()); i++) {
    if (url[i] == '/')
      slash_pos++;
    // param must start with {{
    if (check_pattern(url.data() + i, '{')) {
      const char* param_name_start = url.data() + i + 2;
      const char* param_name_end = param_name_start;
      // param must end with }}
      while (!check_pattern(param_name_end, '}'))
        param_name_end++;

      if (param_name_end != param_name_start and check_pattern(param_name_end, '}')) {
        int size = param_name_end - param_name_start;
        bool is_path = false;
        if (size > 3 and param_name_end[-1] == '.' and param_name_end[-2] == '.' and param_name_end[-3] == '.')
        {
          is_path = true;
          param_name_end -= 3;
        }
        std::string_view param_name(param_name_start, param_name_end - param_name_start);
        info.emplace(param_name, url_parser_info_node{slash_pos, is_path});
      }
    }
  }
  return info;
}

template <typename O>
auto parse_url_parameters(const url_parser_info& fmt, const std::string_view url, O& obj) {
  // get the indexes of the slashes in url.
  std::vector<int> slashes;
  for (int i = 0; i < int(url.size()); i++) {
    if (url[i] == '/')
      slashes.push_back(i);
  }

  // For each field in O...
  //  find the location of the field in the url thanks to fmt.
  //  get it.
  map(obj, [&](auto k, auto v) {
    const char* symbol_str = symbol_string(k);
    auto it = fmt.find(symbol_str);
    if (it == fmt.end()) {
      throw std::runtime_error(std::string("Parameter ") + symbol_str + " not found in url " +
                               url.data());
    } else {
      // Location of the parameter in the url.
      int param_slash = it->second.slash_pos; // index of slash before param.
      if (param_slash >= int(slashes.size()))
        throw http_error::bad_request("Missing url parameter ", symbol_str);

      int param_start = slashes[param_slash] + 1;
      if (it->second.is_path)
      {
        if constexpr(std::is_same<std::decay_t<decltype(obj[k])>, std::string>::value or
                     std::is_same<std::decay_t<decltype(obj[k])>, std::string_view>::value) {
          obj[k] = std::string_view(url.data() + param_start - 1, url.size() - param_start + 1); // -1 to include the first /.
        }
        else {
          throw std::runtime_error("{{path...}} parameters only accept std::string or std::string_view types.");
        }

      }
      else
      {
        int param_end = param_start;
        while (int(url.size()) > (param_end) and url[param_end] != '/')
          param_end++;

        std::string content(url.data() + param_start, param_end - param_start);
        content.resize(MHD_http_unescape(&content[0]));
        try {
          obj[k] = boost::lexical_cast<decltype(v)>(content);
        } catch (std::exception e) {
          throw http_error::bad_request("Cannot decode url parameter ", li::symbol_string(k), " : ",
                                        e.what());
        }
      }
    }
  });
  return obj;
}

inline const char* http_request::header(const char* k) const {
  return MHD_lookup_connection_value(mhd_connection, MHD_HEADER_KIND, k);
}

// template <typename P, typename O>
// void decode_post_parameters_json(O& res, mhd_request* r) const {
//   try {
//     if (r->body.size())
//       json_decode(res, r->body);
//     else
//       json_decode(res, "{}");
//   } catch (const std::exception& e) {
//     throw http_error::bad_request("Error when decoding procedure arguments: ",
//                              e.what());
//   }
// }

// template <typename P, typename T>
// auto decode_parameters(http_request* r, P procedure, T& res) const {
//   try {
//     decode_url_arguments<typename P::path_type>(res, r->url);
//     decode_get_arguments<typename P::route_type::get_parameters_type>(res,
//     r);

//     using post_t = typename P::route_type::post_parameters_type;
//     if (post_t::size() > 0) {
//       const char* encoding = MHD_lookup_connection_value(
//           r->connection, MHD_HEADER_KIND, "Content-Type");
//       if (!encoding)
//         throw http_error::bad_request(std::string(
//             "Content-Type is required to decode the POST parameters"));

//       if (encoding == std::string("application/x-www-form-urlencoded"))
//         decode_post_parameters_urlencoded<post_t>(res, r);
//       else if (encoding == std::string("application/json"))
//         decode_post_parameters_json<post_t>(res, r);
//       else
//         throw http_error::bad_request(std::string("Content-Type not implemented:
//         ") +
//                                  encoding);
//     }
//   } catch (const std::runtime_error& e) {
//     throw http_error::bad_request("Error when decoding procedure arguments: ",
//                              e.what());
//   }
// }

template <typename S, typename V, typename... T>
auto http_request::url_parameters(assign_exp<S, V> e, T... tail) {
  return url_parameters(mmm(e, tail...));
}

template <typename S, typename V, typename... T>
auto http_request::get_parameters(assign_exp<S, V> e, T... tail) {
  return get_parameters(mmm(e, tail...));
}

template <typename S, typename V, typename... T>
auto http_request::post_parameters(assign_exp<S, V> e, T... tail) {
  auto o = mmm(e, tail...);
  return post_parameters(o);
}

template <typename O> auto http_request::url_parameters(const O& res) {
  O r;
  return url_parameters(r);
}

template <typename O> auto http_request::get_parameters(const O& res) {
  O r;
  return get_parameters(r);
}
template <typename O> auto http_request::post_parameters(const O& res) {
  O r;
  return post_parameters(r);
}

template <typename O> auto http_request::url_parameters(O& res) {
  auto info = make_url_parser_info(url_spec);
  return parse_url_parameters(info, url, res);
}

template <typename O> auto http_request::get_parameters(O& res) {
  std::set<void*> found;
  auto add = [&](const char* k, const char* v) {
    url_decode2(found, std::string(k) + "=" + v, res, true);
  };

  MHD_get_connection_values(mhd_connection, MHD_GET_ARGUMENT_KIND,
                            &mhd_keyvalue_iterator<decltype(add)>, &add);

  // Check for missing fields.
  std::string missing = url_decode_check_missing_fields(found, res, true);
  if (missing.size())
    throw http_error::bad_request("Error while decoding the GET parameter: ", missing);
  return res;
}

template <typename O> auto http_request::post_parameters(O& res) {
  try {
    const char* encoding = this->header("Content-Type");
    if (!encoding)
      throw http_error::bad_request(std::string(
          "Content-Type is required to decode the POST parameters"));

    if (encoding == std::string_view("application/x-www-form-urlencoded"))
      url_decode(body, res);
    else if (encoding == std::string_view("application/json"))
      json_decode(body, res);
  } catch (std::exception e) {
    throw http_error::bad_request("Error while decoding the POST parameters: ", e.what());
  }

  return res;
}

} // namespace li
