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

struct mensagem {
	int codigo;
	char conteudo[256];
};

HANDLE criarMailslot() {

	HANDLE mailslotHandle = CreateMailslotW(
		L"\\\\.\\mailslot\\mailslotTeste",
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

	mensagem Mensagem{};

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
				std::cout << "Mensagem recebida: " << Mensagem.conteudo << std::endl;
			}
		}

		Sleep(50);
	}
}

unsigned __stdcall monitorarEncerramento(void* param) {

	HANDLE eventoDeEncerramento = static_cast<HANDLE>(param);

	if (eventoDeEncerramento == nullptr) {
		std::cout << "Hanlde do evento inválido. Código de saída: " << GetLastError() << std::endl;
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
