#pragma once

#include "core/me_defines.h"
#include "core/containers/dynarray.h"
#include "core/me_memory.h"
#include <iterator>
#include <concepts>
#include <type_traits>
#include <tuple>
#include <utility>
// NOTE: has pointer stability

// this map impl currently uses "separate chaining" for collisions
// we store a growable array of *pointers* to k/v pairs (each entry is separately allocated)
// k/v pairs that hash to the same bucket are linked (bucketNext). Each bucket points to the head of it's collision chain
//  there's also a second link (next) that connects all nodes in insertion order.

template <typename KeyType>
    requires std::is_integral_v<KeyType>
u64 Hash(const KeyType& value) noexcept
{
    return static_cast<u64>(std::hash<KeyType>{}(value));
}

template <typename KeyType>
concept HasStaticMemberHash = requires(const KeyType& value)
{
    { KeyType::Hash(value) } -> std::same_as<u64>;
};

template <typename KeyType>
concept HasFreeHash = requires(const KeyType& value)
{
    { Hash(value) } -> std::same_as<u64>;
};

template <typename KeyType>
concept HasStdHash = requires(const KeyType& value)
{
    { std::hash<KeyType>{}(value) } -> std::convertible_to<std::size_t>;
};

struct StandardHasher
{
    template <typename KeyType>
    u64 operator()(const KeyType& v) const noexcept
    {
        if constexpr (HasStaticMemberHash<KeyType>)
        {
            return KeyType::Hash(v);
        }
        else if constexpr (HasFreeHash<KeyType>)
        {
            return Hash(v);
        }
        else if constexpr (HasStdHash<KeyType>)
        {
            return static_cast<u64>(std::hash<KeyType>{}(v));
        }
        else
        {
            static_assert(HasStaticMemberHash<KeyType> || HasFreeHash<KeyType>,
                "Map key type must provide member function `static u64 Hash(const KeyType&)` or a free function `u64 Hash(const KeyType&)`");
        }
    }
};

template <typename Key, typename Value>
struct meMapNode
{
    Key first;
    Value second;
    meMapNode* bucketNext = nullptr;
    meMapNode* next = nullptr;

    meMapNode() = default;
    explicit meMapNode(const Key& key) : first(key), second{} {}

    // for destructuring syntax support
    template <std::size_t Index>
    decltype(auto) get() &;
    template <std::size_t Index>
    decltype(auto) get() const &;
    template <std::size_t Index>
    decltype(auto) get() &&;
};

template <typename Key, typename Value, typename Hasher = StandardHasher>
struct meMap
{
    using HashType = u64;
    using NodeType = meMapNode<Key, Value>;
    static constexpr u32 INITIAL_SIZE = 16;

    meAllocator* allocator = nullptr;
    DynArray<NodeType*> buckets = {};
    NodeType* head = nullptr;
    NodeType* tail = nullptr;
    u32 size = 0;

    meMap()
    {
        Initialize(GetDefaultAllocator(), INITIAL_SIZE);
    }
    explicit meMap(meAllocator* allocator)
    {
        Initialize(allocator ? allocator : GetDefaultAllocator(), INITIAL_SIZE);
    }

    void reserve(u32 capacity)
    {
        const u64 requiredBuckets = GetRequiredBucketCountForSize(capacity);
        if (requiredBuckets > U32_INVALID_ID)
        {
            ME_ASSERT(false && "meMap::reserve capacity is too large");
            return;
        }
        if (requiredBuckets > DynArrayGetCapacity(buckets))
        {
            Rehash(static_cast<u32>(requiredBuckets));
        }
    }

    void clear()
    {
        NodeType* node = head;
        while (node)
        {
            NodeType* next = node->next;
            MEDELETE(allocator, NodeType, node);
            node = next;
        }
        for (u32 i = 0; i < DynArrayGetCapacity(buckets); ++i)
        {
            buckets[i] = nullptr;
        }
        head = nullptr;
        tail = nullptr;
        size = 0;
    }

    meMap(const meMap& other)
    {
        meAllocator* allocator = other.allocator ? other.allocator : GetDefaultAllocator();
        u32 capacity = DynArrayGetCapacity(other.buckets) ? DynArrayGetCapacity(other.buckets) : INITIAL_SIZE;
        Initialize(allocator, capacity);
        for (const NodeType* node = other.head; node; node = node->next)
        {
            (*this)[node->first] = node->second;
        }
    }

