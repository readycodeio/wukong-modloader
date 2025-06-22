#pragma once

void* get_mono_method_desc_new_ptr();
void* mono_method_desc_new(const char* name, bool include_namespace);

void* get_mono_method_desc_search_in_image_ptr();
void* mono_method_desc_search_in_image(void* desc, void* image);

void* get_mono_method_desc_free_ptr();
void mono_method_desc_free(void* desc);
