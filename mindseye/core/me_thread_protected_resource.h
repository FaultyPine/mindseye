#include <mutex>
#include <utility>

template <typename T, typename Mutex = std::mutex>
class ThreadProtectedResource 
{
private:
    T data;
    mutable Mutex mtx;

public:
    template<typename... Args>
    explicit ThreadProtectedResource(Args&&... args) : data(std::forward<Args>(args)...) {}

    class Accessor 
    {
    private:
        T& data_ref;
        std::unique_lock<Mutex> lock;

    friend class ThreadProtectedResource;
        Accessor(T& d, Mutex& m) : data_ref(d), lock(m) {}

    public:
        T* operator->() { return &data_ref; }
        const T* operator->() const { return &data_ref; }
        T& operator*() { return data_ref; }
        const T& operator*() const { return data_ref; }
    };

    // The only way to access the member forces you to take the lock
    Accessor access() 
    {
        return Accessor(data, mtx);
    }
};
