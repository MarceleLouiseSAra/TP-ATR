// processoProdutor.cpp : Este arquivo contém a função 'main'. A execução do programa começa e termina ali.
//
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <windows.h>
#include <process.h>
#include <conio.h>
#include <ctime>

#include "Buffer1.h"

#define NTHREADS 4
#define NOME_MAISLOT L"\\\\.\\mailslot\\comunicacaoPCP"

// "\\\\.\\" - Computador local
// "mailslot\\" - mecaninismo de mailslot
// "comunicacaoPCP" - nome do mailslot

// Estrutura com os dados compartilhados entre as threads
struct DadosCompartilhados {
    Buffer1* buffer;
    HANDLE CloseEvent;
    HANDLE mailslotHandle;
};

struct mensagemSupervisorio {
    unsigned int TIPO = 1;         // Sempre "1"
    unsigned int NSEQ;             // Número sequencial da mensagem
    float VOL;                     // Volume de clínquer (?) do forno
    float PRES;                    // Pressão interna no forno rotativo
    float TZ1;                     // Temperatura da zona 1 do forno rotativo
    float TZ2;                     // Temperatura da zona 2 do forno rotativo
    float TZ3;                     // Temperatura da zona  do forno rotativo
    std::string TIMESTAMP;         // Hora, minuto, segundo e milissegundo
};

struct mensagemPCP {
    unsigned int TIPO = 2;         // Sempre "2"
    unsigned int NSEQ;             // Número sequencial da mensagem
    std::string HORA;              // Hora prevista para início da OP
    unsigned int DURACAO;          // Tempo previsto para execução da OP (minutos)
    char OP[9];                    // Número da Ordem de Produção (OP) em execução
};

std::string formatarMensagem(const mensagemSupervisorio& mensagem) {
    std::ostringstream stream;

    stream << mensagem.TIPO << "/"
        << std::setw(5) << std::setfill('0') << mensagem.NSEQ << "/"
        << std::setw(5) << std::fixed << std::setprecision(1) << mensagem.TZ1 << "/"
        << std::setw(5) << mensagem.TZ2 << "/"
        << std::setw(5) << mensagem.TZ3 << "/"
        << std::setw(5) << mensagem.VOL << "/"
        << std::setw(5) << mensagem.PRES << "/"
        << mensagem.TIMESTAMP;

    std::string mensagemFormatada = stream.str();

    if (mensagemFormatada.size() != 50) {
        std::cout << "Ocorreu um erro na formatação." << std::endl;
        return "";
    }

    return mensagemFormatada;
}

std::string formatarMensagem(const mensagemPCP& mensagem) {
    
    std::ostringstream stream;

    stream << mensagem.TIPO << "/"
        << std::setw(5) << std::setfill('0') << mensagem.NSEQ << "/"
        << mensagem.OP << "/"
        << mensagem.HORA << "/"
        << std::setw(5) << std::setfill('0') << mensagem.DURACAO;

    std::string mensagemFormatada = stream.str();

    if (mensagemFormatada.size() != 31) {
        std::cout << "Ocorreu um erro na formatação." << std::endl;
        return "";
    }

    return mensagemFormatada;
}
// 8 sobrecarga de funções: a partir do parâmetro, o compilador decide quem chamar

