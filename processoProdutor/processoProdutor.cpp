// processoProdutor.cpp : Este arquivo contém a função 'main'. A execução do programa começa e termina ali.
//
#include <iostream>
#include <string>
#include <windows.h>
#include <process.h>
#include <conio.h>

#include "Buffer1.h"

// Estrutura com os dados compartilhados entre as threads
struct DadosCompartilhados {
    Buffer1* buffer;
    HANDLE CloseEvent;
};

#define NTHREADS 4

//              TAREFA 1 - SUPERVISÓRIO

unsigned __stdcall tarefa1(void* param) {

    DadosCompartilhados* dados = (DadosCompartilhados*)param;

    int contador = 1;

    while (WaitForSingleObject(dados->CloseEvent, 0) != WAIT_OBJECT_0) {

        std::string mensagem =
            "Supervisório: mensagem " + std::to_string(contador++);

        if (!dados->buffer->Insert(mensagem, "Supervisório")) {
            break;
        }

        Sleep(1000);
    }

    std::cout << "Tarefa 1 encerrada." << std::endl;

    return 0;
}


//                  TAREFA 2 - PCP

unsigned __stdcall tarefa2(void* param) {

    DadosCompartilhados* dados = (DadosCompartilhados*)param;

    int contador = 1;

    while (WaitForSingleObject(dados->CloseEvent, 0) != WAIT_OBJECT_0) {

        std::string mensagem =
            "PCP: mensagem " + std::to_string(contador++);

        if (!dados->buffer->Insert(mensagem, "PCP")) {
            break;
        }

        Sleep(1000);
    }

    std::cout << "Tarefa 2 encerrada." << std::endl;

    return 0;
}

//              TAREFA 3 - CAPTURA

unsigned __stdcall tarefa3(void* param) {

    DadosCompartilhados* dados = (DadosCompartilhados*)param;

    std::string mensagem;

    while (dados->buffer->Remove(mensagem)) {

        std::cout << "[Tarefa 3] Mensagem recebida: "
                  << mensagem << std::endl;
    }

    std::cout << "Tarefa 3 encerrada." << std::endl;

    return 0;
}

unsigned __stdcall monitorarEncerramento(HANDLE eventoDeEncerramento) {

    if (eventoDeEncerramento != nullptr) {
        std::cout << "Evento de encerramento sinalizado. Encerrando o processo de exibição." << std::endl;
        DWORD result = WaitForSingleObject(eventoDeEncerramento, INFINITE);

        if (result == WAIT_OBJECT_0) {
            ExitProcess(0);
        }
        else {
            std::cout << "Erro ao aguardar o evento de encerramento. Código: " << GetLastError() << std::endl;
        }
    }
}

int main() {

    //evento de encerramento
    HANDLE CloseEvent = CreateEventW(nullptr,TRUE,FALSE,nullptr);

    if (CloseEvent == nullptr) {
        std::cout << "Erro ao criar evento de encerramento. Código: " << GetLastError() << std::endl;
        return 1;
    }

   //inicilaiza o buffer

    Buffer1 buffer(CloseEvent);

    DadosCompartilhados dados;
    dados.buffer = &buffer;
    dados.CloseEvent = CloseEvent;

    //threads

    HANDLE threads[NTHREADS] = {};
    unsigned threadIds[NTHREADS] = {};

    threads[0] = (HANDLE)_beginthreadex(nullptr, 0, tarefa1, &dados, 0, &threadIds[0] );

    threads[1] = (HANDLE)_beginthreadex(nullptr, 0, tarefa2, &dados, 0, &threadIds[1] );

    threads[2] = (HANDLE)_beginthreadex(nullptr, 0, tarefa3, &dados, 0, &threadIds[2] );

    threads[3] = (HANDLE)_beginthreadex(nullptr, 0, monitorarEncerramento, &dados, 0, &threadIds[3]);

    int threadsCriadas = 0;

    for (int i = 0; i < NTHREADS; i++) {
        if (threads[i] != nullptr) {
            threadsCriadas++;
            std::cout << "Thread da Tarefa " << i + 1 << " criada. ID: " << threadIds[i] << std::endl;
        }
        else {
            std::cout << "Erro ao criar a thread da Tarefa " << i + 1 << ". Código: " << GetLastError() << std::endl;
        }
    }

    //encerra threads
    if (threadsCriadas == NTHREADS) {
        std::cout << "\nProcesso Produtor em execução." << std::endl;

        std::cout << "Pressione qualquer tecla para encerrar o teste."<< std::endl;

        _getch(); //tem que ver isso aqui
    }

    // Sinaliza o encerramento das tarefas
    SetEvent(CloseEvent);

    // Aguarda todas as threads que foram criadas
    if (threadsCriadas > 0) {

        HANDLE threadsAtivas[NTHREADS];
        int quantidade = 0;

        for (int i = 0; i < NTHREADS; i++) {
            if (threads[i] != nullptr) {
                threadsAtivas[quantidade++] = threads[i];
            }
        }

        WaitForMultipleObjects(quantidade,threadsAtivas,TRUE,INFINITE);
    }

    //libera os trem

    for (int i = 0; i < NTHREADS; i++) {
        if (threads[i] != nullptr) {
            CloseHandle(threads[i]);
        }
    }

    //buffer.CloseHandles(); closehandles é privado

    CloseHandle(CloseEvent);

    std::cout << "\nProcesso Produtor encerrado." << std::endl;

    return 0;
}