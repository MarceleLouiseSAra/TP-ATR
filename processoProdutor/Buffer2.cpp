#include "Buffer2.h"

#include <iostream>
#include <stdexcept>
#include <utility>

Buffer2::Buffer2(HANDLE CloseEvent)
    : CloseEvent_(CloseEvent),
      mutex_(CreateMutexW(nullptr, FALSE, nullptr)),
      vagas_semaph_(CreateSemaphoreW(
          nullptr, capacity, capacity, nullptr)),
      message_semaph_(CreateSemaphoreW(
          nullptr, 0, capacity, nullptr)) {

    if (mutex_ == nullptr ||
        vagas_semaph_ == nullptr ||
        message_semaph_ == nullptr) {

        CloseHandles();
        throw std::runtime_error(
            "Falha ao criar objetos de sincronizacao do Buffer2"
        );
    }
}

Buffer2::~Buffer2() {
    CloseHandles();
}

bool Buffer2::Insert(const std::string& message) {
    if (message.size() > max_tam) {
        std::cerr << "[Buffer 2] Mensagem muito grande.\n";
        return false;
    }

    if (CloseEvent_ != nullptr &&
        WaitForSingleObject(CloseEvent_, 0) == WAIT_OBJECT_0) {
        return false;
    }

    if (!WaitObject(vagas_semaph_)) {
        return false;
    }

    if (!WaitMutex()) {
        ReleaseSemaphore(vagas_semaph_, 1, nullptr);
        return false;
    }

    data_[end_] = message;
    end_ = (end_ + 1) % capacity;

    ReleaseMutex(mutex_);

    if (!ReleaseSemaphore(message_semaph_, 1, nullptr)) {
        std::cerr << "[Buffer 2] Erro ao sinalizar mensagem.\n";
        return false;
    }

    return true;
}

bool Buffer2::Remove(std::string& message) {
    if (!WaitObject(message_semaph_)) {
        return false;
    }

    if (!WaitMutex()) {
        ReleaseSemaphore(message_semaph_, 1, nullptr);
        return false;
    }

    message = std::move(data_[begin_]);
    data_[begin_].clear();

    begin_ = (begin_ + 1) % capacity;

    ReleaseMutex(mutex_);

    if (!ReleaseSemaphore(vagas_semaph_, 1, nullptr)) {
        std::cerr << "[Buffer 2] Erro ao liberar vaga.\n";
        return false;
    }

    return true;
}

bool Buffer2::WaitObject(HANDLE objeto) const {
    if (CloseEvent_ == nullptr) {
        return WaitForSingleObject(objeto, INFINITE)
            == WAIT_OBJECT_0;
    }

    HANDLE objetos[2] = {CloseEvent_, objeto};

    DWORD resultado = WaitForMultipleObjects(
        2, objetos, FALSE, INFINITE
    );

    if (resultado == WAIT_OBJECT_0) {
        return false;
    }

    return resultado == WAIT_OBJECT_0 + 1;
}

bool Buffer2::WaitMutex() const {
    DWORD resultado = WaitForSingleObject(
        mutex_, INFINITE
    );

    return resultado == WAIT_OBJECT_0 ||
           resultado == WAIT_ABANDONED;
}

void Buffer2::CloseHandles() {
    if (message_semaph_ != nullptr) {
        CloseHandle(message_semaph_);
        message_semaph_ = nullptr;
    }

    if (vagas_semaph_ != nullptr) {
        CloseHandle(vagas_semaph_);
        vagas_semaph_ = nullptr;
    }

    if (mutex_ != nullptr) {
        CloseHandle(mutex_);
        mutex_ = nullptr;
    }
}
  