#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <cstddef>
#include <string>

class Buffer1 {
public:
    static constexpr std::size_t capcity = 100;
    static constexpr std::size_t max_tam = 50;

    //manual-reset, sinalizado no fim
    explicit Buffer1(HANDLE CloseEvent = nullptr);
    ~Buffer1();

    Buffer1(const Buffer1&) = delete;
    Buffer1& operator=(const Buffer1&) = delete;

    //bloqueia se não houver vaga  <->  retorna false se der erro ou no finla
    bool Insert(const std::string& message, const char* origem = "?");

    //bloqueia se não tiver message  <->  retorna false se der erro ou no final
    bool Remove(std::string& message);

private:
    std::array<std::string, capcity> data_{};
    std::size_t begin_ = 0;
    std::size_t end_ = 0;

    HANDLE CloseEvent_ = nullptr;
    HANDLE mutex_ = nullptr;
    HANDLE vagas_semaph_ = nullptr;
    HANDLE message_semaph_ = nullptr;

    bool WaitObjct(HANDLE objeto) const;
    bool WaitMutex() const;
    void CloseHandles();
};