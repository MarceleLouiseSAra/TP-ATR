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

// Variáveis globais
typedef unsigned *CAST_LPDWORD;
const DWORD processesNumber = 3;

#define teclaS 0x73
#define teclaM 0x6D
#define teclaR 0x72
#define teclaP 0x70
#define teclaG 0x67
#define tecla1 0x31
#define tecla2 0x32
#define ESC 0x1B

int tecla = 0;

//--------------------------------------------------
//     INICIALIZAÇÃO DA ESTRUTURA DE CONTROLE
//--------------------------------------------------

struct processo {
	bool processStatus = false;
	STARTUPINFO si;
	PROCESS_INFORMATION newProcessInfo;
	std::string processName = "";
	std::wstring processPath = L"";
};

processo Produtor;
processo Exibicao;
processo GestaoDeProducao;

// Estrutura de dados que reúne os processos-filhos e seus respectivos nomes
std::vector<std::pair<processo*, std::string>> processos = {
	{&Produtor, "Processo Produtor"},
	{&Exibicao, "Processo de Exibição"},
	{&GestaoDeProducao, "Processo de Gestão de Produção"}
};

// Estrutura de dados que reúne os handles dos processos-filhos
std::vector<HANDLE> handlesDosProcessos;

// Estrutura de dados que reúne os handles dos eventos de sincronização
std::vector<HANDLE> handlesDosEventos;

bool inicializarProcesso(processo &processo) {

	// Inicialização dos atributos dos processos-filhos
	processo.processStatus = FALSE;
	ZeroMemory(&processo.si, sizeof(processo.si));
	ZeroMemory(&processo.newProcessInfo, sizeof(processo.newProcessInfo));
	processo.si.cb = sizeof(processo.si);

	// Criação dos processos-filhos
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
		&processo.newProcessInfo);    // Informações do processo criado

	if (!processo.processStatus) {
		std::cout << "Erro na criação do processo " << processo.processName << "! Codigo: " << GetLastError() << std::endl;
		return false;
	}
	else {
		std::cout << "Processo " << processo.processName << " criado com sucesso! PID: " << processo.newProcessInfo.dwProcessId << std::endl;
	}

	return true;

}

void fecharHandleDoProcesso(processo& processo) {
	if (processo.newProcessInfo.hThread != NULL) { // Verifica se o handle da thread do processo-filho está preenchido
		// Se sim, fecha-se o handle
		CloseHandle(processo.newProcessInfo.hThread);
	}

	if (processo.newProcessInfo.hProcess != NULL) { // Verifica se o handle do processo-filho está preenchido
		// Se sim, fecha-se o handle
		CloseHandle(processo.newProcessInfo.hProcess);
	}
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
		std::cout << "O processo filho " << index + 1 << " finalizou." << std::endl;
	} else {
		std::cout << "Erro no monitoramento dos processos filhos. Codigo: " << GetLastError() << std::endl;
	}
}

//--------------------------------------------------
//  FUNÇÃO DE EXIBIÇÃO DO MENU DE OPÇÕES DO SISTEMA
//--------------------------------------------------

void menu() {

	do {
		std::cout << "----------------------------------------" << std::endl;
		std::cout << "         MENU DE OPÇÕES DO SISTEMA      " << std::endl;
		std::cout << "----------------------------------------" << std::endl;
		std::cout << "Digite 's' para bloquear/desbloquear a leitura do Supervisório." << std::endl;
		std::cout << "Digite 'm' para bloquear/desbloquear a leitura do sistema PCP." << std::endl;
		std::cout << "Digite 'r' para bloquear/desbloquear a retirada de mensagens." << std::endl;
		std::cout << "Digite 'p' para bloquear/desbloquear a exibição dos dados de processo." << std::endl;
		std::cout << "Digite 'g' para bloquear/desbloquear a exibição da gestão de produção." << std::endl;
		std::cout << "Digite '1' para solicitar a limpeza da janela da tarefa de exibição dos dados de processo." << std::endl;
		std::cout << "Digite '2' para solicitar a limpeza da janela da tarefa de gestão da produção." << std::endl;
		std::cout << "Digite 'ESC' para sair." << std::endl;
		std::cout << "----------------------------------------" << std::endl;

		tecla = _getch(); // Captura a entrada fornecida pelo usuário

		if (tecla == ESC) {
			std::cout << "Saindo do sistema..." << std::endl;
			//SetEvent(eventoDeEncerramentoDosProcessosFilhos);
		}
		else if (tecla == 's') {
			//SetEvent(eventoDeAlternarEstadoDaLeituraDoSupervisorio);
		}
		else if (tecla == 'm') {
			//SetEvent(eventoDeAlternarEstadoDaLeituraDoPCP);
		}
		else if (tecla == 'r') {
			//SetEvent(eventoDeAlternarEstadoDaRetiradaDeMensagens);
		}
		else if (tecla == 'p') {
			//SetEvent(eventoDeAlternarEstadoDaExibicaoDosDadosDeProcesso);
		}
		else if (tecla == 'g') {
			//SetEvent(eventoDeAlternarEstadoDaExibicaoDaGestaoDeProducao);
		}
		else if (tecla == '1') {
			//SetEvent(eventoDeLimparJanelaDaTarefaDeExibicaoDosDadosDeProcesso);
		}
		else if (tecla == '2') {
			//SetEvent(eventoDeLimparJanelaDaTarefaDeGestaoDaProducao);
		}

	} while (tecla != ESC);

}

