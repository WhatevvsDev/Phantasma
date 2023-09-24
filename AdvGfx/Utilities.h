#pragma once

template <typename T>
inline void rotate_array_right(T* in_array, size_t size)
{
	T* temp_array = new T[size];

	memcpy(temp_array, in_array, sizeof(T) * size);
	memcpy(in_array + 1, temp_array, sizeof(T) * (size - 1));

	delete[] temp_array;
}