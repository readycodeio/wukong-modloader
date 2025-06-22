#pragma once


struct GSList
{
    void* data;
    GSList *next;
};


void* get_glib_new0_ptr();
void* glib_new0(size_t size);


template<typename T>
T* glib_new0()
{
    return static_cast<T*>(glib_new0(sizeof(T)));
}