    meMap(meMap&& other) noexcept
        : allocator(other.allocator)
        , buckets(other.buckets)
        , head(other.head)
        , tail(other.tail)
        , size(other.size)
    {
        other.buckets = {};
        other.head = nullptr;
        other.tail = nullptr;
        other.size = 0;
    }

    meMap& operator=(const meMap& other)
    {
        if (this != &other)
        {
            meMap copy(other);
            Swap(copy);
        }
        return *this;
    }

    meMap& operator=(meMap&& other) noexcept
    {
        if (this != &other)
        {
            Destroy();
            allocator = other.allocator;
            buckets = other.buckets;
            head = other.head;
            tail = other.tail;
            size = other.size;
            other.buckets = {};
            other.head = nullptr;
            other.tail = nullptr;
            other.size = 0;
        }
        return *this;
    }

    ~meMap()
    {
        Destroy();
    }

    inline HashType hashFromKey(const Key& key) const
    {
        return Hasher{}(key);
    }

    bool at(const Key& key, Value*& valueOut)
    {
        NodeType* node = FindNode(key);
        if (node)
        {
            valueOut = &node->second;
            return true;
        }
        return false;
    }

    bool at(const Key& key, const Value*& valueOut) const
    {
        const NodeType* node = FindNode(key);
        if (node)
        {
            valueOut = &node->second;
            return true;
        }
        return false;
    }

    Value& operator[](const Key& key)
    {
        if (Value* valueMaybe = nullptr; at(key, valueMaybe))
        {
            return *valueMaybe;
        }
        EnsureCapacityForInsert();
        HashType hash = hashFromKey(key);
        const u32 bucketIndex = static_cast<u32>(hash % DynArrayGetCapacity(buckets));
        NodeType* node = MENEW(allocator, NodeType, key);
        node->bucketNext = buckets[bucketIndex];
        buckets[bucketIndex] = node;
        if (tail)
        {
            tail->next = node;
        }
        else
        {
            head = node;
        }
        tail = node;
        ++size;
        return node->second;
    }

    // std::unordered_map compat
    template <bool IsConst>
    struct BasicIterator
    {
        using MapType = std::conditional_t<IsConst, const meMap, meMap>;
        using NodePointer = std::conditional_t<IsConst, const NodeType*, NodeType*>;
        using value_type = NodeType;
        using difference_type = u64;
        using pointer = NodePointer;
        using reference = std::conditional_t<IsConst, const NodeType&, NodeType&>;
        using iterator_category = std::forward_iterator_tag;

        MapType* map = nullptr;
        NodePointer node = nullptr;
        u32 bucketIndex = 0;

        BasicIterator() = default;
        BasicIterator(MapType* map, u32 bucketIndex, NodePointer node = nullptr)
            : map(map), node(node), bucketIndex(bucketIndex)
        {
            if (!this->node)
            {
                SkipEmptyBuckets();
            }
        }

        reference operator*() const { return *node; }
        pointer operator->() const { return node; }

        BasicIterator& operator++()
        {
            if (node)
            {
                node = node->bucketNext;
            }
            if (!node)
            {
                ++bucketIndex;
            }
            SkipEmptyBuckets();
            return *this;
        }

        BasicIterator operator++(int)
        {
            BasicIterator previous = *this;
            ++(*this);
            return previous;
        }

        bool operator==(const BasicIterator& other) const
        {
            return map == other.map && node == other.node && bucketIndex == other.bucketIndex;
        }

        bool operator!=(const BasicIterator& other) const
        {
            return !(*this == other);
        }

    private:
        void SkipEmptyBuckets()
        {
            if (!map)
            {
                return;
            }
            const u32 capacity = DynArrayGetCapacity(map->buckets);
            while (!node && bucketIndex < capacity)
            {
                node = map->buckets[bucketIndex];
                if (!node)
                {
                    ++bucketIndex;
                }
            }
        }
    };

    using iterator = BasicIterator<false>;
    using const_iterator = BasicIterator<true>;

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, DynArrayGetCapacity(buckets)); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, DynArrayGetCapacity(buckets)); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

    iterator find(const Key& key)
    {
        NodeType* node = FindNode(key);
        if (!node)
        {
            return end();
        }
        const u32 bucketIndex = static_cast<u32>(hashFromKey(key) % DynArrayGetCapacity(buckets));
        return iterator(this, bucketIndex, node);
    }

    const_iterator find(const Key& key) const
    {
        const NodeType* node = FindNode(key);
        if (!node)
        {
            return end();
        }
        const u32 bucketIndex = static_cast<u32>(hashFromKey(key) % DynArrayGetCapacity(buckets));
        return const_iterator(this, bucketIndex, node);
    }

    void Swap(meMap& other) noexcept
    {
        std::swap(allocator, other.allocator);
        std::swap(buckets, other.buckets);
        std::swap(head, other.head);
        std::swap(tail, other.tail);
        std::swap(size, other.size);
    }

