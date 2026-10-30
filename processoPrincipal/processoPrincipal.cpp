#define WIN32_LEAN_AND_MEAN
#include <windows.h>    // MAX_PATH, CreateProcess, GetModuleFileName
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <filesystem>
#include <conio.h>		// _getch

// Variáveis globais
typedef unsigned *CAST_LPDWORD;
const DWORD processesNumber = 3;
HANDLE processos[processesNumber];

//--------------------------------------------------
//     INICIALIZAÇÃO DA ESTRUTURA DE CONTROLE
//--------------------------------------------------

struct processo {
	bool processStatus;
	STARTUPINFO si;
	PROCESS_INFORMATION newProcessInfo;
	std::string processName;
	std::wstring processPath;
};

processo Produtor;
processo Exibicao;
processo GestaoDeProducao;

bool inicializarProcesso(processo &processo) {

	processo.processStatus = CreateProcess(
		processo.processPath.c_str(), // Caminho do executável, criado durante a compilação
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
//   FUNÇÃO DE MONITORAMENTO DOS PROCESSOS FILHOS
//--------------------------------------------------

unsigned __stdcall monitorarProcesso(void* param) {

	std::cout << "Thread secundária iniciou o monitoramento dos processos filhos." << std::endl;

	DWORD waitResult = WaitForMultipleObjects(processesNumber, 
											  (HANDLE*)param,
											  FALSE,
											  INFINITE);

	if (waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + processesNumber) {
		int index = waitResult - WAIT_OBJECT_0;
		std::cout << "O pProcesso filho " << index + 1 << " finalizou." << std::endl;
	} else {
		std::cout << "Erro no monitoramento dos processos filhos. Codigo: " << GetLastError() << std::endl;
	}
}

//--------------------------------------------------
//	     MECANISMOS PARA ESTABELECIMENTO DE 
//		     COMUNICAÇÃO ENTRE PROCESSOS
//--------------------------------------------------

namespace fs = std::filesystem;

int main() {

	//--------------------------------------------------
	//	  OBTENÇÃO DO DIRETÓRIO DOS EXECUTÁVEIS
	//--------------------------------------------------

	wchar_t currentPath[MAX_PATH];

	DWORD dwResult = GetModuleFileName(NULL, 
									   currentPath, 
									   MAX_PATH);

	if (dwResult == 0) {
		std::cout << "Erro ao obter o caminho do executável atual. Codigo: " << GetLastError() << std::endl;
		return 1;
	}
	else if (dwResult > MAX_PATH) {
		std::cout << "O caminho do executável atual pode ter sido truncado." << std::endl;
		return 1;
	}

	fs::path directory = fs::path(currentPath).parent_path(); // Obtém o diretório do executável atual

	// Bloco de montagem dos caminhos dos executáveis dos processos filhos
	Produtor.processPath = directory / L"processoProdutor.exe";
	Exibicao.processPath = directory / L"processoExibicao.exe";
	GestaoDeProducao.processPath = directory / L"processoGestaoDeProducao.exe";

	//--------------------------------------------------
	// INICIALIZAÇÃO DOS ATRIBUTOS DOS PROCESSOS FILHOS
	//--------------------------------------------------

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

	//--------------------------------------------------
	//		    INICIALIZAÇÃO DOS EXECUTÁVEIS 
	//			    DOS PROCESSOS FILHOS
	//--------------------------------------------------

	inicializarProcesso(Produtor);
	inicializarProcesso(Exibicao);
	inicializarProcesso(GestaoDeProducao);

	//--------------------------------------------------
	//	MONITORAMENTO DOS ESTADOS DOS PROCESSOS FILHOS
	//--------------------------------------------------

	processos[0] = Produtor.newProcessInfo.hProcess;
	processos[1] = Exibicao.newProcessInfo.hProcess;
	processos[2] = GestaoDeProducao.newProcessInfo.hProcess;

	// Criação de uma thread dedicada ao monitoramento dos estados dos demais executáveis

	HANDLE thread;
	DWORD threadId, threadExitCode;

	thread = (HANDLE)_beginthreadex(NULL,
						            0,
						            monitorarProcesso,
						            processos,
						            0,
						            (CAST_LPDWORD) & threadId
	);

	if (thread) {
		std::cout << "Thread de monitoramento criada com sucesso! ID da thread: " << threadId << std::endl;
	}
	else {
		std::cout << "Erro na criação da thread de monitoramento. Codigo: " << GetLastError() << std::endl;
	}

	//--------------------------------------------------
	//		INTERAÇÃO DO USUÁRIO COM O SISTEMA
	//--------------------------------------------------



	//--------------------------------------------------
	//	 AGUARDA O TÉRMINO DA THREAD DE MONITORAMENTO
	//               E FECHA SEU HANDLE
	//--------------------------------------------------

	GetExitCodeThread(thread, &threadExitCode);
	std::cout << "Thread de monitoramento finalizada com o seguinte código de saída: " << threadExitCode << std::endl;

	CloseHandle(thread);

	return 0;
}