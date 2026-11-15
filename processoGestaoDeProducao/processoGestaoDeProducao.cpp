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

#define NOME_MAISLOT L"\\\\.\\mailslot\\comunicacaoPCP"

typedef unsigned* CAST_LPDWORD;

struct mensagem {
	int codigo;
	char conteudo[256];
};

struct mensagemPCP {
	unsigned int TIPO = 2;         // Sempre "2"
	unsigned int NSEQ;             // Número sequencial da mensagem
	time_t HORA = time(nullptr);   // Hora prevista para início da OP
	unsigned int DURACAO;          // Tempo previsto para execução da OP (minutos)
	char OP[8];                    // Número da Ordem de Produção (OP) em execução
};

HANDLE criarMailslot(HANDLE eventoCriacaoMailSlot) {

	HANDLE mailslotHandle = CreateMailslotW(
		NOME_MAISLOT,
		0,									// Tamanho máximo da mensagem (0 significa tamanho ilimitado)
		MAILSLOT_WAIT_FOREVER,				// Tempo de espera para leitura (infinito)
		NULL								// Segurança padrão
	);

	if (mailslotHandle == INVALID_HANDLE_VALUE) {
		std::cout << "[Processo de Gestão de Produção] Erro ao criar o mailslot no processo de gestão de produção. Código de saída: " << GetLastError() << std::endl;
		return INVALID_HANDLE_VALUE;
	} else {
		std::cout << "[Processo de Gestão de Produção] Mailslot criado com sucesso." << std::endl;

		if (!SetEvent(eventoCriacaoMailSlot)) {
			std::cout << "[Processo de Gestão de Produção] Erro ao sinalizar o evento de criação do mailslot. Código de saída: " << GetLastError() << std::endl;
			return INVALID_HANDLE_VALUE;
		}
		else {
			std::cout << "[Processo de Gestão de Produção] Evento de criação do mailslot sinalizado com sucesso." << std::endl;
		}

		return mailslotHandle;
	}

}

