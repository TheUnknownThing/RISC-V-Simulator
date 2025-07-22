#ifndef UTILS_QUEUE_HPP
#define UTILS_QUEUE_HPP

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

    // Get the rear element
    T rear() const {
        if (isEmpty()) return T();
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
