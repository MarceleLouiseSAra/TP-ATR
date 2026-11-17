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
#include <random>

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
        std::cout << "[Processo Produtor] Ocorreu um erro na formatação." << std::endl;
        return "";
    }

    return mensagemFormatada;
}

std::string formatarMensagem(const mensagemPCP& mensagem) {    
    std::ostringstream stream;
    
    stream << mensagem.TIPO << "/"
        << std::setw(5)
        << std::setfill('0')
        << mensagem.NSEQ << "/"
        << mensagem.HORA << "/"
        << std::setw(5)
        << std::setfill('0')
        << mensagem.DURACAO << "/"
        << mensagem.OP;      

    std::string mensagemFormatada = stream.str();

    if (mensagemFormatada.size() != 31) {
        std::cout << "[Processo Produtor] Ocorreu um erro na formatação." << std::endl;
        return "";
    }

    return mensagemFormatada;
}
// 8 sobrecarga de funções: a partir do parâmetro, o compilador decide quem chamar

HANDLE abrirMailslot() { // o processo de gestão de produção precisa ter criado o mailslot antes do produtor tentar abri-lo

    std::cout << "[Processo Produtor] Tentando estabelecer comunicação via mailslot..." << std::endl;

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
		std::cout << "[Processo Produtor] Erro ao tentar estabelecer a comunicação via mailslot no processo produtor. Código de saída: " << GetLastError() << std::endl;
		return INVALID_HANDLE_VALUE;
	} else {
		std::cout << "[Processo Produtor] Comunicação entre processos via mailslot estabelecida com sucesso." << std::endl;
		return mailslot;
	}

}

bool enviarMensagem(HANDLE mailslotHandle, const std::string& mensagem) {

	if (mailslotHandle == INVALID_HANDLE_VALUE) {
		std::cout << "[Processo Produtor] Handle do mailslot inválido." << std::endl;
		return false;
	}

	DWORD bytesEscritos = 0;

	bool escritaBemSucedida = WriteFile(
		mailslotHandle,
		mensagem.data(),
		static_cast<DWORD>(mensagem.size()),
		&bytesEscritos,
		nullptr
	);

	if (!escritaBemSucedida || bytesEscritos != mensagem.size()) {
		std::cout << "[Processo Produtor] [Tarefa 3] Erro ao enviar mensagem para o mailslot. Código de saída: " << GetLastError() << std::endl;
		return false;
	}

	return true;
}

//              TAREFA 1 - SUPERVISÓRIO

unsigned __stdcall tarefa1(void* param) {

    DadosCompartilhados* dados = (DadosCompartilhados*)param;

    unsigned int contador = 0;
    LARGE_INTEGER tempoInit;

    //Ipmlementação do temporizador definitivo
    HANDLE temporiza = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (temporiza == nullptr) {
        std::cout << "Erro na criacao temporizador: " << GetLastError() << std::endl;
        return 1;
    }

    tempoInit.QuadPart = -5000000; // primeiro disparo
    if (!SetWaitableTimer(temporiza, &tempoInit, 500, nullptr, nullptr, FALSE)) {
        std::cout << "Erro na config do temporizador: " << GetLastError() << std::endl;
        CloseHandle(temporiza);
        return 1;
    }

    while (WaitForSingleObject(dados->CloseEvent, 0) != WAIT_OBJECT_0) {

        mensagemSupervisorio mensagem{};

        mensagem.NSEQ = contador;
        contador = (contador + 1) % 100000;
        mensagem.VOL = 250.5f;
        mensagem.PRES = 3.2f;
        mensagem.TZ1 = 120.5f;
        mensagem.TZ2 = 121.0f;
        mensagem.TZ3 = 119.8f;
        
        SYSTEMTIME agroa;
        GetLocalTime(&agroa);

        std::ostringstream hora;

        hora << std::setfill('0')
                << std::setw(2) << agroa.wHour << ":"
                << std::setw(2) << agroa.wMinute << ":"
                << std::setw(2) << agroa.wSecond << "."
                << std::setw(3) << agroa.wMilliseconds;

        mensagem.TIMESTAMP = hora.str();

        std::string mensagemormatada = formatarMensagem(mensagem);

        /*std::string mensagem =
            "Supervisório: mensagem " + std::to_string(contador++);*/

        if (!dados->buffer->Insert(mensagemormatada, "Supervisório")) {
            break;
        }
        std::cout << "[Supervisorio] Mensagem: " << mensagemormatada << std::endl;

        HANDLE objetos[2] = { dados->CloseEvent, temporiza};
        DWORD aguardaTime = WaitForMultipleObjects(2, objetos, FALSE, INFINITE);

        if(aguardaTime != WAIT_OBJECT_0 + 1)  break;
        
        
    }

    std::cout << "[Processo Produtor] Tarefa 1 encerrada." << std::endl;

    CancelWaitableTimer(temporiza);
    CloseHandle(temporiza); 
    return 0;
}


