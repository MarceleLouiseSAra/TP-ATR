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

unsigned __stdcall monitorarEncerramento(void* param) {

    HANDLE eventoDeEncerramento = static_cast<HANDLE>(param);

    if (eventoDeEncerramento == nullptr) {
        std::cout << "Handle do evento inválido. Código de saída: " << GetLastError() << std::endl;
        return 1;

    } else {

        DWORD result = WaitForSingleObject(eventoDeEncerramento, INFINITE);

        if (result == WAIT_OBJECT_0) {
            std::cout << "Evento de encerramento sinalizado. Encerrando o processo produtor." << std::endl;
            return 0;

        } else if (result == WAIT_FAILED) {
            std::cout << "Erro ao aguardar o evento de encerramento. Código: " << GetLastError() << std::endl;
            return 1;
        }
    }

}

int main() {

    //evento de encerramento externo
    HANDLE eventoDeEncerramento = OpenEventW(SYNCHRONIZE,
                                             FALSE,
                                             L"eventoDeEncerramentoDosProcessosFilhos");

    if (eventoDeEncerramento == nullptr) {
        std::cout << "Erro ao abrir o evento de encerramento. Código de saída: " << GetLastError() << std::endl;
        return 1;
    }

    //evento de encerramento interno
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

    threads[3] = (HANDLE)_beginthreadex(nullptr, 0, monitorarEncerramento, eventoDeEncerramento, 0, &threadIds[3]);

    int threadsCriadas = 0;

    for (int i = 0; i < NTHREADS; i++) {
        if (threads[i] != nullptr) {
            threadsCriadas++;
            std::cout << "Thread da Tarefa " << i + 1 << " criada. ID: " << threadIds[i] << std::endl;
        }
        else {
            std::cout << "Erro ao criar a thread da Tarefa " << i + 1 << ". Código: " << GetLastError() << std::endl;
            SetEvent(CloseEvent);
        }
    }

   //encerra threads
   if (threadsCriadas == NTHREADS) {
        std::cout << "\nProcesso Produtor em execução." << std::endl;
        //std::cout << "Pressione qualquer tecla para encerrar o teste." << std::endl;
        //_getch(); //tem que ver isso aqui

        DWORD waitThreadDeMonitoramento = WaitForSingleObject(threads[3], INFINITE); // Espera que a thread de monitoramento detecte a operação do programa principal

        if (waitThreadDeMonitoramento == WAIT_OBJECT_0) {

            DWORD codigoDeSaidaDaThreadDeMonitoramento;

            if (GetExitCodeThread(threads[3], &codigoDeSaidaDaThreadDeMonitoramento)) {

                if (codigoDeSaidaDaThreadDeMonitoramento == 0) {
                    std::cout << "O processo principal solicitou o encerramento dos processos-filhos." << std::endl;
                    // Sinaliza o encerramento das tarefas
                    SetEvent(CloseEvent);
                }
            } else {
                std::cout << "A thread de monitoramento terminou com erro." << std::endl;
            }

        } else {
            std::cout << "Erro ao aguardar a sinalização da thread de monitoramento. Código de saída: " << GetLastError() << std::endl;
        }

        // Aguarda todas as threads que foram criadas
        if (threadsCriadas > 0) {

            HANDLE threadsAtivas[NTHREADS];
            int quantidade = 0;

            for (int i = 0; i < NTHREADS; i++) {
                if (threads[i] != nullptr) {
                    threadsAtivas[quantidade++] = threads[i];
                }
            }

            DWORD waitDemaisThreads = WaitForMultipleObjects(quantidade,threadsAtivas,TRUE,INFINITE);

            if (waitDemaisThreads == WAIT_OBJECT_0) {
                std::cout << "Demais threads devidamente finalizadas." << std::endl;

            } else if (waitDemaisThreads == WAIT_FAILED) {
                std::cout << "Erro ao aguardar a finalização das demais threads. Código de saída: " << GetLastError() << std::endl;

            } else {
                std::cout << "Resultado inesperado ao aguardar o término das demais threads." << std::endl;
            }
        }
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