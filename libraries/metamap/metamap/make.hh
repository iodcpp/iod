#pragma once

#include <li/symbol/symbol.hh>
#include <li/symbol/ast.hh>

namespace li {

  
  template <typename ...Ms>
  struct metamap;

  namespace internal
  {
    
    template <typename S, typename V>
    decltype(auto) exp_to_variable_ref(const assign_exp<S, V>& e)
    {
      return make_variable_reference(S{}, e.right);
    }

    template <typename S, typename V>
    decltype(auto) exp_to_variable(const assign_exp<S, V>& e)
    {
      typedef std::remove_const_t<std::remove_reference_t<V>> vtype;
      return make_variable(S{}, e.right);
    }

    template <typename S>
    decltype(auto) exp_to_variable(const symbol<S>& e)
    {
      return exp_to_variable(S() = int());
    }
    
    template <typename ...T>
    inline decltype(auto) make_metamap_helper(T&&... args)
    {
      return metamap<T...>(std::forward<T>(args)...);
    }
    
  }
  
  // Store copies of values in the map
  static struct {
    template <typename ...T>
    inline decltype(auto) operator()(T&&... args) const
    {
      // Copy values.
      return internal::make_metamap_helper(internal::exp_to_variable(std::forward<T>(args))...);
    }
  } mmm;
  
  // Store references of values in the map
  template <typename ...T>
  inline decltype(auto) make_metamap_reference(T&&... args)
  {
    // Keep references.
    return internal::make_metamap_helper(internal::exp_to_variable_ref(std::forward<T>(args))...);
  }
  
}
