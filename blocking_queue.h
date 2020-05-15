#ifndef blocking_queue_h
#define blocking_queue_h

#include <mutex>
#include <condition_variable>
#include <deque>

namespace estd
{

class semaphore
{
public:
    semaphore(int count = 0)
        : count_(count)
    { }

    void signal();
    void wait();

private:
    std::mutex mtx_;
    std::mutex sig_;
    int count_;
};

void semaphore::signal()
{
    std::unique_lock<std::mutex> lck(mtx_);
    ++count_;
    sig_.unlock();
}

void semaphore::wait()
{
    std::unique_lock<std::mutex> lck(mtx_);
    
    while (count_ == 0)
    {
        sig_.lock();
    }

    --count_;
}

template<typename T>
class blocking_queue
{
public:
    class consumer_iterator
    {
    public:
        consumer_iterator(blocking_queue& bq, bool advance, bool end);

    public:
        consumer_iterator& operator++();
        consumer_iterator& operator--();
        T& operator*() { return value_; }

        bool operator==(const consumer_iterator& it) const { return end_ == it.end_; }
        bool operator!=(const consumer_iterator& it) const { return !(*this == it); }

        void detach(bool enable = true) { detach_ = enable; }
        bool is_detached() const { return detach_; }

    private:
        blocking_queue& bq_;
        T value_;
        bool detach_ = false;
        bool end_;
    };

public:
    void push_back(const T&);
    T pop_back();
    void push_front(const T&);
    T pop_front();
    
    size_t size() const;

    consumer_iterator begin() { return consumer_iterator { *this, true, false }; }
    consumer_iterator end() { return consumer_iterator { *this, true, true }; }
    consumer_iterator rbegin() { return consumer_iterator { *this, false, false }; }
    consumer_iterator rend() { return end(); }

    consumer_iterator begin_detached();
    consumer_iterator rbegin_detached();
    
private:
    std::deque<T> data_;
    mutable std::mutex m_;
    semaphore e_;
    size_t sz;
};

template<typename T>
inline void blocking_queue<T>::push_back(const T& t)
{
    m_.lock();
    data_.push_back(t);
    m_.unlock();
    e_.signal();
}

template<typename T>
inline void blocking_queue<T>::push_front(const T& t)
{
    m_.lock();
    data_.push_front(t);
    m_.unlock();
    e_.signal();
}

template<typename T>
inline T blocking_queue<T>::pop_back()
{
    e_.wait();
    m_.lock();
    T t = data_.back();
    data_.pop_back();
    m_.unlock();
    
    return t;
}

template<typename T>
inline T blocking_queue<T>::pop_front()
{
    e_.wait();
    m_.lock();
    T t = data_.front();
    data_.pop_front();
    m_.unlock();
    
    return t;
}

template<typename T>
inline size_t blocking_queue<T>::size() const
{
    m_.lock();
    size_t sz = data_.size();
    m_.unlock();
    
    return sz;
}

template<typename T>
inline typename blocking_queue<T>::consumer_iterator blocking_queue<T>::begin_detached()
{
    auto bgn = begin();
    bgn.detach();

    return bgn;
}

template<typename T>
inline typename blocking_queue<T>::consumer_iterator blocking_queue<T>::rbegin_detached()
{
    auto rbgn = rbegin();
    rbgn.detach();

    return rbgn;
}

template<typename T>
blocking_queue<T>::consumer_iterator::consumer_iterator(blocking_queue& bq, bool advance, bool end) 
    : bq_(bq), end_(end)
{
    if (end)
    {
        return;
    }
    
    if (advance)
    {
        ++*this;
    }
    else
    {
        --*this;
    }
}

template<typename T>
inline typename blocking_queue<T>::consumer_iterator& blocking_queue<T>::consumer_iterator::operator++() 
{
    if (is_detached() && bq_.size() == 0)
    {
        end_ = true;
        return *this;
    }

    value_ = bq_.pop_front(); 
    return *this; 
}

template<typename T>
inline typename blocking_queue<T>::consumer_iterator& blocking_queue<T>::consumer_iterator::operator--() 
{ 
    if (is_detached() && bq_.size() == 0)
    {
        end_ = true;
        return *this;
    }

    value_ = bq_.pop_back(); 
    return *this; 
}

} // namespace estd

#endif