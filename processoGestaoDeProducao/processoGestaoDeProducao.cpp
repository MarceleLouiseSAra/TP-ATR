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
		std::cout << "O processo de gestão de produção foi encerrado com erro." << std::endl;
	} else {
		std::cout << "Processo de gestão de produção encerrado idealmente." << std::endl;
	}

	CloseHandle(eventoDeEncerramento);

	return 0;
}
