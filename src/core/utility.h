#pragma once

template<class T> struct remove_reference      { using type = T; };
template<class T> struct remove_reference<T&>  { using type = T; };
template<class T> struct remove_reference<T&&> { using type = T; };

template<typename T>
typename remove_reference<T>::type&& move(T&& t) {
  return static_cast<typename remove_reference<T>::type&&>(t);
}

template<typename T>
T&& forward(typename remove_reference<T>::type& t) {
  return static_cast<T&&>(t);
}

template<typename T>
T&& forward(typename remove_reference<T>::type&& t) {
  return static_cast<T&&>(t);
}

template<typename T>
void swap(T& a, T& b) {
  T tmp = move(a);
  a = move(b);
  b = move(tmp);
}

template<typename T, typename U = T>
T exchange(T& obj, U&& new_val) {
  T old_val = move(obj);
  obj = forward<U>(new_val);
  return old_val;
}
