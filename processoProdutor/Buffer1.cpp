#include "Buffer1.h"

#include <iostream>
#include <stdexcept>
#include <utility>

Buffer1::Buffer1(HANDLE CloseEvent)
    : CloseEvent_(CloseEvent),
      mutex_(CreateMutexW(nullptr, FALSE, nullptr)),
      vagas_semaph_(CreateSemaphoreW(nullptr, static_cast<LONG>(capcity), static_cast<LONG>(capcity), nullptr)),
      message_semaph_(CreateSemaphoreW(nullptr, 0,static_cast<LONG>(capcity), nullptr)) {
    if (mutex_ == nullptr || vagas_semaph_ == nullptr || message_semaph_ == nullptr) {
        CloseHandles();
        throw std::runtime_error("Falha ao crirar objetos de sincronizacao do Buffer1");
    }
}

Buffer1::~Buffer1() { CloseHandles(); }

bool Buffer1::Insert(const std::string& message, const char* origem) {
    if (message.size() > max_tam) {
        std::cerr << "Buffer 1: mensagem tem mais de " << max_tam << " caracteres. Cancela operacao\n";
        return false;
    }

    // Termino tem prioridade
    if (CloseEvent_ != nullptr && WaitForSingleObject(CloseEvent_, 0) == WAIT_OBJECT_0) {
        return false;
    }

    // Tenta pegar vaga, avisa e espera se n tem
    const DWORD r = WaitForSingleObject(vagas_semaph_, 0);
    if (r == WAIT_TIMEOUT) {
        std::cout << "[Buffer 1] Lista cheia: " << origem << " bloqueada aguardando vaga\n";
        if (!WaitObjct(vagas_semaph_)) { return false; }
        std::cout << "[Buffer 1] Vaga liberada: " << origem << " retomou\n";
    } else if (r != WAIT_OBJECT_0) {
        return false;
    }

    if (!WaitMutex()) {
        ReleaseSemaphore(vagas_semaph_, 1, nullptr);
        return false;
    }

    data_[end_] = message;
    end_ = (end_ + 1) % capcity;

    ReleaseMutex(mutex_);

    if (!ReleaseSemaphore(message_semaph_, 1, nullptr)) {
        std::cerr << "Buffer 1: falha ao sinalizar mensagem disponivel. Codigo: " << GetLastError() << '\n';
        return false;
    }

    return true;
}

bool Buffer1::Remove(std::string& message) {
    // Se estiver vazio, a tarefa 3 aguarda uma mensagem.
    if (!WaitObjct(message_semaph_)) { return false; }
    if (!WaitMutex()) { 
        ReleaseSemaphore(message_semaph_, 1, nullptr);
        return false;
    }

    message = std::move(data_[begin_]);
    data_[begin_].clear();
    begin_ = (begin_ + 1) % capcity;

    ReleaseMutex(mutex_);

    // Libera para qualquer uma das produtoras.
    if (!ReleaseSemaphore(vagas_semaph_, 1, nullptr)) {
        std::cerr << "Buffer 1: falha ao liberar vaga. Codigo: " << GetLastError() << '\n';
        return false;
    }
    return true;
}

bool Buffer1::WaitObjct(HANDLE objeto) const {
    if (CloseEvent_ == nullptr) { return WaitForSingleObject(objeto, INFINITE) == WAIT_OBJECT_0;  }

    HANDLE objetos[2] = { CloseEvent_, objeto };
    const DWORD resultado =  WaitForMultipleObjects(2, objetos, FALSE, INFINITE);

    //3vento de encerramento tem prioridade se ambos estiverem sinalizados.
    if (resultado == WAIT_OBJECT_0) {
        return false;
    }

    return resultado == WAIT_OBJECT_0 + 1;
}

bool Buffer1::WaitMutex() const {
    const DWORD resultado = WaitForSingleObject(mutex_, INFINITE);
    return resultado == WAIT_OBJECT_0 || resultado == WAIT_ABANDONED;
}

void Buffer1::CloseHandles() {
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