private:
    // 75% load factor, which is also Java's HashMap default
    static constexpr u64 MAX_LOAD_FACTOR_NUMERATOR = 3;
    static constexpr u64 MAX_LOAD_FACTOR_DENOMINATOR = 4;

    static u64 GetRequiredBucketCountForSize(u64 size)
    {
        return (size * MAX_LOAD_FACTOR_DENOMINATOR + MAX_LOAD_FACTOR_NUMERATOR - 1) / MAX_LOAD_FACTOR_NUMERATOR;
    }

    void Initialize(meAllocator* allocator, u32 capacity)
    {
        this->allocator = allocator;
        buckets = DynArrayCreateWithReserved<NodeType*>(allocator, capacity);
    }

    void Destroy()
    {
        clear();
        if (buckets)
        {
            DynArrayDestroy(buckets);
        }
    }

    NodeType* FindNode(const Key& key)
    {
        const u32 capacity = DynArrayGetCapacity(buckets);
        if (capacity == 0)
        {
            return nullptr;
        }
        const u32 bucketIndex = static_cast<u32>(hashFromKey(key) % capacity);
        for (NodeType* node = buckets[bucketIndex]; node; node = node->bucketNext)
        {
            if (node->first == key)
            {
                return node;
            }
        }
        return nullptr;
    }

    const NodeType* FindNode(const Key& key) const
    {
        const u32 capacity = DynArrayGetCapacity(buckets);
        if (capacity == 0)
        {
            return nullptr;
        }
        const u32 bucketIndex = static_cast<u32>(hashFromKey(key) % capacity);
        for (const NodeType* node = buckets[bucketIndex]; node; node = node->bucketNext)
        {
            if (node->first == key)
            {
                return node;
            }
        }
        return nullptr;
    }

    void EnsureCapacityForInsert()
    {
        const u32 capacity = DynArrayGetCapacity(buckets);
        if (GetRequiredBucketCountForSize(static_cast<u64>(size) + 1) > capacity)
        {
            ME_ASSERT(capacity <= U32_INVALID_ID / 2);
            Rehash(capacity * 2);
        }
    }

    void Rehash(u32 capacity)
    {
        if (capacity == 0)
        {
            capacity = INITIAL_SIZE;
        }
        DynArray<NodeType*> newBuckets = DynArrayCreateWithReserved<NodeType*>(allocator, capacity);
        for (NodeType* node = head; node; node = node->next)
        {
            const u32 bucketIndex = static_cast<u32>(hashFromKey(node->first) % capacity);
            node->bucketNext = newBuckets[bucketIndex];
            newBuckets[bucketIndex] = node;
        }
        if (buckets)
        {
            DynArrayDestroy(buckets);
        }
        buckets = newBuckets;
    }

};

// support for the nice syntax `for (const auto& [key, value] : map)`
// since our nodes have more than 2 things in them we need this annoying hackery
template <typename Key, typename Value>
template <std::size_t Index>
decltype(auto) meMapNode<Key, Value>::get() &
{
    static_assert(Index < 2);
    if constexpr (Index == 0)
    {
        return (first);
    }
    else
    {
        return (second);
    }
}

template <typename Key, typename Value>
template <std::size_t Index>
decltype(auto) meMapNode<Key, Value>::get() const &
{
    static_assert(Index < 2);
    if constexpr (Index == 0)
    {
        return (first);
    }
    else
    {
        return (second);
    }
}

template <typename Key, typename Value>
template <std::size_t Index>
decltype(auto) meMapNode<Key, Value>::get() &&
{
    static_assert(Index < 2);
    if constexpr (Index == 0)
    {
        return std::move(first);
    }
    else
    {
        return std::move(second);
    }
}

namespace std
{
    template <typename Key, typename Value>
    struct tuple_size<meMapNode<Key, Value>> : integral_constant<size_t, 2> {};

    template <typename Key, typename Value>
    struct tuple_element<0, meMapNode<Key, Value>>
    {
        using type = Key;
    };

    template <typename Key, typename Value>
    struct tuple_element<1, meMapNode<Key, Value>>
    {
        using type = Value;
    };
}

void meMapTests();