unsigned __stdcall receberMensagens(void* param) {

	HANDLE mailslotHandle = static_cast<HANDLE>(param);

	if (mailslotHandle == INVALID_HANDLE_VALUE) {
		std::cout << "[Processo de Gestão de Produção] Handle do mailslot inválido. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	DWORD tamanhoDaMensagem;
	DWORD quantidadeDeMensagens;
	DWORD bytesLidos;

	mensagemPCP Mensagem{};

	while(true) {
		bool existemMensagens = GetMailslotInfo(
			mailslotHandle,
			NULL,
			&tamanhoDaMensagem,
			&quantidadeDeMensagens,
			NULL						
		);

		if (!existemMensagens) {
			std::cout << "[Processo de Gestão de Produção] Erro ao obter informações do mailslot. Código de saída: " << GetLastError() << std::endl;
			break;
		}

		if (quantidadeDeMensagens > 0) {

			bool leituraBemSucedida = ReadFile(
				mailslotHandle,
				&Mensagem,
				sizeof(Mensagem),
				&bytesLidos,
				NULL
			);

			if (!leituraBemSucedida) {
				std::cout << "[Processo de Gestão de Produção] Erro ao ler mensagem do mailslot. Código de saída: " << GetLastError() << std::endl;
				return 1;

			} else {
				std::cout << "[Processo de Gestão de Produção] Mensagem recebida: " << "" << std::endl;
			}
		}

		return 0;
	}
}

unsigned __stdcall monitorarEncerramento(void* param) {

	HANDLE eventoDeEncerramento = static_cast<HANDLE>(param);

	if (eventoDeEncerramento == nullptr) {
		std::cout << "[Processo de Gestão de Produção] Handle do evento inválido. Código de saída: " << GetLastError() << std::endl;
		return 1;

	} else {

		DWORD result = WaitForSingleObject(eventoDeEncerramento, INFINITE);

		if (result == WAIT_OBJECT_0) {
			std::cout << "[Processo de Gestão de Produção] Evento de encerramento sinalizado. Encerrando o processo de gestão de produção." << std::endl;
			return 0;

		} else if (result == WAIT_FAILED) {
			std::cout << "[Processo de Gestão de Produção] Erro ao aguardar o evento de encerramento. Código: " << GetLastError() << std::endl;
			return 1;
		}
	}

}

int main() {

	SetConsoleOutputCP(CP_UTF8); // Configura a saída do console para UTF-8
	std::cout << "[Processo de Gestão de Produção] Este é o processo de gestão de produção.\n";

	HANDLE eventoDeEncerramento = OpenEventW(SYNCHRONIZE,
											 FALSE, 
											 L"eventoDeEncerramentoDosProcessosFilhos");

	if (eventoDeEncerramento == nullptr) {
		std::cout << "[Processo de Gestão de Produção] Erro ao abrir o evento de encerramento. Código de saída: " << GetLastError() << std::endl;
		//return 1;
	}

	HANDLE eventoCriacaoMailSlot = CreateEventW(nullptr,
								   true,
								   false,
								   L"eventoCriacaoMailSlot");

	if (eventoCriacaoMailSlot == nullptr) {
		std::cout << "[Processo de Gestão de Produção] Erro ao abrir o evento de criação do mailslot. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	HANDLE mailslotHandle = criarMailslot(eventoCriacaoMailSlot);

	if (mailslotHandle == INVALID_HANDLE_VALUE) { 
		return 1;
	}

	//--------------------------------------------------
	//	        CRIAÇÃO DE THREADS SECUNDÁRIAS
	//--------------------------------------------------
	
	// Criação de uma thread de monitoramento do evento de encerramento
	HANDLE threadDeMonitoramento;
	DWORD threadDeMonitoramentoId;

	threadDeMonitoramento = (HANDLE)_beginthreadex(NULL,
												   0,
												   monitorarEncerramento,
												   eventoDeEncerramento,
												   0,
												   (CAST_LPDWORD)&threadDeMonitoramentoId
	);

	if (threadDeMonitoramento) {
		std::cout << "[Processo de Gestão de Produção] Thread de monitoramento criada com sucesso. ID da thread: " << threadDeMonitoramentoId << std::endl;
	}
	else {
		std::cout << "[Processo de Gestão de Produção] Erro na criação da thread de monitoramento. Código: " << GetLastError() << std::endl;
	}

	// Criação de uma thread para receber mensagens do mailslot
	HANDLE threadReceptora;
	DWORD threadReceptoraId;

	threadReceptora = (HANDLE)_beginthreadex(NULL,
		0,
		monitorarEncerramento,
		eventoDeEncerramento,
		0,
		(CAST_LPDWORD)&threadReceptoraId
	);

	if (threadReceptora) {
		std::cout << "[Processo de Gestão de Produção] Thread recptora criada com sucesso. ID da thread: " << threadReceptoraId << std::endl;
	}
	else {
		std::cout << "[Processo de Gestão de Produção] Erro na criação da thread recptora. Código: " << GetLastError() << std::endl;
	}

	//--------------------------------------------------
	//	     	 AGUARDA O TÉRMINO DAS THREADS
	//			     E FECHA SEUS HANDLES
	//--------------------------------------------------



	//--------------------------------------------------
	//    FECHA OS HANDLES DOS EVENTOS E MAILSLOT
	//--------------------------------------------------

	unsigned int resultadoDeMonitorarEncerramento = monitorarEncerramento(eventoDeEncerramento);

	if (resultadoDeMonitorarEncerramento == 0) {
		std::cout << "[Processo de Gestão de Produção] Processo de gestão de produção encerrado idealmente." << std::endl;
	} else {
		std::cout << "[Processo de Gestão de Produção] O processo de gestão de produção foi encerrado com erro." << std::endl;
	}

	CloseHandle(eventoDeEncerramento);
	CloseHandle(mailslotHandle);

	return 0;
}
