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

HANDLE criarMailslot() {

	HANDLE mailslotHandle = CreateMailslotW(
		NOME_MAISLOT,
		0,									// Tamanho máximo da mensagem (0 significa tamanho ilimitado)
		MAILSLOT_WAIT_FOREVER,				// Tempo de espera para leitura (infinito)
		NULL								// Segurança padrão
	);

	if (mailslotHandle == INVALID_HANDLE_VALUE) {
		std::cout << "Erro ao criar o mailslot. Código de saída: " << GetLastError() << std::endl;
		return INVALID_HANDLE_VALUE;
	} else {
		std::cout << "Mailslot criado com sucesso." << std::endl;
		return mailslotHandle;
	}

}

void receberMensagens(HANDLE mailslotHandle) {

	if (mailslotHandle == INVALID_HANDLE_VALUE) {
		std::cout << "Handle do mailslot inválido. Código de saída: " << GetLastError() << std::endl;
		return;
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
			std::cout << "Erro ao obter informações do mailslot. Código de saída: " << GetLastError() << std::endl;
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
				std::cout << "Erro ao ler mensagem do mailslot. Código de saída: " << GetLastError() << std::endl;
				break;

			} else {
				std::cout << "Mensagem recebida: " << "" << std::endl;
			}
		}

		Sleep(50);
	}
}

unsigned __stdcall monitorarEncerramento(void* param) {

	HANDLE eventoDeEncerramento = static_cast<HANDLE>(param);

	if (eventoDeEncerramento == nullptr) {
		std::cout << "Handle do evento inválido. Código de saída: " << GetLastError() << std::endl;
		return 1;

	} else {

		DWORD result = WaitForSingleObject(eventoDeEncerramento, INFINITE);

		if (result == WAIT_OBJECT_0) {

			std::cout << "Evento de encerramento sinalizado. Encerrando o processo de gestão de produção." << std::endl;
			return 0;

		} else if (result == WAIT_FAILED) {

			std::cout << "Erro ao aguardar o evento de encerramento. Código: " << GetLastError() << std::endl;
			return 1;
		}
	}

}

int main() {

	SetConsoleOutputCP(CP_UTF8); // Configura a saída do console para UTF-8
	std::cout << "Este é o processo de gestão de produção.\n";

	HANDLE eventoDeEncerramento = OpenEventW(SYNCHRONIZE,
											 FALSE, 
											 L"eventoDeEncerramentoDosProcessosFilhos");

	if (eventoDeEncerramento == nullptr) {
		std::cout << "Erro ao abrir o evento de encerramento. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	unsigned int resultadoDeMonitorarEncerramento = monitorarEncerramento(eventoDeEncerramento);

	if (resultadoDeMonitorarEncerramento == 0) {
		std::cout << "Processo de gestão de produção encerrado idealmente." << std::endl;
	} else {
		std::cout << "O processo de gestão de produção foi encerrado com erro." << std::endl;
	}

	HANDLE mailslotHandle = criarMailslot();

	if (mailslotHandle == INVALID_HANDLE_VALUE) { 
		return 1;

	} else {
		receberMensagens(mailslotHandle);
	}

	CloseHandle(eventoDeEncerramento);
	CloseHandle(mailslotHandle);

	return 0;
}
