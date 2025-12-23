// clang src/test/alternatives/lib_charconv.cpp -o generated/charconv.dll -shared -std=c++17 -O2
#include <charconv>
#define C_EXPORT extern "C" __declspec(dllexport)
C_EXPORT void from_chars_f32(const char* start, const char* end, float* value) {
  std::from_chars(start, end, *value);
}
C_EXPORT void from_chars_f64(const char* start, const char* end, double* value) {
  std::from_chars(start, end, *value);
}
C_EXPORT char* to_chars_f32(char* start, char* end, float value) {
  auto [ptr, _ec] = std::to_chars(start, end, value, std::chars_format::general);
  return ptr;
}
C_EXPORT char* to_chars_f64(char* start, char* end, double value) {
  auto [ptr, _ec] = std::to_chars(start, end, value, std::chars_format::general);
  return ptr;
}
