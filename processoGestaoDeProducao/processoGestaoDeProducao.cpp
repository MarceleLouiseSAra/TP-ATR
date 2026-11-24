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

#define NOME_MAISLOT L"\\\\.\\mailslot\\comunicacaoPCP"
#define NOME_MAILSLOT_CONTROLE_GESTAO L"\\\\.\\mailslot\\controleGestao"

typedef unsigned* CAST_LPDWORD;

struct mensagem {
	int codigo;
	char conteudo[256];
};

struct mensagemPCP {
	unsigned int TIPO = 2;         // Sempre "2"
	unsigned int NSEQ;             // Número sequencial da mensagem
	std::string HORA;              // Hora prevista para início da OP
	unsigned int DURACAO;          // Tempo previsto para execução da OP (minutos)
	char OP[9];                    // Número da Ordem de Produção (OP) em execução considera o '\0' no final
};
struct DadosRecepcao {
    HANDLE mailslotHandle;
    HANDLE eventoDeEncerramento;
	HANDLE eventoAlternarGestao;
	HANDLE mailslotControleGestao;
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

bool IsAInterpretavelMessage(const std::string& texto, mensagemPCP& mensagem){
	if (texto.size() != 31) return false;
	
	// Mensagens sao separadas por "/"
	if (texto[1] != '/' || texto[7] != '/' || texto[16] != '/' || texto[22] != '/') return false;
	 try {
        if (texto[0] != '2') return false;

        mensagem.TIPO = 2;
        mensagem.NSEQ = std::stoul(texto.substr(2, 5));
        mensagem.HORA = texto.substr(8, 8);
        mensagem.DURACAO = std::stoul(texto.substr(17, 5));
        std::string op = texto.substr(23, 8);
        strcpy_s(mensagem.OP, op.c_str());

 	} catch (const std::exception&) {
        return false;
    }

    return true;    
}
void limparConsole() {

    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO info;

    if (!GetConsoleScreenBufferInfo(console, &info)) return;   
    DWORD tamanho = static_cast<DWORD>(info.dwSize.X)* static_cast<DWORD>(info.dwSize.Y);

    DWORD caracteresEscritos;
    COORD inicio = {0, 0};

    FillConsoleOutputCharacterW(console, L' ', tamanho, inicio, &caracteresEscritos );
    FillConsoleOutputAttribute( console,info.wAttributes,tamanho,inicio,&caracteresEscritos);
	SetConsoleCursorPosition(console, inicio);
}

unsigned __stdcall receberMensagens(void* param) {

	
    DadosRecepcao* dados = static_cast<DadosRecepcao*>(param);

    HANDLE mailslotHandle = dados->mailslotHandle;
    HANDLE eventoDeEncerramento = dados->eventoDeEncerramento;
	HANDLE eventoAlternarGestao = dados->eventoAlternarGestao;
	HANDLE mailslotControleGestao = dados->mailslotControleGestao;

	bool gestaoBloqueada = false;
	HANDLE eventosControle[2] = { eventoDeEncerramento,  eventoAlternarGestao};
      

	if (mailslotHandle == INVALID_HANDLE_VALUE) {
		std::cout << "[Processo de Gestão de Produção] Handle do mailslot inválido. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	DWORD tamanhoDaMensagem;
	DWORD quantidadeDeMensagens;
	DWORD bytesLidos;

	//mensagemPCP Mensagem{};
	char mensagemRecebida[32] = {};

	
	while (true) {
      
		
		DWORD estado = WaitForMultipleObjects(2, eventosControle,FALSE,  50);

		if (estado == WAIT_OBJECT_0) break;
		

		if (estado == WAIT_OBJECT_0 + 1) {
			gestaoBloqueada = !gestaoBloqueada;
			std::cout << "[Gestao] Exibicao da Gestao de Producao " << (gestaoBloqueada ? "BLOQUEADA" : "LIBERADA")	<< std::endl;
		} else if (estado == WAIT_FAILED) {
			std::cout << "[Gestao] Erro ao aguardar eventos: "<< GetLastError() << std::endl;
			return 1;
		}

		// Verifica comandos de controle da Gestao
		DWORD tamanhoComando;
		DWORD quantidadeComandos;

		if (!GetMailslotInfo(mailslotControleGestao,nullptr,&tamanhoComando,&quantidadeComandos,nullptr)) {
			std::cout << "[Gestao] Erro ao consultar mailslot de controle: "<< GetLastError() << std::endl;
			return 1;
		}

		if (quantidadeComandos > 0) {

			char comando[7] = {};
			DWORD bytesComando = 0;
			if (!ReadFile(mailslotControleGestao,comando,sizeof(comando) - 1,&bytesComando,	nullptr)) {
				std::cout << "[Gestao] Erro ao receber comando: "	<< GetLastError() << std::endl;
				return 1;
			}
			if (std::string(comando, bytesComando) == "LIMPAR") limparConsole();
			
		}

		if (gestaoBloqueada) continue;
		

		bool existemMensagens = GetMailslotInfo(mailslotHandle,	NULL,&tamanhoDaMensagem,&quantidadeDeMensagens,NULL);

		if (!existemMensagens) {
			std::cout << "[Processo de Gestão de Produção] Erro ao obter informações do mailslot. Código de saída: " << GetLastError() << std::endl;
			return 1;
		}

		if (quantidadeDeMensagens > 0) {
			if (tamanhoDaMensagem >= sizeof(mensagemRecebida)) {
				std::cout << "Mensagem maior que o buffer de recepcao." << std::endl;
				return 1;
			}
			bool leituraBemSucedida = ReadFile(
				mailslotHandle,
				mensagemRecebida,
				sizeof(mensagemRecebida) - 1,
				&bytesLidos,
				NULL
			);

			if (!leituraBemSucedida) {
				std::cout << "[Processo de Gestão de Produção] Erro ao ler mensagem do mailslot. Código de saída: " << GetLastError() << std::endl;
				return 1;

			} else {
				if (bytesLidos != 31) {
					std::cout << "[Gestao] Tamanho de mensagem invalido: " << bytesLidos << std::endl;
					continue;
				}
				mensagemRecebida[bytesLidos] = '\0';
				//std::cout << "[Processo de Gestão de Produção] Mensagem recebida: "<< mensagemRecebida << std::endl;
				 mensagemPCP mensagem{};

				if (!IsAInterpretavelMessage(mensagemRecebida, mensagem)) {
					std::cout << "[Gestao] Mensagem PCP invalida."<< std::endl;
					continue;
				}
				std::cout << "[Gestao] NSEQ: "
					<< std::setfill('0') << std::setw(5) << mensagem.NSEQ
					<< " | OP: " << mensagem.OP
					<< " | HORA DE INICIO: " << mensagem.HORA
					<< " | DURACAO: " << std::setw(5) << mensagem.DURACAO
					<< std::setfill(' ')
					<< std::endl;
			}
		}		
	}
	return 0;
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
	return 1;
}

int main() {

	SetConsoleOutputCP(CP_UTF8); // Configura a saída do console para UTF-8
	std::cout << "[Processo de Gestão de Produção] Este é o processo de gestão de produção.\n";

	HANDLE eventoDeEncerramento = OpenEventW(SYNCHRONIZE, FALSE, L"eventoDeEncerramentoDosProcessosFilhos");

	if (eventoDeEncerramento == nullptr) {
		std::cout << "[Processo de Gestão de Produção] Erro ao abrir o evento de encerramento. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	// Evento de bloqueio/desbloqueio da Gestao de Producao
	HANDLE eventoAlternarGestao = OpenEventW( SYNCHRONIZE, FALSE,L"eventoDeAlternarEstadoDaExibicaoDaGestaoDeProducao");

	if (eventoAlternarGestao == nullptr) {
		std::cout << "[Processo de Gestao de Producao] Erro ao abrir evento da tecla g: "<< GetLastError() << std::endl;
		CloseHandle(eventoDeEncerramento);
		return 1;
	}


	HANDLE eventoCriacaoMailSlot = CreateEventW(nullptr,  true,  false,  L"eventoCriacaoMailSlot");

	if (eventoCriacaoMailSlot == nullptr) {
		std::cout << "[Processo de Gestão de Produção] Erro ao abrir o evento de criação do mailslot. Código de saída: " << GetLastError() << std::endl;
		return 1;
	}

	HANDLE mailslotHandle = criarMailslot(eventoCriacaoMailSlot);

	if (mailslotHandle == INVALID_HANDLE_VALUE) return 1;

	// Cria o mailslot exclusivo para comandos da Gestao
	HANDLE mailslotControleGestao = CreateMailslotW(NOME_MAILSLOT_CONTROLE_GESTAO, 6, 0, nullptr);

	if (mailslotControleGestao == INVALID_HANDLE_VALUE) {
		std::cout << "[Gestao] Erro ao criar mailslot de controle: "<< GetLastError() << std::endl;
		CloseHandle(mailslotHandle);
		CloseHandle(eventoCriacaoMailSlot);
		CloseHandle(eventoAlternarGestao);
		CloseHandle(eventoDeEncerramento);

		return 1;
	}

	std::cout << "[Gestao] Mailslot de controle criado com sucesso."<< std::endl;

	//--------------------------------------------------
	//	        CRIAÇÃO DE THREADS SECUNDÁRIAS
	//--------------------------------------------------
	
	// Criação de uma thread de monitoramento do evento de encerramento
	HANDLE threadDeMonitoramento;
	DWORD threadDeMonitoramentoId;

	threadDeMonitoramento = (HANDLE)_beginthreadex(NULL, 0, monitorarEncerramento, eventoDeEncerramento, 0, (CAST_LPDWORD)&threadDeMonitoramentoId);

	if (threadDeMonitoramento) 	std::cout << "[Processo de Gestão de Produção] Thread de monitoramento criada com sucesso. ID da thread: " << threadDeMonitoramentoId << std::endl;
	else std::cout << "[Processo de Gestão de Produção] Erro na criação da thread de monitoramento. Código: " << GetLastError() << std::endl;

	// Criação de uma thread para receber mensagens do mailslot
	HANDLE threadReceptora;
	DWORD threadReceptoraId;
	DadosRecepcao dadosRecepcao = {
		mailslotHandle,
		eventoDeEncerramento,
		eventoAlternarGestao,
		mailslotControleGestao
	};
	
	threadReceptora = (HANDLE)_beginthreadex( NULL, 0, receberMensagens, &dadosRecepcao, 0, (CAST_LPDWORD) &threadReceptoraId );
		

	if (threadReceptora) std::cout << "[Processo de Gestão de Produção] Thread recptora criada com sucesso. ID da thread: " << threadReceptoraId << std::endl;
	else std::cout << "[Processo de Gestão de Produção] Erro na criação da thread recptora. Código: " << GetLastError() << std::endl;
	

	
	//--------------------------------------------------
	//    FECHA OS HANDLES DOS EVENTOS E MAILSLOT
	//--------------------------------------------------

	unsigned int resultadoDeMonitorarEncerramento = monitorarEncerramento(eventoDeEncerramento);

	if (resultadoDeMonitorarEncerramento == 0) {
		std::cout << "[Processo de Gestão de Produção] Processo de gestão de produção encerrado idealmente." << std::endl;
	} else {
		std::cout << "[Processo de Gestão de Produção] O processo de gestão de produção foi encerrado com erro." << std::endl;
	}

	//--------------------------------------------------
	//	     	 AGUARDA O TÉRMINO DAS THREADS
	//			     E FECHA SEUS HANDLES
	//--------------------------------------------------
	if (threadReceptora != nullptr) {
    WaitForSingleObject(threadReceptora, INFINITE);
    CloseHandle(threadReceptora);
	}

	if (threadDeMonitoramento != nullptr) {
		WaitForSingleObject(threadDeMonitoramento, INFINITE);
		CloseHandle(threadDeMonitoramento);
	}

	CloseHandle(mailslotControleGestao);
	CloseHandle(eventoAlternarGestao);
	CloseHandle(eventoDeEncerramento);
	CloseHandle(mailslotHandle);
	CloseHandle(eventoCriacaoMailSlot);

	return 0;
}
