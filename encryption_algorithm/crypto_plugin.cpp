#include "pch.h"


#include <vector>

#include "../crypto_plugin.h"




std::vector<unsigned char> key;

//unsigned char test_crypto_func(const unsigned char in, ){
//
//}

CRYPTO_API bool crypto_init(const unsigned char* key_data, size_t key_size){
	if(key_data == nullptr || key_size == 0){
		return false;
	}

	key.assign(key_data, key_data +  key_size);
	return true;
}


CRYPTO_API bool process_buffer(const unsigned char* in_buffer, unsigned char* out_buffer, size_t size, bool is_encrypt = false) {

	if(in_buffer==nullptr || out_buffer==nullptr){
		return false; 
	}
	for (size_t i = 0; i < size; ++i) {
			out_buffer[i] = in_buffer[i] ^ key[i % key.size()];
	}
	//if(is_encrypt){

	//	for (size_t i = 0; i < key_size(); ++i) {
	//		out_buffer[i] = in_buffer[i] ^ key[i % key.size()]; 
	//	}
	//}
	//else { //חארטפנמגûגאול
	//	for (size_t i = 0; i < key_size(); ++i) {
	//		out_buffer[i] = in_buffer[i] ^ key[i % key.size()];
	//	}
	//}
	return true;
}

CRYPTO_API void crypto_clean() {
	key.clear();
}