//                  TAREFA 2 - PCP

unsigned __stdcall tarefa2(void* param) {

    DadosCompartilhados* dados = (DadosCompartilhados*)param;

    unsigned int contador = 0;

    // Cria temp.
    HANDLE temporiza = CreateWaitableTimerW(nullptr, FALSE, nullptr);

    if (temporiza == nullptr) {
        std::cout << "Erro na criacao temporizador PCP: "<< GetLastError() << std::endl;
        return 1;
    }
    std::random_device rand;
    std::mt19937 generator(rand());
    std::uniform_int_distribution<int> intervlaRand(1000, 5000);

    /* TO COLOCANDO COMO COMENTARIO PQ N SEI SE A MSGM TEM QUE SER A MESMA
    mensagemPCP mensagem{};
    mensagem.NSEQ = contador++;
    mensagem.HORA = time(nullptr);
    mensagem.DURACAO = 120;
    std::string op = "QPA" +
        std::string(4 - std::to_string(contador).length(), '0') +
        std::to_string(contador);
    strcpy_s(mensagem.OP, op.c_str());
    std::string mensagemormatada = formatarMensagem(mensagem);
    */

    while (WaitForSingleObject(dados->CloseEvent, 0) != WAIT_OBJECT_0) {

        /*std::string mensagem =
            "PCP: mensagem " + std::to_string(contador++);
        */
        mensagemPCP mensagem{};

    mensagem.NSEQ = contador;
    contador = (contador + 1) % 100000;

    time_t agora = time(nullptr);
    tm hrLocal{};

    localtime_s(&hrLocal, &agora);

    std::ostringstream hora;
    hora << std::put_time(&hrLocal, "%H:%M:%S");

    mensagem.HORA = hora.str();
    mensagem.DURACAO = 120;

    std::ostringstream op;

    op << "QPA-" << std::setw(4) << std::setfill('0') << (contador % 10000);

    strcpy_s(mensagem.OP, op.str().c_str());

    std::string mensagemormatada = formatarMensagem(mensagem);

        if (!dados->buffer->Insert(mensagemormatada, "PCP")) {
            break;
        }
        std::cout << "[PCP] Mensagem: " << mensagemormatada << std::endl;

        int intervalo = intervlaRand(generator);
        LARGE_INTEGER tempoInit;
        tempoInit.QuadPart = -static_cast<LONGLONG>(intervalo) * 10000; 
        
        if(!SetWaitableTimer(temporiza, &tempoInit, 0, nullptr, nullptr, FALSE)) {
            std::cout << "Erro na config do temp PCP: " << GetLastError() << std::endl;
            break;
        }
        HANDLE objetos[2] = { dados->CloseEvent, temporiza };
        DWORD aguardaTime = WaitForMultipleObjects(2, objetos, FALSE, INFINITE);
        if (aguardaTime != WAIT_OBJECT_0 + 1) break;

    }

    std::cout << "[Processo Produtor] Tarefa 2 encerrada." << std::endl;
    CancelWaitableTimer(temporiza);
    CloseHandle(temporiza);
    return 0;
}

//              TAREFA 3 - CAPTURA

unsigned __stdcall tarefa3(void* param) {

    DadosCompartilhados* dados = (DadosCompartilhados*)param;

    std::string mensagem;

    while (dados->buffer->Remove(mensagem)) {

        size_t separador = mensagem.find('/');

        if (separador == std::string::npos) {
            std::cout << "[Processo Produtor] [Tarefa1 3] Mensagem inválida: " << mensagem << std::endl;
            continue;
        }

        std::string tipoDaMensagem = mensagem.substr(0, separador);

        if (tipoDaMensagem == "1") {
            // Encaminhar mensagem para o 2º buffer
        } else if (tipoDaMensagem == "2") {
            // Encaminhar a mensagem para o processo de gestão de produção, por meio do mailslot

            if (enviarMensagem((*dados).mailslotHandle, mensagem)) {
                std::cout << "[Processo Produtor] [Tarefa 3] Mensagem PCP encaminhada ao processo de gestão de produção: " << mensagem << std::endl;
            } else {
                std::cout << "[Processo Produtor] [Tarefa 3] Erro ao encaminhar a mensagem ao processo de gestão de produção." << std::endl;
            }

        } else {
            std::cout << "[Processo Produtor] [Tarefa1 3] Tipo da mensagem desconhecido: " << mensagem << std::endl;
        }
    }

    std::cout << "[Processo Produtor] Tarefa 3 encerrada." << std::endl;

    return 0;
}

unsigned __stdcall monitorarEncerramento(void* param) {

    HANDLE eventoDeEncerramento = static_cast<HANDLE>(param);

    if (eventoDeEncerramento == nullptr) {
        std::cout << "[Processo Produtor] Handle do evento inválido. Código de saída: " << GetLastError() << std::endl;
        return 1;

    } else {

        DWORD result = WaitForSingleObject(eventoDeEncerramento, INFINITE);

        if (result == WAIT_OBJECT_0) {
            std::cout << "[Processo Produtor] Evento de encerramento sinalizado. Encerrando o processo produtor." << std::endl;
            return 0;

        } else if (result == WAIT_FAILED) {
            std::cout << "[Processo Produtor] Erro ao aguardar o evento de encerramento. Código: " << GetLastError() << std::endl;
            return 1;
        }
    }
    return 1;
}

