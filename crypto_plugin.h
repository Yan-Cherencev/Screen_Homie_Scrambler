#pragma once

#include <cstddef>

#ifdef CRYPTO_PLUGIN_EXPORTS
#define CRYPTO_API __declspec(dllexport)
#else
#define CRYPTO_API __declspec(dllimport)
#endif

extern "C" {
	CRYPTO_API bool crypto_init(const unsigned char* key_data, size_t key_size);
	CRYPTO_API bool process_buffer(const unsigned char* in_buffer, unsigned char* out_buffer, size_t size, bool is_encrypt);
	CRYPTO_API void crypto_clean();
}