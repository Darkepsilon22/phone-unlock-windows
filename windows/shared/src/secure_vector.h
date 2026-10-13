#pragma once

#include <windows.h>
#include <cstddef>
#include <vector>

template <typename T>
class SecureVector
{
public:
    SecureVector() = default;

    explicit SecureVector(size_t capacity)
    {
        items_.reserve(capacity);
    }

    SecureVector(const T* items, size_t count) : items_(items, items + count)
    {
    }

    SecureVector(const SecureVector&) = delete;
    SecureVector& operator=(const SecureVector&) = delete;

    SecureVector(SecureVector&& other) noexcept : items_(std::move(other.items_))
    {
    }

    SecureVector& operator=(SecureVector&& other) noexcept
    {
        if (this != &other)
        {
            Wipe();
            items_ = std::move(other.items_);
        }
        return *this;
    }

    ~SecureVector()
    {
        Wipe();
    }

    bool Append(const T* items, size_t count)
    {
        if (items_.size() + count > items_.capacity())
        {
            return false;
        }
        items_.insert(items_.end(), items, items + count);
        return true;
    }

    bool ResizeWithinCapacity(size_t count)
    {
        if (count > items_.capacity())
        {
            return false;
        }
        items_.resize(count);
        return true;
    }

    T* data()
    {
        return items_.data();
    }

    const T* data() const
    {
        return items_.data();
    }

    size_t size() const
    {
        return items_.size();
    }

    bool empty() const
    {
        return items_.empty();
    }

private:
    void Wipe()
    {
        if (!items_.empty())
        {
            SecureZeroMemory(items_.data(), items_.size() * sizeof(T));
        }
        items_.clear();
    }

    std::vector<T> items_;
};

using SecureBytes = SecureVector<BYTE>;

class SecureString
{
public:
    SecureString() = default;

    SecureString(const wchar_t* text, size_t length) : chars_(length + 1)
    {
        chars_.Append(text, length);
        chars_.Append(&Terminator, 1);
    }

    const wchar_t* c_str() const
    {
        return chars_.empty() ? &Terminator : chars_.data();
    }

    size_t length() const
    {
        return chars_.empty() ? 0 : chars_.size() - 1;
    }

    bool empty() const
    {
        return length() == 0;
    }

private:
    static constexpr wchar_t Terminator = L'\0';
    SecureVector<wchar_t> chars_;
};
