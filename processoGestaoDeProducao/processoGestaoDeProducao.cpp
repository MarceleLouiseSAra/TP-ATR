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

void monitoraEncerramento(HANDLE eventoDeEncerramento) {

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
	std::cout << "Este é o processo de gestão de produção.\n";

	monitoraEncerramento();

	return 0;
}