//--------------------------------------------------
//	     MECANISMOS PARA ESTABELECIMENTO DE 
//		     COMUNICAÇÃO ENTRE PROCESSOS
//--------------------------------------------------

namespace fs = std::filesystem;

int main() {

	//--------------------------------------------------
	//				DECLARAÇÃO DOS EVENTOS
	//--------------------------------------------------

	HANDLE eventoDeEncerramentoDosProcessosFilhos = CreateEventW(nullptr,
																 true,
																 false,
																 nullptr);

	handlesDosEventos.push_back(eventoDeEncerramentoDosProcessosFilhos);

	for (HANDLE handleDeUmEvento : handlesDosEventos) {
		if (handleDeUmEvento == nullptr) {
			std::cout << "Erro na criação do evento de sincronização. Codigo: " << GetLastError() << std::endl;
			return 1;
		} else {
			std::cout << "Evento de sincronização " << handleDeUmEvento << " criado com sucesso." << std::endl;
		}
	}

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
	//		    INICIALIZAÇÃO DOS EXECUTÁVEIS 
	//			    DOS PROCESSOS FILHOS
	//--------------------------------------------------

	bool inicializacaoBemSucedida = true;

	for (auto& [processo, nomeDoProcesso] : processos) {
		if (inicializarProcesso(*processo)) {
			// Se a inicialização for bem-sucedida, adiciona o handle do processo à estrutura 
			handlesDosProcessos.push_back((*processo).newProcessInfo.hProcess);
		} else {
			inicializacaoBemSucedida = false;
			break; // Impedir que os demais processos sejam inicializados
		}
	}

	// Rotina de limpeza como tratamento para inicialização mal-sucedida
	if (!inicializacaoBemSucedida) {

		if (eventoDeEncerramentoDosProcessosFilhos != nullptr) {

			SetEvent(eventoDeEncerramentoDosProcessosFilhos);

			for (HANDLE handleDeUmProcesso : handlesDosProcessos) { // Espera o encerramento de um processo de cada vez
				WaitForSingleObject(handleDeUmProcesso, INFINITE); // Espera indefinidamente pela sinalização
				CloseHandle(handleDeUmProcesso);
			}

			CloseHandle(eventoDeEncerramentoDosProcessosFilhos);
		}

		return 1;
	}

	//--------------------------------------------------
	//	MONITORAMENTO DOS ESTADOS DOS PROCESSOS FILHOS
	//--------------------------------------------------

	// Criação de uma thread dedicada ao monitoramento dos estados dos demais executáveis

	HANDLE thread;
	DWORD threadId;

	thread = (HANDLE)_beginthreadex(NULL,
						            0,
						            monitorarProcesso,
									&handlesDosProcessos,
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

	menu(); // Exibe o menu de opções do sistema e aguarda a entrada do usuário

	//--------------------------------------------------
	//	 AGUARDA O TÉRMINO DA THREAD DE MONITORAMENTO
	//               E FECHA SEU HANDLE
	//--------------------------------------------------

	

	//--------------------------------------------------
	//	    FECHA OS HANDLES DOS PROCESSOS FILHOS
	//--------------------------------------------------

	for (auto& [processo, nomeDoProcesso] : processos) {
		fecharHandleDoProcesso(*processo);
	}

	return 0;
}