HANDLE abrirMailslot() { // o processo de gestão de produção precisa ter criado o mailslot antes do produtor tentar abri-lo
    HANDLE mailslot = CreateFileW(
        NOME_MAISLOT,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

	if (mailslot == INVALID_HANDLE_VALUE) {
		std::cout << "Erro ao abrir o mailslot. Código de saída: " << GetLastError() << std::endl;
		return INVALID_HANDLE_VALUE;
	}
	else {
		std::cout << "Comunicação entre processos via mailslot estabelecida com sucesso." << std::endl;
		return mailslot;
	}

}

bool enviarMensagem(HANDLE mailslotHandle, const mensagemPCP& Mensagem) {

	if (mailslotHandle == INVALID_HANDLE_VALUE) {
		std::cout << "Handle do mailslot inválido." << std::endl;
		return false;
	}

	DWORD bytesEscritos;

	bool escritaBemSucedida = WriteFile(
		mailslotHandle,
		&Mensagem,
		sizeof(mensagemPCP),
		&bytesEscritos,
		nullptr
	);

	if (!escritaBemSucedida || bytesEscritos != sizeof(mensagemPCP)) {
		std::cout << "Erro ao enviar mensagem para o mailslot. Código de saída: " << GetLastError() << std::endl;
		return false;
	}

	return true;
}

//              TAREFA 1 - SUPERVISÓRIO

unsigned __stdcall tarefa1(void* param) {

    DadosCompartilhados* dados = (DadosCompartilhados*)param;

    unsigned int contador = 0;

    while (WaitForSingleObject(dados->CloseEvent, 0) != WAIT_OBJECT_0) {

        mensagemSupervisorio mensagem{};

        mensagem.NSEQ = contador++;
        mensagem.VOL = 250.5f;
        mensagem.PRES = 3.2f;
        mensagem.TZ1 = 120.5f;
        mensagem.TZ2 = 121.0f;
        mensagem.TZ3 = 119.8f;
        mensagem.TIMESTAMP = time(nullptr);

        std::string mensagemormatada = formatarMensagem(mensagem);

        /*std::string mensagem =
            "Supervisório: mensagem " + std::to_string(contador++);*/

        if (!dados->buffer->Insert(mensagemormatada, "Supervisório")) {
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

    unsigned int contador = 0;

    mensagemPCP mensagem{};

    mensagem.NSEQ = contador++;
    mensagem.HORA = time(nullptr);
    mensagem.DURACAO = 120;

    std::string op = "QPA" +
        std::string(4 - std::to_string(contador).length(), '0') +
        std::to_string(contador);

    strcpy_s(mensagem.OP, op.c_str());

    std::string mensagemormatada = formatarMensagem(mensagem);

    while (WaitForSingleObject(dados->CloseEvent, 0) != WAIT_OBJECT_0) {

        /*std::string mensagem =
            "PCP: mensagem " + std::to_string(contador++);*/

        if (!dados->buffer->Insert(mensagemormatada, "PCP")) {
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
    HANDLE CloseEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    if (CloseEvent == nullptr) {
        std::cout << "Erro ao criar evento de encerramento. Código de saída: " << GetLastError() << std::endl;
        return 1;
    }

    // Abre a conexão com o processo de gestão de produção via mailslot
    HANDLE mailslotHandle = abrirMailslot();

    if (mailslotHandle == INVALID_HANDLE_VALUE) {
        std::cout << "Erro ao criar mailslot. Código de saída: " << GetLastError() << std::endl;
        return 1;
    }

    //inicilaiza o buffer
    Buffer1 buffer(CloseEvent);

    // Inicialização de uma instância da estrutura "DadosCompartilhados"
    DadosCompartilhados dados;
    dados.buffer = &buffer;
    dados.CloseEvent = CloseEvent;
    dados.mailslotHandle = mailslotHandle;

    //threads

    HANDLE threads[NTHREADS] = {};
    unsigned threadIds[NTHREADS] = {};

    threads[0] = (HANDLE)_beginthreadex(nullptr, 0, tarefa1, &dados, 0, &threadIds[0]);

    threads[1] = (HANDLE)_beginthreadex(nullptr, 0, tarefa2, &dados, 0, &threadIds[1]);

    threads[2] = (HANDLE)_beginthreadex(nullptr, 0, tarefa3, &dados, 0, &threadIds[2]);

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
    CloseHandle(mailslotHandle);

    std::cout << "\nProcesso Produtor encerrado." << std::endl;

    return 0;
}