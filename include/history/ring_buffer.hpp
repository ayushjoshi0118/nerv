#pragma once

#include <vector>
#include <cstddef>
#include <stdexcept>

namespace nerv {

template <typename T, size_t Capacity>
class RingBuffer {
public:
    RingBuffer() : m_data(Capacity), m_head(0), m_count(0) {}

    void push(const T& item) {
        m_data[m_head] = item;
        m_head = (m_head + 1) % Capacity;
        if (m_count < Capacity) {
            m_count++;
        }
    }

    void clear() {
        m_head = 0;
        m_count = 0;
    }

    size_t size() const { return m_count; }
    size_t capacity() const { return Capacity; }
    bool empty() const { return m_count == 0; }
    bool full() const { return m_count == Capacity; }

    // Access item in chronological order: index 0 is oldest, size()-1 is newest
    const T& operator[](size_t index) const {
        if (index >= m_count) {
            throw std::out_of_range("RingBuffer index out of range");
        }
        size_t start = (m_count < Capacity) ? 0 : m_head;
        size_t actual_idx = (start + index) % Capacity;
        return m_data[actual_idx];
    }

    const T& newest() const {
        if (empty()) throw std::runtime_error("RingBuffer is empty");
        size_t idx = (m_head == 0) ? (Capacity - 1) : (m_head - 1);
        return m_data[idx];
    }

    const T& oldest() const {
        if (empty()) throw std::runtime_error("RingBuffer is empty");
        size_t start = (m_count < Capacity) ? 0 : m_head;
        return m_data[start];
    }

    // Export sequential copy for rendering (e.g. ImGui PlotLines)
    std::vector<T> to_vector() const {
        std::vector<T> result;
        result.reserve(m_count);
        for (size_t i = 0; i < m_count; ++i) {
            result.push_back((*this)[i]);
        }
        return result;
    }

private:
    std::vector<T> m_data;
    size_t m_head;
    size_t m_count;
};

} // namespace nerv
