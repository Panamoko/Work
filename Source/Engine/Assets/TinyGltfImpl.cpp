// tinygltf / stb_image の実装をこの1ファイルに埋め込む（プロジェクト全体で1か所だけ）
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include <tiny_gltf.h>