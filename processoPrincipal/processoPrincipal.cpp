#define WIN32_LEAN_AND_MEAN
#include <windows.h>    // MAX_PATH, CreateProcess, GetModuleFileName
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <filesystem>
#include <conio.h>		// _getch

//--------------------------------------------------
//     INICIALIZAÇÃO DA ESTRUTURA DE CONTROLE
//--------------------------------------------------

struct processo {
	bool processStatus;
	STARTUPINFO si;
	PROCESS_INFORMATION newProcessInfo;
	std::string processName;
	std::wstring path;
};

processo Produtor;
processo Exibicao;
processo GestaoDeProducao;

bool inicializarProcesso(processo &processo) {

	processo.processStatus = CreateProcess(
		processo.path.c_str(),		  // Caminho do executável
		NULL,                         // Argumentos da linha de comando
		NULL,                         // Atributos de segurança do processo
		NULL,                         // Atributos de segurança da thread
		FALSE,                        // Herança de identificadores
		NORMAL_PRIORITY_CLASS,        // Flags de criação
		NULL,                         // Ambiente do novo processo
		NULL,                         // Diretório de trabalho do novo processo
		&processo.si,                 // Informações de inicialização
		&processo.newProcessInfo)     // Informações do processo criado
	;

	if (!processo.processStatus) {
		std::cout << "Erro na criação do processo " << processo.processName << "! Codigo: " << GetLastError() << std::endl;
		return false;
	}
	else {
		std::cout << "Processo " << processo.processName << " criado com sucesso! PID: " << processo.newProcessInfo.dwProcessId << std::endl;
	}

	return true;

}

//--------------------------------------------------
//	     MECANISMOS PARA ESTABELECIMENTO DE 
//		     COMUNICAÇÃO ENTRE PROCESSOS
//--------------------------------------------------

//--------------------------------------------------
//		INICIALIZAÇÃO DOS DEMAIS EXECUTÁVEIS
//--------------------------------------------------

namespace fs = std::filesystem;

int main() {

	wchar_t currentPath[MAX_PATH];

	DWORD dwResult = GetModuleFileName(NULL, 
					  currentPath, 
					  MAX_PATH);

	if (dwResult == 0) {
		std::cout << "Erro ao obter o caminho do executável atual. Codigo: " << GetLastError() << std::endl;
		return 1;
	}
	else if (dwResult > MAX_PATH) {
		std::cout << "O caminho pode ter sido truncado." << std::endl;
		return 1;
	}

	fs::path directory = fs::path(currentPath).parent_path(); // Obtém o diretório do executável atual

	// Bloco de montagem dos caminhos dos executáveis dos processos filhos
	Produtor.path = directory / L"processoProdutor.exe";
	Exibicao.path = directory / L"processoExibicao.exe";
	GestaoDeProducao.path = directory / L"processoGestaoDeProducao.exe";

	// Bloco de inicialização dos atributos dos processos filhos
	Produtor.processStatus = FALSE;
	ZeroMemory(&Produtor.si, sizeof(Produtor.si));
	ZeroMemory(&Produtor.newProcessInfo, sizeof(Produtor.newProcessInfo));
	Produtor.si.cb = sizeof(Produtor.si);
	Produtor.processName = "Processo Produtor";

	Exibicao.processStatus = FALSE;
	ZeroMemory(&Exibicao.si, sizeof(Exibicao.si));
	ZeroMemory(&Exibicao.newProcessInfo, sizeof(Exibicao.newProcessInfo));
	Exibicao.si.cb = sizeof(Exibicao.si);
	Exibicao.processName = "Processo de Exibição";

	GestaoDeProducao.processStatus = FALSE;
	ZeroMemory(&GestaoDeProducao.si, sizeof(GestaoDeProducao.si));
	ZeroMemory(&GestaoDeProducao.newProcessInfo, sizeof(GestaoDeProducao.newProcessInfo));
	GestaoDeProducao.si.cb = sizeof(GestaoDeProducao.si);
	GestaoDeProducao.processName = "Processo de Gestão de Produção";

	return 0;
}

//--------------------------------------------------
//	   MONITORAMENTO DA INTEGRIDADE DE EXECUÇÃO
//--------------------------------------------------

//--------------------------------------------------
//		INTEREÇÃO DO USUÁRIO COM O SISTEMA
//--------------------------------------------------