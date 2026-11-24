#define WIN32_LEAN_AND_MEAN
#include <windows.h>    // MAX_PATH, CreateProcess, GetModuleFileName
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <filesystem>
#include <conio.h>		// _getch
#include <sstream>
#include <iomanip>

#define NOME_MAILSLOT_EXIBICAO \
    L"\\\\.\\mailslot\\comunicacaoSupervisorio"

typedef unsigned *CAST_LPDWORD;

struct DadosRecepcaoExibicao {
    HANDLE mailslotHandle;
    HANDLE eventoDeEncerramento;
};
struct mensagemSupervisorio {
    unsigned int TIPO;
    unsigned int NSEQ;
    float TZ1;
    float TZ2;
    float TZ3;
    float VOL;
    float PRES;
    std::string TIMESTAMP;
};

bool interpretarMensagemSupervisorio(const std::string& texto, mensagemSupervisorio& mensagem) {
    if (texto.size() != 50)  return false;
    // Verifica os separadores do protocolo
    if (texto[1] != '/' || texto[7] != '/' || texto[13] != '/' || texto[19] != '/' || texto[25] != '/' || texto[31] != '/' || texto[37] != '/') return false;

    try {
        if (texto[0] != '1')  return false;        

        mensagem.TIPO = 1;
        mensagem.NSEQ = std::stoul(texto.substr(2, 5));

        mensagem.TZ1 = std::stof(texto.substr(8, 5));
        mensagem.TZ2 = std::stof(texto.substr(14, 5));
        mensagem.TZ3 = std::stof(texto.substr(20, 5));

        mensagem.VOL = std::stof(texto.substr(26, 5));
        mensagem.PRES = std::stof(texto.substr(32, 5));

        mensagem.TIMESTAMP = texto.substr(38, 12);

    } catch (const std::exception&) {
        return false;
    }

    return true;
}


HANDLE criarMailslotExibicao() {

    HANDLE mailslotHandle = CreateMailslotW(NOME_MAILSLOT_EXIBICAO, 0, MAILSLOT_WAIT_FOREVER, nullptr);

    if (mailslotHandle == INVALID_HANDLE_VALUE) {
        std::cout<< "[Processo de Exibicao] Erro ao criar mailslot. Codigo: " << GetLastError() << std::endl;
        return INVALID_HANDLE_VALUE;
    }
    std::cout << "[Processo de Exibicao] Mailslot criado com sucesso."  << std::endl;
    return mailslotHandle;
}
  
unsigned __stdcall receberMensagensSupervisorio(void* param) {

    DadosRecepcaoExibicao* dados =
        static_cast<DadosRecepcaoExibicao*>(param);

    HANDLE mailslotHandle = dados->mailslotHandle;
    HANDLE eventoDeEncerramento = dados->eventoDeEncerramento;

    if (mailslotHandle == INVALID_HANDLE_VALUE ||
        eventoDeEncerramento == nullptr) {
        return 1;
    }

    DWORD tamanhoDaMensagem;
    DWORD quantidadeDeMensagens;
    DWORD bytesLidos;

    char mensagemRecebida[51] = {};

    while (true) {

        DWORD estado = WaitForSingleObject(
            eventoDeEncerramento, 0
        );

        if (estado == WAIT_OBJECT_0) {
            break;
        }

        if (estado != WAIT_TIMEOUT) {
            std::cout << "[Exibicao] Erro ao verificar encerramento."
                      << std::endl;
            return 1;
        }

        BOOL existemMensagens = GetMailslotInfo(
            mailslotHandle,
            nullptr,
            &tamanhoDaMensagem,
            &quantidadeDeMensagens,
            nullptr
        );

        if (!existemMensagens) {
            std::cout << "[Exibicao] Erro ao consultar mailslot: "
                      << GetLastError() << std::endl;
            return 1;
        }

        if (quantidadeDeMensagens > 0) {

            if (tamanhoDaMensagem != 50) {
                std::cout << "[Exibicao] Tamanho de mensagem invalido."
                          << std::endl;
                return 1;
            }

            BOOL leituraBemSucedida = ReadFile(
                mailslotHandle,
                mensagemRecebida,
                sizeof(mensagemRecebida) - 1,
                &bytesLidos,
                nullptr
            );

            if (!leituraBemSucedida) {
                std::cout << "[Exibicao] Erro ao ler mensagem: "
                          << GetLastError() << std::endl;
                return 1;
            }

            if (bytesLidos != 50) {
                std::cout << "[Exibicao] Quantidade de bytes invalida."
                          << std::endl;
                return 1;
            }

            mensagemRecebida[bytesLidos] = '\0';

            mensagemSupervisorio mensagem{};

			if (!interpretarMensagemSupervisorio(
					mensagemRecebida, mensagem)) {

				std::cout << "[Exibicao] Mensagem invalida."
						<< std::endl;
				continue;
			}
			std::ostringstream saida;

			saida << "[Exibicao] NSEQ: "
				<< std::setfill('0') << std::setw(5) << mensagem.NSEQ
				<< std::fixed << std::setprecision(1)
				<< " | TZ1: " << std::setw(5) << mensagem.TZ1
				<< " | TZ2: " << std::setw(5) << mensagem.TZ2
				<< " | TZ3: " << std::setw(5) << mensagem.TZ3
				<< " | VOL: " << std::setw(5) << mensagem.VOL
				<< " | PRES: " << std::setw(5) << mensagem.PRES
				<< " | TIMESTAMP: " << mensagem.TIMESTAMP;

			std::cout << saida.str() << std::endl;
		}

        DWORD espera = WaitForSingleObject(
            eventoDeEncerramento, 50
        );

        if (espera == WAIT_OBJECT_0) {
            break;
        }

        if (espera == WAIT_FAILED) {
            std::cout << "[Exibicao] Erro na espera: "
                      << GetLastError() << std::endl;
            return 1;
        }
    }

    return 0;
}
unsigned __stdcall monitorarEncerramento(void* param) {

	HANDLE eventoDeEncerramento = static_cast<HANDLE>(param);

	if (eventoDeEncerramento == nullptr) {
		std::cout << "Hanlde do evento inválido. Código de saída: " << GetLastError() << std::endl;
		return 1;

	} else {

		DWORD result = WaitForSingleObject(eventoDeEncerramento, INFINITE);

		if (result == WAIT_OBJECT_0) {

			std::cout << "Evento de encerramento sinalizado. Encerrando o processo de exibição." << std::endl;
			return 0;

		} else if (result == WAIT_FAILED) {

			std::cout << "Erro ao aguardar o evento de encerramento. Código: " << GetLastError() << std::endl;
			return 1;
		}
	}

	return 0;
}

