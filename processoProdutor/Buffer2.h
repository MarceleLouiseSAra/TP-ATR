
#pragma once
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <array>
#include <cstddef>
#include <string>

class Buffer2 {
public:
    static constexpr std::size_t capacity = 200;
    static constexpr std::size_t max_tam = 50;

    explicit Buffer2(HANDLE CloseEvent = nullptr);
    ~Buffer2();

    Buffer2(const Buffer2&) = delete;
    Buffer2& operator=(const Buffer2&) = delete;

    bool Insert(const std::string& message);
    bool Remove(std::string& message);
    bool TryRemove(std::string& message);

private:
    std::array<std::string, capacity> data_{};

    std::size_t begin_ = 0;
    std::size_t end_ = 0;

    HANDLE CloseEvent_ = nullptr;
    HANDLE mutex_ = nullptr;
    HANDLE vagas_semaph_ = nullptr;
    HANDLE message_semaph_ = nullptr;

    bool WaitObject(HANDLE objeto) const;
    bool WaitMutex() const;
    void CloseHandles();
};