int main() {

    //evento de encerramento interno
    HANDLE CloseEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    if (CloseEvent == nullptr) {
        std::cout << "[Processo Produtor] Erro ao criar evento de encerramento. Código de saída: " << GetLastError() << std::endl;
        return 1;
    }

    //evento de encerramento externo
    HANDLE eventoDeEncerramento = OpenEventW(SYNCHRONIZE,
                                             FALSE,
                                             L"eventoDeEncerramentoDosProcessosFilhos");

    if (eventoDeEncerramento == nullptr) {
        std::cout << "[Processo Produtor] Erro ao abrir o evento de encerramento. Código de saída: " << GetLastError() << std::endl;
        return 1;
    }

   // Sleep(10000); // processo produtor espera pela criação do evento de criação do mailslot antes de tentar abri-lo
   /* HANDLE eventoCriacaoMailSlot = OpenEventW(SYNCHRONIZE,
                                             FALSE,
                                             L"eventoCriacaoMailSlot");
*/

HANDLE eventoCriacaoMailSlot =  CreateEventW( nullptr,   TRUE,  FALSE,  L"eventoCriacaoMailSlot");
      
    if (eventoCriacaoMailSlot == nullptr) {
        std::cout << "[Processo Produtor] Erro ao abrir o evento de criação do mailslot. Código de saída: " << GetLastError() << std::endl;
        return 1;
    }

	//WaitForSingleObject(eventoCriacaoMailSlot, 10000); // Aguarda o sinal do evento de criação do mailslot antes de tentar abrir o mailslot
    DWORD resultado = WaitForSingleObject(
        eventoCriacaoMailSlot,
        10000
        );

    if (resultado != WAIT_OBJECT_0) {
        std::cout << "[Processo Produtor] Erro ou timeout aguardando o mailslot." << std::endl;
        CloseHandle(eventoCriacaoMailSlot);
        return 1;
    }

    CloseHandle(eventoCriacaoMailSlot);

    HANDLE mailslotHandle = abrirMailslot(); // Abre a conexão com o processo de gestão de produção via mailslot

    if (mailslotHandle == INVALID_HANDLE_VALUE) {
        std::cout << "[Processo Produtor] Erro ao abrir mailslot. Código de saída: " << GetLastError() << std::endl;
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
            std::cout << "[Processo Produtor] Thread da Tarefa " << i + 1 << " criada. ID: " << threadIds[i] << std::endl;
        }
        else {
            std::cout << "[Processo Produtor] [Processo Produtor] Erro ao criar a thread da Tarefa " << i + 1 << ". Código: " << GetLastError() << std::endl;
            SetEvent(CloseEvent);
        }
    }

   //encerra threads
   if (threadsCriadas == NTHREADS) {
        std::cout << "[Processo Produtor] \nProcesso Produtor em execução." << std::endl;
        //std::cout << "[Processo Produtor] Pressione qualquer tecla para encerrar o teste." << std::endl;
        //_getch(); //tem que ver isso aqui

        DWORD waitThreadDeMonitoramento = WaitForSingleObject(threads[3], INFINITE); // Espera que a thread de monitoramento detecte a operação do programa principal

        if (waitThreadDeMonitoramento == WAIT_OBJECT_0) {

            DWORD codigoDeSaidaDaThreadDeMonitoramento;

            if (GetExitCodeThread(threads[3], &codigoDeSaidaDaThreadDeMonitoramento)) {

                if (codigoDeSaidaDaThreadDeMonitoramento == 0) {
                    std::cout << "[Processo Produtor] O processo principal solicitou o encerramento dos processos-filhos." << std::endl;
                    // Sinaliza o encerramento das tarefas
                    SetEvent(CloseEvent);
                }
            } else {
                std::cout << "[Processo Produtor] A thread de monitoramento terminou com erro." << std::endl;
            }

        } else {
            std::cout << "[Processo Produtor] Erro ao aguardar a sinalização da thread de monitoramento. Código de saída: " << GetLastError() << std::endl;
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
                std::cout << "[Processo Produtor] Demais threads devidamente finalizadas." << std::endl;

            } else if (waitDemaisThreads == WAIT_FAILED) {
                std::cout << "[Processo Produtor] Erro ao aguardar a finalização das demais threads. Código de saída: " << GetLastError() << std::endl;

            } else {
                std::cout << "[Processo Produtor] Resultado inesperado ao aguardar o término das demais threads." << std::endl;
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

    std::cout << "[Processo Produtor] \nProcesso Produtor encerrado." << std::endl;

    return 0;
}