int main() {

	SetConsoleOutputCP(CP_UTF8); // Configura a saída do console para UTF-8
    std::cout << "Este é o processo de exibição.\n";

	// Seção de abertura de eventos
	HANDLE eventoDeEncerramento = OpenEventW(SYNCHRONIZE,
											 FALSE,
										 	 L"eventoDeEncerramentoDosProcessosFilhos");

	if (eventoDeEncerramento == nullptr) {
		std::cout << "Erro ao abrir o evento de encerramento. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	HANDLE eventoDeLimparJanelaDaTarefaDeExibicaoDosDadosDeProcesso = OpenEventW(SYNCHRONIZE,
		FALSE,
		L"eventoDeLimparJanelaDaTarefaDeExibicaoDosDadosDeProcesso");

	if (eventoDeLimparJanelaDaTarefaDeExibicaoDosDadosDeProcesso == nullptr) {
		std::cout << "Erro ao abrir o evento de limpeza do console de Exibição. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	HANDLE eventoDeLimparJanelaDaTarefaDeGestaoDaProducao = OpenEventW(SYNCHRONIZE,
		FALSE,
		L"eventoDeLimparJanelaDaTarefaDeGestaoDaProducao");

	if (eventoDeLimparJanelaDaTarefaDeGestaoDaProducao == nullptr) {
		std::cout << "Erro ao abrir o evento de limpeza do console de Exibição. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	// Seção de criação de eventos
	HANDLE eventoCriacaoMailslotExibicao = CreateEventW(
		nullptr,
		TRUE,
		FALSE,
		L"eventoCriacaoMailslotExibicao"
	);

	if (eventoCriacaoMailslotExibicao == nullptr) {
		std::cout << "Erro ao criar evento: " << GetLastError() << std::endl;
		CloseHandle(eventoDeEncerramento);
		return 1;
	}

	// Continua chamando a mesma função
	HANDLE mailslotHandle = criarMailslotExibicao();

	if (mailslotHandle == INVALID_HANDLE_VALUE) {
		CloseHandle(eventoCriacaoMailslotExibicao);
		CloseHandle(eventoDeEncerramento);
		return 1;
	}

	// Avisa que o mailslot está pronto
	if (!SetEvent(eventoCriacaoMailslotExibicao)) {
		std::cout << "Erro ao sinalizar evento: "<< GetLastError() << std::endl;
		CloseHandle(mailslotHandle);
		CloseHandle(eventoCriacaoMailslotExibicao);
		CloseHandle(eventoDeEncerramento);
		return 1;
	}

	
	DadosRecepcaoExibicao dadosRecepcao = {
		mailslotHandle,
		eventoDeEncerramento
	};

	// Cria a thread receptora do Supervisorio
	unsigned int threadReceptoraId = 0;

	HANDLE threadReceptora = (HANDLE)_beginthreadex(
		nullptr,
		0,
		receberMensagensSupervisorio,
		&dadosRecepcao,
		0,
		&threadReceptoraId
	);

	if (threadReceptora == nullptr) {
		std::cout << "[Exibicao] Erro ao criar thread receptora."
				<< std::endl;

		// Aguarda o encerramento geral antes de liberar os recursos.
		monitorarEncerramento(eventoDeEncerramento);

		CloseHandle(mailslotHandle);
		CloseHandle(eventoCriacaoMailslotExibicao);
		CloseHandle(eventoDeEncerramento);
		return 1;
	}

	std::cout << "[Exibicao] Thread receptora criada. ID: "
			<< threadReceptoraId << std::endl;

	unsigned int resultadoDeMonitorarEncerramento =
		monitorarEncerramento(eventoDeEncerramento);
  

	if (resultadoDeMonitorarEncerramento == 0) {
		std::cout << "Processo de exibição encerrado idealmente." << std::endl;
	} else {
		std::cout << "O processo de exibição foi encerrado com erro." << std::endl;
	}

	
	DWORD esperaThread =
		WaitForSingleObject(
			threadReceptora,
			INFINITE
		);

	if (esperaThread != WAIT_OBJECT_0) {
		std::cout
			<< "[Exibicao] Erro ao aguardar thread: "
			<< GetLastError()
			<< std::endl;
		return 1;
	}

	CloseHandle(threadReceptora);
	CloseHandle(mailslotHandle);
	CloseHandle(eventoCriacaoMailslotExibicao);
	CloseHandle(eventoDeEncerramento);

	return 0;
      
}
