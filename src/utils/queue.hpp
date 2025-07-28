#ifndef UTILS_QUEUE_HPP
#define UTILS_QUEUE_HPP

#include <stdexcept>

template <typename T>
class CircularQueue {
private:
    T* arr;
    int capacity;
    int frontIdx;
    int rearIdx;
    int count;

public:
    // Constructor
    CircularQueue(int size) : capacity(size), frontIdx(0), rearIdx(-1), count(0) {
        arr = new T[capacity];
    }

    // Destructor
    ~CircularQueue() {
        delete[] arr;
    }

    T& get(int index) {
        int offset = (frontIdx + index) % capacity;
        return arr[offset];
    }

    bool remove(int index) {
        if (index < 0 || index >= count) return false;
        for (int i = index; i < count - 1; i++) {
            arr[i] = arr[i + 1];
        }
        count--;
        return true;
    }

    // Add an element to the queue
    bool enqueue(const T& value) {
        if (isFull()) return false;
        rearIdx = (rearIdx + 1) % capacity;
        arr[rearIdx] = value;
        count++;
        return true;
    }

    // Remove an element from the queue
    bool dequeue() {
        if (isEmpty()) return false;
        frontIdx = (frontIdx + 1) % capacity;
        count--;
        return true;
    }

    // Get the front element
    T front() const {
        if (isEmpty()) return T();
        return arr[frontIdx];
    }

    T& front() {
        if (isEmpty()) {
            throw std::runtime_error("CircularQueue::front() called on empty queue");
        }
        return arr[frontIdx];
    }

    // Get the rear element
    T rear() const {
        if (isEmpty()) return T();
        return arr[rearIdx];
    }

    T& rear() {
        if (isEmpty()) {
            throw std::runtime_error("CircularQueue::rear() called on empty queue");
        }
        return arr[rearIdx];
    }

    // Check if the queue is empty
    bool isEmpty() const {
        return count == 0;
    }

    // Check if the queue is full
    bool isFull() const {
        return count == capacity;
    }

    // Get the current size of the queue
    int size() const {
        return count;
    }
};

#endif // UTILS_QUEUE_HPP
