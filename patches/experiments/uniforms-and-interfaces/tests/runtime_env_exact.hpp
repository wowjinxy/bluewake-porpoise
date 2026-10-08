// Actual frozen common.cpp function, unmodified.
bool sparse_vertex_uniforms_enabled() {
  static const bool enabled = [] {
    const char* value = std::getenv("DOL_GX_SPARSE_UNIFORMS");
    return value != nullptr && std::strcmp(value, "1") == 0;
  }();
  return enabled;
}
